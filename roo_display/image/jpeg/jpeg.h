#pragma once

#include "roo_display/core/drawable.h"
#include "roo_display/image/jpeg/lib/tjpgd.h"
#include "roo_display/image/jpeg/lib/tjpgdcnf.h"
#include "roo_display/io/memory.h"
#include "roo_display/io/stream.h"

namespace roo_display {

namespace internal {

// Virtual adapter for a templated resource stream.
class ByteStream {
 public:
  virtual ~ByteStream() = default;

  // reads up to count bytes. Returns the number of bytes read.
  virtual int read(uint8_t* buf, int count) = 0;

  virtual void skip(uint32_t count) = 0;
};

template <typename RawStream>
class ByteStreamFor : public ByteStream {
 public:
  ByteStreamFor(RawStream stream) : raw_stream_(std::move(stream)) {}

  int read(uint8_t* buf, int count) override {
    return raw_stream_.read(buf, count);
  }

  void skip(uint32_t count) override { raw_stream_.skip(count); }

 private:
  RawStream raw_stream_;
};

template <typename RawStream>
std::unique_ptr<ByteStreamFor<RawStream>> CreateStream(RawStream stream) {
  return std::unique_ptr<ByteStreamFor<RawStream>>(
      new ByteStreamFor<RawStream>(std::move(stream)));
}

}  // namespace internal

class JpegDecoder {
 public:
  JpegDecoder();

 private:
  template <typename Resource>
  friend class JpegImage;
  friend size_t jpeg_read(JDEC*, uint8_t*, size_t);
  friend int jpeg_draw_rect(JDEC* jdec, void* data, JRECT* rect);

  template <typename Resource>
  void getDimensions(Resource& resource, int16_t& width, int16_t& height) {
    if (!open(resource, width, height)) {
      return;
    }
    close();
  }

  template <typename Resource>
  void draw(Resource& resource, const Surface& s, uint8_t scale, int16_t& width,
            int16_t& height) {
    if (!open(resource, width, height)) {
      return;
    }
    drawInternal(s, scale);
    close();
  }

  void drawInternal(const Surface& s, uint8_t scale);

  template <typename Resource>
  bool open(Resource& resource, int16_t& width, int16_t& height) {
    input_ = internal::CreateStream(resource.Open());
    if (openInternal(width, height)) {
      return true;
    }
    close();
    return false;
  }

  // Opens the image and reads its width and height.
  bool openInternal(int16_t& width, int16_t& height);

  void close() { input_ = nullptr; }

  std::unique_ptr<uint8_t[]> workspace_;
  JDEC jdec_;

  std::unique_ptr<internal::ByteStream> input_;
  const Surface* surface_;
};

template <typename Resource = PrgMemResource>
class JpegImage : public Drawable {
 public:
  template <typename... Args>
  JpegImage(JpegDecoder& decoder, Args&&... args)
      : JpegImage(Resource(std::forward<Args>(args)...), decoder) {}

  JpegImage(Resource resource, JpegDecoder& decoder)
      : decoder_(decoder),
        resource_(std::move(resource)),
        width_(-1),
        height_(-1) {}

  Box extents() const override {
    if (width_ < 0 || height_ < 0) {
      decoder_.getDimensions(resource_, width_, height_);
    }
    return Box(0, 0, width_ - 1, height_ - 1);
  }

 private:
  void drawTo(const Surface& s) const override {
    // We update the width and height during drawing, so that the file does not
    // need to be re-read just to fetch the dimensions.
    decoder_.draw(resource_, s, 0, width_, height_);
  }

  JpegDecoder& decoder_;

  Resource resource_;
  mutable int16_t width_;
  mutable int16_t height_;
};

}  // namespace roo_display
