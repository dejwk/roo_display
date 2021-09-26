#include "roo_display/internal/hashtable.h"

#include "gtest/gtest.h"

namespace roo_display {
namespace internal {

TEST(Hashtable, Empty) {
  HashSet<int> s;
  EXPECT_EQ(0, s.size());
  EXPECT_EQ(s.begin(), s.end());
  EXPECT_EQ(s.end(), s.find(5));
}

TEST(Hashtable, Singleton) {
  HashSet<int> s;
  s.insert(4);
  EXPECT_EQ(1, s.size());
  auto itr = s.begin();
  ++itr;
  EXPECT_EQ(itr, s.end());
  EXPECT_TRUE(s.contains(4));
  EXPECT_FALSE(s.contains(5));
  EXPECT_NE(s.end(), s.find(4));
  EXPECT_EQ(s.end(), s.find(5));
}

TEST(Hashtable, Rehash) {
  HashSet<int> s;
  for (int i = 0; i < 15; ++i) {
    s.insert(i);
  } 
  for (int i = 0; i < 15; ++i) {
    EXPECT_TRUE(s.contains(i));
  }
}

TEST(Hashtable, StressSmallRange) {
  HashSet<uint8_t> test;
  std::set<uint8_t> control;
  for (int i = 0; i < 10000; ++i) {
    uint8_t v = rand();
    test.insert(v);
    control.insert(v);
  } 
  EXPECT_EQ(control, std::set<uint8_t>(test.begin(), test.end()));
}

TEST(Hashtable, StressLargeRange) {
  HashSet<uint16_t> test;
  std::set<uint16_t> control;
  for (int i = 0; i < 50000; ++i) {
    uint8_t v = rand();
    test.insert(v);
    control.insert(v);
  } 
  EXPECT_EQ(control, std::set<uint16_t>(test.begin(), test.end()));
}

}  // namespace internal
}  // namespace roo_display