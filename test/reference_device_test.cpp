#include "fake/roo_display/fake/reference_device.h"

#include "gtest/gtest.h"
#include "roo_display/color/named.h"
#include "test/testing_viewport.h"

namespace roo_display {
namespace {

// Verifies framebuffer copies work in the reference display's non-blending
// mode, which is used by the chart emulator.
TEST(ReferenceDeviceTest, CopiesPixelsWithoutBlendingSupport) {
  testing::TestViewport viewport;
  ReferenceDisplayDevice device(4, 2, viewport);
  device.init();
  EXPECT_FALSE(device.getCapabilities().supportsBlending());
  EXPECT_TRUE(device.getCapabilities().supportsBlitCopy());

  device.begin();
  device.setAddress(0, 0, 1, 0, BlendingMode::kSource);
  device.fill(Color(0xFFFF5500), 2);
  device.blitCopy(0, 0, 1, 0, 2, 0);
  device.end();

  EXPECT_EQ(viewport.getPixel(0, 0), viewport.getPixel(2, 0));
  EXPECT_EQ(viewport.getPixel(1, 0), viewport.getPixel(3, 0));
  EXPECT_NE(viewport.getPixel(2, 0), viewport.getPixel(2, 1));
}

// Verifies bulk drawing and copying retain the order of nonuniform pixels.
TEST(ReferenceDeviceTest, CopiesMulticolorPixelsInOrder) {
  testing::TestViewport viewport;
  ReferenceDisplayDevice device(4, 1, viewport);
  device.init();

  device.begin();
  device.setAddress(0, 0, 1, 0, BlendingMode::kSource);
  Color pixels[] = {color::Red, color::Blue};
  device.write(pixels, 2);
  device.blitCopy(0, 0, 1, 0, 2, 0);
  device.end();

  EXPECT_EQ(viewport.getPixel(0, 0), color::Red.asArgb());
  EXPECT_EQ(viewport.getPixel(1, 0), color::Blue.asArgb());
  EXPECT_EQ(viewport.getPixel(2, 0), color::Red.asArgb());
  EXPECT_EQ(viewport.getPixel(3, 0), color::Blue.asArgb());
}

}  // namespace
}  // namespace roo_display
