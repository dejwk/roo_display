#pragma once

#include <assert.h>
#include <inttypes.h>

#include <memory>

namespace roo_display {
namespace internal {

// Slightly higher than conventional 0.7, mostly so that the smallest hashtable
// (with capacity 11) can still hold 8 elements.
static constexpr float kMaxFillRatio = 0.73;

// Sequence of the largest primes of the format 4n+3, less than 2^k,
// for k = 4 ... 16. When used as hash map capacities, they are known to
// enable quadratic residue search to visit the entire array.
static constexpr uint16_t kRadkePrimes[] = {
    0xb,   0x1f,  0x3b,   0x7f,   0xfb,   0x1f7, 0x3fb,
    0x7f7, 0xffb, 0x1fff, 0x3feb, 0x7fcf, 0xffef};

// These are precalculated (2^48 - 1) / the corresponding radke prime + 1.
// See
// https://lemire.me/blog/2019/02/08/faster-remainders-when-the-divisor-is-a-constant-beating-compilers-and-libdivide/
static constexpr uint64_t kRadkePrimeInverts[] = {
    0x1745d1745d18, 0x84210842109, 0x456c797dd4a, 0x20408102041, 0x105197f7d74,
    0x824a4e60b4,   0x4050647d9e,  0x202428adc4,  0x100501907e,  0x800400201,
    0x401506e65,    0x200c44b25,   0x100110122};

// Returns n % kRadkePrimes[idx].
inline uint16_t fastmod(uint32_t n, int idx) {
  uint64_t lowbits = (kRadkePrimeInverts[idx] * n) & 0x0000FFFFFFFFFFFF;
  return (lowbits * kRadkePrimes[idx]) >> 48;
}

inline int initialCapacityIdx(uint16_t size_hint) {
  uint32_t capacity = (uint32_t)(((float)size_hint) / kMaxFillRatio) + 1;
  for (int radkeIdx = 0; radkeIdx < 12; ++radkeIdx) {
    if (kRadkePrimes[radkeIdx] >= capacity) return radkeIdx;
  }
  return 12;
}

struct DefaultHashFn {
  template <typename T>
  T operator()(const T& val) const {
    return val;
  }
};

// For maps, where Key == Entry.
struct DefaultKeyFn {
  template <typename Entry>
  Entry operator()(const Entry& entry) const {
    return entry;
  }
};

// Memory-conscious small flat hashtable. It can hold up to 64000 elements.
template <typename Entry, typename Key, typename HashFn = DefaultHashFn,
          typename KeyFn = DefaultKeyFn>
class Hashtable {
 public:
  enum State { EMPTY, DELETED, FULL };

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry*;
    using reference = Entry&;

    Iterator() : Iterator(nullptr, 0) {}

    const Entry& operator*() const { return ht_->buffer_[pos_]; }

    void operator++() {
      uint16_t capacity = ht_->capacity();
      do {
        ++pos_;
      } while (pos_ < capacity && ht_->states_[pos_] != FULL);
    }

    bool operator==(const Iterator& other) const {
      return ht_ == other.ht_ && pos_ == other.pos_;
    }

    bool operator!=(const Iterator& other) const {
      return ht_ != other.ht_ || pos_ != other.pos_;
    }

   private:
    friend class Hashtable;

    Iterator(const Hashtable<Entry, Key, HashFn, KeyFn>* ht, uint16_t pos)
        : ht_(ht), pos_(pos) {}

    const Hashtable<Entry, Key, HashFn, KeyFn>* ht_;
    uint16_t pos_;
  };

  Hashtable(HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn())
      : Hashtable(8, hash_fn, key_fn) {}

  Hashtable(uint16_t size_hint, HashFn hash_fn = HashFn(),
            KeyFn key_fn = KeyFn())
      : hash_fn_(hash_fn),
        key_fn_(key_fn),
        capacity_idx_(initialCapacityIdx(size_hint)),
        size_(0),
        resize_threshold_(
            capacity_idx_ == 12
                ? 64000
                : (uint16_t)(((float)kRadkePrimes[capacity_idx_]) *
                             kMaxFillRatio)),
        buffer_(new Entry[kRadkePrimes[capacity_idx_]]),
        states_(new State[kRadkePrimes[capacity_idx_]]) {
    std::fill(&states_[0], &states_[kRadkePrimes[capacity_idx_]], EMPTY);
  }

  Hashtable(Hashtable&& other) = default;
  Hashtable& operator=(Hashtable&& other) = default;

  uint16_t capacity() const { return kRadkePrimes[capacity_idx_]; }

  Iterator begin() const {
    uint16_t cap = capacity();
    uint16_t pos = 0;
    for (; pos < cap; ++pos) {
      if (states_[pos] == FULL) break;
    }
    return Iterator(this, pos);
  }

  Iterator end() const { return Iterator(this, capacity()); }

  uint16_t size() const { return size_; }

  Iterator find(const Key& key) const {
    const uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] == FULL && key_fn_(buffer_[pos]) == key) {
      return Iterator(this, pos);
    }
    const uint16_t cap = capacity();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] == FULL && key_fn_(buffer_[p]) == key) {
        return Iterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  bool contains(const Key& key) const { return find(key).pos_ != capacity(); }

  // Returns {the iterator to the new element, true} if the element was
  // successfuly inserted; {the iterator to an existing element, false} if an
  // entry with the same key has already been in the hashmap.
  std::pair<Iterator, bool> insert(Entry val) {
    Key key = key_fn_(val);
    uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    // Fast path.
    if (states_[pos] == FULL && (pos == val || key_fn_(buffer_[pos]) == key)) {
      return std::make_pair(Iterator(this, pos), false);
    }
    if (size_ >= resize_threshold_) {
      // Before rehashing see if maybe the entry is already in the hashtable.
      Iterator itr = find(val);
      if (itr != end()) {
        return std::make_pair(itr, false);
      }
      // Need to rehash.
      assert(capacity_idx_ < 12);  // Or, exceeded maximum hashtable size.
      Hashtable<Entry, Key, HashFn, KeyFn> newt(capacity(), hash_fn_, key_fn_);
      for (const auto& e : *this) {
        newt.insert(e);
      }
      *this = std::move(newt);
      // Retry the fast path.
      pos = fastmod(hash_fn_(key), capacity_idx_);
      if (states_[pos] == FULL &&
          (pos == val || key_fn_(buffer_[pos]) == key)) {
        return std::make_pair(Iterator(this, pos), false);
      }
    }
    // Fast path for not found.
    if (states_[pos] == EMPTY) {
      states_[pos] = FULL;
      buffer_[pos] = std::move(val);
      ++size_;
      return std::make_pair(Iterator(this, pos), true);
    }
    const uint16_t cap = capacity();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) {
        // We can insert here.
        states_[p] = FULL;
        buffer_[p] = std::move(val);
        ++size_;
        return std::make_pair(Iterator(this, p), true);
      }
      if (states_[p] == FULL &&
          (buffer_[p] == val || key_fn_(buffer_[p]) == key)) {
        return std::make_pair(Iterator(this, p), false);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 private:
  friend class Iterator;

  HashFn hash_fn_;
  KeyFn key_fn_;
  int capacity_idx_;
  uint16_t size_;
  uint16_t resize_threshold_;
  std::unique_ptr<Entry[]> buffer_;
  std::unique_ptr<State[]> states_;
};

template <typename Key, typename HashFn = DefaultHashFn>
using HashSet = Hashtable<Key, Key, HashFn, DefaultKeyFn>;

}  // namespace internal
}  // namespace roo_display