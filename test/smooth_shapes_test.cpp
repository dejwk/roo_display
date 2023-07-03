
#include "roo_display/color/color.h"
#include "roo_display/shape/smooth.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(SmoothShapes, DrawSmallCirclesCenterWhole) {
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

TEST(SmoothShapes, DrawSmallCirclesCenterXoff) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(2, 2, 3, 4),
                             "55"
                             "FF"
                             "55"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 4),
                             "3DD3"
                             "7777"
                             "3DD3"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 1, 4, 5),
                             "0660"
                             "A99A"
                             "F00F"
                             "A99A"
                             "0660"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 5),
                             "07EE70"
                             "4C11C4"
                             "770077"
                             "4C11C4"
                             "07EE70"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 0, 5, 6),
                             "027720"
                             "4F88F4"
                             "C4004C"
                             "F0000F"
                             "C4004C"
                             "4F88F4"
                             "027720"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 6, 6),
                             "019EE910"
                             "0C7007C0"
                             "5A0000A5"
                             "77000077"
                             "5A0000A5"
                             "0C7007C0"
                             "019EE910"));
}

TEST(SmoothShapes, DrawSmallCirclesCenterXoffYoff) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(2, 3, 3, 4),
                             "CC"
                             "CC"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 5),
                             "0660"
                             "6AA6"
                             "6AA6"
                             "0660"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 5),
                             "5EE5"
                             "E33E"
                             "E33E"
                             "5EE5"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 6),
                             "016610"
                             "1D99D1"
                             "690096"
                             "690096"
                             "1D99D1"
                             "016610"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 6),
                             "09EE90"
                             "991199"
                             "E1001E"
                             "E1001E"
                             "991199"
                             "09EE90"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 6, 7),
                             "00277200"
                             "07E88E70"
                             "2E1001E2"
                             "78000087"
                             "78000087"
                             "2E1001E2"
                             "07E88E70"
                             "00277200"));
}

TEST(SmoothShapes, FillSmallCirclesCenterWhole) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "171"
                             "7F7"
                             "171"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "9F9"
                             "FFF"
                             "9F9"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "04740"
                             "4FFF4"
                             "7FFF7"
                             "4FFF4"
                             "04740"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "2BFB2"
                             "BFFFB"
                             "FFFFF"
                             "BFFFB"
                             "2BFB2"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "0057500"
                             "0AFFFA0"
                             "5FFFFF5"
                             "7FFFFF7"
                             "5FFFFF5"
                             "0AFFFA0"
                             "0057500"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 3.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "06CFC60"
                             "6FFFFF6"
                             "CFFFFFC"
                             "FFFFFFF"
                             "CFFFFFC"
                             "6FFFFF6"
                             "06CFC60"));
}

TEST(SmoothShapes, DrawTinyShapes) {
  EXPECT_THAT(CoercedTo<Alpha8>(SmoothFilledCircle({2, 3}, 0.5, color::Black),
                                Alpha8(color::Black)),
              // Pi/4 * 245 = hex(c8).
              MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "C8"));

  EXPECT_THAT(CoercedTo<Alpha8>(SmoothFilledCircle({2.1, 3.05}, 0.2, color::Black),
                                Alpha8(color::Black)),
              // Pi * (0.2)^2 * 245 = hex(20).
              MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "20"));

  EXPECT_THAT(CoercedTo<Alpha8>(SmoothCircle({2, 3}, 0, color::Black),
                                Alpha8(color::Black)),
              MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "C8"));
}

}  // namespace roo_display
