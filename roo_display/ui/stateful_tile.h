#pragma once

#include <vector>

#include "roo_display/core/box.h"
#include "roo_display/core/drawable.h"
#include "roo_display/ui/tile.h"

namespace roo_display {

// Deprecated.
class StatefulTile : public roo_display::Drawable {
 public:
  StatefulTile(std::initializer_list<const Drawable*> drawables,
               int initial_state = 0, Alignment alignment = kCenter | kMiddle,
               Color bgcolor = color::Transparent)
      : tiles_(), extents_(), state_(initial_state) {
    for (auto i : drawables) {
      extents_ = Box::extent(extents_, i->extents());
    }
    initTiles(drawables, extents_, alignment, bgcolor);
  }

  StatefulTile(std::initializer_list<const Drawable*> drawables, Box extents,
               int initial_state = 0, Alignment alignment = kCenter | kMiddle,
               Color bgcolor = color::Transparent)
      : tiles_(), extents_(std::move(extents)), state_(initial_state) {
    initTiles(drawables, extents, alignment, bgcolor);
  }

  Box extents() const override { return extents_; }

 protected:
  bool setState(int state) {
    if (state_ == state) return false;
    state_ = state;
    return true;
  }

  int getState() const { return state_; }

 private:
  void initTiles(std::initializer_list<const Drawable*> tiles,
                 const Box& extents, Alignment alignment, Color bgcolor) {
    for (auto i : tiles) {
      tiles_.emplace_back(i, extents, alignment, bgcolor);
    }
  }

  void drawTo(const roo_display::Surface& s) const override {
    s.drawObject(tiles_[state_]);
  }

  std::vector<Tile> tiles_;
  Box extents_;
  int state_;
};

}  // namespace roo_display