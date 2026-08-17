#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/color/named.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic.h"

namespace {

roo_display::DefaultSpi spi;
roo_display::Ili9341spi<5, 16, -1> panel(spi);
roo_display::Display display(panel);

}  // namespace

extern "C" void app_main() {
  spi.init(18, 19, 23);
  display.init(roo_display::color::Black);
  roo_display::DrawingContext dc(display);
  dc.draw(roo_display::FilledRect(10, 10, 29, 29, roo_display::color::Red));
}
