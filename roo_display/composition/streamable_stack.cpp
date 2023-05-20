#include "roo_display/composition/streamable_stack.h"

namespace roo_display {

namespace {

struct Chunk {
  Chunk(uint16_t width, uint16_t input_mask)
      : width_(width), input_mask_(input_mask) {}

  uint16_t width_;
  uint16_t input_mask_;
};

class Block {
 public:
  Block(uint16_t height) : height_(height), all_inputs_(0) {}

  void AddChunk(uint16_t width, uint16_t input_mask) {
    chunks_.emplace_back(width, input_mask);
    all_inputs_ |= input_mask;
  }

  void Merge(uint16_t x_offset, uint16_t width, uint16_t input_mask);

 private:
  friend class Composition;

  uint16_t height_;
  uint16_t all_inputs_;
  std::vector<Chunk> chunks_;
};

inline void Block::Merge(uint16_t x_offset, uint16_t width,
                         uint16_t input_mask) {
  std::vector<Chunk> newchunks;
  newchunks.reserve(chunks_.size());

  auto i = chunks_.begin();
  int16_t cursor = 0;
  uint16_t remaining_old_width;
  all_inputs_ |= input_mask;
  if (x_offset > 0) {
    while (cursor + i->width_ <= x_offset) {
      newchunks.push_back(std::move(*i));
      cursor += i->width_;
      ++i;
    }
    remaining_old_width = i->width_;
    if (cursor < x_offset) {
      uint16_t w = x_offset - cursor;
      newchunks.emplace_back(w, i->input_mask_);
      cursor += w;
      remaining_old_width -= w;
    }
  } else {
    remaining_old_width = i->width_;
  }
  while (true) {
    if (width == 0) {
      newchunks.emplace_back(remaining_old_width, i->input_mask_);
      cursor += remaining_old_width;
      i++;
      if (i == chunks_.end()) {
        chunks_.swap(newchunks);
        return;
      } else {
        remaining_old_width = i->width_;
      }
    } else {
      uint16_t w = std::min(remaining_old_width, width);
      newchunks.emplace_back(w, i->input_mask_ | input_mask);
      if (w == remaining_old_width) {
        i++;
        if (i == chunks_.end()) {
          chunks_.swap(newchunks);
          return;
        } else {
          remaining_old_width = i->width_;
        }
      } else {
        remaining_old_width -= w;
      }
      width -= w;
      cursor += w;
    }
  }
}

class Program {
 public:
  uint16_t get(uint16_t idx) const { return prg_[idx]; }
  std::size_t size() const { return prg_.size(); }

 private:
  friend class Composition;

  std::vector<uint16_t> prg_;
};

enum Instruction {
  LOOP = 20000,
  RET = 20001,
  EXIT = 30000,
  BLANK = 10001,
  WRITE = 10002,
  WRITE_SINGLE = 10003,
  SKIP = 10004
};

class Composition {
 public:
  Composition(const Box& bounds) : bounds_(bounds), input_count_(0) {
    data_.emplace_back(bounds.height());
    data_.back().AddChunk(bounds.width(), 0);
  }

  // Extents must be pre-intersected with bounds_.
  bool Add(const Box& extents, BlendingMode blending_mode);

  void Compile(Program* prg);

 private:
  Box bounds_;
  std::vector<Box> input_extents_;
  std::vector<BlendingMode> blending_modes_;
  std::vector<Block> data_;
  int input_count_;
};

// Returns true for a blending mode when a transparent source implies
// transparent result.
bool IsBlendingModeSourceClearing(BlendingMode blending_mode) {
  switch (blending_mode) {
    case BLENDING_MODE_SOURCE:
    case BLENDING_MODE_SOURCE_IN:
    case BLENDING_MODE_SOURCE_OUT:
    case BLENDING_MODE_DESTINATION_IN:
    case BLENDING_MODE_DESTINATION_ATOP:
    case BLENDING_MODE_CLEAR: {
      return true;
    }
    default: {
      return false;
    }
  }
}

// Returns true for a blending mode when a transparent destination implies
// transparent result.
bool IsBlendingModeDestinationClearing(BlendingMode blending_mode) {
  switch (blending_mode) {
    case BLENDING_MODE_SOURCE_IN:
    case BLENDING_MODE_SOURCE_ATOP:
    case BLENDING_MODE_DESTINATION:
    case BLENDING_MODE_DESTINATION_IN:
    case BLENDING_MODE_DESTINATION_OUT:
    case BLENDING_MODE_CLEAR: {
      return true;
    }
    default: {
      return false;
    }
  }
}

inline void Composition::Compile(Program* prg) {
  std::vector<uint16_t>* code = &prg->prg_;
  // First, emit initial skips.
  int i = 0;
  for (const auto& input : input_extents_) {
    if (input.yMin() < bounds_.yMin()) {
      code->push_back(SKIP);
      code->push_back(i);
      code->push_back((bounds_.yMin() - input.yMin()) * input.width());
    }
    i++;
  }
  // Then, go block-by-block.
  for (const auto& block : data_) {
    // if (block.chunks_.size() == 1) {
    //   uint16_t mask = block.chunks_[0].input_mask_;
    //   uint32_t total = (uint32_t)block.chunks_[0].width_ * block.height_;
    //   if (total <= 65535) {
    //     // Optimize fully empty blocks.
    //     if (mask == 0) {
    //       code->push_back(BLANK);
    //       code->push_back(total);
    //       continue;
    //     }
    //   }
    //   // See if we can merge, which is OK if all affected inputs are
    //   // non-extending.
    //   int i = 0;
    //   while (!(mask & 1)) {
    //     ++i;
    //     mask >>= 1;
    //   }
    //   int first = i;
    //   while (true) {
    //     const auto& input = input_extents_[i];
    //     if (input.xMin() < bounds_.xMin() || input.xMax() > bounds_.xMax())
    //       break;
    //     mask >>= 1;
    //     if (mask == 0) {
    //       // Success. Merge.
    //       if (i == first) {
    //         code->push_back(WRITE_SINGLE);
    //         code->push_back(first);
    //         code->push_back(total);
    //       } else {
    //         code->push_back(WRITE);
    //         code->push_back(block.chunks_[0].input_mask_);
    //         code->push_back(total);
    //       }
    //       break;
    //     }
    //     while (true) {
    //       ++i;
    //       if (mask & 1) break;
    //       mask >>= 1;
    //     }
    //   }
    //   if (mask == 0) continue;
    // }

    // Emit the loop code.
    code->push_back(LOOP);
    code->push_back(block.height_);
    // First, emit potential left skips.
    i = 0;
    for (const auto& input : input_extents_) {
      if (input.xMin() < bounds_.xMin() && block.all_inputs_ & (1 << i)) {
        code->push_back(SKIP);
        code->push_back(i);
        code->push_back(bounds_.xMin() - input.xMin());
      }
      i++;
    }
    // Now, the chunks.
    for (const auto& chunk : block.chunks_) {
      uint16_t mask = chunk.input_mask_;
      // See if some inputs should be skipped because they get completely
      // overwritten due to blending modes.
      {
        uint16_t skip_mask = mask;
        int index = 0;
        int max_skipped_index = 0;
        while (skip_mask != 0) {
          if ((skip_mask & 1) == 0) {
            BlendingMode blending_mode = blending_modes_[index];
            if (IsBlendingModeSourceClearing(blending_mode)) {
              max_skipped_index = index;
            }
          }
          ++index;
          skip_mask >>= 1;
        }
        int skip_mask_mask = ((1 << max_skipped_index) - 1);
        skip_mask &= skip_mask_mask;
        mask &= ~skip_mask_mask;
        index = 0;
        while (skip_mask > 0) {
          if (skip_mask & 1) {
            code->push_back(SKIP);
            code->push_back(index);
            code->push_back(chunk.width_);
          }
          skip_mask >>= 1;
        }
      }
      // See if some inputs should be skipped because they are drawn over
      // transparent destinations in drawing modes that result in clearance.
      {
        int index = 0;
        uint16_t m = mask;
        bool dst_clear = true;
        while (m > 0) {
          if ((m & 1) != 0) {
            if (dst_clear &&
                IsBlendingModeDestinationClearing(blending_modes_[index])) {
              code->push_back(SKIP);
              code->push_back(index);
              code->push_back(chunk.width_);
              mask &= ~(1 << index);
            } else {
              dst_clear = false;
            }
          }
          ++index;
          m >>= 1;
        }
      }
      if (mask == 0) {
        code->push_back(BLANK);
        code->push_back(chunk.width_);
      } else {
        uint16_t mask_copy = mask;
        int index = 0;
        while (!(mask_copy & 1)) {
          mask_copy >>= 1;
          ++index;
        }
        if (mask_copy == 1) {
          code->push_back(WRITE_SINGLE);
          code->push_back(index);
          code->push_back(chunk.width_);
        } else {
          code->push_back(WRITE);
          code->push_back(mask);
          code->push_back(chunk.width_);
        }
      }
    }
    // Finally, emit potential right skips.
    i = 0;
    for (const auto& input : input_extents_) {
      if (input.xMax() > bounds_.xMax() && block.all_inputs_ & (1 << i)) {
        code->push_back(SKIP);
        code->push_back(i);
        code->push_back(input.xMax() - bounds_.xMax());
      }
      i++;
    }
    code->push_back(RET);
  }
  code->push_back(EXIT);
}

class Engine {
 public:
  Engine(const Program* program) : program_(program), pc_(0) {}

  Instruction fetch() {
    while (true) {
      Instruction i = (Instruction)program_->get(pc_++);
      switch (i) {
        case LOOP: {
          loop_counter_ = program_->get(pc_++);
          loop_ret_ = pc_;
          continue;
        }
        case RET: {
          --loop_counter_;
          if (loop_counter_ > 0) {
            pc_ = loop_ret_;
          }
          continue;
        }
        default:
          return i;
      }
    }
  }

  uint16_t read_word() { return program_->get(pc_++); }

 private:
  const Program* program_;
  uint16_t pc_;
  uint16_t loop_ret_;
  uint16_t loop_counter_;
};

inline bool Composition::Add(const Box& extents, BlendingMode blending_mode) {
  input_extents_.push_back(extents);
  blending_modes_.push_back(blending_mode);
  int input_idx = input_count_;
  uint16_t input_mask = 1 << input_idx;
  input_count_++;
  // Box extents = Box::Intersect(bounds_, full_extents);
  // if (extents.empty()) return false;
  std::vector<Block> newdata;
  newdata.reserve(data_.capacity());
  auto i = data_.begin();
  int16_t cursor;
  uint16_t remaining_old_height;
  uint16_t remaining_new_height = extents.height();
  cursor = bounds_.yMin();
  while (cursor + i->height_ <= extents.yMin()) {
    newdata.push_back(std::move(*i));
    cursor += i->height_;
    i++;
  }
  remaining_old_height = i->height_;
  if (cursor < extents.yMin()) {
    uint16_t height = extents.yMin() - cursor;
    newdata.push_back(*i);
    newdata.back().height_ = height;
    cursor += height;
    remaining_old_height -= height;
  }
  while (true) {
    if (remaining_new_height == 0) {
      newdata.push_back(std::move(*i));
      newdata.back().height_ = remaining_old_height;
      cursor += remaining_old_height;
      i++;
      if (i == data_.end()) {
        data_.swap(newdata);
        return true;
      }
      remaining_old_height = i->height_;
    } else {
      uint16_t height = std::min(remaining_old_height, remaining_new_height);
      if (height == remaining_old_height) {
        newdata.push_back(std::move(*i));
        i++;
        remaining_old_height = (i == data_.end()) ? 0 : i->height_;
      } else {
        newdata.push_back(*i);
        remaining_old_height -= height;
      }
      remaining_new_height -= height;
      newdata.back().height_ = height;
      newdata.back().Merge(extents.xMin() - bounds_.xMin(), extents.width(),
                           input_mask);
      if (remaining_old_height == 0) {
        data_.swap(newdata);
        return true;
      }
      cursor += height;
    }
  }
}

void WriteRect(Engine* engine, const Box& bounds,
               internal::BufferingStream* streams,
               const BlendingMode* blending_modes, const Surface& s) {
  s.out().setAddress(bounds, s.blending_mode());
  BufferedColorWriter writer(s.out());
  while (true) {
    switch (engine->fetch()) {
      case EXIT: {
        return;
      }
      case BLANK: {
        uint16_t count = engine->read_word();
        writer.writeColorN(s.bgcolor(), count);
        break;
      }
      case SKIP: {
        uint16_t input = engine->read_word();
        uint16_t count = engine->read_word();
        streams[input].skip(count);
        break;
      }
      case WRITE_SINGLE: {
        uint16_t input = engine->read_word();
        uint16_t count = engine->read_word();
        do {
          Color* buf = writer.buffer_ptr();
          uint16_t batch = writer.remaining_buffer_space();
          if (batch > count) batch = count;
          if (s.bgcolor() == color::Transparent) {
            streams[input].read(buf, batch);
          } else {
            FillColor(buf, batch, s.bgcolor());
            streams[input].blend(buf, batch, blending_modes[input]);
          }
          writer.advance_buffer_ptr(batch);
          count -= batch;
        } while (count > 0);
        break;
      }
      case WRITE: {
        uint16_t inputs = engine->read_word();
        uint16_t count = engine->read_word();
        do {
          uint16_t input = 0;
          uint16_t input_mask = inputs;
          Color* buf = writer.buffer_ptr();
          uint16_t batch = writer.remaining_buffer_space();
          if (batch > count) batch = count;
          while (true) {
            if (input_mask & 1) {
              streams[input].read(buf, batch);
              break;
            }
            input++;
            input_mask >>= 1;
          }
          while (true) {
            input++;
            input_mask >>= 1;
            if (input_mask == 0) break;
            if (input_mask & 1) {
              streams[input].blend(buf, batch, blending_modes[input]);
            }
          }
          // NOTE(dawidk): alpha-blending is expensive, and it is usually
          // better to blend all inputs together, and only then blend the
          // results onto the background.
          if (s.bgcolor() != color::Transparent) {
            for (int i = 0; i < batch; ++i) {
              buf[i] = AlphaBlend(s.bgcolor(), buf[i]);
            }
          }
          writer.advance_buffer_ptr(batch);
          count -= batch;
        } while (count > 0);
        break;
      }
      default: {
        // Unexpected.
        return;
      }
    }
  }
}

void WriteVisible(Engine* engine, const Box& bounds,
                  internal::BufferingStream* streams,
                  const BlendingMode* blending_modes, const Surface& s) {
  BufferedPixelWriter writer(s.out(), s.blending_mode());
  uint16_t x = bounds.xMin();
  uint16_t y = bounds.yMin();
  while (true) {
    switch (engine->fetch()) {
      case EXIT: {
        return;
      }
      case BLANK: {
        uint16_t count = engine->read_word();
        x += count;
        if (x > bounds.xMax()) {
          y += (x - bounds.xMin()) / bounds.width();
          x = (x - bounds.xMin()) % bounds.width() + bounds.xMin();
        }
        break;
      }
      case SKIP: {
        uint16_t input = engine->read_word();
        uint16_t count = engine->read_word();
        streams[input].skip(count);
        break;
      }
      case WRITE_SINGLE: {
        uint16_t input = engine->read_word();
        uint16_t count = engine->read_word();
        if (s.bgcolor() == color::Transparent) {
          while (count-- > 0) {
            Color c = streams[input].next();
            if (c.a() != 0) {
              writer.writePixel(x, y, c);
            }
            ++x;
            if (x > bounds.xMax()) {
              x = bounds.xMin();
              ++y;
            }
          }
        } else {
          while (count-- > 0) {
            Color c = streams[input].next();
            if (c.a() != 0) {
              writer.writePixel(x, y, AlphaBlend(s.bgcolor(), c));
            }
            ++x;
            if (x > bounds.xMax()) {
              x = bounds.xMin();
              ++y;
            }
          }
        }
        break;
      }
      case WRITE: {
        uint16_t inputs = engine->read_word();
        uint16_t count = engine->read_word();
        Color buf[kPixelWritingBufferSize];
        do {
          uint16_t input = 0;
          uint16_t input_mask = inputs;
          uint16_t batch = kPixelWritingBufferSize;
          if (batch > count) batch = count;
          while (true) {
            if (input_mask & 1) {
              streams[input].read(buf, batch);
              break;
            }
            input++;
            input_mask >>= 1;
          }
          while (true) {
            input++;
            input_mask >>= 1;
            if (input_mask == 0) break;
            if (input_mask & 1) {
              streams[input].blend(buf, batch, blending_modes[input]);
            }
          }
          // NOTE(dawidk): alpha-blending is expensive, and it is usually
          // better to blend all inputs together, and only then blend the
          // results onto the background.
          if (s.bgcolor() == color::Transparent) {
            for (int i = 0; i < batch; ++i) {
              if (buf[i].a() != 0) {
                writer.writePixel(x, y, buf[i]);
              }
              ++x;
              if (x > bounds.xMax()) {
                x = bounds.xMin();
                ++y;
              }
            }
          } else {
            for (int i = 0; i < batch; ++i) {
              if (buf[i].a() != 0) {
                writer.writePixel(x, y, AlphaBlend(s.bgcolor(), buf[i]));
              }
              ++x;
              if (x > bounds.xMax()) {
                x = bounds.xMin();
                ++y;
              }
            }
          }
          count -= batch;
        } while (count > 0);
        break;
      }
      default: {
        // Unexpected.
        return;
      }
    }
  }
}

class StreamableComboStream : public PixelStream {
 public:
  StreamableComboStream(Program prg,
                        std::vector<internal::BufferingStream> streams,
                        std::vector<BlendingMode> blending_modes)
      : prg_(std::move(prg)),
        engine_(&prg_),
        streams_(std::move(streams)),
        blending_modes_(std::move(blending_modes)),
        remaining_count_(0) {}

  void Read(Color* buf, uint16_t size) override {
    Color* result = buf;
    do {
      while (remaining_count_ == 0) {
        last_instruction_ = engine_.fetch();
        switch (last_instruction_) {
          case EXIT: {
            return;
          }
          case BLANK: {
            remaining_count_ = engine_.read_word();
            break;
          }
          case SKIP: {
            uint16_t input = engine_.read_word();
            uint16_t count = engine_.read_word();
            streams_[input].skip(count);
            continue;
          }
          case WRITE_SINGLE: {
            input_ = engine_.read_word();
            remaining_count_ = engine_.read_word();
            break;
          }
          case WRITE: {
            input_ = engine_.read_word();
            remaining_count_ = engine_.read_word();
            break;
          }
          default: {
            // Unexpected.
            return;
          }
        }
      }
      uint16_t batch = std::min(size, remaining_count_);
      switch (last_instruction_) {
        case BLANK: {
          FillColor(result, batch, color::Transparent);
          break;
        }
        case WRITE_SINGLE: {
          streams_[input_].read(result, batch);
          break;
        }
        case WRITE: {
          uint16_t input = 0;
          uint16_t input_mask = input_;
          while (true) {
            if (input_mask & 1) {
              streams_[input].read(result, batch);
              break;
            }
            input++;
            input_mask >>= 1;
          }
          while (true) {
            input++;
            input_mask >>= 1;
            if (input_mask == 0) break;
            if (input_mask & 1) {
              streams_[input].blend(result, batch, blending_modes_[input]);
            }
          }
          break;
        }
        default: {
          // Unexpected.
          break;
        }
      }
      result += batch;
      size -= batch;
      remaining_count_ -= batch;
    } while (size > 0);
  }

 private:
  Program prg_;
  Engine engine_;
  std::vector<internal::BufferingStream> streams_;
  std::vector<BlendingMode> blending_modes_;
  Instruction last_instruction_;
  uint16_t input_;
  uint16_t remaining_count_;
};

}  // namespace

void StreamableStack::drawTo(const Surface& s) const {
  Box bounds = Box::Intersect(s.clip_box(), extents_.translate(s.dx(), s.dy()));
  if (bounds.empty()) return;
  std::vector<internal::BufferingStream> streams;
  std::vector<BlendingMode> blending_modes;
  Composition composition(bounds);
  for (const auto& input : inputs_) {
    Box extents =
        Box::Intersect(input.extents(), bounds.translate(-s.dx(), -s.dy()));
    if (composition.Add(extents.translate(s.dx(), s.dy()),
                        input.blending_mode())) {
      streams.emplace_back(input.createStream(extents), extents.area());
    } else {
      streams.emplace_back(nullptr, 0);
    }
    blending_modes.push_back(input.blending_mode());
  }

  Program prg;
  composition.Compile(&prg);
  Engine engine(&prg);
  if (s.fill_mode() == FILL_MODE_RECTANGLE) {
    WriteRect(&engine, bounds, &*streams.begin(), &*blending_modes.begin(), s);
  } else {
    WriteVisible(&engine, bounds, &*streams.begin(), &*blending_modes.begin(),
                 s);
  }
}

std::unique_ptr<PixelStream> StreamableStack::createStream() const {
  Box bounds = extents();
  std::vector<internal::BufferingStream> streams;
  std::vector<BlendingMode> blending_modes;
  Composition composition(bounds);
  for (const auto& input : inputs_) {
    Box extents = Box::Intersect(input.extents(), bounds);
    if (composition.Add(extents, input.blending_mode())) {
      streams.emplace_back(input.createStream(extents), extents.area());
    } else {
      streams.emplace_back(nullptr, 0);
    }
    blending_modes.push_back(input.blending_mode());
  }

  Program prg;
  composition.Compile(&prg);
  return std::unique_ptr<PixelStream>(new StreamableComboStream(
      std::move(prg), std::move(streams), std::move(blending_modes)));
}

std::unique_ptr<PixelStream> StreamableStack::createStream(
    const Box& clip_box) const {
  Box bounds = Box::Intersect(extents(), clip_box);
  std::vector<internal::BufferingStream> streams;
  std::vector<BlendingMode> blending_modes;
  Composition composition(bounds);
  for (const auto& input : inputs_) {
    Box extents = Box::Intersect(input.extents(), bounds);
    if (composition.Add(extents, input.blending_mode())) {
      streams.emplace_back(input.createStream(extents), extents.area());
    } else {
      streams.emplace_back(nullptr, 0);
    }
    blending_modes.push_back(input.blending_mode());
  }

  Program prg;
  composition.Compile(&prg);
  return std::unique_ptr<PixelStream>(new StreamableComboStream(
      std::move(prg), std::move(streams), std::move(blending_modes)));
}

}  // namespace roo_display
