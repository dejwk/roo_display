#pragma once

#include <memory>

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/image/png/lib/PNGdec.h"
#include "roo_io/core/multipass_input_stream.h"
#include "roo_io_arduino/fs/arduino_file_resource.h"

namespace roo_display {

class PngDecoder {
 public:
  PngDecoder();

 private:
  friend class PngImage;
  friend int32_t png_read(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen);
  friend int32_t png_seek(PNGFILE* pFile, int32_t iPosition);

  void getDimensions(const roo_io::MultipassResource& resource, int16_t& width,
                     int16_t& height);

  bool open(const roo_io::MultipassResource& resource, int16_t& width,
            int16_t& height);

  void draw(const roo_io::MultipassResource& resource, const Surface& s,
            uint8_t scale, int16_t& width, int16_t& height);

  void close() { input_ = nullptr; }

  std::unique_ptr<PNGIMAGE> pngdec_;

  std::unique_ptr<roo_io::MultipassInputStream> input_;

  // Used for indexed color modes.
  Palette palette_;
};

class PngImage : public Drawable {
 public:
  PngImage(PngDecoder& decoder, roo_io::MultipassResource& resource)
      : decoder_(decoder), resource_(resource) {
    decoder_.getDimensions(resource_, width_, height_);
  }

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

  roo_io::MultipassResource& resource_;
  mutable int16_t width_;
  mutable int16_t height_;
};

class PngFile : public Drawable {
 public:
  PngFile(PngDecoder& decoder, ::fs::FS& fs, String path)
      : resource_(fs, path.c_str()), img_(decoder, resource_) {}

  PngFile(PngDecoder& decoder, roo_io::Filesystem& fs, String path)
      : resource_(fs, path.c_str()), img_(decoder, resource_) {}

  Box extents() const override { return img_.extents(); }

 private:
  void drawTo(const Surface& s) const override { s.drawObject(img_); }

  roo_io::ExtendedArduinoFileResource resource_;
  PngImage img_;
};

}  // namespace roo_display
