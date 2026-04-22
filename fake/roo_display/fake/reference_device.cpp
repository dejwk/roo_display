#include "fake/roo_display/fake/reference_device.h"

#include <algorithm>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_time.h"

namespace roo_display {

template <typename Mode>
void ReferenceDisplayDevice::initBackbuffer(const Mode& mode, Color initial) {
  size_t bytes =
      (static_cast<size_t>(Mode::bits_per_pixel) * raw_width() * raw_height() +
       7) /
      8;
  backbuffer_storage_ = std::make_unique<roo::byte[]>(bytes);
  auto output = std::make_unique<OffscreenDevice<Mode>>(
      raw_width(), raw_height(), backbuffer_storage_.get(), mode);
  output->fillRect(0, 0, raw_width() - 1, raw_height() - 1, initial);
  auto raster = std::make_unique<ConstDramRasterBE<Mode>>(
      raw_width(), raw_height(), backbuffer_storage_.get(), mode);
  rasterizable_ = std::move(raster);
  output_device_ = std::move(output);
}

ReferenceDisplayDevice::ReferenceDisplayDevice(
    int w, int h, roo_testing_transducers::Viewport& viewport)
    : ReferenceDisplayDevice(w, h, ColorMode::kRgb565, false, viewport) {}

ReferenceDisplayDevice::ReferenceDisplayDevice(
    int w, int h, ColorMode color_mode, bool blendable,
    roo_testing_transducers::Viewport& viewport)
    : DisplayDevice(w, h),
      viewport_(viewport),
      bgcolor_(0xFF7F7F7F),
      addr_x0_(0),
      addr_y0_(0),
      addr_x1_(w - 1),
      addr_y1_(h - 1),
      blending_mode_(BlendingMode::kSourceOver),
      color_mode_(color_mode),
      blendable_(blendable),
      orienter_(w, h) {
  Color initial(0xFF, 0xF0, 0x80, 0x80);
  switch (color_mode_) {
    case ColorMode::kRgb888: {
      initBackbuffer(Rgb888(), initial);
      break;
    }
    case ColorMode::kRgb565: {
      initBackbuffer(Rgb565(), initial);
      break;
    }
    case ColorMode::kGrayscale8: {
      initBackbuffer(Grayscale8(), initial);
      break;
    }
    case ColorMode::kGrayscale1: {
      initBackbuffer(Monochrome(Color(0xFF, 0xFF, 0xFF), Color(0, 0, 0)),
                     initial);
      break;
    }
  }
}

void ReferenceDisplayDevice::begin() { output_device_->begin(); }

void ReferenceDisplayDevice::end() {
  output_device_->end();
  viewport_.flush();
}

void ReferenceDisplayDevice::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                        uint16_t y1,
                                        roo_display::BlendingMode mode) {
  addr_x0_ = x0;
  addr_y0_ = y0;
  addr_x1_ = x1;
  addr_y1_ = y1;
  cursor_x_ = x0;
  cursor_y_ = y0;
  blending_mode_ = mode;
  output_device_->setAddress(x0, y0, x1, y1,
                             blendable_ ? mode : BlendingMode::kSource);
}

const DisplayOutput::ColorFormat& ReferenceDisplayDevice::getColorFormat()
    const {
  return output_device_->getColorFormat();
}

const DisplayOutput::Capabilities& ReferenceDisplayDevice::getCapabilities()
    const {
  static const Capabilities kBlendable(/*supports_blending=*/true,
                                       /*supports_blit_copy=*/true);
  static const Capabilities kNotBlendable;
  return blendable_ ? kBlendable : kNotBlendable;
}

void ReferenceDisplayDevice::orientationUpdated() {
  output_device_->setOrientation(orientation());
  orienter_.setOrientation(orientation());
}

void ReferenceDisplayDevice::blitRect(int16_t x0, int16_t y0, int16_t x1,
                                      int16_t y1) {
  orienter_.orientRect(x0, y0, x1, y1);
  if (x0 > x1 || y0 > y1) return;

  DCHECK_GE(x0, 0);
  DCHECK_LT(x1, raw_width());
  DCHECK_GE(y0, 0);
  DCHECK_LT(y1, raw_height());

  if (x0 == x1 && y0 == y1) {
    Color pixel;
    rasterizable_->readColorRect(x0, y0, x1, y1, &pixel);
    viewport_.fillRect(x0, y0, x1, y1, pixel.asArgb());
    return;
  }

  int32_t width = x1 - x0 + 1;
  int32_t height = y1 - y0 + 1;
  uint32_t area = width * height;
  if (area == 0) return;

  readback_color_tmp_.resize(area);
  uint32_t* viewport_buffer = new uint32_t[area];
  bool uniform =
      rasterizable_->readColorRect(x0, y0, x1, y1, readback_color_tmp_.data());
  if (uniform) {
    uint32_t argb = readback_color_tmp_[0].asArgb();
    for (uint32_t i = 0; i < area; ++i) {
      viewport_buffer[i] = argb;
    }
  } else {
    for (uint32_t i = 0; i < area; ++i) {
      viewport_buffer[i] = readback_color_tmp_[i].asArgb();
    }
  }

  viewport_.drawRect(x0, y0, x1, y1, viewport_buffer);
}

Color ReferenceDisplayDevice::preblend(roo_display::BlendingMode mode,
                                       Color color) const {
  return ApplyBlending(mode, bgcolor_, color);
}

void ReferenceDisplayDevice::fillRect(int16_t x0, int16_t y0, int16_t x1,
                                      int16_t y1, BlendingMode mode,
                                      Color color) {
  if (!blendable_) {
    color = preblend(mode, color);
    mode = BlendingMode::kSource;
  }
  int16_t x0_copy = x0, y0_copy = y0, x1_copy = x1, y1_copy = y1;
  output_device_->fillRects(mode, color, &x0, &y0, &x1, &y1, 1);
  blitRect(x0_copy, y0_copy, x1_copy, y1_copy);
}

void ReferenceDisplayDevice::write(roo_display::Color* colors,
                                   uint32_t pixel_count) {
  if (pixel_count == 0) return;

  int16_t start_x = cursor_x_;
  int16_t start_y = cursor_y_;

  output_device_->write(colors, pixel_count);
  advance(pixel_count);
  blitAdvancedRegion(start_x, start_y, cursor_x_, cursor_y_);
}

void ReferenceDisplayDevice::fill(roo_display::Color color,
                                  uint32_t pixel_count) {
  if (pixel_count == 0) return;

  int16_t start_x = cursor_x_;
  int16_t start_y = cursor_y_;

  output_device_->fill(color, pixel_count);
  advance(pixel_count);
  blitAdvancedRegion(start_x, start_y, cursor_x_, cursor_y_);
}

void ReferenceDisplayDevice::drawDirectRect(const roo::byte* data,
                                            size_t row_width_bytes,
                                            int16_t src_x0, int16_t src_y0,
                                            int16_t src_x1, int16_t src_y1,
                                            int16_t dst_x0, int16_t dst_y0) {
  if (src_x1 < src_x0 || src_y1 < src_y0) return;

  output_device_->drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                                 src_y1, dst_x0, dst_y0);

  int16_t width = src_x1 - src_x0 + 1;
  int16_t height = src_y1 - src_y0 + 1;
  blitRect(dst_x0, dst_y0, dst_x0 + width - 1, dst_y0 + height - 1);
}

void ReferenceDisplayDevice::blitCopy(int16_t src_x0, int16_t src_y0,
                                      int16_t src_x1, int16_t src_y1,
                                      int16_t dst_x0, int16_t dst_y0) {
  if (dst_x0 == src_x0 && dst_y0 == src_y0) return;
  if (!blendable_ || src_x1 < src_x0 || src_y1 < src_y0) return;
  output_device_->blitCopy(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0);
  int16_t w = src_x1 - src_x0;
  int16_t h = src_y1 - src_y0;
  blitRect(dst_x0, dst_y0, dst_x0 + w, dst_y0 + h);
}

void ReferenceDisplayDevice::advance(uint32_t pixel_count) {
  if (pixel_count == 0) return;
  if (static_cast<uint32_t>(addr_x1_ - cursor_x_ + 1) > pixel_count) {
    // Fast path - still within the current line.
    cursor_x_ += static_cast<int16_t>(pixel_count);
    return;
  }
  pixel_count -= static_cast<uint32_t>(addr_x1_ - cursor_x_ + 1);
  uint32_t window_width = static_cast<uint32_t>(addr_x1_ - addr_x0_ + 1);
  cursor_y_ += static_cast<int16_t>(pixel_count / window_width) + 1;
  cursor_x_ = addr_x0_ + static_cast<int16_t>(pixel_count % window_width);
}

void ReferenceDisplayDevice::blitAdvancedRegion(int16_t start_x,
                                                int16_t start_y, int16_t end_x,
                                                int16_t end_y) {
  if (start_x == end_x && start_y == end_y) return;

  // Defensive.
  if (end_y < start_y || (end_y == start_y && end_x <= start_x)) {
    blitRect(addr_x0_, addr_y0_, addr_x1_, addr_y1_);
    return;
  }

  // Single line - quick path.
  if (start_y == end_y) {
    blitRect(start_x, start_y, end_x - 1, end_y);
    return;
  }

  // Finish the first line.
  blitRect(start_x, start_y, addr_x1_, start_y);

  // Fill the middle lines.
  if (start_y + 1 <= end_y - 1) {
    blitRect(addr_x0_, start_y + 1, addr_x1_, end_y - 1);
  }

  // Draw the last line if needed.
  if (end_x > addr_x0_) {
    blitRect(addr_x0_, end_y, end_x - 1, end_y);
  }
}

void ReferenceDisplayDevice::writeRects(BlendingMode mode, Color* color,
                                        int16_t* x0, int16_t* y0, int16_t* x1,
                                        int16_t* y1, uint16_t count) {
  if (count == 0) return;
  for (uint16_t i = 0; i < count; ++i) {
    fillRect(x0[i], y0[i], x1[i], y1[i], mode, color[i]);
  }
}

void ReferenceDisplayDevice::fillRects(BlendingMode mode, Color color,
                                       int16_t* x0, int16_t* y0, int16_t* x1,
                                       int16_t* y1, uint16_t count) {
  if (count == 0) return;
  for (uint16_t i = 0; i < count; ++i) {
    fillRect(x0[i], y0[i], x1[i], y1[i], mode, color);
  }
}

void ReferenceDisplayDevice::writePixels(BlendingMode mode,
                                         roo_display::Color* colors,
                                         int16_t* xs, int16_t* ys,
                                         uint16_t pixel_count) {
  if (pixel_count == 0) return;

  constexpr uint16_t kBlockSize = 64;
  while (pixel_count > 0) {
    uint16_t block_count = std::min<uint16_t>(kBlockSize, pixel_count);

    int16_t xs_copy[kBlockSize];
    int16_t ys_copy[kBlockSize];
    std::copy(xs, xs + block_count, xs_copy);
    std::copy(ys, ys + block_count, ys_copy);

    output_device_->writePixels(mode, colors, xs, ys, block_count);
    blitPixels(xs_copy, ys_copy, block_count);

    pixel_count -= block_count;
    colors += block_count;
    xs += block_count;
    ys += block_count;
  }
}

void ReferenceDisplayDevice::fillPixels(BlendingMode mode,
                                        roo_display::Color color, int16_t* xs,
                                        int16_t* ys, uint16_t pixel_count) {
  if (pixel_count == 0) return;

  constexpr uint16_t kBlockSize = 64;
  while (pixel_count > 0) {
    uint16_t block_count = std::min<uint16_t>(kBlockSize, pixel_count);

    int16_t xs_copy[kBlockSize];
    int16_t ys_copy[kBlockSize];
    std::copy(xs, xs + block_count, xs_copy);
    std::copy(ys, ys + block_count, ys_copy);

    output_device_->fillPixels(mode, color, xs, ys, block_count);
    blitPixels(xs_copy, ys_copy, block_count);

    pixel_count -= block_count;
    xs += block_count;
    ys += block_count;
  }
}

void ReferenceDisplayDevice::blitPixels(int16_t* xs, int16_t* ys,
                                        uint16_t pixel_count) {
  if (pixel_count == 0) return;

  int16_t* xs_oriented = xs;
  int16_t* ys_oriented = ys;
  orienter_.orientPixels(xs_oriented, ys_oriented, pixel_count);

  constexpr uint16_t kBlockSize = 64;
  Color colors[kBlockSize];
  rasterizable_->readColors(xs_oriented, ys_oriented, pixel_count, colors);

  for (uint16_t i = 0; i < pixel_count; ++i) {
    viewport_.fillRect(xs_oriented[i], ys_oriented[i], xs_oriented[i],
                       ys_oriented[i], colors[i].asArgb());
  }
}

void ReferenceDisplayDevice::init() {
  viewport_.init(raw_width(), raw_height());
}

ReferenceTouchDevice::ReferenceTouchDevice(
    roo_testing_transducers::Viewport& viewport)
    : TouchDevice(), viewport_(viewport) {}

roo_display::TouchResult ReferenceTouchDevice::getTouch(
    roo_display::TouchPoint* points, int max_points) {
  int16_t w = viewport_.width();
  int16_t h = viewport_.height();
  if (w < 0 || h < 0) {
    // Viewport not yet initialized / visible.
    return roo_display::TouchResult();
  }
  int16_t x_display, y_display;
  bool result = viewport_.isMouseClicked(&x_display, &y_display);
  if (result) {
    roo_display::TouchPoint& tp = points[0];
    tp.id = 1;
    tp.x = (int32_t)4096 * x_display / w;
    tp.y = (int32_t)4096 * y_display / h;
    tp.z = 100;
    return TouchResult(roo_time::Uptime::Now(), 1);
  }
  return TouchResult();
}

}  // namespace roo_display
