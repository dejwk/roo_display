#pragma once

#include "roo_backport/byte.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/internal/nibble_rect.h"

namespace roo_display {

// Display device filter which reduces the amount of redundant background
// re-drawing, for a collection of up to 15 designated background colors.
//
// A natural, most efficient way to avoid needless redrawing of identically
// colored pixel content is to use a frame buffer that 'remembers' the last
// color written for each of the device pixel. With such framebuffer, redundant
// redraws are completely eliminated - an attempt to write a pixel with the same
// color as previously, are silently discarded. Unfortunately, such a frame
// buffer requires significant RAM, which microcontrollers will usually not have
// enough to spare. For example, to buffer the contents of an ILI9486 display,
// we need 480 x 320 x 2 (width x height x bytes per pixel) = 300 KB of RAM.
//
// This device filter uses a compromise, capable of achieving a very significant
// reduction in redundant redraws, at a fraction of the memory footprint.
// Specifically, it focuses on use cases in which large areas are getting
// rect-filled using colors from a small palette. This is common in GUIs, that
// use solid backgrounds and some amount of solid area fills using a predefined
// set of colors. The device maintains a reduced-resolution framebuffer, whose
// each pixel corresponds to a rectangular area of the underlying device. Each
// pixel of the framebuffer can take one of 16 values. Values 1-15 mean that
// the corresponding rectangle in the underlying device is known to be entirely
// filled with the specific color from a 15-color palette. Value 0 means that
// this is not the case; i.e. that the corresponding rectangle either isn't
// known to be mono-color, or that it contains pixels with colors outside of
// that small palette. When 'Fill-rect' is called on this filter, using a color
// from the pre-specified 15-color palette, the implementation omits writes
// to the areas that are already known to be filled with the given color.
//
// With 4x resolution along both X and Y axes, i.e. when each pixel of the
// framebuffer corresponds to a 4x4 rectangle, we achieve 64-fold reduction in
// the RAM footprint. For example, the frame buffer for the aforementioned
// ILI9486 display now only uses 4800 bytes.
//
// This filter is meant to be used directly on top of a physical device. In
// particular, it expects the underlying display output to have zero offset.
// That is, the (0, 0) nibble of the background mask corresponds to the output
// rectangle (0, 0, kBgFillOptimizerWindowSize - 1, kBgFillOptimizerWindowSize -
// 1).
/// Display output filter that avoids redundant background fills.
class BackgroundFillOptimizer : public DisplayOutput {
 public:
  /// Size of the coarse background grid in pixels.
  static constexpr const int kBlock = 4;

  /// Minimum rectangle width for dynamic palette enrollment.
  static constexpr const int kDynamicPaletteMinWidth = 8;

  /// Minimum rectangle height for dynamic palette enrollment.
  static constexpr const int kDynamicPaletteMinHeight = 8;


  /// Backing buffer for the optimizer state.
  class FrameBuffer {
   public:
    /// Return buffer size in bytes for a given device size.
    ///
    /// @param w Device width in pixels.
    /// @param h Device height in pixels.
    /// @return Required backing buffer size in bytes.
    static constexpr size_t SizeForDimensions(int16_t w, int16_t h) {
      return ((((w - 1) / kBlock + 1) + 1) / 2) *
             ((((h - 1) / kBlock + 1) + 1) / 2) * 2;
    }

    /// Create a frame buffer with dynamic allocation.
    ///
    /// Buffer is zero-initialized. Call `setPalette()` before use.
    FrameBuffer(int16_t width, int16_t height);

    /// Create a frame buffer using caller-provided storage.
    ///
    /// The buffer is not cleared. It must be large enough for the device
    /// dimensions (see `SizeForDimensions()`). Call `setPalette()` before use.
    FrameBuffer(int16_t width, int16_t height, roo::byte* buffer);

    /// Set the palette (size must be <= 15).
    ///
    /// If the buffer is not known to be clear, call `invalidate()` after
    /// changing the palette.
    void setPalette(const Color* palette, uint8_t palette_size);

    /// Mark the underlying screen as prefilled with the given color.
    void setPrefilled(Color color);

    /// Set palette using an initializer list.
    void setPalette(std::initializer_list<Color> palette);

    /// Clear the entire background mask.
    void invalidate();

    /// Set whether the underlying display swaps x/y.
    void setSwapXY(bool swap);

    /// Clear the mask for the area covering `rect`.
    void invalidateRect(const Box& rect);

    const Color* palette() const { return palette_; }
    const uint8_t palette_size() const { return palette_size_; }

    /// Accessor for testing: retrieve the internal nibble mask.
    const internal::NibbleRect& mask() const { return background_mask_; }

   private:
    friend class BackgroundFillOptimizer;
    friend class BackgroundFillOptimizerDevice;

    FrameBuffer(int16_t width, int16_t height, roo::byte* buffer,
                bool owns_buffer);

    void prefilled(uint8_t idx_in_palette);

    internal::NibbleRect background_mask_;
    Color palette_[15];
    uint8_t palette_size_;
    bool swap_xy_;
    std::unique_ptr<roo::byte[]> owned_buffer_;
  };

  /// Create a background fill optimizer filter.
  BackgroundFillOptimizer(DisplayOutput& output, FrameBuffer& frame_buffer);

  virtual ~BackgroundFillOptimizer() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override;

  void write(Color* color, uint32_t pixel_count) override;

  void fill(Color color, uint32_t pixel_count) override;

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override;

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override;

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override;

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override;

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override;

  const ColorFormat& getColorFormat() const override {
    return output_.getColorFormat();
  }

 private:
  friend class BackgroundFillOptimizerDevice;

  void updatePalette(Color* palette, uint8_t* palette_size,
                     uint8_t pinned_palette_size);

  void setPrefilled(Color color);

  uint8_t tryAddIdxInPaletteDynamic(Color color);

  uint8_t tryAddIdxInPaletteOnSecondConsecutiveColor(Color color);

  void resetPendingDynamicPaletteColor();

  void updateMaskValue(int16_t x, int16_t y, uint8_t old_value,
                       uint8_t new_value);

  void decrementPaletteUsage(uint8_t old_value);

  void incrementPaletteUsage(uint8_t new_value, uint16_t count = 1);

  void fillMaskRect(const Box& box, uint8_t new_value);

  void resetMaskAndUsage(uint8_t mask_value);

  void recountMaskUsage();

  bool isReclaimablePaletteIdx(uint8_t idx) const;

  void setWriteCursorOrd(uint32_t ord);

  void advanceWriteCursor(uint32_t pixel_count);

  uint32_t pixelsUntilStripeEnd() const;

  void beginPassthroughIfNeeded();

  void passthroughWrite(Color* color, uint32_t pixel_count);

  void passthroughFill(Color color, uint32_t pixel_count);

  void flushDeferredUniformRun();

  void clearMaskForOrdRange(uint32_t start_ord, uint32_t pixel_count);

  // Grid-aligned, block-based fast path (strict alignment required).
  bool tryProcessGridAlignedBlockStripes(Color*& color,
                                         uint32_t& pixel_count);

  void processAlignedFullStripeBlock(Color* color, int16_t bx, int16_t by, int16_t aw_width);

  void emitUniformScanRun(Color color, int16_t start_y, uint32_t count);

  template <typename Filler>
  void fillRectBg(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  Filler& filler, uint8_t palette_idx);

  DisplayOutput& output_;
  internal::NibbleRect* background_mask_;
  Color* palette_;
  uint8_t* palette_size_;

  Box address_window_;
  uint32_t address_window_area_;
  int16_t bx_min_;
  int16_t bx_max_;
  bool address_window_block_haligned_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  uint32_t cursor_ord_;
  uint16_t palette_usage_count_[16];
  uint8_t pinned_palette_size_;
  bool palette_full_hint_;
  bool has_pending_dynamic_palette_color_;
  Color pending_dynamic_palette_color_;

  enum class WriteScanState { kScan, kPassthrough };
  WriteScanState write_scan_state_;
  bool passthrough_address_set_;
  bool scan_uniform_active_;
  Color scan_uniform_color_;
  int16_t scan_uniform_start_y_;
  uint32_t scan_uniform_count_;
};

/// Display device wrapper that applies background fill optimization.
class BackgroundFillOptimizerDevice : public DisplayDevice {
 public:
  /// Create a wrapper device.
  BackgroundFillOptimizerDevice(DisplayDevice& device);

  /// Set the palette and optional prefilled color.
  void setPalette(const Color* palette, uint8_t palette_size,
                  Color prefilled = color::Transparent);

  /// Set palette using an initializer list.
  void setPalette(std::initializer_list<Color> palette,
                  Color prefilled = color::Transparent);

  void init() override { device_.init(); }

  void orientationUpdated() override {
    device_.orientationUpdated();
    buffer_.setSwapXY(orientation().isXYswapped());
    // Orientation swap reinterprets the same backing mask with transposed
    // geometry, so rebuild usage counters from the current mask contents.
    optimizer_.recountMaskUsage();
  }

  virtual void setBgColorHint(Color bgcolor) {
    device_.setBgColorHint(bgcolor);
  }

  void begin() override { device_.begin(); }

  void end() override { device_.end(); }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    optimizer_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    optimizer_.write(color, pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    optimizer_.fill(color, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    optimizer_.writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    optimizer_.fillRects(mode, color, x0, y0, x1, y1, count);
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    optimizer_.writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    optimizer_.fillPixels(mode, color, x, y, pixel_count);
  }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    optimizer_.drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                              src_y1, dst_x0, dst_y0);
  }

  const ColorFormat& getColorFormat() const override {
    return device_.getColorFormat();
  }

  /// Accessor for testing: retrieve the internal frame buffer.
  const BackgroundFillOptimizer::FrameBuffer& buffer() const { return buffer_; }

 private:
  DisplayDevice& device_;
  BackgroundFillOptimizer::FrameBuffer buffer_;
  BackgroundFillOptimizer optimizer_;
};

}  // namespace roo_display
