
#include "roo_display/driver/common/compactor.h"

#include <memory>
#include <ostream>
#include <random>
// #include "gtest/gtest.h"
// #include "roo_display/color/color.h"
// #include "roo_display/core/color_io.h"
// #include "roo_display/core/offscreen.h"
#include "testing.h"

using namespace testing;

// std::default_random_engine generator;

namespace roo_display {

struct Write {
  Write(int16_t offset, int16_t x, int16_t y,
        Compactor::WriteDirection direction, int16_t count)
      : offset(offset), x(x), y(y), direction(direction), count(count) {}

  int16_t offset;
  int16_t x;
  int16_t y;
  Compactor::WriteDirection direction;
  int16_t count;
};

std::ostream& operator<<(std::ostream& os, const Write& write) {
  os << "{offset:" << write.offset << ", x:" << write.x << ", y:" << write.y
     << ", direction:" << write.direction << ", count:" << write.count << "}";
  return os;
}

bool operator==(const Write& a, const Write& b) {
  return a.offset == b.offset && a.x == b.x && a.y == b.y &&
         a.direction == b.direction && a.count == b.count;
}

class MockWriter {
 public:
  MockWriter(std::initializer_list<Write> expected)
      : expected_(expected), idx_(0) {}

  void operator()(int16_t offset, int16_t x, int16_t y,
                  Compactor::WriteDirection direction, int16_t count) {
    ASSERT_LT(idx_, expected_.size());
    Write actual(offset, x, y, direction, count);
    EXPECT_EQ(expected_[idx_++], actual);
  }

 private:
  std::vector<Write> expected_;
  int idx_;
};

TEST(Compactor, RightSimple) {
  MockWriter writer({Write(0, 4, 5, Compactor::RIGHT, 3)});
  int16_t xs[] = {4, 5, 6};
  int16_t ys[] = {5, 5, 5};
  Compactor().drawPixels(xs, ys, 3, std::move(writer));
}

TEST(Compactor, RightWithReps) {
  MockWriter writer({
      Write(0, 4, 5, Compactor::RIGHT, 3),
      Write(3, 6, 5, Compactor::RIGHT, 1),
      Write(4, 8, 5, Compactor::RIGHT, 3),
  });
  int16_t xs[] = {4, 5, 6, 6, 8, 9, 10};
  int16_t ys[] = {5, 5, 5, 5, 5, 5, 5};
  Compactor().drawPixels(xs, ys, 7, std::move(writer));
}

TEST(Compactor, DownSimple) {
  MockWriter writer({Write(0, 5, 4, Compactor::DOWN, 3)});
  int16_t xs[] = {5, 5, 5};
  int16_t ys[] = {4, 5, 6};
  Compactor().drawPixels(xs, ys, 3, std::move(writer));
}

TEST(Compactor, DownWithReps) {
  MockWriter writer({
      Write(0, 5, 4, Compactor::DOWN, 3),
      Write(3, 5, 6, Compactor::RIGHT, 1),
      Write(4, 5, 8, Compactor::DOWN, 3),
  });
  int16_t xs[] = {5, 5, 5, 5, 5, 5, 5};
  int16_t ys[] = {4, 5, 6, 6, 8, 9, 10};
  Compactor().drawPixels(xs, ys, 7, std::move(writer));
}

TEST(Compactor, RightDownRightDown) {
  MockWriter writer({
      Write(0, 4, 5, Compactor::RIGHT, 3),
      Write(3, 6, 6, Compactor::DOWN, 2),
      Write(5, 7, 7, Compactor::RIGHT, 2),
      Write(7, 8, 8, Compactor::DOWN, 2),
  });
  int16_t xs[] = {4, 5, 6, 6, 6, 7, 8, 8, 8};
  int16_t ys[] = {5, 5, 5, 6, 7, 7, 7, 8, 9};
  Compactor().drawPixels(xs, ys, 9, std::move(writer));
}

TEST(Compactor, RightSingleDownSingleRight) {
  MockWriter writer({
      Write(0, 4, 5, Compactor::RIGHT, 3),
      Write(3, 7, 7, Compactor::RIGHT, 1),
      Write(4, 8, 8, Compactor::DOWN, 2),
      Write(6, 9, 10, Compactor::RIGHT, 1),
      Write(7, 10, 11, Compactor::RIGHT, 2),
  });
  int16_t xs[] = {4, 5, 6, 7, 8, 8, 9, 10, 11};
  int16_t ys[] = {5, 5, 5, 7, 8, 9, 10, 11, 11};
  Compactor().drawPixels(xs, ys, 9, std::move(writer));
}

TEST(Compactor, LeftRight) {
  MockWriter writer({
      Write(0, 4, 5, Compactor::LEFT, 3),
      Write(3, 2, 4, Compactor::RIGHT, 3),
  });
  int16_t xs[] = {4, 3, 2, 2, 3, 4};
  int16_t ys[] = {5, 5, 5, 4, 4, 4};
  Compactor().drawPixels(xs, ys, 6, std::move(writer));
}

TEST(Compactor, UpRightDownLeft) {
  MockWriter writer({
      Write(0, 7, 11, Compactor::UP, 4),
      Write(4, 8, 8, Compactor::RIGHT, 3),
      Write(7, 10, 9, Compactor::DOWN, 3),
      Write(10, 9, 11, Compactor::LEFT, 3),
  });
  int16_t xs[] = {7, 7, 7, 7, 8, 9, 10, 10, 10, 10, 9, 8, 7};
  int16_t ys[] = {11, 10, 9, 8, 8, 8, 8, 9, 10, 11, 11, 11, 11};
  Compactor().drawPixels(xs, ys, 13, std::move(writer));
}

}  // namespace roo_display
