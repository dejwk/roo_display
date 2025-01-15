#pragma once

#include <memory>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/image/jpeg/lib/tjpgd.h"
#include "roo_display/image/jpeg/lib/tjpgdcnf.h"
#include "roo_io.h"
#include "roo_io/core/multipass_input_stream.h"
#include "roo_io/fs/filesystem.h"
#include "roo_io_arduino.h"
#include "roo_io_arduino/fs/arduino_file_resource.h"
#include "roo_logging.h"

namespace roo_display {

class JpegDecoder {
 public:
  JpegDecoder();

 private:
  friend class JpegImage;
  friend size_t jpeg_read(JDEC*, uint8_t*, size_t);
  friend int jpeg_draw_rect(JDEC* jdec, void* data, JRECT* rect);

  bool getDimensions(const roo_io::MultipassResource& resource, int16_t& width,
                     int16_t& height);

  bool open(const roo_io::MultipassResource& resource, int16_t& width,
            int16_t& height);

  void draw(const roo_io::MultipassResource& resource, const Surface& s,
            uint8_t scale, int16_t& width, int16_t& height);

  void close() { input_ = nullptr; }

  std::unique_ptr<uint8_t[]> workspace_;
  JDEC jdec_;

  std::unique_ptr<roo_io::MultipassInputStream> input_;
  const Surface* surface_;
};

class JpegImage : public Drawable {
 public:
  JpegImage(JpegDecoder& decoder, roo_io::MultipassResource& resource)
      : decoder_(decoder), resource_(resource) {
    decoder_.getDimensions(resource_, width_, height_);
  }

  Box extents() const override { return Box(0, 0, width_ - 1, height_ - 1); }

 private:
  void drawTo(const Surface& s) const override {
    decoder_.draw(resource_, s, 0, width_, height_);
  }

  JpegDecoder& decoder_;

  roo_io::MultipassResource& resource_;
  mutable int16_t width_;
  mutable int16_t height_;
};

class JpegFile : public Drawable {
 public:
  JpegFile(JpegDecoder& decoder, ::fs::FS& fs, String path)
      : resource_(fs, path.c_str()), img_(decoder, resource_) {}

  JpegFile(JpegDecoder& decoder, roo_io::Filesystem& fs, String path)
      : resource_(fs, path.c_str()), img_(decoder, resource_) {}

  Box extents() const override { return img_.extents(); }

 private:
  void drawTo(const Surface& s) const override { s.drawObject(img_); }

  roo_io::ExtendedArduinoFileResource resource_;
  JpegImage img_;
};

}  // namespace roo_display
