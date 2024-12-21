#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flexViewport(viewport, 6, FlexViewport::kRotationRight),
        display(flexViewport),
        touch(flexViewport, FakeXpt2046Spi::Calibration(318, 346, 3824, 3909,
                                                        false, true, false)) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());

    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(2, touch.cs());
  }
} emulator;

#endif

#include "SPI.h"
#include "roo_display.h"
// #include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/font/font.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Condensed/12.h"
#include "roo_io/text/string_printf.h"

using namespace roo_display;

// Provide your driver config.

#include "roo_display/driver/ili9341.h"
Ili9341spi<5, 17, 27> display_device(Orientation().rotateLeft());
TouchXpt2046<2> touch_device;

// Initial calibration.
TouchCalibration precalib(269, 249, 3829, 3684, Orientation::LeftDown());

Display display(display_device, touch_device, precalib);

// LedcBacklit backlit(16, 1);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::DarkGray);
}

class TouchCalibrator {
 public:
  TouchCalibrator(Display& display, const Font& font)
      : display_(display), font_(font), state_(IDLE), last_touch_event_us_(0) {}

  void loop() {
    int16_t xstart;
    int16_t ystart;
    int16_t x;
    int16_t y;
    while (true) {
      switch (state_) {
        case IDLE: {
          // Display the status and wait for click.
          StatusPane pane(display_.touchCalibration(), font_, display_.width());
          {
            DrawingContext dc(display_);
            dc.draw(pane, kCenter | kMiddle);
          }
          while (true) {
            TouchPoint tp;
            auto touch = display_.getTouch(&tp, 1);
            if (touch.touch_points != 0) {
              state_ = IDLE_SINGLE_PRESSED;
              last_touch_event_us_ = touch.timestamp_us;
              x = tp.x;
              y = tp.y;
              xstart = x;
              ystart = y;
              // Clear the status and continue the event loop in the new state.
              {
                DrawingContext dc(display_);
                dc.erase(pane, kCenter | kMiddle);
              }
              break;
            }
          }
          break;
        }
        case IDLE_SINGLE_PRESSED:
        case IDLE_DOUBLE_PRESSED: {
          int16_t old_x = x;
          int16_t old_y = y;
          // Draw the crosshairs and track movement, until released.
          while (true) {
            {
              DrawingContext dc(display);
              dc.draw(Line(0, y, display.width() - 1, y, color::Red));
              dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
              if (x != old_x) {
                dc.draw(Line(old_x, 0, old_x, display.height() - 1,
                             color::DarkGray));
              }
              if (y != old_y) {
                dc.draw(Line(0, old_y, display.width() - 1, old_y,
                             color::DarkGray));
              }
              dc.draw(Line(0, y, display.width() - 1, y, color::Red));
              dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
            }
            TouchPoint tp;
            auto touch = display_.getTouch(&tp, 1);
            if (touch.touch_points != 0) {
              old_x = x;
              old_y = y;
              x = tp.x;
              y = tp.y;
              if ((x - xstart) * (x - xstart) + (y - ystart) * (y - ystart) >
                  400) {
                // Drag. Remove the possibility to interpret as double-click.
                state_ = IDLE_SINGLE_PRESSED;
              }
            } else {
              uint32_t touch_time = touch.timestamp_us - last_touch_event_us_;
              {
                DrawingContext dc(display);
                dc.draw(Line(0, old_y, display.width() - 1, old_y,
                             color::DarkGray));
                dc.draw(Line(old_x, 0, old_x, display.height() - 1,
                             color::DarkGray));
              }
              if (touch_time > 250000 || touch_time < 50000) {
                // Too fast, or too slow; not a click - go back to the idle
                // mode.
                state_ = IDLE;
              } else {
                // Click registered. First click or a second click?
                last_touch_event_us_ = micros();
                if (state_ == IDLE_SINGLE_PRESSED) {
                  state_ = IDLE_SINGLE_CLICKED;
                } else {
                  // Double-click confirmed; begin calibration.
                  state_ = CALIBRATING;
                }
              }
              break;
            }
          }
          break;
        }
        case IDLE_SINGLE_CLICKED: {
          // Wait for a timeout or a double click.
          while (true) {
            uint32_t time_unclicked = micros() - last_touch_event_us_;
            if (time_unclicked > 300000) {
              // Timeout.
              state_ = IDLE;
              break;
            }
            TouchPoint tp;
            auto touch = display_.getTouch(&tp, 1);
            if (touch.touch_points != 0) {
              if (time_unclicked < 50000) {
                // Too fast! May be a glitch.
                state_ = IDLE_SINGLE_PRESSED;
              } else {
                state_ = IDLE_DOUBLE_PRESSED;
                last_touch_event_us_ = touch.timestamp_us;
              }
              break;
            }
          }
          break;
        }
        case CALIBRATING: {
          calibrate();
          state_ = IDLE;
        }
      }
    }
  }

  void calibrate() {
    int raw_width = display.width();
    int raw_height = display.height();
    if (display_.orientation().isXYswapped()) {
      std::swap(raw_width, raw_height);
    }
    CalibrationInput input;
    // Generally, the resolutions will almost certainly be divisible by 8, so
    // it is a good default choice for margins.
    Box calib_rect(raw_width / 8, raw_height / 8, raw_width * 7 / 8 - 1,
                   raw_height * 7 / 8 - 1);
    Orientation orientation;
    bool swap_xy = false;
    TouchCalibration c = display.touchCalibration();
    display.setTouchCalibration(TouchCalibration());
    readCalibrationData(calib_rect.xMin(), calib_rect.yMin(), input.tl);
    readCalibrationData(calib_rect.xMax(), calib_rect.yMin(), input.tr);
    readCalibrationData(calib_rect.xMax(), calib_rect.yMax(), input.br);
    readCalibrationData(calib_rect.xMin(), calib_rect.yMax(), input.bl);
    display.setTouchCalibration(c);
    if (abs(input.tr.x - input.tl.x) > abs(input.tr.y - input.tl.y) &&
        abs(input.br.x - input.bl.x) > abs(input.br.y - input.bl.y) &&
        abs(input.bl.x - input.tl.x) < abs(input.bl.y - input.tl.y) &&
        abs(input.br.x - input.tr.x) < abs(input.br.y - input.tr.y)) {
      // Non-swapped orientation.
    } else if (abs(input.tr.x - input.tl.x) < abs(input.tr.y - input.tl.y) &&
               abs(input.br.x - input.bl.x) < abs(input.br.y - input.bl.y) &&
               abs(input.bl.x - input.tl.x) > abs(input.bl.y - input.tl.y) &&
               abs(input.br.x - input.tr.x) > abs(input.br.y - input.tr.y)) {
      input.swapXY();
      orientation = orientation.swapXY();
    } else {
      Serial.println("X vs Y readout is inconclusive.");
      failOnInconclusive();
      state_ = IDLE;
      return;
    }

    if (input.tr.x > input.tl.x && input.br.x > input.bl.x) {
      // No flip.
    } else if (input.tr.x < input.tl.x && input.br.x < input.bl.x) {
      input.flipHorizontally();
      orientation = orientation.flipHorizontally();
    } else {
      Serial.println("Horizontal direction is inconclusive.");
      failOnInconclusive();
      state_ = IDLE;
      return;
    }

    if (input.bl.y > input.tl.y && input.br.y > input.tr.y) {
      // No flip.
    } else if (input.bl.y < input.tl.y && input.br.y < input.tr.y) {
      input.flipVertically();
      orientation = orientation.flipVertically();
    } else {
      Serial.println("Vertical direction is inconclusive.");
      failOnInconclusive();
      state_ = IDLE;
      return;
    }

    int16_t xMin = (input.tl.x + input.bl.x) / 2;
    int16_t yMin = (input.tl.y + input.tr.y) / 2;
    int16_t xMax = (input.tr.x + input.br.x) / 2;
    int16_t yMax = (input.bl.y + input.br.y) / 2;

    // Now, these correspond to the calib_rect, which is 3/4 of the size
    // in each direction. We now need to compensate to the full screen.
    int16_t xborder = (xMax - xMin) / 6;
    int16_t yborder = (yMax - yMin) / 6;
    Serial.printf("Calibrated successfully: %d %d %d %d; direction: %s\n",
                  xMin - xborder, yMin - yborder, xMax + xborder,
                  yMax + yborder, orientation.asString());
    display_.setTouchCalibration(
        TouchCalibration(xMin - xborder, yMin - yborder, xMax + xborder,
                         yMax + yborder, orientation));
    {
      DrawingContext dc(display_);
      dc.draw(TextLabel("COMPLETED", font_, color::Black), kCenter | kMiddle);
      delay(2000);
      dc.clear();
      state_ = IDLE;
    }
  }

 private:
  struct CalibrationInput {
    Point tl;
    Point tr;
    Point bl;
    Point br;

    void swapXY() {
      tl = SwapXY(tl);
      tr = SwapXY(tr);
      bl = SwapXY(bl);
      br = SwapXY(br);
    }

    void flipHorizontally() {
      std::swap(tl, tr);
      std::swap(bl, br);
    }

    void flipVertically() {
      std::swap(tl, bl);
      std::swap(tr, br);
    }
  };

  enum State {
    IDLE = 0,
    IDLE_SINGLE_PRESSED = 1,
    IDLE_SINGLE_CLICKED = 2,
    IDLE_DOUBLE_PRESSED = 3,
    CALIBRATING = 4,
  };

  // Shows current status of the calibration, plus instructions.
  class StatusPane : public Drawable {
   public:
    StatusPane(TouchCalibration calibration, const Font& font, int16_t w)
        : font_(font),
          current_(calibration),
          width_(w),
          line_height_(font_.metrics().maxHeight()) {}

    Box extents() const override {
      return Box(0, 0, width_ - 1, 4 * line_height_ - 1);
    }

   private:
    void drawInteriorTo(const Surface& s) const override {
      DrawingContext dc(s);
      dc.draw(TextLabel("Current:", font_, color::Black), kTop | kCenter);
      const Box& b = current_.bounds();
      dc.draw(TextLabel(roo_io::StringPrintf("%d, %d, %d, %d, %s", b.xMin(),
                                             b.yMin(), b.xMax(), b.yMax(),
                                             current_.orientation().asString()),
                        font_, color::Black),
              kTop.shiftBy(line_height_) | kCenter);
      dc.draw(TextLabel("Press to test", font_, color::Black),
              kTop.shiftBy(line_height_ * 2) | kCenter);
      dc.draw(TextLabel("Double-click to calibrate", font_, color::Black),
              kTop.shiftBy(line_height_ * 3) | kCenter);
    }

    const Font& font_;
    TouchCalibration current_;
    int16_t width_;
    int16_t line_height_;
  };

  static Point SwapXY(Point p) { return Point{.x = p.y, .y = p.x}; }

  void readCalibrationData(int16_t x, int16_t y, Point& point) {
    Box instruction_box(0, 0, 99, 35);
    // (x, y) are in raw display coordinates; need to apply the orientation.
    Orientation orientation = display_.orientation();
    int16_t w =
        orientation.isXYswapped() ? display_.height() : display_.width();
    int16_t h =
        orientation.isXYswapped() ? display_.width() : display_.height();
    if (orientation.isRightToLeft()) {
      x = w - x - 1;
    }
    if (orientation.isBottomToTop()) {
      y = h - y - 1;
    }
    if (orientation.isXYswapped()) {
      std::swap(x, y);
    }

    {
      DrawingContext dc(display_);

      // Draw the target blue line.
      dc.draw(FilledCircle::ByRadius(x, y, 2, color::Blue));

      // Draw the arrow.
      FpPoint center{display_.width() / 2 - 0.5, display_.height() / 2 - 0.5};
      FpPoint arrow_finish{0.9 * x + 0.1 * center.x, 0.9 * y + 0.1 * center.y};
      FpPoint arrow_base{0.8 * x + 0.2 * center.x, 0.8 * y + 0.2 * center.y};
      dc.draw(SmoothLine(center, arrow_base, color::Black));
      dc.draw(SmoothWedgedLine(arrow_base, 7, arrow_finish, 0, color::Black,
                               ENDING_FLAT));

      // Draw the instruction box.
      dc.draw(Rect(instruction_box.xMin() - 1, instruction_box.yMin() - 1,
                   instruction_box.xMax() + 1, instruction_box.yMax() + 1,
                   color::Black),
              kCenter | kMiddle);

      // Draw the instruction.
      dc.draw(MakeTileOf(TextLabel("Press and hold", font_, color::Black),
                         instruction_box, kCenter | kMiddle),
              kCenter | kMiddle);
    }
    TouchPoint tp;
    int32_t sum_x = 0, sum_y = 0;
    bool pressed = false;
    for (int i = 0; i < 128; ++i) {
      while (display_.getRawTouch(&tp, 1).touch_points == 0) {
        if (pressed) {
          DrawingContext dc(display_);
          dc.draw(MakeTileOf(TextLabel("Press and hold", font_, color::Black),
                             instruction_box, kCenter | kMiddle),
                  kCenter | kMiddle);
          pressed = false;
        }
      }
      if (!pressed) {
        DrawingContext dc(display_);
        // Draw the 'hold' instruction.
        dc.draw(MakeTileOf(TextLabel("Hold", font_, color::Black),
                           instruction_box, kCenter | kMiddle),
                kCenter | kMiddle);
      }
      pressed = true;
      Serial.printf("Registered: %d, %d\n", tp.x, tp.y);
      sum_x += tp.x;
      sum_y += tp.y;
    }
    point.x = (sum_x + 64) / 128;
    point.y = (sum_y + 64) / 128;
    {
      DrawingContext dc(display_);
      dc.clear();
      dc.draw(MakeTileOf(TextLabel("Release", font_, color::Black),
                         instruction_box, kCenter | kMiddle),
              kCenter | kMiddle);
    }
    while (display_.getRawTouch(&tp, 1).touch_points != 0) {
    }
    {
      DrawingContext dc(display_);
      dc.clear();
    }
    delay(200);
  }

  void failOnInconclusive() {
    DrawingContext dc(display_);
    dc.draw(TextLabel("FAILED: inconclusive readings.", font_, color::Black),
            kCenter | kMiddle);
    delay(2000);
    dc.clear();
  }

  Display& display_;
  const Font& font_;
  State state_;
  uint32_t last_touch_event_us_;
};

void loop(void) {
  TouchCalibrator calibrator(display, font_NotoSans_Condensed_12());
  calibrator.loop();
}
