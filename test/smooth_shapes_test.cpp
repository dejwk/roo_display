
#include "roo_display/color/color.h"
#include "roo_display/shape/smooth.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(SmoothShapes, DrawSmallCirclesWholeCenter) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "171"
                             "7F7"
                             "171"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "9F9"
                             "F0F"
                             "9F9"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "04740"
                             "4E7E4"
                             "77077"
                             "4E7E4"
                             "04740"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "2BFB2"
                             "B606B"
                             "F000F"
                             "B606B"
                             "2BFB2"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "0057500"
                             "0AB7BA0"
                             "5B000B5"
                             "7700077"
                             "5B000B5"
                             "0AB7BA0"
                             "0057500"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "06CFC60"
                             "6C303C6"
                             "C30003C"
                             "F00000F"
                             "C30003C"
                             "6C303C6"
                             "06CFC60"));
}

}  // namespace roo_display
