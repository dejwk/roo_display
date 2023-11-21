#pragma once

#include <memory>

#include "FS.h"
#include "roo_display/io/resource.h"
#include "roo_display/io/stream.h"

namespace roo_display {

static const int kFileBufferSize = 64;

namespace internal {

class RawFileStream {
 public:
  RawFileStream(File file) : rep_(new Rep(std::move(file))) {}

  uint8_t read() { return rep_->read(); }
  void skip(uint32_t count) { rep_->skip(count); }

 private:
  class Rep {
   public:
    Rep(File file);
    ~Rep();
    uint8_t read();
    int read(uint8_t* buf, int count);
    void advance(uint32_t count);
    void skip(uint32_t count);

   private:
    Rep(const Rep&) = delete;
    Rep(Rep&&) = delete;
    Rep& operator=(const Rep&) = delete;

    File file_;
    uint8_t buffer_[kFileBufferSize];
    uint8_t offset_;
    uint8_t length_;
  };

  // We keep the content on the heap for the following reasons:
  // * stack space is very limited, and we need some buffer cache
  // * underlying file structures are using heap anyway
  // * we want the stream object to be cheaply movable (it is passed
  //   around as a rvalue reference to data streaming functions)
  std::unique_ptr<Rep> rep_;
};

inline RawFileStream::Rep::Rep(File file)
    : file_(std::move(file)), offset_(0), length_(0) {}

inline RawFileStream::Rep::~Rep() { file_.close(); }

inline uint8_t RawFileStream::Rep::read() {
  if (offset_ >= length_) {
    length_ = file_.read(buffer_, kFileBufferSize);
    offset_ = 0;
  }
  return buffer_[offset_++];
}

inline void RawFileStream::Rep::advance(uint32_t count) {
  uint32_t remaining = (length_ - offset_);
  if (count < remaining) {
    offset_ += count;
  } else {
    offset_ = 0;
    length_ = 0;
    count -= remaining;
    file_.seek(count, SeekCur);
  }
}

inline void RawFileStream::Rep::skip(uint32_t count) { advance(count); }

class FileStream : public ResourceStream {
 public:
  FileStream(File file) : file_(std::move(file)) {}

  int read(uint8_t* buf, int count) override { return file_.read(buf, count); }

  bool skip(uint32_t count) override { return file_.seek(count, SeekCur); }

  // Returns true on success.
  bool seek(uint32_t offset) override { return file_.seek(offset); }

  int size() override { return file_.size(); }

 private:
  File file_;
};

}  // namespace internal

class FileResource {
 public:
  FileResource(fs::FS& fs, String path) : fs_(fs), path_(std::move(path)) {}

  internal::RawFileStream createRawStream() const {
    auto f = fs_.open(path_);
    return internal::RawFileStream(std::move(f));
  }

  std::unique_ptr<internal::FileStream> open() const {
    return std::unique_ptr<internal::FileStream>(
        new internal::FileStream(fs_.open(path_)));
  }

 private:
  FS& fs_;
  String path_;
};

}  // namespace roo_display