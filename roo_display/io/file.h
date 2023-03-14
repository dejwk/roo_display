#pragma once

#include <memory>
#include "FS.h"
#include "roo_display/io/stream.h"

namespace roo_display {

static const int kFileBufferSize = 64;

class FileStream {
 public:
  FileStream(File file) : rep_(new Rep(std::move(file))) {}

  uint8_t read() { return rep_->read(); }
  int read(uint8_t* buf, int count) { return rep_->read(buf, count); }
  void advance(uint32_t count) { rep_->advance(count); }
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

class FileResource {
 public:
  FileResource(fs::FS& fs, String path) : fs_(fs), path_(std::move(path)) {}

  FileStream Open() const {
    auto f = fs_.open(path_);
    return FileStream(std::move(f));
  }

 private:
  FS& fs_;
  String path_;
};

// Inline method implementations below.

inline FileStream::Rep::Rep(File file)
    : file_(std::move(file)), offset_(0), length_(0) {}

inline FileStream::Rep::~Rep() { file_.close(); }

inline uint8_t FileStream::Rep::read() {
  if (offset_ >= length_) {
    length_ = file_.read(buffer_, kFileBufferSize);
    offset_ = 0;
  }
  return buffer_[offset_++];
}

inline int FileStream::Rep::read(uint8_t* buf, int count) {
  int result = 0;
  while (offset_ < length_ && count > 0) {
    *buf++ = buffer_[offset_++];
    --count;
    ++result;
  }
  int read = file_.read(buf, count);
  return result + read;
}

inline void FileStream::Rep::advance(uint32_t count) {
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

inline void FileStream::Rep::skip(uint32_t count) { advance(count); }

}  // namespace roo_display