#include "roo_display/image/jpeg/jpeg.h"

#include "roo_display.h"
#include "roo_display/core/raster.h"
#include "roo_backport/byte.h"

namespace roo_display {

// Per http://elm-chan.org/fsw/tjpgd/en/appnote.html: 3100 is the minimum;
// 'FASTDECODE = 1' needs extra 320 bytes; 'FASTDECODE = 2' needs extra 6144
// bytes.
#if JD_FASTDECODE == 0
#define TJPGD_WORKSPACE_SIZE 3100
#elif JD_FASTDECODE == 1
#define TJPGD_WORKSPACE_SIZE 3500
#elif JD_FASTDECODE == 2
#define TJPGD_WORKSPACE_SIZE (3500 + 6144)
#endif

JpegDecoder::JpegDecoder()
    : workspace_(new uint8_t[TJPGD_WORKSPACE_SIZE]),
      jdec_(),
      input_(nullptr),
      surface_(nullptr) {}

size_t jpeg_read(JDEC* jdec, uint8_t* buf, size_t size) {
  JpegDecoder* decoder = (JpegDecoder*)jdec->device;
  if (buf != nullptr) {
    return decoder->input_->read((roo::byte*)buf, size);
  } else {
    decoder->input_->skip(size);
    return size;
  }
}

int jpeg_draw_rect(JDEC* jdec, void* data, JRECT* rect) {
  JpegDecoder* decoder = (JpegDecoder*)jdec->device;
  const Surface* surface = decoder->surface_;
  if (rect->top + surface->dy() > surface->clip_box().yMax()) {
    // The rest of the image will be clipped out; we can finish early.
    return 0;
  }
  Box box(rect->left, rect->top, rect->right, rect->bottom);
  ConstDramRaster<Rgb888> raster(box, (const roo::byte*)data);
  {
    ResumeOutput resume(surface->out());
    surface->drawObject(raster);
  }
  return 1;
}

bool JpegDecoder::getDimensions(const roo_io::MultipassResource& resource,
                                int16_t& width, int16_t& height) {
  if (!open(resource, width, height)) {
    width = 0;
    height = 0;
    return false;
  }
  close();
  return true;
}

bool JpegDecoder::open(const roo_io::MultipassResource& resource,
                       int16_t& width, int16_t& height) {
  input_ = resource.open();
  if (jd_prepare(&jdec_, &jpeg_read, workspace_.get(), TJPGD_WORKSPACE_SIZE,
                 this) == JDR_OK) {
    width = jdec_.width;
    height = jdec_.height;
    return true;
  }
  close();
  return false;
}

void JpegDecoder::draw(const roo_io::MultipassResource& resource,
                       const Surface& s, uint8_t scale, int16_t& width,
                       int16_t& height) {
  PauseOutput pause(s.out());
  if (!open(resource, width, height)) {
    return;
  }

  surface_ = &s;
  jd_decomp(&jdec_, &jpeg_draw_rect, scale);
  surface_ = nullptr;

  close();
}

}  // namespace roo_display
