#include <string>

#include "Arduino.h"
#include "fltk_device.h"
#include "roo_display.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/12.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/18.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/27.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/40.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/60.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/8.h"
#include "roo_smooth_fonts/NotoSansMono_Bold/90.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/12.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/18.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/27.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/40.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/60.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/8.h"
#include "roo_smooth_fonts/NotoSansMono_Regular/90.h"
#include "roo_smooth_fonts/NotoSans_Bold/12.h"
#include "roo_smooth_fonts/NotoSans_Bold/18.h"
#include "roo_smooth_fonts/NotoSans_Bold/27.h"
#include "roo_smooth_fonts/NotoSans_Bold/40.h"
#include "roo_smooth_fonts/NotoSans_Bold/60.h"
#include "roo_smooth_fonts/NotoSans_Bold/8.h"
#include "roo_smooth_fonts/NotoSans_Bold/90.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/12.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/18.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/27.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/40.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/60.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/8.h"
#include "roo_smooth_fonts/NotoSans_BoldItalic/90.h"
#include "roo_smooth_fonts/NotoSans_Condensed/12.h"
#include "roo_smooth_fonts/NotoSans_Condensed/18.h"
#include "roo_smooth_fonts/NotoSans_Condensed/27.h"
#include "roo_smooth_fonts/NotoSans_Condensed/40.h"
#include "roo_smooth_fonts/NotoSans_Condensed/60.h"
#include "roo_smooth_fonts/NotoSans_Condensed/8.h"
#include "roo_smooth_fonts/NotoSans_Condensed/90.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/12.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/18.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/27.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/40.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/60.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/8.h"
#include "roo_smooth_fonts/NotoSans_CondensedItalic/90.h"
#include "roo_smooth_fonts/NotoSans_Italic/12.h"
#include "roo_smooth_fonts/NotoSans_Italic/18.h"
#include "roo_smooth_fonts/NotoSans_Italic/27.h"
#include "roo_smooth_fonts/NotoSans_Italic/40.h"
#include "roo_smooth_fonts/NotoSans_Italic/60.h"
#include "roo_smooth_fonts/NotoSans_Italic/8.h"
#include "roo_smooth_fonts/NotoSans_Italic/90.h"
#include "roo_smooth_fonts/NotoSans_Regular/12.h"
#include "roo_smooth_fonts/NotoSans_Regular/18.h"
#include "roo_smooth_fonts/NotoSans_Regular/27.h"
#include "roo_smooth_fonts/NotoSans_Regular/40.h"
#include "roo_smooth_fonts/NotoSans_Regular/60.h"
#include "roo_smooth_fonts/NotoSans_Regular/8.h"
#include "roo_smooth_fonts/NotoSans_Regular/90.h"
#include "roo_smooth_fonts/NotoSerif_Bold/12.h"
#include "roo_smooth_fonts/NotoSerif_Bold/18.h"
#include "roo_smooth_fonts/NotoSerif_Bold/27.h"
#include "roo_smooth_fonts/NotoSerif_Bold/40.h"
#include "roo_smooth_fonts/NotoSerif_Bold/60.h"
#include "roo_smooth_fonts/NotoSerif_Bold/8.h"
#include "roo_smooth_fonts/NotoSerif_Bold/90.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/12.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/18.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/27.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/40.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/60.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/8.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/90.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/12.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/18.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/27.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/40.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/60.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/8.h"
#include "roo_smooth_fonts/NotoSerif_Condensed/90.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/12.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/18.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/27.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/40.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/60.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/8.h"
#include "roo_smooth_fonts/NotoSerif_CondensedItalic/90.h"
#include "roo_smooth_fonts/NotoSerif_Italic/12.h"
#include "roo_smooth_fonts/NotoSerif_Italic/18.h"
#include "roo_smooth_fonts/NotoSerif_Italic/27.h"
#include "roo_smooth_fonts/NotoSerif_Italic/40.h"
#include "roo_smooth_fonts/NotoSerif_Italic/60.h"
#include "roo_smooth_fonts/NotoSerif_Italic/8.h"
#include "roo_smooth_fonts/NotoSerif_Italic/90.h"
#include "roo_smooth_fonts/NotoSerif_Regular/12.h"
#include "roo_smooth_fonts/NotoSerif_Regular/18.h"
#include "roo_smooth_fonts/NotoSerif_Regular/27.h"
#include "roo_smooth_fonts/NotoSerif_Regular/40.h"
#include "roo_smooth_fonts/NotoSerif_Regular/60.h"
#include "roo_smooth_fonts/NotoSerif_Regular/8.h"
#include "roo_smooth_fonts/NotoSerif_Regular/90.h"

using namespace roo_display;

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h" 
St7789_240x240<13, 2, 4> device;

Display display(&device, nullptr);

struct FontFamily {
  FontFamily(const Font& f8, const Font& f12, const Font& f18, const Font& f27,
             const Font& f40, const Font& f60, const Font& f90)
      : f8(f8), f12(f12), f18(f18), f27(f27), f40(f40), f60(f60), f90(f90) {}

  const Font& f8;
  const Font& f12;
  const Font& f18;
  const Font& f27;
  const Font& f40;
  const Font& f60;
  const Font& f90;
};

FontFamily notoSansRegular(font_NotoSans_Regular_8(),
                           font_NotoSans_Regular_12(),
                           font_NotoSans_Regular_18(),
                           font_NotoSans_Regular_27(),
                           font_NotoSans_Regular_40(),
                           font_NotoSans_Regular_60(),
                           font_NotoSans_Regular_90());

FontFamily notoSansItalic(font_NotoSans_Italic_8(), font_NotoSans_Italic_12(),
                          font_NotoSans_Italic_18(), font_NotoSans_Italic_27(),
                          font_NotoSans_Italic_40(), font_NotoSans_Italic_60(),
                          font_NotoSans_Italic_90());

FontFamily notoSansBold(font_NotoSans_Bold_8(), font_NotoSans_Bold_12(),
                        font_NotoSans_Bold_18(), font_NotoSans_Bold_27(),
                        font_NotoSans_Bold_40(), font_NotoSans_Bold_60(),
                        font_NotoSans_Bold_90());

FontFamily notoSansBoldItalic(font_NotoSans_BoldItalic_8(),
                              font_NotoSans_BoldItalic_12(),
                              font_NotoSans_BoldItalic_18(),
                              font_NotoSans_BoldItalic_27(),
                              font_NotoSans_BoldItalic_40(),
                              font_NotoSans_BoldItalic_60(),
                              font_NotoSans_BoldItalic_90());

FontFamily notoSansCondensed(font_NotoSans_Condensed_8(),
                             font_NotoSans_Condensed_12(),
                             font_NotoSans_Condensed_18(),
                             font_NotoSans_Condensed_27(),
                             font_NotoSans_Condensed_40(),
                             font_NotoSans_Condensed_60(),
                             font_NotoSans_Condensed_90());

FontFamily notoSansCondensedItalic(font_NotoSans_CondensedItalic_8(),
                                   font_NotoSans_CondensedItalic_12(),
                                   font_NotoSans_CondensedItalic_18(),
                                   font_NotoSans_CondensedItalic_27(),
                                   font_NotoSans_CondensedItalic_40(),
                                   font_NotoSans_CondensedItalic_60(),
                                   font_NotoSans_CondensedItalic_90());

FontFamily notoSerifRegular(font_NotoSerif_Regular_8(),
                            font_NotoSerif_Regular_12(),
                            font_NotoSerif_Regular_18(),
                            font_NotoSerif_Regular_27(),
                            font_NotoSerif_Regular_40(),
                            font_NotoSerif_Regular_60(),
                            font_NotoSerif_Regular_90());

FontFamily notoSerifItalic(font_NotoSerif_Italic_8(),
                           font_NotoSerif_Italic_12(),
                           font_NotoSerif_Italic_18(),
                           font_NotoSerif_Italic_27(),
                           font_NotoSerif_Italic_40(),
                           font_NotoSerif_Italic_60(),
                           font_NotoSerif_Italic_90());

FontFamily notoSerifBold(font_NotoSerif_Bold_8(), font_NotoSerif_Bold_12(),
                         font_NotoSerif_Bold_18(), font_NotoSerif_Bold_27(),
                         font_NotoSerif_Bold_40(), font_NotoSerif_Bold_60(),
                         font_NotoSerif_Bold_90());

FontFamily notoSerifBoldItalic(font_NotoSerif_BoldItalic_8(),
                               font_NotoSerif_BoldItalic_12(),
                               font_NotoSerif_BoldItalic_18(),
                               font_NotoSerif_BoldItalic_27(),
                               font_NotoSerif_BoldItalic_40(),
                               font_NotoSerif_BoldItalic_60(),
                               font_NotoSerif_BoldItalic_90());

FontFamily notoSerifCondensed(font_NotoSerif_Condensed_8(),
                              font_NotoSerif_Condensed_12(),
                              font_NotoSerif_Condensed_18(),
                              font_NotoSerif_Condensed_27(),
                              font_NotoSerif_Condensed_40(),
                              font_NotoSerif_Condensed_60(),
                              font_NotoSerif_Condensed_90());

FontFamily notoSerifCondensedItalic(font_NotoSerif_CondensedItalic_8(),
                                    font_NotoSerif_CondensedItalic_12(),
                                    font_NotoSerif_CondensedItalic_18(),
                                    font_NotoSerif_CondensedItalic_27(),
                                    font_NotoSerif_CondensedItalic_40(),
                                    font_NotoSerif_CondensedItalic_60(),
                                    font_NotoSerif_CondensedItalic_90());

FontFamily notoSansMonoRegular(font_NotoSansMono_Regular_8(),
                               font_NotoSansMono_Regular_12(),
                               font_NotoSansMono_Regular_18(),
                               font_NotoSansMono_Regular_27(),
                               font_NotoSansMono_Regular_40(),
                               font_NotoSansMono_Regular_60(),
                               font_NotoSansMono_Regular_90());

FontFamily notoSansMonoBold(font_NotoSansMono_Bold_8(),
                            font_NotoSansMono_Bold_12(),
                            font_NotoSansMono_Bold_18(),
                            font_NotoSansMono_Bold_27(),
                            font_NotoSansMono_Bold_40(),
                            font_NotoSansMono_Bold_60(),
                            font_NotoSansMono_Bold_90());

void setup() {
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::Gainsboro);
}

int16_t printLn(DrawingContext& dc, const Font& font, int16_t x, int16_t y,
                const std::string& text, Color color) {
  dc.draw(TextLabel(font, text, color), x, y + font.metrics().glyphYMax());
  return font.metrics().linespace() + 1;
}

void printText(const FontFamily& fonts) {
  DrawingContext dc(&display);
  dc.fill(color::Gainsboro);
  int16_t y = 1;
  const char* text = "Zażółć gęślą jaźń 12345.67890 !@#$%^&*()";
  Color color = color::Black;
  dc.setBgColor(color::BlanchedAlmond);
  y += printLn(dc, fonts.f8, 10, y, text, color);
  dc.setBgColor(color::LemonChiffon);
  y += printLn(dc, fonts.f12, 10, y, text, color);
  dc.setBgColor(color::Beige);
  y += printLn(dc, fonts.f18, 10, y, text, color);
  dc.setBgColor(color::PapayaWhip);
  y += printLn(dc, fonts.f27, 10, y, text, color);
  dc.setBgColor(color::PowderBlue);
  y += printLn(dc, fonts.f40, 10, y, text, color);
  dc.setBgColor(color::LightGreen);
  y += printLn(dc, fonts.f60, 10, y, text, color);
  dc.setBgColor(color::MistyRose);
  y += printLn(dc, fonts.f90, 10, y, text, color);
  delay(1000);
}

void loop() {
  // Sans serif fonts.
  printText(notoSansRegular);
  printText(notoSansItalic);
  printText(notoSansBold);
  printText(notoSansBoldItalic);
  printText(notoSansCondensed);
  printText(notoSansCondensedItalic);
  // Serif fonts.
  printText(notoSerifRegular);
  printText(notoSerifItalic);
  printText(notoSerifBold);
  printText(notoSerifBoldItalic);
  printText(notoSerifCondensed);
  printText(notoSerifCondensedItalic);
  // Monotype font.
  printText(notoSansMonoRegular);
  printText(notoSansMonoBold);
}
