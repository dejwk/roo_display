#include "roo_display/image/jpeg/jpeg.h"
#include "roo_display/core/raster.h"

namespace roo_display {

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
  if (jd_prepare(&jdec_, &jpeg_read, workspace_, sizeof(workspace_), this) ==
      JDR_OK) {
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
