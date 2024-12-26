#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/image/jpeg/lib/tjpgd.h"
#include "roo_display/image/jpeg/lib/tjpgdcnf.h"
#include "roo_io/core/multipass_input_stream.h"
#include "roo_io_arduino/fs/arduino_file_resource.h"
#include "roo_logging.h"

namespace roo_display {

class JpegDecoder {
 public:
  JpegDecoder();

 private:
  template <typename Resource>
  friend class JpegImage;
  friend size_t jpeg_read(JDEC*, uint8_t*, size_t);
  friend int jpeg_draw_rect(JDEC* jdec, void* data, JRECT* rect);

  void getDimensions(const roo_io::MultipassResource& resource, int16_t& width,
                     int16_t& height) {
    if (!open(resource, width, height)) {
      return;
    }
    close();
  }

  void draw(const roo_io::MultipassResource& resource, const Surface& s,
            uint8_t scale, int16_t& width, int16_t& height) {
    if (!open(resource, width, height)) {
      return;
    }
    drawInternal(s, scale);
    close();
  }

  void drawInternal(const Surface& s, uint8_t scale);

  bool open(const roo_io::MultipassResource& resource, int16_t& width,
            int16_t& height) {
    input_ = resource.open();
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

  std::unique_ptr<roo_io::MultipassInputStream> input_;
  const Surface* surface_;
};

template <typename Resource>
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

class JpegFile : public Drawable {
 public:
  JpegFile(JpegDecoder& decoder, ::fs::FS& fs, String path)
      : resource_(fs, path), img_(decoder, resource_) {}

  Box extents() const override { return img_.extents(); }

 private:
  void drawTo(const Surface& s) const override { s.drawObject(img_); }

  roo_io::ArduinoFileResource resource_;
  JpegImage<roo_io::ArduinoFileResource> img_;
};

// using JpegFile = JpegImage<roo_io::ArduinoFileResource>;

}  // namespace roo_display
