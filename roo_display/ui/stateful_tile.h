#pragma once

#include <vector>

#include "roo_display/core/box.h"
#include "roo_display/core/drawable.h"
#include "roo_display/ui/tile.h"

namespace roo_display {

class StatefulTile : public roo_display::Drawable {
 public:
  StatefulTile(std::initializer_list<const Drawable*> drawables,
               int initial_state = 0, HAlign halign = HAlign::Center(),
               VAlign valign = VAlign::Middle(),
               Color bgcolor = color::Transparent)
      : tiles_(), extents_(), state_(initial_state) {
    for (auto i : drawables) {
      extents_ = Box::extent(extents_, i->extents());
    }
    initTiles(drawables, extents_, halign, valign, bgcolor);
  }

  StatefulTile(std::initializer_list<const Drawable*> drawables, Box extents,
               int initial_state = 0, HAlign halign = HAlign::Center(),
               VAlign valign = VAlign::Middle(),
               Color bgcolor = color::Transparent)
      : tiles_(), extents_(std::move(extents)), state_(initial_state) {
    initTiles(drawables, extents, halign, valign, bgcolor);
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
                 const Box& extents, HAlign halign, VAlign valign,
                 Color bgcolor) {
    for (auto i : tiles) {
      tiles_.emplace_back(i, extents, halign, valign, bgcolor);
    }
  }

  void drawTo(roo_display::DisplayOutput* output, int16_t x, int16_t y,
              const roo_display::Box& clip_box,
              roo_display::Color bgcolor) const override {
    drawObject(output, x, y, clip_box, bgcolor, tiles_[state_]);
  }

  std::vector<Tile> tiles_;
  Box extents_;
  int state_;
};

}  // namespace roo_display