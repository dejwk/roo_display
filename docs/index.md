# roo_display

A graphics and UI library for embedded displays.

## Overview

roo_display provides:
- A device abstraction for displays and touch controllers.
- Drawables, streamables, and rasterizables for efficient rendering.
- Shapes, text, and UI helpers (tiles, alignment, labels).
- Image decoders and streamable image formats.
- Filters and blending for composition.

## Quick start

```cpp
using namespace roo_display;

products::makerfabs::Esp32s3ParallelIpsCapacitive800x480 device;
Display display(device);

device.initTransport();

display.init(color::DarkGray);
{
  DrawingContext dc(display);
  dc.draw(TextLabel("Hello", font, color::White), kCenter | kMiddle);
}
```

## API areas

- Core: `DisplayDevice`, `DrawingContext`, `Drawable`, `Streamable`, `Rasterizable`
- Color: `Color`, color modes, gradients, blending
- Shapes: basic and smooth shapes
- Text: fonts and labels
- UI: tiles and alignment
- Images: raster, RLE, PNG, JPEG
- Filters: background/foreground, clip masks, color filters
- Products: device wrappers for common boards

## Programming guide

For an incremental introduction to the library, see the programming guide:

- doc/programming_guide.md

## Building docs

Run `doxygen` in the repository root. Output is generated to `docs/doxygen/html`.
