#include "roo_display/composition/streamable_stack.h"

#include "roo_display/internal/composition.h"
#include "roo_logging.h"

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

  // Full coverage adds no boundaries; retain existing partition storage.
  void AddFullWidthInput(uint16_t input_mask) {
    all_inputs_ |= input_mask;
    for (Chunk& chunk : chunks_) chunk.input_mask_ |= input_mask;
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
  // Intersecting one interval introduces at most two new boundaries.
  newchunks.reserve(chunks_.size() + 2);

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
  uint16_t get(size_t idx) const { return prg_[idx]; }
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

// Splits pixel counts without widening the compact instruction encoding.
void EmitCount(std::vector<uint16_t>* code, Instruction instruction,
               uint32_t count, uint16_t input = 0) {
  while (count > 0) {
    uint16_t batch = std::min<uint32_t>(count, 65535);
    code->push_back(instruction);
    if (instruction != BLANK) code->push_back(input);
    code->push_back(batch);
    count -= batch;
  }
}

class Composition {
 public:
  Composition(const Box& bounds, size_t registered_inputs)
      : bounds_(bounds), replacing_inputs_(0), input_count_(0) {
    if (bounds.empty()) return;
    CHECK_LE(registered_inputs, StreamableStack::kMaxInputs)
        << "StreamableStack has " << registered_inputs << " registered inputs; "
        << "supports at most " << StreamableStack::kMaxInputs;
    blending_modes_.reserve(registered_inputs);
    data_.emplace_back(bounds.height());
    data_.back().AddChunk(bounds.width(), 0);
  }

  // Extents must be pre-intersected with bounds_.
  bool Add(const Box& extents, BlendingMode blending_mode, bool opaque);

  // Compiles clipped spans and returns the mask of sources actually sampled.
  uint16_t Compile(Program* prg);

 private:
  uint16_t analyzeInputs(uint16_t mask) const;

  Box bounds_;
  std::vector<BlendingMode> blending_modes_;
  std::vector<Block> data_;
  uint16_t replacing_inputs_;
  int input_count_;
};

// Applies the existing absent-source rule, then removes exact no-ops over
// Transparent. Other alpha-zero results can carry RGB or Background.
uint16_t Composition::analyzeInputs(uint16_t mask) const {
  int first_input = 0;
  for (int index = 0; index < input_count_; ++index) {
    uint16_t bit = 1u << index;
    if (((mask & bit) == 0 &&
         internal::IsAbsentSourceClearing(blending_modes_[index])) ||
        (mask & replacing_inputs_ & bit) != 0) {
      first_input = index;
    }
  }
  mask &= ~((1u << first_input) - 1);
  for (int index = 0; index < input_count_; ++index) {
    if ((mask & (1u << index)) == 0) continue;
    // Of the original transparent-destination eliminations, only Destination
    // always leaves the exact initial Transparent value unchanged.
    if (blending_modes_[index] != BlendingMode::kDestination) break;
    mask &= ~(1u << index);
  }
  return mask;
}

// Advances eliminated streams and emits the surviving inputs in insertion
// order.
void EmitSpan(std::vector<uint16_t>* code, uint16_t original_mask,
              uint16_t mask, uint32_t count) {
  uint16_t skip_mask = original_mask & ~mask;
  for (uint16_t index = 0; skip_mask != 0; ++index, skip_mask >>= 1) {
    if ((skip_mask & 1u) != 0) EmitCount(code, SKIP, count, index);
  }
  if (mask == 0) {
    EmitCount(code, BLANK, count);
    return;
  }
  uint16_t first = 0;
  uint16_t remaining = mask;
  while ((remaining & 1u) == 0) {
    ++first;
    remaining >>= 1;
  }
  if (remaining == 1) {
    EmitCount(code, WRITE_SINGLE, count, first);
  } else {
    EmitCount(code, WRITE, count, mask);
  }
}

uint16_t Composition::Compile(Program* prg) {
  // A source erased everywhere needs neither a stream nor cursor advances.
  uint16_t used_inputs = 0;
  for (const Block& block : data_) {
    for (const Chunk& chunk : block.chunks_) {
      used_inputs |= analyzeInputs(chunk.input_mask_);
    }
  }
  std::vector<uint16_t>* code = &prg->prg_;
  for (const Block& block : data_) {
    // Inputs are already clipped to bounds, so a full-width span is contiguous
    // in every contributing stream and can combine all rows of the block.
    if (block.chunks_.size() == 1) {
      uint16_t mask = block.chunks_[0].input_mask_;
      uint32_t count =
          static_cast<uint32_t>(block.chunks_[0].width_) * block.height_;
      EmitSpan(code, mask & used_inputs, analyzeInputs(mask), count);
      continue;
    }
    if (block.height_ > 1) {
      code->push_back(LOOP);
      code->push_back(block.height_);
    }
    for (const Chunk& chunk : block.chunks_) {
      EmitSpan(code, chunk.input_mask_ & used_inputs,
               analyzeInputs(chunk.input_mask_), chunk.width_);
    }
    if (block.height_ > 1) code->push_back(RET);
  }
  code->push_back(EXIT);
  return used_inputs;
}

class Engine {
 public:
  Engine(const Program* program)
      : program_(program), pc_(0), loop_ret_(0), loop_counter_(0) {}

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
  size_t pc_;
  size_t loop_ret_;
  uint16_t loop_counter_;
};

inline bool Composition::Add(const Box& extents, BlendingMode blending_mode,
                             bool opaque) {
  CHECK_LT(static_cast<size_t>(input_count_), StreamableStack::kMaxInputs)
      << "StreamableStack has " << input_count_ + 1u << " registered inputs; "
      << "supports at most " << StreamableStack::kMaxInputs;
  blending_modes_.push_back(blending_mode);
  int input_idx = input_count_;
  uint16_t input_mask = 1u << input_idx;
  input_count_++;
  if (blending_mode == BlendingMode::kSource ||
      (blending_mode == BlendingMode::kSourceOver && opaque)) {
    replacing_inputs_ |= input_mask;
  }
  if (extents.empty()) return false;
  if (extents == bounds_) {
    for (Block& block : data_) block.AddFullWidthInput(input_mask);
    return true;
  }
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

// Initializes the stack with the ordinary blend operation, preserving
// alpha-zero RGB.
template <BlendingMode mode>
__attribute__((always_inline)) inline Color InitializeFirstInput(Color sample) {
  if (mode == BlendingMode::kSourceOver) {
    // Keep source-over initialization to an alpha test even in size-optimized
    // builds, which may otherwise retain a call to the general blend operator.
    return sample.a() == 0 ? color::Transparent : sample;
  }
  return BlendOp<mode>().blend(color::Transparent, sample);
}

struct FirstInputBlender {
  template <BlendingMode mode>
  void operator()(Color* buffer, uint16_t count) const {
    for (uint16_t i = 0; i < count; ++i) {
      buffer[i] = InitializeFirstInput<mode>(buffer[i]);
    }
  }
};

// Initializes a batch without requesting unused run metadata from the input.
void ReadFirstInput(internal::BufferingStream& stream, BlendingMode mode,
                    Color* buffer, uint16_t count) {
  stream.read(buffer, count);
  internal::BlenderSpecialization<FirstInputBlender>(mode, buffer, count);
}

// A deterministic blend preserves any uniform run reported by the source.
void ReadFirstInput(internal::BufferingStream& stream, BlendingMode mode,
                    Color* buffer, uint16_t count, uint32_t& run_length) {
  stream.read(buffer, count, run_length);
  internal::BlenderSpecialization<FirstInputBlender>(mode, buffer, count);
}

// Composes all selected inputs against the initially transparent stack result.
void ReadInputs(uint16_t mask, internal::BufferingStream* streams,
                const BlendingMode* modes, Color* buffer, uint16_t count) {
  uint16_t input = 0;
  while ((mask & 1u) == 0) {
    ++input;
    mask >>= 1;
  }
  ReadFirstInput(streams[input], modes[input], buffer, count);
  while ((mask >>= 1) != 0) {
    ++input;
    if ((mask & 1u) != 0) streams[input].blend(buffer, count, modes[input]);
  }
}

// The minimum of the contributing uniform prefixes is a guaranteed uniform
// prefix of their composition. Unknown metadata conservatively suppresses it.
void ReadInputsWithRuns(uint16_t mask, internal::BufferingStream* streams,
                        const BlendingMode* modes, Color* buffer,
                        uint16_t count, uint32_t& run_length) {
  uint16_t input = 0;
  while ((mask & 1u) == 0) {
    ++input;
    mask >>= 1;
  }
  ReadFirstInput(streams[input], modes[input], buffer, count, run_length);
  while ((mask >>= 1) != 0) {
    ++input;
    if ((mask & 1u) == 0) continue;
    uint32_t source_run = 0;
    streams[input].blend(buffer, count, modes[input], source_run);
    run_length = std::min(run_length, source_run);
  }
}

// Resolves a composed color with a background operation selected outside pixel
// loops. Source selects transparent backgrounds and must still replace explicit
// Background.
template <BlendingMode background_mode>
__attribute__((always_inline)) inline Color ResolveBackground(
    Color composed, Color background) {
  if (background_mode == BlendingMode::kSource &&
      composed == color::Background) {
    return background;
  }
  return BlendOp<background_mode>().blend(background, composed);
}

// Selects the background operation once per composed batch.
void ResolveBackground(Color* buffer, uint16_t count, Color background) {
  if (background == color::Transparent) {
    for (uint16_t i = 0; i < count; ++i) {
      buffer[i] =
          ResolveBackground<BlendingMode::kSource>(buffer[i], background);
    }
  } else if (background.isOpaque()) {
    for (uint16_t i = 0; i < count; ++i) {
      buffer[i] = ResolveBackground<BlendingMode::kSourceOverOpaque>(
          buffer[i], background);
    }
  } else {
    for (uint16_t i = 0; i < count; ++i) {
      buffer[i] =
          ResolveBackground<BlendingMode::kSourceOver>(buffer[i], background);
    }
  }
}

// Keeps visibility checks and cursor advancement inside the caller's pixel
// loop, including in size-optimized embedded builds. Source-over inputs cannot
// contribute an explicit Background placeholder to visible coverage.
template <BlendingMode background_mode, bool allow_background = true>
__attribute__((always_inline)) inline void WriteVisiblePixel(
    BufferedPixelWriter& writer, const Box& bounds, int32_t& x, int32_t& y,
    Color composed, Color background) {
  if (composed.a() != 0) {
    writer.writePixel(x, y,
                      BlendOp<background_mode>().blend(background, composed));
  } else if (allow_background && composed == color::Background) {
    writer.writePixel(x, y, background);
  }
  if (++x > bounds.xMax()) {
    x = bounds.xMin();
    ++y;
  }
}

// Consumes source/source-over directly. Nonzero-alpha samples are unchanged by
// either first-input mode; zero-alpha samples only paint Background in source
// mode. This folds first-input evaluation into the visibility check.
template <BlendingMode input_mode, BlendingMode background_mode>
void WriteSingleVisible(internal::BufferingStream& stream, uint16_t count,
                        BufferedPixelWriter& writer, const Box& bounds,
                        int32_t& x, int32_t& y, Color background) {
  static_assert(input_mode == BlendingMode::kSource ||
                    input_mode == BlendingMode::kSourceOver,
                "Direct visible reads require source or source-over");
  while (count-- > 0) {
    WriteVisiblePixel<background_mode, input_mode == BlendingMode::kSource>(
        writer, bounds, x, y, stream.next(), background);
  }
}

// Emits a composed batch with a fixed background operation. Inlining keeps
// cursor references from escaping, so the enclosing loops can keep registers.
template <BlendingMode background_mode>
__attribute__((always_inline)) inline void WriteVisiblePixels(
    const Color* buffer, uint16_t count, BufferedPixelWriter& writer,
    const Box& bounds, int32_t& x, int32_t& y, Color background) {
  for (uint16_t i = 0; i < count; ++i) {
    WriteVisiblePixel<background_mode>(writer, bounds, x, y, buffer[i],
                                       background);
  }
}

// Keeps common modes on the direct path; other modes share batched evaluation
// to avoid specializing the entire drawing loop for every blend mode.
template <BlendingMode background_mode>
void WriteSingleVisible(internal::BufferingStream& stream, BlendingMode mode,
                        uint16_t count, BufferedPixelWriter& writer,
                        const Box& bounds, int32_t& x, int32_t& y,
                        Color background) {
  if (mode == BlendingMode::kSource) {
    WriteSingleVisible<BlendingMode::kSource, background_mode>(
        stream, count, writer, bounds, x, y, background);
  } else if (mode == BlendingMode::kSourceOver) {
    WriteSingleVisible<BlendingMode::kSourceOver, background_mode>(
        stream, count, writer, bounds, x, y, background);
  } else {
    Color buffer[kPixelWritingBufferSize];
    do {
      uint16_t batch = std::min<uint32_t>(kPixelWritingBufferSize, count);
      ReadFirstInput(stream, mode, buffer, batch);
      WriteVisiblePixels<background_mode>(buffer, batch, writer, bounds, x, y,
                                          background);
      count -= batch;
    } while (count > 0);
  }
}

void WriteRect(Engine* engine, const Box& bounds,
               internal::BufferingStream* streams,
               const BlendingMode* blending_modes, const Surface& s) {
  s.out().setAddress(bounds, s.blending_mode());
  BufferedColorWriter writer(s.out());
  const Color background = s.bgcolor();
  while (true) {
    switch (engine->fetch()) {
      case EXIT:
        return;
      case BLANK: {
        uint16_t count = engine->read_word();
        writer.writeColorN(background, count);
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
        BlendingMode mode = blending_modes[input];
        do {
          Color* buffer = writer.buffer_ptr();
          uint16_t batch =
              std::min<uint32_t>(writer.remaining_buffer_space(), count);
          if (mode == BlendingMode::kSourceOver &&
              background != color::Transparent) {
            // Source-over against transparent, then the surface background,
            // equals source-over directly onto that background.
            FillColor(buffer, batch, background);
            streams[input].blend(buffer, batch, BlendingMode::kSourceOver);
          } else {
            ReadFirstInput(streams[input], mode, buffer, batch);
            // Source-over initialized against transparent cannot produce
            // Background.
            if (mode != BlendingMode::kSourceOver) {
              ResolveBackground(buffer, batch, background);
            }
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
          Color* buffer = writer.buffer_ptr();
          uint16_t batch =
              std::min<uint32_t>(writer.remaining_buffer_space(), count);
          ReadInputs(inputs, streams, blending_modes, buffer, batch);
          // Blend onto the background once, after combining the input layers.
          ResolveBackground(buffer, batch, background);
          writer.advance_buffer_ptr(batch);
          count -= batch;
        } while (count > 0);
        break;
      }
      default:
        return;
    }
  }
}

void WriteVisible(Engine* engine, const Box& bounds,
                  internal::BufferingStream* streams,
                  const BlendingMode* blending_modes, const Surface& s) {
  BufferedPixelWriter writer(s.out(), s.blending_mode());
  const Color background = s.bgcolor();
  int32_t x = bounds.xMin();
  int32_t y = bounds.yMin();
  while (true) {
    switch (engine->fetch()) {
      case EXIT:
        return;
      case BLANK: {
        int32_t offset = x - bounds.xMin() + engine->read_word();
        y += offset / bounds.width();
        x = bounds.xMin() + offset % bounds.width();
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
        if (background == color::Transparent) {
          WriteSingleVisible<BlendingMode::kSource>(
              streams[input], blending_modes[input], count, writer, bounds, x,
              y, background);
        } else {
          WriteSingleVisible<BlendingMode::kSourceOver>(
              streams[input], blending_modes[input], count, writer, bounds, x,
              y, background);
        }
        break;
      }
      case WRITE: {
        uint16_t inputs = engine->read_word();
        uint16_t count = engine->read_word();
        Color buffer[kPixelWritingBufferSize];
        do {
          uint16_t batch = std::min<uint32_t>(kPixelWritingBufferSize, count);
          ReadInputs(inputs, streams, blending_modes, buffer, batch);
          if (background == color::Transparent) {
            WriteVisiblePixels<BlendingMode::kSource>(buffer, batch, writer,
                                                      bounds, x, y, background);
          } else {
            WriteVisiblePixels<BlendingMode::kSourceOver>(
                buffer, batch, writer, bounds, x, y, background);
          }
          count -= batch;
        } while (count > 0);
        break;
      }
      default:
        return;
    }
  }
}

class StreamableComboStream : public PixelStream {
 public:
  using PixelStream::read;

  StreamableComboStream(Program prg,
                        std::vector<internal::BufferingStream> streams,
                        std::vector<BlendingMode> blending_modes)
      : prg_(std::move(prg)),
        engine_(&prg_),
        streams_(std::move(streams)),
        blending_modes_(std::move(blending_modes)),
        last_instruction_(BLANK),
        input_(0),
        remaining_count_(0) {}

  void read(Color* buf, uint16_t size, uint32_t& run_length) override {
    run_length = 0;
    if (size == 0) return;
    Color* result = buf;
    bool first_batch = true;
    do {
      if (!prepareSpan()) return;
      uint16_t batch = std::min(size, remaining_count_);
      switch (last_instruction_) {
        case BLANK: {
          if (first_batch) {
            run_length = remaining_count_;
          }
          FillColor(result, batch, color::Transparent);
          break;
        }
        case WRITE_SINGLE: {
          if (first_batch) {
            uint32_t input_run_length = 0;
            ReadFirstInput(streams_[input_], blending_modes_[input_], result,
                           batch, input_run_length);
            run_length = std::min<uint32_t>(input_run_length, remaining_count_);
          } else {
            ReadFirstInput(streams_[input_], blending_modes_[input_], result,
                           batch);
          }
          break;
        }
        case WRITE: {
          if (first_batch) {
            ReadInputsWithRuns(input_, streams_.data(), blending_modes_.data(),
                               result, batch, run_length);
            run_length = std::min<uint32_t>(run_length, remaining_count_);
          } else {
            ReadInputs(input_, streams_.data(), blending_modes_.data(), result,
                       batch);
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
      first_batch = false;
    } while (size > 0);
  }

  void skip(uint32_t count) override {
    while (count > 0) {
      if (!prepareSpan()) return;
      uint16_t batch = std::min<uint32_t>(count, remaining_count_);
      if (last_instruction_ == WRITE_SINGLE) {
        streams_[input_].skip(batch);
      } else if (last_instruction_ == WRITE) {
        uint16_t mask = input_;
        for (size_t i = 0; mask != 0; ++i, mask >>= 1) {
          if ((mask & 1u) != 0) streams_[i].skip(batch);
        }
      }
      remaining_count_ -= batch;
      count -= batch;
    }
  }

 private:
  // Advances program control and eliminated inputs to the next output span.
  // Both reading and skipping must execute exactly the same cursor updates.
  bool prepareSpan() {
    if (last_instruction_ == EXIT) return false;
    while (remaining_count_ == 0) {
      last_instruction_ = engine_.fetch();
      switch (last_instruction_) {
        case EXIT:
          return false;
        case BLANK:
          remaining_count_ = engine_.read_word();
          break;
        case SKIP: {
          uint16_t input = engine_.read_word();
          uint16_t count = engine_.read_word();
          streams_[input].skip(count);
          break;
        }
        case WRITE_SINGLE:
        case WRITE:
          input_ = engine_.read_word();
          remaining_count_ = engine_.read_word();
          break;
        default:
          return false;
      }
    }
    return true;
  }

  Program prg_;
  Engine engine_;
  std::vector<internal::BufferingStream> streams_;
  std::vector<BlendingMode> blending_modes_;
  Instruction last_instruction_;
  uint16_t input_;
  uint16_t remaining_count_;
};

// Compile geometry before opening sources so wholly erased layers do not
// allocate decoders or read pixels. All geometry is in stack coordinates.
void PrepareComposition(const std::vector<StreamableStack::Input>& inputs,
                        const Box& bounds, Program* program,
                        std::vector<internal::BufferingStream>* streams,
                        std::vector<BlendingMode>* modes) {
  Composition composition(bounds, inputs.size());
  if (!bounds.empty()) {
    for (const StreamableStack::Input& input : inputs) {
      Box clipped = Box::Intersect(input.extents(), bounds);
      bool opaque = !clipped.empty() && input.source()->getTransparencyMode() ==
                                            TransparencyMode::kNone;
      composition.Add(clipped, input.blending_mode(), opaque);
    }
  }
  uint16_t used = composition.Compile(program);
  if (bounds.empty()) return;
  streams->reserve(inputs.size());
  modes->reserve(inputs.size());
  for (size_t i = 0; i < inputs.size(); ++i) {
    const StreamableStack::Input& input = inputs[i];
    if ((used & (1u << i)) != 0) {
      Box clipped = Box::Intersect(input.extents(), bounds);
      streams->emplace_back(input.createStream(clipped), clipped.area());
    } else {
      streams->emplace_back(nullptr, 0);
    }
    modes->push_back(input.blending_mode());
  }
}

}  // namespace

void StreamableStack::drawTo(const Surface& s) const {
  Box bounds = Box::Intersect(s.clip_box(), extents_.translate(s.dx(), s.dy()));
  if (bounds.empty()) return;
  std::vector<internal::BufferingStream> streams;
  std::vector<BlendingMode> blending_modes;
  Program prg;
  PrepareComposition(inputs_, bounds.translate(-s.dx(), -s.dy()), &prg,
                     &streams, &blending_modes);
  Engine engine(&prg);
  if (s.fill_mode() == FillMode::kExtents) {
    WriteRect(&engine, bounds, streams.data(), blending_modes.data(), s);
  } else {
    WriteVisible(&engine, bounds, streams.data(), blending_modes.data(), s);
  }
}

std::unique_ptr<PixelStream> StreamableStack::createStream() const {
  return createStream(extents());
}

std::unique_ptr<PixelStream> StreamableStack::createStream(
    const Box& clip_box) const {
  Box bounds = Box::Intersect(extents(), clip_box);
  std::vector<internal::BufferingStream> streams;
  std::vector<BlendingMode> blending_modes;
  Program prg;
  PrepareComposition(inputs_, bounds, &prg, &streams, &blending_modes);
  return std::unique_ptr<PixelStream>(new StreamableComboStream(
      std::move(prg), std::move(streams), std::move(blending_modes)));
}

}  // namespace roo_display
