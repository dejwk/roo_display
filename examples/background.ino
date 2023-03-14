#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeSt77xxSpi display;

  Emulator()
      : viewport(), flexViewport(viewport, 1), display(flexViewport, 240, 240) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(2, display.dc());
    FakeEsp32().gpio.attachOutput(4, display.rst());
  }
} emulator;

#endif

#include <string>

#include "roo_display.h"
#include "roo_display/core/raster.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/string_printer.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSans_Italic/12.h"
#include "roo_smooth_fonts/NotoSans_Italic/18.h"
#include "roo_smooth_fonts/NotoSans_Italic/27.h"
#include "roo_smooth_fonts/NotoSans_Italic/40.h"
#include "roo_smooth_fonts/NotoSans_Italic/60.h"
#include "roo_smooth_fonts/NotoSans_Italic/8.h"
#include "roo_smooth_fonts/NotoSans_Regular/40.h"
#include "roo_smooth_fonts/NotoSans_Regular/60.h"

using namespace roo_display;

// Using setBackground, you can set (uncompressed) images, as well as
// syntethically crated backgrounds such as gradients and meshes,
// as a background for any drawing primitives (including antialiazed fonts).
// You can do it for every created DrawingContext separately, but if the
// background is static, you can also set it once on the display, and then
// DrawingContexts will inherit it.
//
// DrawingContext::clear() respects the background. In other words, if the
// background is set, DrawingContext::clear() fills the clip box with the
// background.
//
// You can add a rectangular background to any drawable, using Tile. It is
// a good way to draw buttons or labels, over solid backgrounds or gradients.
//
// Backgrounds of tiles can be translucent, in which case they get
// alpha-blended over the underlying surface's background.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_240x240<5, 2, 4> device;

Display display(device);

static const int kGradientSize = 200;
Color hsvGradient[kGradientSize];

void initGradient();

auto slantedGradient = MakeRasterizable(
    Box::MaximumBox(),
    [](int16_t x, int16_t y) {
      return hsvGradient[(kGradientSize + x - y / 4) % kGradientSize];
    },
    TRANSPARENCY_NONE);

Color hashColor1;
Color hashColor2;

auto hashGrid = MakeRasterizable(
    Box::MaximumBox(),
    [](int16_t x, int16_t y) {
      return (x / 4 + y / 4) % 2 == 0 ? hashColor1 : hashColor2;
    },
    TRANSPARENCY_NONE);

void setup() {
  Serial.begin(9600);
  initGradient();
  hashColor1 = color::LightGray;
  hashColor2 = color::DarkGray;
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init();
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
  display.clear();
}

void simpleBackground() {
  // This example shows how anti-aliased text gets rendered over arbitrary
  // backgrounds (in this case, a slanted gradient).
  //
  // The text is shown at 3 different zoom levels, so that it is easier to
  // see that the colors of translucent (anti-aliased) pixels indeed inherit
  // from the background; i.e. it is green-ish when the text falls over green,
  // blue-ish if it falls over blue, etc.
  auto labelOrig = StringViewLabel("Afy", font_NotoSans_Italic_60(),
                                   color::Black, FILL_MODE_RECTANGLE);
  auto labelScaled = StringViewLabel("Afy", font_NotoSans_Italic_12(),
                                     color::Black, FILL_MODE_RECTANGLE);
  auto labelScaledMore = StringViewLabel("Afy", font_NotoSans_Italic_8(),
                                         color::Black, FILL_MODE_RECTANGLE);
  int16_t dx = (display.width() - labelOrig.extents().width()) / 2;
  int16_t dy = labelOrig.metrics().glyphYMax() +
               (display.height() - labelOrig.metrics().height()) / 2;
  // The 2nd 'rescaled' label is slightly larger, so we truncate to avoid
  // artifacts at the edges. (We could also do the opposite, but we would need
  // to use Tile).
  auto clip_box = labelOrig.extents().translate(dx, dy);
  {
    display.setBackground(&slantedGradient);
    DrawingContext dc(display);
    dc.clear();
    dc.setClipBox(clip_box);
    dc.draw(labelOrig, dx, dy);
  }
  delay(2000);
  {
    DrawingContext dc(display);
    dc.setClipBox(clip_box);
    dc.setTransformation(Transformation().scale(5, 5));
    dc.draw(labelScaled, dx, dy);
  }
  delay(2000);
  {
    DrawingContext dc(display);
    dc.setClipBox(clip_box);
    dc.setTransformation(Transformation().scale(7, 7));
    dc.draw(labelScaledMore, dx, dy);
  }
  delay(2000);
}

void tileWithSemiTransparentBackground() {
  // This example demonstrates the possibility to use semi-transparent
  // backgrounds for 'icons' (tiles), which can be combined with (i.e.,
  // alpha-blended over) arbitrary screen background. Note how in this example
  // we vary the level of transparency of the hashed background, without
  // causing any visible flicker.
  //
  // This feature can be used to create visual effects such as translucent
  // icons, etc.
  display.clear();
  bool sc1 = false;
  bool sc2 = false;
  for (int i = 0; i < 1000; i++) {
    auto label =
        StringViewLabel("Ostendo", font_NotoSans_Italic_27(), color::Black);
    auto tile = MakeTileOf(label, Box(0, 0, 103, 27), kCenter | kMiddle);
    tile.setBackground(&hashGrid);
    int16_t dx = (display.width() - tile.extents().width()) / 2;
    int16_t dy = (display.height() - tile.extents().height()) / 2;
    {
      DrawingContext dc(display);
      dc.draw(tile, dx, dy);
    }
    uint8_t a1 = hashColor1.a();
    if (sc1) {
      if (a1 == 255) {
        sc1 = !sc1;
      } else {
        hashColor1.set_a(a1 + 1);
      }
    } else {
      if (a1 == 0) {
        sc1 = !sc1;
      } else {
        hashColor1.set_a(a1 - 1);
      }
    }

    uint8_t a2 = hashColor2.a();
    if (sc2) {
      if (a2 >= 254) {
        sc2 = !sc2;
      } else {
        hashColor2.set_a(a2 + 2);
      }
    } else {
      if (a2 <= 1) {
        sc2 = !sc2;
      } else {
        hashColor2.set_a(a2 - 2);
      }
    }
    delay(5);
  }
}

class Timer {
 public:
  Timer() : start_(millis()) {}
  void reset() { start_ = millis(); }
  unsigned long read() const { return start_ < 0 ? 0 : millis() - start_; }

 private:
  unsigned long start_;
};

bool update(char newResult, char* prevResult) {
  if (newResult == *prevResult) return false;
  *prevResult = newResult;
  return true;
}

int timerToString(unsigned long millis, char* result) {
  unsigned long t = millis;
  int first_changed = 6;
  if (update((t % 10) + '0', result + 5)) first_changed = 5;
  t /= 10;
  if (update((t % 10) + '0', result + 4)) first_changed = 4;
  t /= 10;
  if (update((t % 10) + '0', result + 3)) first_changed = 3;
  t /= 10;
  result[2] = '"';
  if (update((t % 10) + '0', result + 1)) first_changed = 1;
  result[1] = (t % 10) + '0';
  t /= 10;
  if (update((t % 10) + '0', result + 0)) first_changed = 0;
  return first_changed;
}

struct TimerBenchmark {
  bool clip;
  bool inplace;
  bool background;
};

void printCentered(const std::string& text, int16_t y) {
  ClippedStringViewLabel label(text, font_NotoSans_Italic_18(), color::Black);
  DrawingContext dc(display);
  dc.draw(label, kCenter | kMiddle.shiftBy(y));
}

void timerBenchmark(TimerBenchmark* benchmark, unsigned int seconds) {
  if (benchmark->background) {
    display.setBackground(&slantedGradient);
  } else {
    display.setBackground(color::LightGray);
  }
  display.clear();

  Timer timer;
  char time[] = "      ";
  int frames = 0;
  double fps;
  const Font& timerFont = font_NotoSans_Regular_60();
  const Font& fpsFont = font_NotoSans_Italic_18();
  GlyphMetrics digitMetrics = timerFont.getHorizontalStringMetrics("0");
  GlyphMetrics timerMetrics = timerFont.getHorizontalStringMetrics("00\"000");
  Box timerBox = timerMetrics.screen_extents();
  Box fpsBox =
      fpsFont.getHorizontalStringMetrics("100000.0 fps").screen_extents();

  // Draw label of the benchmark.
  std::string bgStr =
      benchmark->background ? "gradient background" : "solid background";
  std::string clipStr = benchmark->clip ? "smart clipping" : "naive clipping";
  std::string inplaceStr =
      benchmark->inplace ? "smart overwrite" : "naive overwrite";
  printCentered(bgStr, 60);
  printCentered(clipStr, 80);
  printCentered(inplaceStr, 100);

  unsigned long lastFpsRefresh = 0;
  unsigned long fpsRefreshFrequency = 500;
  timer.reset();
  while (timer.read() < seconds * 1000) {
    unsigned long t = timer.read();
    fps = 1000.0 * frames / t;
    int first_changed = timerToString(t, time);
    {
      auto label = ClippedStringViewLabel(time, timerFont, color::Black);
      // In case of the (optimal) inplace drawing, we need to use
      // color::Background (which is actually the default which could
      // generally be omitted), so that the entire bounding box of the glyph
      // gets redrawn, erasing any previous content in one pass. In the
      // (sub-optimal) case of non-inplace drawing, we are going to clear the
      // background before drawing the glyph, which means that the glyph can
      // draw only the non-empty pixels, rather than the entire bounding box.
      // It is achieved by using color::Transparent.
      //
      // Try experimenting, e.g. unconditionally setting fill mode to
      // color::Transparent, and see how it affects rendering.
      auto tile = MakeTileOf(
          label, timerBox, kNoAlign,
          (benchmark->inplace ? color::Background : color::Transparent));
      auto clear = Clear();
      DrawingContext dc(display);
      // Since everything is oriented about the center, let's move the origin
      // there to make it easier.
      dc.setTransformation(Transformation().translate(display.width() / 2,
                                                      display.height() / 2));
      int16_t timerDx = -timerBox.width() / 2;
      if (benchmark->clip) {
        // We restrict the clip box to only include the digits that have
        // actually changed since last draw.
        int16_t clip_offset = (6 - first_changed) * digitMetrics.advance();
        Box clip_box = Box(timerBox.xMax() + 1 - clip_offset, timerBox.yMin(),
                           timerBox.xMax(), timerBox.yMax());
        auto clippedTile =
            TransformedDrawable(Transformation().clip(clip_box), &tile);
        if (!benchmark->inplace) {
          // Clear the (clipped) background.
          dc.draw(TransformedDrawable(Transformation().clip(clip_box), &clear),
                  timerDx, 0);
        }
        // Draw the glyphs.
        dc.draw(clippedTile, timerDx, 0);
      } else {
        if (!benchmark->inplace) {
          // Clear the (unclipped) timer background.
          dc.draw(TransformedDrawable(Transformation().clip(timerBox), &clear),
                  timerDx, 0);
        }
        // Draw the glyphs.
        dc.draw(tile, timerDx, 0);
      }
      if (t - lastFpsRefresh > fpsRefreshFrequency) {
        lastFpsRefresh = t;
        StringPrinter p;
        p.printf("%0.1f fps", fps);
        auto fpsTile = MakeTileOf(
            StringViewLabel(p.get(), fpsFont, color::Black), fpsBox, kCenter);
        dc.draw(fpsTile, -fpsBox.width() / 2, -fpsBox.height() / 2 + 30);
      }
    }
    frames++;
  }
}

// 20x20 raster, 4-bit grayscale, 200 bytes
static const uint8_t diamond_plate_data[] PROGMEM = {
    0xCB, 0x96, 0x67, 0x77, 0x60, 0x00, 0x27, 0x87, 0x77, 0x78, 0xAF, 0xFB,
    0x76, 0x77, 0x85, 0x35, 0x88, 0x78, 0x77, 0x75, 0x19, 0xAE, 0xD8, 0x67,
    0x78, 0x88, 0x77, 0x78, 0x77, 0x82, 0x05, 0x78, 0xDD, 0x96, 0x77, 0x77,
    0x77, 0x78, 0x77, 0x85, 0x10, 0x87, 0x7B, 0xD9, 0x67, 0x77, 0x77, 0x77,
    0x88, 0x78, 0x70, 0x29, 0x77, 0xAB, 0x86, 0x77, 0x87, 0x77, 0x88, 0x78,
    0x95, 0x03, 0x97, 0x7A, 0xA7, 0x67, 0x77, 0x77, 0x87, 0x77, 0x79, 0x40,
    0x39, 0x88, 0x99, 0x77, 0x77, 0x77, 0x77, 0x77, 0x78, 0x94, 0x02, 0x89,
    0x88, 0x87, 0x77, 0x77, 0x77, 0x77, 0x77, 0x79, 0x50, 0x05, 0x75, 0x77,
    0x77, 0x77, 0x66, 0x67, 0x77, 0x77, 0x97, 0x20, 0x01, 0x78, 0x78, 0x66,
    0x9A, 0xC9, 0x77, 0x77, 0x78, 0x86, 0x35, 0x77, 0x77, 0x79, 0xCE, 0xC7,
    0x87, 0x77, 0x77, 0x78, 0x88, 0x77, 0x78, 0x9B, 0xA9, 0x33, 0x87, 0x77,
    0x77, 0x77, 0x77, 0x77, 0x8B, 0xA8, 0x76, 0x05, 0x87, 0x78, 0x77, 0x77,
    0x77, 0x78, 0x89, 0x97, 0x81, 0x28, 0x87, 0x77, 0x77, 0x77, 0x67, 0x88,
    0x87, 0x79, 0x30, 0x78, 0x78, 0x77, 0x77, 0x77, 0x78, 0x77, 0x87, 0x84,
    0x05, 0x87, 0x78, 0x87, 0x77, 0x77, 0x88, 0x68, 0x89, 0x30, 0x48, 0x77,
    0x87, 0x77, 0x77, 0x77, 0x86, 0x79, 0x83, 0x05, 0x97, 0x77, 0x76, 0x67,
    0x78, 0x78, 0x63, 0x75, 0x00, 0x59, 0x87, 0x77,
};

// In this example, we use a 4-bit grayscale background image data, but we map
// it to ARGB in a custom way, only using the upper-half of the range (so that
// the background remains reasonably 'light'). Such custom mapping can be
// implemented by supplying a custom color mode class to the Raster constructor.
class MyGrayscale {
 public:
  static const int8_t bits_per_pixel = 4;

  // The input is in the range of [0-15]. We map it to [0x80 - 0xF8] x RGB.
  inline constexpr Color toArgbColor(uint8_t in) const {
    return Color(0xFF808080 | in * 0x080808);
  }

  // OK to left as dummy, as long as we only read pre-defined data, and not
  // trying to draw (e.g. by specializing an Offscreen) using this color mode.
  inline constexpr uint8_t fromArgbColor(Color color) const { return 0; }

  // Indicates that all emitted colors are non-transparent. The library uses
  // this hint to optimize alpha-blending.
  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

const Raster<const uint8_t PROGMEM*, MyGrayscale>& diamond_plate() {
  static Raster<const uint8_t PROGMEM*, MyGrayscale> value(
      20, 20, diamond_plate_data, MyGrayscale());
  return value;
}

auto tile_pattern = MakeTiledRaster(&diamond_plate());

void scrollingText() {
  // This example demonstrates how to use arbitrary raster as a background
  // texture. We can still draw anything on top of it, including anti-aliased
  // text.
  display.setBackground(&tile_pattern);
  display.clear();

  StringViewLabel label(
      "Check out this awesome text banner. Note anti-aliased "
      "glyphs, with overlapping bounding boxes: 'Afy', 'fff'.  ",
      font_NotoSans_Italic_40(), color::DarkRed, FILL_MODE_RECTANGLE);
  int16_t dy = (label.font().metrics().ascent() + display.height()) / 2;
  for (int i = 0; i < label.extents().width(); i += 8) {
    {
      DrawingContext dc(display);
      dc.draw(label, display.width() - i, dy);
    }
    delay(40);
  }
  delay(2000);
}

void loop() {
  simpleBackground();
  tileWithSemiTransparentBackground();
  scrollingText();

  TimerBenchmark b;

  // This is the fastest and the most smooth version of text update. We use a
  // solid background, only redraw characters that have actually changed, and
  // use in-place over-write (which eliminates any flicker).
  b.clip = true;
  b.inplace = true;
  b.background = false;
  timerBenchmark(&b, 12);

  // This one demonstrates that flicker-less text update works even for
  // complex backgrounds. Rendering the background has some performance
  // impact, but ESP32 still pulls over 200 qps with a fast display.
  b.background = true;
  timerBenchmark(&b, 12);

  // The remaining examples are sub-optimal, and shown for the reference.

  // Back to the plain background, but now we clear the entire rectangle
  // before drawing each digit. This technique causes visible flicker (and tends
  // to be slower than over-write in place).
  b.inplace = false;
  b.background = false;
  timerBenchmark(&b, 5);

  // Same as before, but on a 'fancy' background. Flicker is even more visible
  // in this case.
  b.background = true;
  timerBenchmark(&b, 5);

  // The following two examples are like previous two, but with naive clipping.
  // That is, instead of just redrawing the modified digits, we redraw all of
  // them. It has obvious impact on fps, and further increases flicker.
  b.clip = false;
  b.background = false;
  timerBenchmark(&b, 5);
  b.background = true;
  timerBenchmark(&b, 5);
}

Color hsvToRgb(double h, double s, double v) {
  // See https://en.wikipedia.org/wiki/HSL_and_HSV.
  double c = v * s;
  double hp = h / 60.0;
  int ihp = (int)hp;
  double x = c * (1 - abs((hp - 2 * (ihp / 2)) - 1));
  double m = v - c;
  int ix = (int)(255 * x);
  int ic = (int)(255 * c);
  int im = (int)(255 * m);
  int ixm = ix + im;
  int icm = ic + im;
  switch (ihp % 6) {
    case 0:
      return Color(icm, ixm, im);
    case 1:
      return Color(ixm, icm, im);
    case 2:
      return Color(im, icm, ixm);
    case 3:
      return Color(im, ixm, icm);
    case 4:
      return Color(ixm, im, icm);
    case 5:
      return Color(icm, im, ixm);
    default:
      return Color(0);
  }
}

void initGradient() {
  double dh = 360.0 / kGradientSize;
  double h = 0;
  for (int i = 0; i < kGradientSize; ++i) {
    hsvGradient[i] = hsvToRgb(h, 0.5, 0.9);
    h += dh;
  }
}
