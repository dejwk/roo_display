#include "roo_display/composition/rasterizable_stack.h"

#include "roo_display/composition/streamable_stack.h"
#include "roo_display/internal/composition.h"
#include "roo_logging.h"

namespace roo_display {

namespace {
static const int kMaxBufSize = 64;

// Finds where to resume with Transparent after the last input that clears the
// entire query. Inspect only stored bounds and modes, so erased sources are
// never asked for pixels or uniform colors.
size_t FirstInputAfterLastClear(
    const std::vector<RasterizableStack::Input>& inputs, const Box& box) {
  for (size_t i = inputs.size(); i > 0; --i) {
    const RasterizableStack::Input& input = inputs[i - 1];
    if (internal::IsAbsentSourceClearing(input.blending_mode()) &&
        Box::Intersect(input.extents(), box).empty()) {
      return i;
    }
  }
  return 0;
}

// Clears the four strips outside a nonempty source rectangle contained in box.
// The destination buffer must already contain one color per pixel.
void ClearOutsideRect(const Box& box, const Box& source, Color* result) {
  int32_t stride = box.width();
  int32_t top_count = (source.yMin() - box.yMin()) * stride;
  int32_t bottom_offset = (source.yMax() - box.yMin() + 1) * stride;
  FillColor(result, top_count, color::Transparent);
  FillColor(result + bottom_offset, box.area() - bottom_offset,
            color::Transparent);
  int32_t left_count = source.xMin() - box.xMin();
  int32_t right_offset = source.xMax() - box.xMin() + 1;
  for (int32_t row = top_count; row < bottom_offset; row += stride) {
    FillColor(result + row, left_count, color::Transparent);
    FillColor(result + row + right_offset, stride - right_offset,
              color::Transparent);
  }
}
}  // namespace

void RasterizableStack::readColors(const int16_t* x, const int16_t* y,
                                   uint32_t count, Color* result) const {
  FillColor(result, count, color::Transparent);
  int16_t newx[kMaxBufSize];
  int16_t newy[kMaxBufSize];
  Color newresult[kMaxBufSize];
  uint32_t offsets[kMaxBufSize];
  for (auto r = inputs_.begin(); r != inputs_.end(); r++) {
    Box bounds = r->extents();
    bool clears_outside = internal::IsAbsentSourceClearing(r->blending_mode());
    uint32_t offset = 0;
    while (offset < count) {
      int buf_size = 0;
      do {
        if (bounds.contains(x[offset], y[offset])) {
          newx[buf_size] = x[offset] - r->dx();
          newy[buf_size] = y[offset] - r->dy();
          offsets[buf_size] = offset;
          ++buf_size;
        } else if (clears_outside) {
          result[offset] = color::Transparent;
        }
        offset++;
      } while (offset < count && buf_size < kMaxBufSize);
      if (buf_size == 0) continue;
      r->source()->readColors(newx, newy, buf_size, newresult);
      ApplyBlendingInPlaceIndexed(r->blending_mode(), result, newresult,
                                  buf_size, offsets);
    }
  }
}

bool RasterizableStack::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                      int16_t yMax, Color* result) const {
  *result = color::Transparent;
  Box box(xMin, yMin, xMax, yMax);
  size_t first_input = FirstInputAfterLastClear(inputs_, box);
  if (first_input == inputs_.size()) return true;

  bool is_uniform_color = true;
  int16_t row_stride = box.width();
  int32_t pixel_count = box.area();
  Color buffer[pixel_count];

  // Materialize the current uniform result into the full destination buffer.
  auto expand_result = [&]() {
    DCHECK(is_uniform_color);
    is_uniform_color = false;
    FillColor(&result[1], pixel_count - 1, *result);
  };

  // Blend one uniform layer color into the clipped destination rectangle.
  auto blend_uniform_rect = [&](const Box& clipped, BlendingMode mode,
                                Color color) {
    if (is_uniform_color) {
      *result = ApplyBlending(mode, *result, color);
      return;
    }
    int16_t clipped_width = clipped.width();
    int32_t dst_offset =
        (clipped.yMin() - yMin) * row_stride + (clipped.xMin() - xMin);
    for (int16_t y = clipped.yMin(); y <= clipped.yMax(); ++y) {
      ApplyBlendingSingleSourceInPlace(mode, &result[dst_offset], color,
                                       clipped_width);
      dst_offset += row_stride;
    }
  };

  // Blend a per-pixel layer buffer into the clipped destination rectangle.
  auto blend_buffer_rect = [&](const Box& clipped, BlendingMode mode) {
    DCHECK(!is_uniform_color);
    int16_t clipped_width = clipped.width();
    uint32_t src_offset = 0;
    int32_t dst_offset =
        (clipped.yMin() - yMin) * row_stride + (clipped.xMin() - xMin);
    for (int16_t y = clipped.yMin(); y <= clipped.yMax(); ++y) {
      ApplyBlendingInPlace(mode, &result[dst_offset], &buffer[src_offset],
                           clipped_width);
      dst_offset += row_stride;
      src_offset += clipped_width;
    }
  };

  for (size_t i = first_input; i < inputs_.size(); ++i) {
    const Input& input = inputs_[i];
    BlendingMode mode = input.blending_mode();
    bool clears_outside = internal::IsAbsentSourceClearing(mode);
    Box clipped = Box::Intersect(input.extents(), box);
    if (clipped.empty()) continue;
    int16_t src_x_min = clipped.xMin() - input.dx();
    int16_t src_y_min = clipped.yMin() - input.dy();
    int16_t src_x_max = clipped.xMax() - input.dx();
    int16_t src_y_max = clipped.yMax() - input.dy();
    bool partial = !clipped.contains(box);

    if (is_uniform_color && partial) {
      Color layer_color;
      if (input.source()->readUniformColorRect(src_x_min, src_y_min, src_x_max,
                                               src_y_max, &layer_color)) {
        Color inside = ApplyBlending(mode, *result, layer_color);
        Color outside = clears_outside ? color::Transparent : *result;
        if (inside == outside) {
          *result = inside;
          continue;
        }
        expand_result();
        if (clears_outside) ClearOutsideRect(box, clipped, result);
        blend_uniform_rect(clipped, mode, layer_color);
        continue;
      }

      // Covered and uncovered pixels may now differ.
      expand_result();
    }
    if (clears_outside && partial) ClearOutsideRect(box, clipped, result);

    if (input.source()->readColorRect(src_x_min, src_y_min, src_x_max,
                                      src_y_max, buffer)) {
      blend_uniform_rect(clipped, mode, *buffer);
      continue;
    }

    if (is_uniform_color) {
      expand_result();
    }
    blend_buffer_rect(clipped, mode);
  }
  if (!is_uniform_color) {
    // See if maybe it actually is.
    for (int32_t i = 1; i < pixel_count; ++i) {
      if (result[i] != result[0]) {
        // Definitely not uniform.
        return false;
      }
    }
  }
  return true;
}

bool RasterizableStack::readUniformColorRect(int16_t xMin, int16_t yMin,
                                             int16_t xMax, int16_t yMax,
                                             Color* result) const {
  Color accumulated = color::Transparent;
  Box box(xMin, yMin, xMax, yMax);
  size_t first_input = FirstInputAfterLastClear(inputs_, box);
  for (auto r = inputs_.begin() + first_input; r != inputs_.end(); ++r) {
    BlendingMode mode = r->blending_mode();
    bool clears_outside = internal::IsAbsentSourceClearing(mode);
    Box clipped = Box::Intersect(r->extents(), box);
    if (clipped.empty()) continue;
    Color layer_color;
    if (!r->source()->readUniformColorRect(
            clipped.xMin() - r->dx(), clipped.yMin() - r->dy(),
            clipped.xMax() - r->dx(), clipped.yMax() - r->dy(), &layer_color)) {
      return false;
    }
    Color inside = ApplyBlending(mode, accumulated, layer_color);
    if (!clipped.contains(box)) {
      Color outside = clears_outside ? color::Transparent : accumulated;
      if (inside != outside) return false;
    }
    accumulated = inside;
  }
  *result = accumulated;
  return true;
}

std::unique_ptr<PixelStream> RasterizableStack::createStream() const {
  // For very small rasterizables, building and compiling the streamable stack
  // may be more expensive than just reading pixels directly, so we can skip it
  // and use the default implementation that reads pixels one by one using
  // readColors(). The threshold here is somewhat arbitrary and can be tuned
  // based on benchmarks.
  if (extents_.area() <= 128) {
    return Rasterizable::createStream();
  }
  StreamableStack stack(extents_);
  stack.setAnchorExtents(anchor_extents_);
  for (const auto& input : inputs_) {
    Box source_extents = input.extents().translate(-input.dx(), -input.dy());
    stack.addInput(input.source(), source_extents, input.dx(), input.dy())
        .withMode(input.blending_mode());
  }
  return stack.createStream();
}

std::unique_ptr<PixelStream> RasterizableStack::createStream(
    const Box& clip_box) const {
  // For very small rasterizables, building and compiling the streamable stack
  // may be more expensive than just reading pixels directly, so we can skip it
  // and use the default implementation that reads pixels one by one using
  // readColors(). The threshold here is somewhat arbitrary and can be tuned
  // based on benchmarks.
  Box clipped_extents = Box::Intersect(extents_, clip_box);
  if (clipped_extents.area() <= 128) {
    return Rasterizable::createStream(clipped_extents);
  }
  StreamableStack stack(clipped_extents);
  stack.setAnchorExtents(anchor_extents_);
  for (const auto& input : inputs_) {
    Box source_extents = input.extents().translate(-input.dx(), -input.dy());
    stack.addInput(input.source(), source_extents, input.dx(), input.dy())
        .withMode(input.blending_mode());
  }
  return stack.createStream(clip_box);
}

}  // namespace roo_display