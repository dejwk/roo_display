#include "roo_display/image/jpeg/jpeg.h"

#include "roo_display/core/raster.h"

namespace roo_display {

// From TFT_eSPI:
// Do not change this, it is the minimum size in bytes of the workspace needed
// by the decoder
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
    return decoder->input_->read(buf, size);
  } else {
    decoder->input_->skip(size);
    return size;
  }
}

int jpeg_draw_rect(JDEC* jdec, void* data, JRECT* rect) {
  JpegDecoder* decoder = (JpegDecoder*)jdec->device;
  const Surface* surface = decoder->surface_;
  Box box(rect->left, rect->top, rect->right, rect->bottom);
  ConstDramRaster<Rgb888> raster(box, (const uint8_t*)data);
  surface->drawObject(raster, 0, 0);
  return 1;
}

bool JpegDecoder::openInternal(int16_t& width, int16_t& height) {
  if (jd_prepare(&jdec_, &jpeg_read, workspace_.get(), TJPGD_WORKSPACE_SIZE,
                 this) == JDR_OK) {
    width = jdec_.width;
    height = jdec_.height;
    return true;
  }
  return false;
}

void JpegDecoder::drawInternal(const Surface& s, uint8_t scale) {
  surface_ = &s;
  jd_decomp(&jdec_, &jpeg_draw_rect, scale);
  surface_ = nullptr;
}

}  // namespace roo_display
