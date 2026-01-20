#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/products/waveshare/waveshare-esp32-s3-touch-lcd-43/waveshare_esp32s3_touch_lcd_43.h"
#include "roo_display/shape/basic.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Regular/27.h"

using namespace roo_display;
using namespace roo_display::products::waveshare;

// Create hardware object
WaveshareEsp32s3TouchLcd43 hardware;
Display display(hardware);

// Touch statistics
uint32_t touchCount = 0;
uint32_t lastTouchTime = 0;

// Reset button coordinates (position and size)
const int16_t RESET_BTN_X = 650;
const int16_t RESET_BTN_Y = 30;
const int16_t RESET_BTN_W = 130;
const int16_t RESET_BTN_H = 60;

void setup() {
  Serial.begin(115200);

  Serial.println("\n=== Waveshare Display Test ===\n");

  // Initialize I2C bus
  hardware.initTransport();

  // Initialize hardware (CH422G and GT911)
  if (!hardware.init()) {
    Serial.println("Init failed!");
    while (1) delay(1000);
  }

  // Print display information to serial console
  Serial.println("Display Information:");
  Serial.printf("  Resolution: %d x %d\n", 
                display.width(), display.height());
  Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.println();

  // Initialize display with black background
  display.init(color::Black);

  // Draw geometric shapes in a row
  DrawingContext dc(display);

  int16_t centerY = 240;
  int16_t startX = 100;
  int16_t spacing = 140;

  dc.draw(TextLabel("***    Just a very basic test    ***",
                    font_NotoSans_Regular_27(), color::Cyan),
          50, 50);

  // Draw six different shapes to demonstrate display capabilities
  dc.draw(FilledCircle::ByExtents(startX, centerY - 50, 50, 
                                   color::Red));
  dc.draw(FilledRect(startX + spacing - 50, centerY - 50,
                     startX + spacing + 50, centerY + 50, 
                     color::Green));
  dc.draw(Rect(startX + 2 * spacing - 50, centerY - 50,
               startX + 2 * spacing + 50, centerY + 50, 
               color::Blue));
  dc.draw(Circle::ByExtents(startX + 3 * spacing, centerY - 50,
                            50, color::Cyan));
  dc.draw(FilledRect(startX + 4 * spacing - 50, centerY - 50,
                     startX + 4 * spacing + 50, centerY + 50,
                     color::Magenta));
  dc.draw(Line(startX + 5 * spacing - 50, centerY - 50,
               startX + 5 * spacing + 50, centerY + 50, 
               color::Yellow));
  dc.draw(Line(startX + 5 * spacing + 50, centerY - 50,
               startX + 5 * spacing - 50, centerY + 50, 
               color::Yellow));

  // Draw reset button in top-right corner
  dc.draw(FilledRect(RESET_BTN_X, RESET_BTN_Y,
                     RESET_BTN_X + RESET_BTN_W, 
                     RESET_BTN_Y + RESET_BTN_H,
                     color::DarkRed));
  dc.draw(Rect(RESET_BTN_X, RESET_BTN_Y,
               RESET_BTN_X + RESET_BTN_W, 
               RESET_BTN_Y + RESET_BTN_H,
               color::White));
  dc.draw(TextLabel("Reset", font_NotoSans_Regular_27(), 
                    color::White),
          RESET_BTN_X + 25, RESET_BTN_Y + 38);

  dc.draw(TextLabel("Open serial console and touch screen ...",
                    font_NotoSans_Regular_27(), color::White),
          50, 420);

  Serial.println("Ready! Touch test active.\n");
  Serial.println("Touch format: [count] (x, y) @ time_ms [region]");
  Serial.println("------------------------------------------------");
}

void loop() {
  int16_t x, y;

  if (display.getTouch(x, y)) {
    uint32_t now = millis();
    touchCount++;

    // Check if reset button was pressed
    if (x >= RESET_BTN_X && x <= (RESET_BTN_X + RESET_BTN_W) &&
        y >= RESET_BTN_Y && y <= (RESET_BTN_Y + RESET_BTN_H)) {
      Serial.println("\n*** RESET BUTTON PRESSED ***\n");
      delay(500);
      ESP.restart();
    }

    // Determine which region of the screen was touched
    const char* region = "unknown";
    if (y < 100) {
      region = "top";
    } else if (y > 380) {
      region = "bottom";
    } else if (y > 140 && y < 340) {
      region = "shapes-row";
    }

    // Print detailed touch information to serial console
    Serial.printf("[%4lu] Touch: (%3d, %3d) @ %6lu ms [%s]",
                  touchCount, x, y, now, region);

    // Calculate and print time since last touch
    if (lastTouchTime > 0) {
      uint32_t delta = now - lastTouchTime;
      Serial.printf(" (delta: %lu ms)", delta);
    }
    Serial.println();

    lastTouchTime = now;

    // Visualize touch with yellow circle at touch position
    DrawingContext dc(display);
    dc.draw(FilledCircle::ByExtents(x - 10, y - 10, 20, 
                                     color::Yellow));

    delay(50);
  }

  delay(10);
}