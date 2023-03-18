
#include "roo_display/image/image.h"
#include "roo_display/core/color.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(Image, XBitmap) {
  unsigned char test_bits[] = {0x00, 0x00, 0xfe, 0x03, 0xfe, 0x03, 0x1e, 0x00,
                               0x3e, 0x00, 0x76, 0x00, 0x66, 0x00, 0x06, 0x00,
                               0x06, 0x00, 0x06, 0x00, 0x00, 0x00};

  XBitmap<ConstDramPtr> bmp(12, 11, test_bits, color::White, color::Black);
  EXPECT_THAT(bmp, MatchesContent(WhiteOnBlack(), 12, 11,
                                  "            "
                                  " *********  "
                                  " *********  "
                                  " ****       "
                                  " *****      "
                                  " ** ***     "
                                  " **  **     "
                                  " **         "
                                  " **         "
                                  " **         "
                                  "            "));
}

}  // namespace roo_display
