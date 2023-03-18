#pragma once

#include "roo_display.h"
#include "roo_display/core/color_indexed.h"
#include "roo_display/io/resource.h"
#include "roo_display/io/file.h"
#include "roo_display/io/resource.h"
#include "roo_display/image/png/lib/PNGdec.h"

namespace roo_display {

class PngDecoder {
 public:
  PngDecoder();

 private:
  template <typename Resource>
  friend class PngImage;
  friend int32_t png_read(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen);
  friend int32_t png_seek(PNGFILE *pFile, int32_t iPosition);

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
    input_ = resource.open();
    if (input_ == nullptr) return false;
    if (openInternal(width, height)) {
      return true;
    }
    close();
    return false;
  }

  // Opens the image and reads its width and height.
  bool openInternal(int16_t& width, int16_t& height);

  void close() { input_ = nullptr; }

  std::unique_ptr<PNGIMAGE> pngdec_;

  std::unique_ptr<ResourceStream> input_;

  // Used for indexed color modes.
  Palette palette_;
};

template <typename Resource>
class PngImage : public Drawable {
 public:
  template <typename... Args>
  PngImage(PngDecoder& decoder, Args&&... args)
      : PngImage(Resource(std::forward<Args>(args)...), decoder) {}

  PngImage(Resource resource, PngDecoder& decoder)
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

  PngDecoder& decoder_;

  Resource resource_;
  mutable int16_t width_;
  mutable int16_t height_;
};

using PngFile = PngImage<FileResource>;

}  // namespace roo_display
