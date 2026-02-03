// Smooth font benchmark for roo_display.
#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9486/ili9486spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9486Spi display;

  Emulator() : viewport(), flexViewport(viewport, 6), display(flexViewport) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());
  }
} emulator;

#endif

#include <string>

#include "roo_display.h"
#include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSansMono_Regular/12.h"
#include "roo_fonts/NotoSans_Bold/15.h"
#include "roo_fonts/NotoSans_Bold/27.h"
#include "roo_fonts/NotoSans_Condensed/12.h"
#include "roo_fonts/NotoSans_Italic/18.h"
#include "roo_fonts/NotoSans_Italic/90.h"
#include "roo_fonts/NotoSans_Regular/12.h"
#include "roo_fonts/NotoSans_Regular/18.h"
#include "roo_fonts/NotoSans_Regular/27.h"

using namespace roo_display;

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/ili9486.h"
Ili9486spi<5, 17, 27> device;

LedcBacklit backlit(16, 1);
Display display(device);

struct FontCase {
  const char* name;
  const Font& (*font)();
};

const char* const kTestStrings[] = {
    "Hello, World!",
    "The quick brown fox jumps over the lazy dog 0123456789.",
    "Afy AVA WA To Ta Yo Tj Ty Te",
    "VaWw Wy Yo Ta Te To We Wo",
    "Zażółć gęślą jaźń 12345.67890",
    "Pchnąć w tę łódź jeża lub ośm skrzyń fig.",
    "μΩ ₿ ķ — symbols + diacritics",
    "()[]{}<>!?:;,-_+=/*\\|\"'",
};

const FontCase kFonts[] = {
    {"NotoSans Regular 12", &font_NotoSans_Regular_12},
    {"NotoSans Regular 18", &font_NotoSans_Regular_18},
    {"NotoSans Regular 27", &font_NotoSans_Regular_27},
    {"NotoSans Bold 15", &font_NotoSans_Bold_15},
    {"NotoSans Bold 27", &font_NotoSans_Bold_27},
    {"NotoSans Italic 18", &font_NotoSans_Italic_18},
    {"NotoSans Italic 90", &font_NotoSans_Italic_90},
    {"NotoSans Condensed 12", &font_NotoSans_Condensed_12},
    {"NotoSansMono Regular 12", &font_NotoSansMono_Regular_12},
};

void drawStrings(DrawingContext& dc, const Font& font, FillMode fill_mode) {
  int16_t x = 6;
  int16_t y = 6 + font.metrics().ascent();
  int16_t line_height = font.metrics().linespace() + 2;

  for (size_t i = 0; i < sizeof(kTestStrings) / sizeof(kTestStrings[0]); ++i) {
    roo::string_view text = kTestStrings[i];
    dc.draw(StringViewLabel(text, font, color::Black, fill_mode), x, y);
    y += line_height;
  }
}

unsigned long benchmarkFont(const Font& font, FillMode fill_mode) {
  {
    DrawingContext dc(display);
    dc.fill(color::White);
  }
  unsigned long start = micros();
  {
    DrawingContext dc(display);
    dc.setBackgroundColor(color::White);
    drawStrings(dc, font, fill_mode);
  }
  return micros() - start;
}

void runFontBenchmark(const Font& font, const char* name) {
  unsigned long visible = benchmarkFont(font, FILL_MODE_VISIBLE);
  delay(200);
  unsigned long rect = benchmarkFont(font, FILL_MODE_RECTANGLE);
  delay(200);

  Serial.printf("%-28s %12lu %12lu\n", name, visible, rect);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::White);
}

void runAllBenchmarks() {
  Serial.println("Text benchmark (smooth fonts)");
  Serial.println(
      "--------------------------------------------------------------");
  Serial.println("Font                                VISIBLE      RECTANGLE");
  Serial.println(
      "--------------------------------------------------------------");
  for (size_t i = 0; i < sizeof(kFonts) / sizeof(kFonts[0]); ++i) {
    runFontBenchmark(kFonts[i].font(), kFonts[i].name);
  }

  Serial.println("Done!");
}

void loop() {
  runAllBenchmarks();
  delay(1000);
}