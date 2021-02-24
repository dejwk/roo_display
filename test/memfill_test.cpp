
#include <memory>

#include "gtest/gtest.h"

#include "roo_display/internal/memfill.h"

using namespace testing;

// std::default_random_engine generator;

namespace roo_display {

void TrivialPatternFill(uint8_t* buf, const uint8_t* val, int pattern_size,
                        uint32_t repetitions) {
  while (repetitions-- > 0) {
    for (int i = 0; i < pattern_size; ++i) {
      *buf++ = val[i];
    }
  }
}

class ByteBuffer {
 public:
  ByteBuffer(uint32_t size) : size_(size), data_(new uint8_t[size + 1]) {
    memset(data_.get(), 0, size + 1);
  }

  uint32_t size() const { return size_; }
  const uint8_t* data() const { return data_.get(); }
  uint8_t* data() { return data_.get(); }

  ~ByteBuffer() {
    EXPECT_EQ(0, data_.get()[size_]) << "Write-over detected";
  }

 private:
  uint32_t size_;
  std::unique_ptr<uint8_t[]> data_;
};

std::ostream& operator<<(std::ostream& os, const ByteBuffer& buf) {
  for (uint32_t i = 0; i < buf.size(); ++i) {
    if (i > 0) os << ", ";
    os << (int)buf.data()[i];
  }
  return os;
}

bool operator==(const ByteBuffer& a, const ByteBuffer& b) {
  for (uint32_t i = 0; i < a.size(); ++i) {
    if (a.data()[i] != b.data()[i]) return false;
  }
  return true;
}

class FillerTester {
 public:
  FillerTester(uint32_t size) : size_(size), actual_(size), expected_(size) {}

  void patternFill2(uint32_t pos, uint32_t count, const uint8_t* val) {
    TrivialPatternFill(expected_.data() + pos, val, 2, count);
    pattern_fill<2>(actual_.data() + pos, count, val);
    EXPECT_EQ(expected_, actual_);
  }

  void patternFill3(uint32_t pos, uint32_t count, const uint8_t* val) {
    TrivialPatternFill(expected_.data() + pos, val, 3, count);
    pattern_fill<3>(actual_.data() + pos, count, val);
    EXPECT_EQ(expected_, actual_);
  }

  void patternFill4(uint32_t pos, uint32_t count, const uint8_t* val) {
    TrivialPatternFill(expected_.data() + pos, val, 4, count);
    pattern_fill<4>(actual_.data() + pos, count, val);
    EXPECT_EQ(expected_, actual_);
  }

 private:
  uint32_t size_;
  ByteBuffer actual_;
  ByteBuffer expected_;
};

uint8_t pattern[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

TEST(PatternFill2, Empty) {
  FillerTester tester(32);
  tester.patternFill2(4, 0, pattern);
  tester.patternFill2(5, 0, pattern + 2);
}

TEST(PatternFill2, Seven) {
  FillerTester tester(32);
  tester.patternFill2(5, 7, pattern);
}

TEST(PatternFill2, Eight) {
  FillerTester tester(32);
  tester.patternFill2(5, 8, pattern);
}

TEST(PatternFill2, ShortAligned) {
  FillerTester tester(32);
  tester.patternFill2(4, 1, pattern);
  tester.patternFill2(8, 3, pattern + 2);
}

TEST(PatternFill2, ShortMisaligned) {
  FillerTester tester(32);
  tester.patternFill2(3, 1, pattern);
  tester.patternFill2(5, 3, pattern + 2);
}

TEST(PatternFill2, LongAligned) {
  FillerTester tester(32);
  tester.patternFill2(4, 12, pattern);
}

TEST(PatternFill2, LongMisaligned) {
  FillerTester tester(32);
  tester.patternFill2(3, 12, pattern);
}

TEST(PatternFill2, LongMisalignedEq) {
  FillerTester tester(32);
  uint8_t pattern[] = {0x12, 0x12};
  tester.patternFill2(3, 12, pattern);
}

// Fill3

TEST(PatternFill3, Empty) {
  FillerTester tester(32);
  tester.patternFill3(4, 0, pattern);
  tester.patternFill3(5, 0, pattern + 2);
}

TEST(PatternFill3, Seven) {
  FillerTester tester(32);
  tester.patternFill3(5, 7, pattern);
}

TEST(PatternFill3, Eight) {
  FillerTester tester(32);
  tester.patternFill3(5, 8, pattern);
}

TEST(PatternFill3, ShortAligned) {
  FillerTester tester(32);
  tester.patternFill3(4, 1, pattern);
  tester.patternFill3(8, 3, pattern + 2);
}

TEST(PatternFill3, ShortMisaligned) {
  FillerTester tester(32);
  tester.patternFill3(3, 1, pattern);
  tester.patternFill3(5, 3, pattern + 2);
}

TEST(PatternFill3, LongAligned) {
  FillerTester tester(50);
  tester.patternFill3(4, 12, pattern);
}

TEST(PatternFill3, LongMisaligned) {
  FillerTester tester(50);
  tester.patternFill3(3, 12, pattern);
}

TEST(PatternFill3, LongMisalignedEq) {
  FillerTester tester(50);
  uint8_t pattern[] = {0x12, 0x12};
  tester.patternFill3(3, 12, pattern);
}

TEST(PatternFill3, VeryLongAligned) {
  FillerTester tester(80);
  tester.patternFill3(4, 19, pattern);
}

TEST(PatternFill3, VeryLongMisaligned) {
  FillerTester tester(80);
  tester.patternFill3(3, 19, pattern);
}

TEST(PatternFill3, VeryLongMisalignedEq) {
  FillerTester tester(80);
  uint8_t pattern[] = {0x12, 0x12};
  tester.patternFill3(3, 19, pattern);
}

// Fill4

TEST(PatternFill4, Empty) {
  FillerTester tester(32);
  tester.patternFill4(4, 0, pattern);
  tester.patternFill4(5, 0, pattern + 2);
}

TEST(PatternFill4, Seven) {
  FillerTester tester(50);
  tester.patternFill4(5, 7, pattern);
}

TEST(PatternFill4, Eight) {
  FillerTester tester(50);
  tester.patternFill4(5, 8, pattern);
}

TEST(PatternFill4, ShortAligned) {
  FillerTester tester(50);
  tester.patternFill4(4, 1, pattern);
  tester.patternFill4(8, 3, pattern + 2);
}

TEST(PatternFill4, ShortMisaligned) {
  FillerTester tester(50);
  tester.patternFill4(3, 1, pattern);
  tester.patternFill4(5, 3, pattern + 2);
}

TEST(PatternFill4, LongAligned) {
  FillerTester tester(60);
  tester.patternFill4(4, 12, pattern);
}

TEST(PatternFill4, LongMisaligned1) {
  FillerTester tester(60);
  tester.patternFill4(5, 11, pattern);
}

TEST(PatternFill4, LongMisaligned2) {
  FillerTester tester(60);
  tester.patternFill4(6, 11, pattern);
}

TEST(PatternFill4, LongMisaligned3) {
  FillerTester tester(60);
  tester.patternFill4(7, 11, pattern);
}

TEST(PatternFill4, LongMisalignedEq) {
  FillerTester tester(60);
  uint8_t pattern[] = {0x12, 0x12};
  tester.patternFill4(3, 12, pattern);
}

TEST(PatternFill4, VeryLongAligned) {
  FillerTester tester(100);
  tester.patternFill4(4, 19, pattern);
}

TEST(PatternFill4, VeryLongMisaligned) {
  FillerTester tester(100);
  tester.patternFill4(3, 19, pattern);
}

TEST(PatternFill4, VeryLongMisalignedEq) {
  FillerTester tester(100);
  uint8_t pattern[] = {0x12, 0x12};
  tester.patternFill4(5, 19, pattern);
}

}  // namespace roo_display
