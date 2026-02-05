# For TFT_eSPI users

## Device drivers

At this time, `roo_display` natively supports fewer devices than TFT_eSPI.

Luckily, if you have a working TFT_eSPI setup, you can still use `roo_display` via an [adapter driver](../src/roo_display/driver/TFT_eSPI_adapter.h). The adapter driver relies on low-level routines provided by TFT_eSPI, namely: `setWindow`, `pushBlock`, and `pushPixels`.

## Performance

Generally, `roo_display` matches performance of TFT_eSPI. See [bechmark results](../benchmarks/adafruit.ino). In some areas, e.g. text display, it can be significantly faster.

Native SPI drivers of `roo_display` utilize some of the same low-level SPI optimizations as TFT_eSPI, with some new additions. For example, the drivers are vectorized, amortizing the overhead of virtual method calls. Drawing adjacent pixels get optimized by the driver to minimize SPI overhead.

## Why use roo_display?

`roo_display` offers certain unique features:

* excellent, zero-flicker, efficient text handling, supporting anti-aliasing and kerning;
* flexible positioning (alignment, scaling, rotation), applicable to anything you draw;
* rich collection of color modes for offscreen buffers and images;
* full support for transparency and translucency;
* compositing: alpha-blending, backgrounds, overlays, Porter-Duff, blending modes;
* touch: excellent quality drivers; supporting multi-touch on compliant devices;
* companion collection of [Material icons](programming_guide.md#using-the-material-icons-collection);
* extensibility: it is easy to add your own drawables that will be rendered with excellent performance.

See [programming guide](programming_guide.md) for details.

## Using the library

### Namespace

Everything is in the `roo_display` namespace, to avoid naming clashes. To keep your code, simple, you may want to import the namespace at the beginning of your code:

```cpp
using namespace roo_display;
```

### Initializing the transport

When using native drivers, you generally need to initialize the transport explicitly - e.g. by calling `SPI.begin()` from `setup()` for SPI-based devices.

This design choice enables flexibility. You can share the SPI bus between the display and some other devices (e.g. an SD card reader). You can also connect and control multiple displays.

### DrawingContext

In `roo_display`, you don't write directly to the display; instead, you need to create a `DrawingContext` first. The `DrawingContext` controls the lifetime of SPI transactions, and contains some mutable drawing state. (The display itself is stateless). It makes it easier to 'reset' state - simply drop a context object:

```cpp
{
  DrawingContext dc(display);
  dc.setBackgroundColor(color::White);
  dc.draw(StringViewLabel("Foo", font_NotoSans_Italic_60(),
                          color::Black), kCenter | kMiddle);
  // Destroying the DrawingContext at the end of the block.
}
```

### Drawables

The `DrawingContext` contains only very few drawing routines. You will mostly use one of the overloaded versions of `draw`, as we did in the example above.

The actual stuff that you can draw is spread across several header files:

* Basic shapes: [roo_display/shape/basic.h](../src/roo_display/shape/basic.h)
* Smooth (anti-aliased) shapes: [roo_display/shape/smooth.h](../src/roo_display/shape/smooth.h)
* Text: [roo_display/ui/text_label.h](../src/roo_display/ui/text_label.h)
* Images (JPEG, PNG, custom formats): [roo_display/image/](../src/roo_display/image/)
* Memory buffers: [roo_display/core/raster.h](../src/roo_display/core/raster.h)

### Color

`roo_display` internally uses 32-bit ARGB8888 to represent color. You can use translucent colors and alpha-blending (particularly useful with offscreens; see below). Translation to the device's color mode is performed in the driver.

Most SPI displays are RGB565, but there are exceptions. In particular, the ILI9488 SPI driver uses 18-bit RGB666. Indeed, `roo_display` utilizes the full supported color spectrum on those displays.

### In-memory buffers ('sprites')

Support for drawing to memory buffers is provided by the [Offscreen](programming_guide.md#using-off-screen-buffers) template class, defined in [roo_display/core/offscreen.h](../src/roo_display/core/offscreen.h). To draw to an offscreen, you can simply create a `DrawingContext` for it, as if it was a 'virtual' display.

Offscreens are themselves drawable. To draw an offscreen to the screen, you just call `DrawingContext::draw(offscreen)` on the screen's drawing context.

Offscreens support a variety of color modes: ARGB 8888, RGBA 8888, RGB 888, ARGB 6666, ARGB 4444, RGB 565, Gray8, Gray4, GrayAlpha8, Alpha8, Alpha4, Indexed8, Indexed4, Indexed2, Indexed1. You can also control byte and bit order.

### Drawing fast horizontal and vertical lines

Just call generic routines:

```cpp
dc.draw(Line(x, y0, x, y1));
dc.draw(Line(x0, y, x1, y));
```

They will internally call the optimized routines for 'horizontal' and 'vertical' lines.

### Images

In addition to out-of-the-box support for [JPEG](programming_guide.md#jpeg) and [PNG](programming_guide.md#png), `roo_display` provides custom, [RLE-encoded image format](programming_guide.md#built-in-image-format) with a dedicated importer. The images can be stored in and drawn from PROGMEM or a filesystem (SPIFFS, SD).

