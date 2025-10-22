# Programming guide

## Part 1: Getting started

### Connecting your display

This step is necessary if your display and your microcontroller are separate devices. In this case, you need to connect them together.

SPI displays are common. For these, you need to connect at least 4 signals: SPI SCK, SPI MOSI, display CS (chip select), and display DC (data/command). With touch displays, you will need to connect a couple of other signals - e.g. SPI MISO and touch CS in case of a SPI-based touch device. Additionally, you may need to connect a BL signal (backlit, used to control brightness), and a RST signal (hardware reset of the display). BL may usually be connected directly to VCC if you don't plan to adjust brightness programmatically.

For modern microcontrollers such as ESP32, you can use pretty much any digital I/O pins to connect these signals. If you can, use default SPI pins; it will make configuration a bit simpler.

### Basic setup

The minimalistic GUI application consists of the following basic steps:

* Instantiate a display driver for your hardware
* Instantiate the main display object
* Initialize the transport (e.g. SPI)
* Initialize the display: set parameters such like screen orientation, background color; clear the screen.

For example, a basic app that uses an ILI 9341 SPI display may look like this:

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

void setup() {
  // Use default SPI pins, or specify your own.
  SPI.begin();

  // Set the background color, and clear the screen.
  display.init(color::White);
}

void loop() {}
```

This application configures an ILI 9341 display, using the default SPI interface with default pins (on ESP32, these are 18 for SCK, 23 for MOSI, and 19 for MISO), as well as GPIO 5 for CS, GPIO 2 for DC, and GPIO 4 for RST. The screen is left-rotated, i.e. in a 320x240 portrait mode.

> If you have a touch display or an integrated device (display + microcontroller), look at the [products](https://github.com/dejwk/roo_display/tree/master/roo_display/products) subdirectory for a simpler way to configure your hardware.

We will now look at the steps in more detail.

#### Device drivers

All builtin drivers can be found in the `roo_display/driver` subdirectory. Look near the end of the header file to find the class name to use.

SPI drivers are generally class templates looking like this:

```cpp
template<pinCS, pinDC, pinRST>
class XXXspi { ... };
```

The template arguments allow you to specify pins that connect to your display's
CS, DC and RST signals. The driver may accept additional template parameters, e.g. overriding the default SPI settings. See the driver's comments for more details.
As an example, to specify SPI settings for the ST7796s driver, you can declare it
like this:

```cpp
St7796sspi<kCsPin, kDcPin, kRstPin, roo_display::DefaultSpi,
           roo_display::SpiSettings<40000000, MSBFIRST, SPI_MODE0>>
    device;
```

When you instantiate the SPI driver without any additional parameters, it will assume the default SPI interface. If you need to override that interface, you can do so by specifying one in the constructor:

```cpp
decltype(SPI) vspi(VSPI);

Ili9341spi<5, 2, 4> device(vspi);
Display display(device);

void setup() {
  vspi.begin();
}
```

#### Transport

As the examples above illustrate, you need to manually initialize the transport before doing anything else with the display. In this case, since we are using SPI, we need to explicitly call `SPI.begin()`.

By separating SPI initialization from display initialization, `roo_display` naturally supports complex configurations. For example, you can connect multiple displays, along with other SPI devices, to a single SPI bus.

#### Screen orientation

Display devices tend to support 8 different orientations. These result from a combination of 3 boolean flags: flipping along the horizontal axis, flipping along the vertical axis, and swapping the axes. Equivalently, the same 8 orientations can be considered to be a combination of 4 rotations (0, 90, 180, and 270 degrees) with a possible mirror flip along the X direction.

The `Orientation` class allows you to construct the desired orientation intuitively, e.g. as a combination of rotations and flips. In simple cases, a simple `rotateLeft()`, `rotateRight()`, or `rotateUpsideDown()` will suffice, but the other methods may come handy in case you need to change the orientation dynamically (e.g. flip the display or rotate it).

Internally, the orientation object is represented by just a single byte, and it should be passed by value.

#### The display object

The display object encapsulates the driver, and it is the 'root' of your future interactions with the library. Post-initialization, you use it primarily to create drawing contexts, as described below. You may also use it to change the display orientation, and global settings such as the main clipping rectangle, a global background, or touch calibration settings. It is also used for retrieving touch input. These features will be described in later sections.

#### Backlit

Some displays provide a dedicated pin for backlit (brightness) control. You can connect it to VCC for full brightness. You can also connect it to a GPIO pin to control backlit programmatically (by sending a pulse-wave-modulated signal to the GPIO pin).

Backlit is independent from the display driver. `roo_display` provides some utility classes to help manage backlit. Specifically, for Espressif-based controllers, you can use the `LedcBacklit` class:

```cpp
#include "roo_display/backlit/esp32_ledc.h"

LedcBacklit backlit(/* pin */ 16);

// ...

void setup() {
  // ...
  backlit.begin();
}

void loop() {
  // ...
  // Optional: adjust brightness.
  backlit.setIntensity(n);  // in the [0-256] range.
}

```

### DrawingContext and drawables

To draw an object, it must implement the ```Drawable``` interface:

```cpp
class Drawable {
 public:

  // Returns the bounding box encompassing all pixels that need to be drawn.
  virtual Box extents() const = 0;

  // Returns the boundaries to be used when aligning this drawable.
  virtual Box anchorExtents() const { return extents(); }

 private:
  virtual void drawTo(const Surface &s) const;
};
```

Later, you will see how to implement your own drawables. We will then dive into the Drawable interface (particularly, the ```drawTo()``` method) in more detail. For now, we will draw common, existing drawables.

Importantly, though, the set of drawable primitives is open-ended. You are not restricted to a set of predefined primitives. You can roll your own drawables that will be rendered as efficiently as the built-ins, and they will work seamlessly with all the features of the library (clipping, transformations, fliters, etc.)

So, how do we actually _draw_ a drawable?

To do so, we need to make one more step, which is to create a _drawing context_:

```cpp
DrawingContext dc(display);
dc.draw(drawable);
```

Drawing contexts further 'virtualize' the display, and allow tweaks that we will soon dive into. Drawing contexts keep SPI transactions alive, so they should be transient, i.e. destroyed quickly after you are done with the drawing.

### Drawing basic geometry

Let us put all of this together do draw some basic shapes:

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void loop() {
  DrawingContext dc(display);
  // Top-left corner at (20,20) and a diameter of 160.
  dc.draw(FilledCircle::ByExtents(20, 20, 160, color::Red));
  // Filled rectangle with corners at (10,15) and (80,120).
  dc.draw(FilledRect(10, 15, 80, 120, color::Orange));
  // Line from (10,10) to (120,80).
  dc.draw(Line(10, 10, 120, 80, color::Blue));

  delay(10000);
}
```

![img1](images/img1.png)

In a [later section](programming_guide.md#drawing-shapes), we will explore how to draw other shapes, and how to use anti-aliasing.

### Color

The `roo_display` library internally uses 32-bit ARGB color. Most SPI displays use RGB565, with 16 bits per pixel. The conversion from the 32-bit ARGB to the device color is performed in the driver - that is, if your driver is capable of handling 24- or 32-bit color, you will get it without any loss of quality, but in most cases, your driver will downgrade to RGB565.

The 'A' in ARGB stands for 'alpha', which means 'translucency'. The `roo_display` library supports translucent color modes with alpha-blending. Most displays do not support transparency directly, so we will explore these color modes more in the section on offscreen drawing and overlays.

The color is represented by the ```Color``` class. It simply encapsulates the uint32_t integer, but provides convenience methods to simplify common operations.

You can specify regular, opaque RGB colors by calling the 3-argument constructor:

```cpp
Color c(0xF0, 0xC0, 0xC0);
```

You can also set each channel individually:

```cpp
c.set_r(0x30);
c.set_a(0x43);
```

You can also set a color directly from the uint32_t ARGB representation:

```cpp
c = Color(0xF0F0F0F0);
```

Additionally, the library defines 140 named [HTML color constants](https://www.w3schools.com/colors/colors_groups.asp):

```cpp
c = color::LightGoldenRodYellow;
```

as well as a convenience function to specify a shade of gray:

```cpp
c = Graylevel(0x40);  // Dark gray.
```

### Drawing simple text

To draw text, you need to pick a font, and use a ```TextLabel```:

```cpp
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Regular/27.h"

/// ...

void loop() {
  DrawingContext dc(display);
  dc.draw(
    TextLabel(
        "Hello, World! μΩ ₿ ķ",
        font_NotoSans_Regular_27(),
        Color(0xFF404080)), 
    5, 30);
}
```

The 'origin' of the text label is at the baseline and to the left of the text. We need to shift it a little (applying the 5, 30 offset) so that it fits on the screen with some margins.

![img2](images/img2.png)

> Note: `TextLabel` makes a copy of the string argument. If that argument is known to remain valid throughout the lifetime of the label, you can use `StringViewLabel` instead, which avoids the copy. In particular, `StringViewLabel` is always safe to use if the argument is a compile-time C string constant, and it is usually safe when the label is transient, as in most examples in this guide.

### Backgrounds and overwriting

By default, `roo_display` draws the minimum possible number of pixels:

```cpp
/// ...

void loop() {
  DrawingContext dc(display);
  dc.draw(
    TextLabel("12:00", font_NotoSans_Regular_27(), color::Blue),
    5, 30);
  dc.draw(
    TextLabel("21:41", font_NotoSans_Regular_27(), color::Red),
    5, 30);

  delay(10000);
}
```

![img3](images/img3.png)

Often, you need to overwrite previous content. In order to do so without flicker (which would have occured if you simply cleared the background), you can tell the drawing context to fill the entire extents rectangle, returned by `object.extents()`, when drawing an `object`. You can do so by setting `fill mode` to `FILL_MODE_RECTANGLE` (replacing the default, `FILL_MODE_VISIBLE`):

```cpp
/// ...

void loop() {
  DrawingContext dc(display);
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(
    TextLabel("12:00 ", font_NotoSans_Regular_27(), color::Blue),
    5, 30);
  dc.draw(
    TextLabel("21:41 ", font_NotoSans_Regular_27(), color::Red),
    5, 30);

  delay(10000);
}
```

![img4](images/img4.png)

So far, we have been using the default background color, which we had set in the call to `device.init()`. That default background may be overriden in the drawing context:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.setBackgroundColor(color::Pink);
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(
    TextLabel(
        "Hello, World!",
        font_NotoSans_Regular_27(),
        color::Black),
    15, 35);
  dc.setFillMode(FILL_MODE_VISIBLE);
  dc.draw(
    TextLabel(
        "Hello, World!",
        font_NotoSans_Regular_27(),
        color::Black),
    15, 75);
}
```

![img5](images/img5.png)

Note that since the font is anti-aliased, setting the correct background color matters even with `FILL_MODE_VISIBLE`. Using background that does not match the underlying background results in 'colored' text edges, as seen in the example above.

### Alignment

The library allows drawing with specified horizontal and vertical alignment, so that you don't need to calculate and specify exact pixel offsets, ad we have been doing so far. Let's look at a simple example:

```cpp
void loop() {
  auto circle = FilledCircle::ByExtents({0, 0}, 40, color::DarkGray);
  DrawingContext dc(display);
  dc.draw(circle, kLeft | kTop);
  dc.draw(circle, kCenter | kTop);
  dc.draw(circle, kRight | kTop);
  dc.draw(circle, kLeft | kMiddle);
  dc.draw(circle, kCenter | kMiddle);
  dc.draw(circle, kRight | kMiddle);
  dc.draw(circle, kLeft | kBottom);
  dc.draw(circle, kCenter | kBottom);
  dc.draw(circle, kRight | kBottom);
}
```

![img6](images/img6.png)

You can adjust the alignment using specific offsets:

```cpp
void loop() {
  auto circle = FilledCircle::ByExtents({0, 0}, 40, color::DarkGray);
  DrawingContext dc(display);
  dc.draw(circle, kLeft.shiftBy(10) | kTop.shiftBy(10));
  dc.draw(circle, kCenter | kTop.shiftBy(10));
  dc.draw(circle, kRight.shiftBy(-10) | kTop.shiftBy(10));
  dc.draw(circle, kLeft.shiftBy(10) | kMiddle);
  dc.draw(circle, kCenter | kMiddle);
  dc.draw(circle, kRight.shiftBy(-10) | kMiddle);
  dc.draw(circle, kLeft.shiftBy(10) | kBottom.shiftBy(-10));
  dc.draw(circle, kCenter | kBottom.shiftBy(-10));
  dc.draw(circle, kRight.shiftBy(-10) | kBottom.shiftBy(-10));
}
```

![img7](images/img7.png)

What if you want to draw on the portion of the screen, and use that portion for alignment, rathen than the display boundaries? Not a problem - you can specify the desired bounds when creating the drawing context. The following example draws 4 shapes centered in 4 areas the screen is split into:

```cpp
void loop() {
  int w = display.width();
  int h = display.height();
  auto centered = kMiddle | kCenter;
  {
    DrawingContext dc(display, Box(0, 0, w / 2 - 1, h / 2 - 1));
    dc.draw(
      FilledCircle::ByExtents({0, 0}, 40, color::Purple),
      centered);
  }
  {
    DrawingContext dc(display, Box(w / 2, 0, w - 1, h / 2 - 1));
    dc.draw(
      FilledTriangle({0, 0}, {10, 50}, {80, 5}, color::Crimson),
      centered);
  }
  {
    DrawingContext dc(display, Box(0, h / 2, w / 2 - 1, h - 1));
    dc.draw(
      FilledRoundRect(0, 0, 70, 30, 10, color::ForestGreen),
      centered);
  }
  {
    DrawingContext dc2(display, Box(w / 2, h / 2, w - 1, h - 1));
    dc2.draw(
      FilledRect(0, 0, 40, 80, color::DodgerBlue),
      centered);
  }
}
```

![img8](images/img8.png)

The example above has one caveat: the sub-pictures are still using the absolute screen coordinates. The alignment does its thing to position the content relative to the declared bounds, but if you tried to draw something without alignment, you would still need to use those full screen coordinates. Instead, it is often desirable to draw to a 'virtual canvas' as if it was a separate, smaller device, and 'forget' that it belongs to the larger, physical coordinate system.

This can be achieved by additionally specifying the offset in the DrawingContext constructor. Usually, then, the bounds should have the top-left corner at (0, 0), making it the origin of the 'virtual canvas':

```cpp
void drawStuff(DrawingContext& dc) {
  dc.draw(FilledCircle::ByExtents({10, 10}, 30, color::Purple));
  dc.draw(FilledTriangle({60, 10}, {70, 40}, {120, 15}, color::Crimson));
  dc.draw(FilledRoundRect(10, 50, 60, 80, 10, color::ForestGreen));
  dc.draw(FilledRect(80, 60, 120, 80, color::DodgerBlue));
}

void loop() {
  uint16_t w = display.width();
  uint16_t h = display.height();
  // Declare the bounds of the desired 'virtual canvas'.
  Box box(0, 0, w / 2 - 1, h / 2 - 1);
  {
    // No offset: draw everything at top-left corner.
    DrawingContext dc(display, 0, 0, box);
    drawStuff(dc);
  }
  {
    // Everything shifted horizontally.
    DrawingContext dc(display, w / 2, 0, box);
    drawStuff(dc);
  }
  {
    // Everything shifted vertically.
    DrawingContext dc(display, 0, h / 2, box);
    drawStuff(dc);
  }
  {
    // Everything shifted both horizontally and vertically.
    DrawingContext dc(display, w / 2, h / 2, box);
    drawStuff(dc);
  }

  delay(10000);
}
```

![img8](images/img63.png)

This technique allows you to treat a DrawingContext as a virtual 'canvas', separating the concerns of content drawing from the concerns of physical rendering. As an example, say you wanted to try out the examples from this guide on a high-resolution device, but constraining the display to a 320x240 centered view. You can do so by replacing

```cpp
DrawingContext dc(display);
```

with

```cpp
  DrawingContext dc(display, (display.width() - 320) / 2,
                    (display.height() - 240) / 2, Box(0, 0, 319, 239));
```

Voila!

#### Anchor extents

For the purpose of alignment, drawables may declare 'anchor extents' that are different than the regular extents defining the bounding rectangle of their content. We will see the examples of that later.

As we saw in the examples, `kLeft` aligns left anchor of the object to the left side of the screen, `kCenter` aligns the center of the object to the center of the screen, and so on. You can override the 'destination' anchor for more specialized alignments. The following example draws the same text twice, first aligning the _top_ of the text to the middle of the sceen, and then aligning the _bottom_ of the text to the middle of the screen:

```cpp
#include "roo_fonts/NotoSerif_Italic/60.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  DrawingContext dc(display);
  auto label = TextLabel("Hello!", font, color::Black);
  dc.draw(label, kCenter | kTop.toMiddle());
  dc.draw(label, kCenter | kBottom.toMiddle());

  delay(10000);
}
```

![img11](images/img11.png)

Twere are two other alignment options, `kOrigin` and `kBaseline`. They correspond to the zero coordinate in the X and Y directions, respectively. If you use them without modifiers, the object will remain unaligned along the specific axis. For example:

```cpp
dc.draw(object, kOrigin | kBaseline);
```

is equivalent to

```cpp
dc.draw(object);
```

Using these options with `to` modifiers allows you to specify where to align the zero coordinate to. It is particularly useful for aligning text at baseline, as we will see in the later section.

Given the alignment, you can calculate the absolute offset that it will apply to a given object, and use it do draw other content:

```cpp
void loop() {
  Alignment a = kCenter | kMiddle;
  TextLabel label("Hello!", font_NotoSerif_Italic_60(), color::Black);
  DrawingContext dc(display);
  Offset offset = a.resolveOffset(dc.bounds(), label.anchorExtents());
  // Equivalent to dc.draw(label, a).
  dc.draw(label, offset.dx, offset.dy);
  // Draw rect border around the text, adding 5px padding.
  Box box = label.extents().translate(offset.dx, offset.dy);

  dc.draw(Rect(box.xMin() - 5, box.yMin() - 5, box.xMax() + 5,
               box.yMax() + 5, color::Red));
}
```

![img13](images/img13.png)

### Tiles

Often, you need to draw variable-sized contents over some preallocated space. A simple example is to overwrite some previously drawn text with new text that might possibly be shorter. In this case, even if you set `FILL_MODE_RECTANGLE`, the original contents will not be completely erased, since the new contents has smaller extents.

You can solve this problem using the `Tile` utility class. A tile is a rectangular 'container' of a predefined size, which you can think of as padding of a specified background color around your drawable. The position of your drawable within the tile is given by an alignment.

Tiles redraw their contents without flicker, as long as the contents itself draws without flicker, which is true for `TextLabel` and most other builtin drawables.

Tile takes a pointer to the interior drawable when constructed:

```cpp
#include "roo_fonts/NotoSans_Regular/27.h"

// ...
void loop() {
  const auto& font = font_NotoSans_Regular_27();
  TextLabel label1("Hello, hello, world!", font, color::Black);
  TextLabel label2("Hello, world!", font, color::Black);
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  Tile tile1(&label1, extents, a, color::Gainsboro);
  dc.draw(tile1);
  delay(2000);
  Tile tile2(&label2, extents, a, color::Gainsboro);
  dc.draw(tile2);
  delay(2000);
}
```

You can inline some of the objects as temporaries by using `MakeTileOf`:

```cpp
void loop() {
  const auto& font = font_NotoSans_Regular_27();
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  dc.draw(MakeTileOf(
      TextLabel("Hello, hello, world!", font, color::Black),
      extents, a, color::Gainsboro));
  delay(2000);
  dc.draw(MakeTileOf(
      TextLabel("Hello, world!", font, color::Black),
      extents, a, color::Gainsboro));
  delay(2000);
}
```

![img16](images/img16.png)

You can omit the tile's background color. It will then default to `color::Background`, inheriting the drawing context's background, but still forcing `FILL_MODE_RECTANGLE`. That is, such a tile will draw its entire extents, regardless of the fill mode settings. It is most likely the the most commonly needed behavior: the background is 'invisible' yet the contents get redrawn when needed without extra hassle.

If you need to, however, you can force the tile to respect the fill mode settings, by explicitly specifying `color::Transparent` as the background.

You can also specify a translucent background color. In this case, the actual background will be the alpha-blend of the tile's background over the drawing context's background:

```cpp
void setup() {
  // ...
  DrawingContext dc(display);
  int w = dc.width();
  int h = dc.height();
  dc.draw(FilledRect(0, h / 2, w - 1, h - 1, color::Khaki));
}

void loop() {
  auto tile = MakeTileOf(
      TextLabel(
          "Hello, world!",
          font_NotoSans_Regular_27(),
          color::Black),
      Box(10, 10, 309, 49),
      kLeft.shiftBy(5) | kMiddle,
      color::Red.withA(0x30));
  DrawingContext dc(display);
  dc.draw(tile);
  dc.setBackgroundColor(color::Khaki);
  dc.draw(tile, 0, dc.height() / 2);
  delay(10000);
}
```

![img17](images/img17.png)

### Clipping

You can set a _clip box_ on a drawing context in order to constrain the drawn area to a specific rectangle.

```cpp
void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  int w = dc.width();
  int h = dc.height();
  DrawingContext dc(display);
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  dc.setClipBox(0, 0, w - 1, h / 2 - 1);
  label.setColor(color::Maroon);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipBox(0, h / 2, w - 1, h - 1);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);

  delay(10000);
}
```

![img14](images/img14.png)

You can also set a _clip mask_, which is a bit mask allowing to define arbitrary clipping shapes.

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic.h"

#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/60.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

// 320x60 pixels.
uint8_t mask_data[320 * 60 / 8];

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
  for (int i = 0; i < 30; i++) {
    // Alternate rows: bits set, bits cleared.
    memset(mask_data + 2 * i * 320 / 8, 0xFF, 320 / 8);
    memset(mask_data + (2 * i + 1) * 320 / 8, 0x00, 320 / 8);
  }
}

void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  DrawingContext dc(display);
  int w = dc.width();
  int h = dc.height();
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  ClipMask mask1(mask_data, Box(0, 90, 319, 149));
  dc.setClipMask(&mask1);
  label.setColor(color::DarkSalmon);
  dc.draw(label, kCenter | kMiddle);
  // Same as before, but shifted by one row.
  ClipMask mask2(mask_data, Box(0, 91, 319, 150));
  dc.setClipMask(&mask2);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipMask(nullptr);  // To be on the safe side.

  delay(10000);
}
```

![img15](images/img15.png)

You will see an easier way to initialize a clip mask in the section on offscreen drawing.

> Note: drawing with a clip mask is noticeably slower. Use wisely.

### Transformations

You can apply basic affine transformations (integer scaling, right-angle rotation, translation) to anything you draw.

A transformation is represented by the `Transformation` object. Calling its default constructor creates an 'identity' transformation, i.e. one that does not change anything. You can then use it as a starting point to compose your transformation, by calling methods that add specific modifications, such as: translation, scaling, rotation, coordinate swapping, and others. You can also use a more specialized constructor to initialize your transformation in one step.

Likely the most important application of transformations is rotated or scaled text:

```cpp
#include "roo_display/filter/transformation.h"
#include "roo_fonts/NotoSerif_Italic/27.h"

// ...

void loop() {
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(10, 3));
  dc.draw(
      TextLabel("F", font_NotoSerif_Italic_27(), color::Black),
      kCenter | kMiddle);
  dc.setTransformation(Transformation().rotateLeft());
  dc.draw(
      TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
      kLeft | kMiddle);
  dc.setTransformation(Transformation().rotateRight().flipX());
  dc.draw(
      TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
      kRight | kMiddle);
  delay(10000);
}
```

![img18](images/img18.png)

### Drawing individual pixels

There is no 'Pixel' drawable. Drawing pixels as individual drawables would be slow.

Instead, you can draw pixels in batches, using the following idiom:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 100 + (int)(20 * sin(i / 20.0)), color::Black);
    }
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 180 + (int)(20 * cos(i / 20.0)), color::Red);
    }
  });
  delay(10000);
}
```

![img19](images/img19.png)

Pixel drawing respects settings of the drawing context, such as the clip region, and transformation:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.setClipBox(20, 20, 299, 219);
  dc.setTransformation(Transformation().scale(3, 3));
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 30 + (int)(10 * sin(i / 10.0)), color::Black);
    }
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 50 + (int)(10 * cos(i / 10.0)), color::Red);
    }
  });
  delay(10000);
}
```

![img20](images/img20.png)

> Note: `PixelWriter` buffers the pixels and flushes them in batches, in order to optimize the underlying write operations. Therefore, the pixels may not appear until `drawPixels()` returns.

When possible, try to draw subsequent pixels in horizontal or vertical streaks, i.e., write pixels in a single direction as much as possible. It may help the underlying device driver to reduce communication volume (e.g. SPI transfers), thus improving performance.

## Part 2: Features in depth

### Working with text

This section digs deeper into the details of handling text output.

#### Fonts

As most things in `roo_display`, a font is represented by an abstract class: `Font`. Most of the time, you can treat a font as an opaque object that you pass to `TextLabel`.

Logically, each font is a collection of _glyphs_ (individual symbols), covering some subset of Unicode.

##### Pre-packaged fonts

The primary concrete font implementation in `roo_display` is the `SmoothFont` class. A smooth font stores glyphs in PROGMEM, using a custom internal data format. Smooth fonts are anti-aliased (they use 4-bit alpha), and they support kerning. In order to use a smooth font, all you need to do is to include and reference it, as we saw in the examples above.

Most smooth fonts are _proportional_ (e.g. have varying glyph width), but they can also be _monotype_ (fixed width).

Only the fonts that you actually reference get linked into your program. Don't worry when you see font files compiling - if you don't use them, your program's binary will not carry them.

The `roo_display` library comes with a collection of free Noto fonts in various sizes and faces. They include Sans, Sans condensed, Mono, and Serif, and they come with bold and italic variants. The default charset includes about 340 glyphs, covering basic Latin, Latin-1 supplement, Latin Extended-A, as well as assorted symbols (currency symbols, some commonly used Greek letters, etc.) You can quite easily see what specific glyphs are supported simply by looking at the font's `cpp` file.

The footprint of smooth fonts in PROGMEM is reasonably small - unless you use many large fonts, they should not bloat your binaries. For example, the Noto Sans regular 27, which we have used a lot in the examples, uses 34KB (about 100 bytes per glyph on average). Large, complex fonts need more space: for example, Noto Serif italic 90 uses 177 KB total (about 520 bytes per glyph on average).

> Note: storing fonts in PROGMEM makes them much faster to render than if they were stored on an external SD card or SPIFFS.

In addition to smooth fonts, you will also find a simple fixed-size 5x7 'Adafruit' ASCII font. You will likely not find it very useful, except perhaps as a simple case study of the font implementation.

##### Importing custom fonts

There are many reasons why you might want to import additional fonts. Maybe you want to use a different face; maybe you need a different size, or maybe you need a different set of glyphs. Whatever the reason, importing fonts is easy, with help of [roo_display_font_importer](https://github.com/dejwk/roo_display_font_importer). This Java-based tool will convert any font available in your system, at any size you want, and with any glyph subset you want, to a smooth font that you can use with `roo_display`.

As an example, let's import all available glyphs from the OpenSymbol font:

```
wget https://github.com/dejwk/roo_display_font_importer/releases/download/1.0/roo_display_font_importer.zip
unzip roo_display_font_importer.zip
./roo_display_font_importer/bin/roo_display_font_importer --output-dir src -font OpenSymbol -sizes 40 -charset 0-FFFF
```

The resulting font has 162KB and over 1000 glyphs. Let's look at some of them:

```cpp
#include "OpenSymbol/40.h"

// ...

void loop() {
  const Font& font = font_OpenSymbol_40();
  DrawingContext dc(display);
  dc.draw(TextLabel("▲△▴▵▶▷▸▹►▻", font, color::Black), 0, 40);
  dc.draw(TextLabel("▼▽▾▿◀◁◂◃◄◅", font, color::Black), 0, 80);
  dc.draw(TextLabel("◊○●◗◦☎☑☒☛☞", font, color::Black), 0, 120);
  dc.draw(TextLabel("☹☺☼♠♣♥♦✂✈✍✎✓✔", font, color::Black), 0, 160);
  dc.draw(TextLabel("✗✘✙✚✛✜✢✣✤✥✫✬✭✮✯", font, color::Black), 0, 200);
  dc.draw(TextLabel("✰✳✴✵✶✷✸✹✿❁❄❍❏❐❑❒", font, color::Black), 0, 240);
  delay(10000);
}
```

![img21](images/img21.png)

##### Metrics

You can obtain general font metrics by calling `font.metrics()`. The returned `FontMetrics` object allows you to obtain FreeType properties of the font, such as ascent, descent, linegap, linespace, as well as the maximum glyph extents. You can also find out the minimum left- and right-side bearing, which specify how much can a glyph 'stick out' of its anchor extents. More on that below, in the section on alignment.

Generally, using font metrics can help write your application so that it can work with different font sizes.

You can also call `font.properties()` to find out basic facts about the font, such as whether it is proportional or monospace, whether it is antialiased, and whether it uses kerning.

Finally, by calling `font.getHorizontalStringGlyphMetrics(StringView)` you can measure the properties of glyphs or entire strings, without drawing them. Specifically, you can obtain a `GlyphMetrics` object, which reveals the string's bounding box, as well as the advance width, which is essentially the width for the purpose of alignment.

That said, the same properties are also captured by `TextLabel`'s `extents()` and `anchorExtents()`, which are generally simpler to use (see below).

#### Text alignment

Text alignment can be a little tricky. When optically left- or right-aligned, a glyph, such as 'y', may actually have a 'tail' that hang outside of the alignment boundary. Another glyph, such as 'i', may have a gap. Top and bottom alignment does not depend on glyphs at all, but only on the overall font's metrics, so that the text doesn't jump up and down when the content changes. Also, in many cases, you may need to keep text aligned at baseline.

Luckily, all these subtleties are captured by the `TextLabel`'s `anchorExtents()`. Let's see what they may look like for some sample text:

```cpp
#include "roo_fonts/NotoSerif_Italic/90.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_90();
  int dx = 40;
  int dy = 160;
  DrawingContext dc(display);
  auto label = TextLabel("yeah.", font, color::Black);
  dc.setBackgroundColor(color::LightGray);
  dc.draw(FilledRect(label.extents(), color::LightGray), dx, dy);
  dc.draw(Line(dx, 0, dx, dc.height() - 1, color::Red));
  dc.draw(Line(0, dy, dc.width() - 1, dy, color::Red));
  dc.draw(Rect(label.anchorExtents(), color::Blue), dx, dy);

  dc.draw(label, dx, dy);

  delay(10000);
}
```

![img9](images/img9.png)

Here, the gray box is a minimum bounding rectangle, and the blue frame corresponds to the anchor extents. Red crosshairs indicate the axes, i.e. the cross is at (0, 0).

(These bounding values of the `TextLabel` are derived from the font and glyph metrics: the width of the blue frame is the advance, the 'overhang' of the letter 'y' is equal to the left-side bearing, and so on).

When you draw aligned text using the built-in mechanisms, the library will do the right thing. You only need to keep in mind that some left or right padding may be needed (for left-aligned and right-aligned text, respectively) to leave room for overhangs, as they may otherwise get truncated:

```cpp
#include "roo_fonts/NotoSerif_Italic/27.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_27();
  auto label = TextLabel("yeah.", font, color::Black);
  int dx = 10;
  int dy = 10;
  DrawingContext dc(display);
  int w = dc.width();
  int h = dc.height();
  dc.draw(Line(0, dy, w - 1, dy, color::Red));
  dc.draw(Line(0, h - dy - 1, w - 1, h - dy - 1, color::Red));
  dc.draw(Line(dx, 0, dx, h - 1, color::Red));
  dc.draw(Line(w - dx - 1, 0, w - dx - 1, h - 1, color::Red));
  dc.draw(label, kLeft.shiftBy(dx) | kTop.shiftBy(dy));
  dc.draw(label, kCenter | kTop.shiftBy(dy));
  dc.draw(label, kRight.shiftBy(-dx) | kTop.shiftBy(dy));
  dc.draw(label, kLeft.shiftBy(dx) | kMiddle);
  dc.draw(label, kCenter | kMiddle);
  dc.draw(label, kRight.shiftBy(-dx) | kMiddle);
  dc.draw(label, kLeft.shiftBy(dx) | kBottom.shiftBy(-dy));
  dc.draw(label, kCenter | kBottom.shiftBy(-dy));
  dc.draw(label, kRight.shiftBy(-dx) | kBottom.shiftBy(-dy));

  delay(10000);
}
```

![img10](images/img10.png)

If you want to know exactly how much padding is needed for a given font so that it never truncates any glyph, use `font.metrics().minLsb()` and `font.metrics().minRsb()`. Note that 'overhangs' are represented by negative values. That is, if these methods return non-negative values, the text will never overhang for that font.

##### Aligning at baseline

Text baseline always has y-coordinate zero. Therefore, using `kBaseline` will keep the text aligned at baseline:

```cpp
#include "roo_fonts/NotoSerif_Italic/10.h"
#include "roo_fonts/NotoSerif_Italic/15.h"
#include "roo_fonts/NotoSerif_Italic/18.h"
#include "roo_fonts/NotoSerif_Italic/27.h"
#include "roo_fonts/NotoSerif_Italic/40.h"

// ...

void loop() {
  int dx = 5;
  auto label1 = TextLabel(
    "Hello ", font_NotoSerif_Italic_10(), color::Black);
  auto label2 = TextLabel(
    "Hello ", font_NotoSerif_Italic_15(), color::Black);
  auto label3 = TextLabel(
    "Hello ", font_NotoSerif_Italic_18(), color::Black);
  auto label4 = TextLabel(
    "Hello ", font_NotoSerif_Italic_27(), color::Black);
  auto label5 = TextLabel(
    "Hello ", font_NotoSerif_Italic_40(), color::Black);
  DrawingContext dc(display);
  dc.draw(label1, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label1.anchorExtents().width();
  dc.draw(label2, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label2.anchorExtents().width();
  dc.draw(label3, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label3.anchorExtents().width();
  dc.draw(label4, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label4.anchorExtents().width();
  dc.draw(label5, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label1.anchorExtents().width();

  delay(10000);
}
```

![img12](images/img12.png)

Note that for spacing the words, we relied on the fact that `anchorExtents().width()` is equal to the text advance, and that the 'space' glyph has proper advance for its font size, resulting in naturally-looking spacing.

#### UTF-8

Text labels, as well as any other routines operating on strings, assume UTF-8 encoding. Most code editors also use UTF-8, which means that you can simply paste Unicode content into your string literals, and things just work (as we saw in the OpenSymbol example above).

The library inherits a few utilities to work with UTF-8, in the `"roo_io/text/unicode.h"` header:

* `WriteUtf8Char()` is a convenience function that helps converting regular, multi-byte UTF code points to UTF-8.
* `Utf8Decoder` is a utility class that does the opposite, i.e. it allows to extract subsequent code points out of UTF-8 content.

#### Formatted text

The library (or rather, its dependency library `roo_io`) provides convenience 'sprintf-like' utilities in `"roo_io/text/string_printf.h"`. You can use them to format text into `std::string`, which is used in other places such as the `TextLabel`.

> Note: do not hesitate to use `std::string`. It works well on microcontrollers such as ESP32. For best performance, strings should be passed by value and _moved_ (rather than copied). Classes such as `TextLabel` already provide suitable move constructors, making things 'just work'.

```cpp
TextLabel label(StringPrintf("%.1f°C", temp), font, color);
```

#### Drawing numbers

Digits are often monotype, even in proportional fonts. Sometimes you can use this property to minimize redraws; e.g. use glyph metrics to determine positions of digits, and then update the individual digits.

Be careful, though: even though the _anchor_ extents of all digits may be the same, the regular extents are not. The bounding box of digit '1' is going to be narrower than that of digit '0', even though they have the same advance. For this reason, you may still need to use `Tile` when rendering numeric content.

### Drawing shapes

#### Basic shapes

The file ```roo_display/shape/basic.h``` lets you draw some basic single-color shapes, namely:

* lines,
* outlined and filled circles, triangles, and straight (non-rotated) rectangles, possibly with round edges.

```cpp
// ...
#include "roo_display/shape/basic.h"

// ...

void loop() {
    DrawingContext dc(display);

    dc.draw(FilledCircle::ByExtents(10, 10, 80, color::Purple));
    dc.draw(Circle::ByExtents(10, 110, 80, color::Purple));
    dc.draw(FilledRect(110, 10, 159, 89, color::Red));
    dc.draw(Rect(110, 110, 159, 189, color::Red));
    dc.draw(FilledRoundRect(180, 10, 229, 89, 8, color::Blue));
    dc.draw(RoundRect(180, 110, 229, 189, 8, color::Blue));
    dc.draw(FilledTriangle({245, 10}, {305, 20}, {250, 89}, color::Orange));
    dc.draw(Triangle({245, 110}, {305, 120}, {250, 189}, color::Orange));

    dc.draw(Line({10, 210}, {305, 230}, color::Brown));
    dc.draw(Line({10, 230}, {305, 210}, color::Brown));

  delay(10000);
}
```

![img39](images/img39.png)

All the coordinates are specified as integers.

Circles and filled circles can be specified in two ways:

1. the traditional way: center and radius,
2. the alternative: top-left corner of the bounding rectangle, and a diameter.

The former is more idiomatic, but only the latter allows you to specify circles with diameters that are even numbers. (If you specify the circle as a center point and a radius, it will always be symmetric about the center point, thus making the external diameter equal to 2 * r + 1).

#### Anti-aliased shapes

A more recently introduced collection of anti-aliased flicker-free shapes can be found in ```roo_display/shape/smooth.h```. For starters, you can use it to smooth out commonly drawn primitives, such as circles, rounded rectangles, and triangles:

```cpp
#include "roo_display/shape/smooth.h"

// ...

void loop() {
  DrawingContext dc(display);

  dc.draw(SmoothFilledCircle({45.5, 45.5}, 40, color::Purple));
  dc.draw(SmoothCircle({45.5, 145.5}, 39.5, color::Purple));
  dc.draw(SmoothFilledRoundRect(179.5, 9.5, 229.5, 89.5, 7.5, color::Blue));
  dc.draw(SmoothRoundRect(180, 110, 229, 189, 8, color::Blue));
  dc.draw(SmoothFilledTriangle({245, 10}, {305, 20}, {250, 89}, color::Orange));

  dc.draw(SmoothLine({10, 210}, {305, 230}, color::Brown));
  dc.draw(SmoothLine({10, 230}, {305, 210}, color::Brown));
}
```

![img40](images/img40.png)

Note that the coordinates of smooth shapes are floating-point. It allows defing the shapes with sub-pixel precision, for example to implement extremely smooth animations (we will see examples of that [later](programming_guide.md#advanced-example-animated-analog-gauge)). But it also requires that we begin thinking of pixels as 'squares'. The integer values of the coordinates not correspond to the _centers_ of the pixels. Consequently, a pixel with integer coordinates (x, y) now has the floating-point bounds of (x - 0.5, y - 0.5, x + 0.5, y + 0.5). The top-left corner of a display with resolution (width x height) is at (-0.5, -0.5), and the bottom-right corner is at (width - 0.5, height - 0.5).

For 'outlined' shapes, we now need to think about the 'thickness' of the outline, which, in the case above, is equal to 1 pixel. The circle radiuses and rectangle extents that you specify correspond to the _middle_ of the outline. Consequently, the outer extents of a rounded rectangle, as well as an outer radius of a circle, is larger by 0.5 than the specified extents and the radius, and conversely, the inner extents / radius are smaller by 0.5 than the specified extents / radius. This is why we used different specs for the filled circle and round rect: compared to the non-filled counterparts, we extended them by 0.5 on each side. It causes the entire pixel 'rectangles' at boundaries to be considered as belonging to the filled shape. (But experiment and see what would happen if we didn't.)

To further highlight this subtle point, see how very small circles, both filled and outlined, are rendered depending on the position of the center (mid-pixel or in-between-pixels):

```cpp
void loop() {
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(5, 5));

  dc.draw(SmoothFilledCircle({3, 5}, 0.5, color::Black));
  dc.draw(SmoothFilledCircle({10, 5}, 1, color::Black));
  dc.draw(SmoothFilledCircle({17, 5}, 1.5, color::Black));
  dc.draw(SmoothFilledCircle({25, 5}, 2, color::Black));
  dc.draw(SmoothFilledCircle({35, 5}, 2.5, color::Black));
  dc.draw(SmoothFilledCircle({45, 5}, 3, color::Black));
  dc.draw(SmoothFilledCircle({55, 5}, 3.5, color::Black));

  dc.draw(SmoothCircle({3, 15}, 0.5, color::Black));
  dc.draw(SmoothCircle({10, 15}, 1, color::Black));
  dc.draw(SmoothCircle({17, 15}, 1.5, color::Black));
  dc.draw(SmoothCircle({25, 15}, 2, color::Black));
  dc.draw(SmoothCircle({35, 15}, 2.5, color::Black));
  dc.draw(SmoothCircle({45, 15}, 3, color::Black));
  dc.draw(SmoothCircle({55, 15}, 3.5, color::Black));

  dc.draw(SmoothFilledCircle({3.5, 25.5}, 0.5, color::Black));
  dc.draw(SmoothFilledCircle({10.5, 25.5}, 1, color::Black));
  dc.draw(SmoothFilledCircle({17.5, 25.5}, 1.5, color::Black));
  dc.draw(SmoothFilledCircle({25.5, 25.5}, 2, color::Black));
  dc.draw(SmoothFilledCircle({35.5, 25.5}, 2.5, color::Black));
  dc.draw(SmoothFilledCircle({45.5, 25.5}, 3, color::Black));
  dc.draw(SmoothFilledCircle({55.5, 25.5}, 3.5, color::Black));

  dc.draw(SmoothCircle({3.5, 35.5}, 0.5, color::Black));
  dc.draw(SmoothCircle({10.5, 35.5}, 1, color::Black));
  dc.draw(SmoothCircle({17.5, 35.5}, 1.5, color::Black));
  dc.draw(SmoothCircle({25.5, 35.5}, 2, color::Black));
  dc.draw(SmoothCircle({35.5, 35.5}, 2.5, color::Black));
  dc.draw(SmoothCircle({45.5, 35.5}, 3, color::Black));
  dc.draw(SmoothCircle({55.5, 35.5}, 3.5, color::Black));
}
```

![img41](images/img41.png)

> Note: smooth circles are always specifies via center point and a radius. With floating-point coordinates, there is no need to use bounding box and diameters.

> Note: some microcontrollers, e.g. ESP32-S2 and ESP32-C3, do not have a
> floating-point coprocessor (FPU). On these devices, smooth shapes will render
> slowly. For smooth shapes, prefer devices with a coprocessor, e.g. ESP32 or
> ESP32-S3.

#### Thick and color-filled outlines

Speaking of thickness: you are not limited to the 1-pixel-thick outlines. You can specify arbitrary positive floating-point thickness:

```cpp

// ...

void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 10; ++i) {
    float dx = 30 * i;
    dc.draw(SmoothThickCircle({25 + dx, 25}, 10, 0.25 * (i + 1), color::Black));
    dc.draw(SmoothThickCircle({25 + dx, 55}, 10, 0.25 * (i + 11), color::Black));
    dc.draw(SmoothThickCircle({25 + dx, 85}, 10, 0.25 * (i + 21), color::Black));
    dc.draw(SmoothThickCircle({25 + dx, 115}, 10, 0.25 * (i + 31), color::Black));

    dc.draw(SmoothThickRoundRect(15 + dx, 150, 35 + dx, 170, 5, 0.25 * (i + 1), color::Black));
    dc.draw(SmoothThickRoundRect(15 + dx, 180, 35 + dx, 200, 5, 0.25 * (i + 11), color::Black));
    dc.draw(SmoothThickRoundRect(15 + dx, 210, 35 + dx, 230, 5, 0.25 * (i + 21), color::Black));
  }
}
```

![img42](images/img42.png)

You can also optionally fill the interiors of the outlines with color:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.draw(SmoothThickCircle({80, 100}, 50, 10, color::DarkSlateBlue, color::Orange));

  Box btn(190, 80, 279, 119);
  dc.draw(SmoothThickRoundRect(btn.xMin(), btn.yMin(), btn.xMax(), btn.yMax(),
                               8, 3.5, color::Navy, color::LightGray));
  dc.setBackgroundColor(color::LightGray);
  dc.draw(MakeTileOf(TextLabel("OK", font_NotoSans_Bold_15(), color::Black),
                     btn, kCenter | kMiddle, color::Transparent));

  delay(10000);
}
```

![img43](images/img43.png)

#### Thick lines

You can add thickness to lines:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 50; ++i) {
    float dy = 10 * i - 15;
    dc.draw(SmoothThickLine({25, dy}, {294, dy + 20}, 0.25 * (i + 1), color::Black));
  }

  delay(10000);
}
```

![img44](images/img44.png)

Note that the endings of lines are rounded, which becomes more obvious as the lines get thicker:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    float dy = 25 * i + 2.5 * i * i + 10;
    dc.draw(SmoothThickLine({25, dy}, {294, dy + 20}, 5 * (i + 1), color::Black));
  }

  delay(10000);
}
```

![img45](images/img45.png)

You can also choose to use flat line endings:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    float dy = 25 * i + 2.5 * i * i + 10;
    dc.draw(SmoothThickLine({25, dy}, {294, dy + 20}, 5 * (i + 1),
                            color::Black, ENDING_FLAT));
  }

  delay(10000);
}
```

![img46](images/img46.png)

Note that flat-ended lines are essentially rectangles. This means that you can use them to draw arbitrarily rotated rectangles:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 12; ++i) {
    float x = cosf(2 * M_PI * i / 12);
    float y = sinf(2 * M_PI * i / 12);
    dc.draw(SmoothThickLine({160 + x * 70, 120 + y * 70},
                            {160 + x * 100, 120 + y * 100},
                            20, color::Black, ENDING_FLAT));
  }

  dc.draw(
      SmoothRotatedFilledRect({160, 120}, 50, 50, M_PI / 4, color::Crimson));

  delay(10000);
}
```

![img47](images/img47.png)

Note that for the center square, we used the utility method `SmoothRotatedFilledRect` that draws a rectangle with specified center, sides, and rotation angle.

#### Wedges

You can specify different widths for two ends of a line, thus creating 'wedge-like' shapes:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 12; ++i) {
    float x = cosf(2 * M_PI * i / 12);
    float y = sinf(2 * M_PI * i / 12);
    dc.draw(SmoothWedgedLine({160 + x * 90, 120 + y * 90}, 40,
                              {160 + x * 115, 120 + y * 115}, 50,
                              color::Black, ENDING_FLAT));

    dc.draw(SmoothWedgedLine({160 + x * 50, 120 + y * 50}, 20,
                              {160 + x * 80, 120 + y * 80}, 8,
                              color::Blue, ENDING_ROUNDED));
  }
  dc.draw(
      SmoothRotatedFilledRect({160, 120}, 50, 50, M_PI / 4, color::Crimson));

  delay(10000);
}
```

![img48](images/img48.png)

The width can go down all the way to zero, allowing drawing of indicator needles of various sharpness:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 15; ++i) {
    dc.draw(SmoothWedgedLine({20 + i * 20, 120}, 15,
                              {20 + i * 20, 10}, 14 - i,
                              color::Black));
  }

  for (int i = 0; i < 7; ++i) {
    dc.draw(SmoothWedgedLine({20 + i * 45, 210}, 4 + i * 5,
                              {20 + i * 45, 140}, 0,
                              color::DarkRed));
  }

  delay(10000);
}
```

![img49](images/img49.png)

#### Arcs

You can draw arcs with a specified radius as well as the start and end angle:

```cpp
void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 10; ++i) {
    dc.draw(SmoothArc({160, 120}, i * 10,
                      (i + 1) * -0.3, (i + 1) * 0.3,
                      color::Black));
  }

  delay(10000);
}
```

![img50](images/img50.png)

Just like with lines, you can specify thickness and ending style:

```cpp

#include "roo_display/color/hsv.h"

// ...

void loop() {
  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    dc.draw(SmoothThickArc({160, 120}, i * 21, 18,
                            (i + 1) * -0.5, (i + 1) * 0.5,
                            HsvToRgb(i * 15, 1.0f, 0.7f),
                            ENDING_ROUNDED));
  }

  delay(10000);
}
```

![img51](images/img51.png)

Round-ended arcs can be useful for implementing touch controls:

```cpp
void loop() {
  DrawingContext dc(display);
  float radius = 70;
  float width = 50;
  dc.draw(SmoothThickArc({160, 120}, radius, width,
                          -M_PI * 2 / 3, M_PI * 2 / 3, color::LightSkyBlue));

  float angle = -M_PI * 1 / 3;
  float xc = 160 + sinf(angle) * radius;
  float yc = 120 - cosf(angle) * radius;
  dc.setBackgroundColor(color::LightGray);
  dc.draw(SmoothFilledCircle({xc, yc}, width / 2 - 5, color::Black));

  delay(10000);
}
```

![img52](images/img52.png)

Flat-ended arcs, on the other hand, can be useful in drawing radial gauges:

```cpp
void loop() {
  DrawingContext dc(display);
  float radius = 120;
  float width = 30;
  FpPoint center{130, 190};
  dc.draw(SmoothThickArc(center, radius, width, -M_PI * 1 / 6 - 0.05,
                          M_PI * 1 / 2 + 0.05, color::LightGray, ENDING_FLAT));
  dc.setBackgroundColor(color::LightGray);
  for (int i = 0; i < 51; ++i) {
    float angle = (i / 50.0f * 2 / 3 - 1.0f / 6.0f) * M_PI;
    Serial.println(angle);
    float x = sinf(angle);
    float y = -cosf(angle);
    float tick_inner_radius = radius - width / 2 + 5;
    float tick_len = (i % 5 == 0 ? 20 : 10);
    float tick_radius = tick_inner_radius + tick_len;
    dc.draw(SmoothLine({x * tick_inner_radius, y * tick_inner_radius},
                        {x * tick_radius, y * tick_radius}, color::Black),
            center.x, center.y);
  }
  float needle_radius = radius - width / 2 - 5;
  float needle_angle = 0.1 * M_PI;
  float needle_x = sinf(needle_angle);
  float needle_y = -cosf(needle_angle);
  dc.draw(SmoothWedgedLine({center}, 10,
                            {center.x + needle_x * needle_radius,
                            center.y + needle_y * needle_radius},
                            0, color::Red));

  delay(10000);
}
```

![img53](images/img53.png)

[Later](programming_guide.md#advanced-example-animated-analog-gauge), we will see how to animate these without flicker.

#### Pies

By drawing an arc with flat ends and width exactly equal to half the radius, we get a pie. The library provides a convenience function to create these:

```cpp
void loop() {
  DrawingContext dc(display);
  float radius = 90;
  FpPoint center{160, 120};
  float areas[] = {0.35, 0.27, 0.2, 0.18};
  Color colors[] = {color::Red, color::Green, color::Orange, color::RoyalBlue};
  float start_angle = 0.0f;
  for (int i = 0; i < 4; ++i) {
    float angle = areas[i] * (2 * M_PI);
    float end_angle = start_angle + angle;
    float mid_angle = start_angle + angle * 0.5f;
    dc.draw(SmoothPie({center.x + sinf(mid_angle) * 1,
                        center.y - cosf(mid_angle) * 1},
                      radius, start_angle, end_angle, colors[i]));
    start_angle += angle;
  }

  delay(10000);
}
```

![img54](images/img54.png)

#### Caveats of anti-aliased graphics

Anti-aliased graphics can work wonders on low-resolution displays, increasing the perceived resolution significantly. Nonetheless, anti-aliasing comes with some caveats, that we will describe below.

##### Background

Smooth primitives will generally only look good on single-color backgrounds, and you need to be careful to set the background color correctly.

For anti-aliasing to work with non-monochrome background, you need a display driver that natively supports alpha-blending (e.g. using a parallel driver for ESP32-S3), or you need to use an intermediate memory buffer (described [later](programming_guide.md#using-off-screen-buffers)).

##### Low-color modes

Anti-aliasing will not work well in low-color modes - e.g. on black-and-white displays, or when drawing to an offscreen buffer that uses an indexed color mode. You need many colors to render anti-aliased edges.

##### Performance

Anti-aliased primitives take 2-3 times longer to draw, compared with basic counterparts.

### Drawing images and icons

The `roo_display` library supports drawing JPEG and PNG images out of the box.

For low-footprint artwork such as icons and small images, the library provides an alternative, highly efficient image format, supporting a wide range of color modes, and featuring a lightweight lossless compression. A companion `roo_display_image_importer` tool allows you to convert your image files to this internal format. In most cases, small images and icons can be embedded directly into your program. You also have an option to store them on SPIFFS or some external file system.

If you just want to use some common icons, there is a companion library of [roo_icons](https://github.com/dejwk/roo_icons), containing over 34000 Google [Material Design icons](https://fonts.google.com/icons?icon.set=Material+Icons) in various styles and 4 predefined sizes.

#### JPEG

You can easily draw JPEG images stored on an SD card, SPIFFS, or some other file system:

```cpp
#include "roo_display/image/jpeg/jpeg.h"
#include "SD.h"

// ...

static const int pinSdCs = 4;  // Or whatever yours is.

void setup() {
  // ...
  pinMode(pinSdCs, OUTPUT);
  digitalWrite(pinSdCs, HIGH);
  SD.begin(pinSdCs);
}

void loop() {
  JpegDecoder decoder;
  JpegFile roo(decoder, SD, "/roo.jpg");
  DrawingContext dc(display);
  dc.draw(roo, kCenter | kMiddle);
}
```

![img22](images/img22.png)

You can also draw JPEG images stored inline in PROGMEM, or coming from any other source represented by an instance of `roo_io::MultipassResource`:

```cpp
static const unsigned char data[] PROGMEM = { /* bytes */ };

void loop() {
  JpegDecoder decoder;
  roo_io::MemoryResource<roo::byte* PROGMEM> resource(
      (const roo::byte* PROGMEM)data,
      (const roo::byte* PROGMEM)data + sizeof(data));
  JpegImage img(decoder, resource);
  // ...
}
```

If the image data comes from yet another source, you can still draw it - you just need to implement your own resource class. See `roo_display/io/resource.h` for details.

`JpegDecoder` is based on the [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html) library by ChaN.

The `JpegDecoder` instance uses about 7 KB of heap RAM. You probably want to use it as a local variable with short lifetime so that it does not keep that RAM used when not needed.

> The decoder supports baseline JPEGs only. In particular, it does _not_ support progressive JPEGs.

Image drawables behave like any other drawables, i.e. they can be transformed, clipped, etc.:

```cpp
void loop() {
  JpegDecoder decoder;
  JpegFile roo(decoder, SD, "/roo.jpg");
  DrawingContext dc(display);
  dc.setTransformation(
      Transformation().translate(-100, -100).scale(3, 3).rotateRight());
  dc.setClipBox(120, 80, 359, 239);
  dc.draw(roo, 240, 160);
}
```

![img24](images/img24.png)

#### PNG

Drawing PNG files is very similar to drawing JPEGs:

```cpp
#include "roo_display/image/png/png.h"
#include "SD.h"

// ...

static const int pinSdCs = 4;  // Or whatever yours is.

void setup() {
  // ...
  pinMode(pinSdCs, OUTPUT);
  digitalWrite(pinSdCs, HIGH);
  SD.begin(pinSdCs);
}

void loop() {
  PngDecoder decoder;
  PngFile penguin(decoder, SD, "/penguin.png");
  DrawingContext dc(display);
  dc.draw(penguin, kCenter | kMiddle);
}
```

![img23](images/img23.png)

Similarly to JPEGs, you can draw PNGs from arbitrary sources, not only filesystems - by using the `PngImage` class.

`PngDecoder` is based on the [PNGdec](https://github.com/bitbank2/PNGdec) library by Larry Bank, covered by Apache License 2.0.

The `PngDecoder` instance uses about 40 KB of heap RAM. You may want to unallocate the decoder when not in use (e.g. by keeping it as a local variable during drawing).

The decoder supports the most popular PNG formats, namely: RGBA8, RGB8, Grayscale8, GrayAlpha8, as well as the indexed formats with 8, 4, 2, and 1 bits per pixel.

PNG supports transparency, and if your files have it, `roo_display` will render it properly. You can use standard drawing context options, namely, the fill mode and the background color, to control the rendering:

```cpp
void loop() {
  {
    // Draw the checker pattern.
    DrawingContext dc(display);
    for (int i = 0; i < 60; ++i) {
      for (int j = 0; j < 40; ++j) {
        dc.draw(FilledRect(i * 8, j * 8, i * 8 + 7, j * 8 + 7,
                          (i + j) % 2 == 1 ? color::White : color::MistyRose));
      }
    }
  }
  PngDecoder decoder;
  PngFile penguin(decoder, SD, "/penguin-small.png");
  {
    DrawingContext dc(display, Box(20, 0, 239, 319));
    // Drawing with the default fill mode (FILL_MODE_VISIBLE).
    dc.draw(penguin, kCenter | kMiddle);
  }
  {
    DrawingContext dc(display, Box(240, 0, 459, 319));
    dc.setFillMode(FILL_MODE_RECTANGLE);
    dc.setBackgroundColor(color::MistyRose);
    dc.draw(penguin, kCenter | kMiddle);
  }
}
```

![img25](images/img25.png)

#### Built-in image format

Using PNG files for icons and simple static images has a number of disadvantages:

* The PNG decoder requires non-trivial amount of RAM (~40 KB), which you may not have to spare. It is also somewhat CPU-internsive.
* Using a filesystem is not cheap. Serving images from SD or even SPIFFS is slow compared to serving directly from PROGMEM.
* The PNG format may not be best for really tiny images due to various overheads. PNGs have plenty of optional metadata that you may need to strip to bring the size down.

The `roo_display` library is able to serve raw, uncompressed raster formats, using a wide variety of color encodings, from 32 bit-per-pixel ARGB down to 1 bit-per-pixel bitmask. Attitionally, the library supports a custom, lightweight run-length compression, which can further significantly reduce the image footprint. For typical artwork, the data will likely be small enough to be stored directly in PROGMEM.

The following example draws a 16-point grayscale gradient:

```cpp
#include "roo_display/core/raster.h"

// ...

void loop() {
  static const uint8_t gradient[] PROGMEM = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
  };

  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(20, 200));
  dc.draw(ProgMemRaster<Grayscale4>(16, 1, gradient), kCenter | kMiddle);
}
```

![img26](images/img26.png)

And here is how to draw a 1-bit-per-pixel bitmask:

```cpp
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/core/raster.h"

// ...

void setup() {
  // ...
  display.init(color::Black);
}

void loop() {
  static const uint8_t invader[] PROGMEM = {
    0b00001000, 0b00100000,
    0b00000100, 0b01000000,
    0b00001111, 0b11100000,
    0b00011011, 0b10110000,
    0b00111111, 0b11111000,
    0b00101111, 0b11101000,
    0b00101000, 0b00101000,
    0b00000110, 0b11000000,
  };

  static Color colors[] PROGMEM = { color::Transparent, color::Green };

  static Palette palette = Palette::ReadOnly(colors, 2);

  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(20, 20));
  dc.draw(ProgMemRaster<Indexed1>(16, 8, invader, &palette), kCenter | kMiddle);
}
```

![img27](images/img27.png)

You can also use `ConstDramRaster` to draw from DRAM rather than PROGMEM.

Now, let's draw something cooler: a real picture using 16-color palette and the run-length encoding:

```cpp
#include "roo_display/image/image.h"

// ...

static const Color penguin_palette[] PROGMEM = {
  Color(0x00000000),   Color(0xfffcfefb),   Color(0xff000000),   Color(0xfff8c200),
  Color(0xffdcdfdb),   Color(0xffcfd1ce),   Color(0xff595a57),   Color(0xffa8aaa7),
  Color(0xff1e1a16),   Color(0xffeaca5f),   Color(0xff2f2f29),   Color(0xff888a86),
  Color(0xffe9ba14),   Color(0xfff6e6a4),   Color(0xffeed994),   Color(0xffffface),
};

// Image file penguin 60x45, Indexed, 4-bit (16-color) palette,  RLE, 547 bytes.
static const uint8_t penguin_data[] PROGMEM = {
  0x88, 0x00, 0x83, 0x22, 0x8F, 0x00, 0x02, 0x02, 0x26, 0x6A, 0x83, 0x22, 0x00, 0x20, 0x8D, 0x00,
  0x07, 0x2A, 0x51, 0x11, 0xB2, 0x22, 0x28, 0xA2, 0x22, 0x8C, 0x00, 0x01, 0x22, 0x85, 0x81, 0x11,
  0x05, 0x1B, 0x26, 0x51, 0x11, 0x56, 0x22, 0x8A, 0x00, 0x02, 0x02, 0x22, 0x61, 0x81, 0x11, 0x01,
  0x15, 0xB1, 0x81, 0x11, 0x02, 0x14, 0x62, 0x20, 0x89, 0x00, 0x81, 0x22, 0x00, 0x71, 0x81, 0x11,
  0x01, 0x44, 0x41, 0x82, 0x11, 0x01, 0x58, 0x22, 0x88, 0x00, 0x07, 0x02, 0x22, 0x28, 0x51, 0x11,
  0x14, 0xA5, 0x57, 0x82, 0x11, 0x02, 0x58, 0x22, 0x20, 0x87, 0x00, 0x00, 0x02, 0x81, 0x22, 0x04,
  0x71, 0x11, 0x14, 0x65, 0x75, 0x82, 0x11, 0x02, 0x72, 0x22, 0x20, 0x87, 0x00, 0x82, 0x22, 0x00,
  0xB1, 0x81, 0x11, 0x01, 0x14, 0x64, 0x81, 0x11, 0x01, 0x14, 0x62, 0x81, 0x22, 0x87, 0x00, 0x82,
  0x22, 0x00, 0xA4, 0x81, 0x11, 0x04, 0x1B, 0x2A, 0x74, 0x44, 0x7A, 0x82, 0x22, 0x86, 0x00, 0x00,
  0x02, 0x82, 0x22, 0x05, 0x26, 0x41, 0x1F, 0x9C, 0xCC, 0x00, 0x84, 0x22, 0x00, 0x20, 0x85, 0x00,
  0x00, 0x02, 0x83, 0x22, 0x01, 0x8B, 0x79, 0x81, 0x33, 0x01, 0xC7, 0x6A, 0x83, 0x22, 0x00, 0x20,
  0x85, 0x00, 0x00, 0x02, 0x82, 0x22, 0x07, 0x26, 0x74, 0x11, 0x93, 0x33, 0xD1, 0x14, 0x76, 0x82,
  0x22, 0x00, 0x20, 0x85, 0x00, 0x82, 0x22, 0x01, 0x2A, 0x74, 0x81, 0x11, 0x01, 0xDC, 0x39, 0x81,
  0x11, 0x01, 0x14, 0x76, 0x82, 0x22, 0x85, 0x00, 0x82, 0x22, 0x01, 0x65, 0x41, 0x81, 0x11, 0x01,
  0x1E, 0xCF, 0x82, 0x11, 0x01, 0x45, 0xB8, 0x81, 0x22, 0x85, 0x00, 0x81, 0x22, 0x01, 0x2B, 0x54,
  0x82, 0x11, 0x01, 0x1F, 0xD1, 0x82, 0x11, 0x04, 0x14, 0x4B, 0x82, 0x22, 0x20, 0x83, 0x00, 0x82,
  0x22, 0x01, 0x65, 0x41, 0x87, 0x11, 0x02, 0x14, 0x44, 0xB2, 0x81, 0x22, 0x82, 0x00, 0x00, 0x02,
  0x81, 0x22, 0x02, 0x2A, 0x54, 0x41, 0x88, 0x11, 0x01, 0x44, 0x56, 0x82, 0x22, 0x81, 0x00, 0x82,
  0x22, 0x01, 0x27, 0x44, 0x89, 0x11, 0x02, 0x44, 0x47, 0x82, 0x81, 0x22, 0x00, 0x20, 0x83, 0x22,
  0x01, 0xA5, 0x44, 0x89, 0x11, 0x02, 0x14, 0x44, 0x62, 0x82, 0x22, 0x81, 0x00, 0x81, 0x22, 0x01,
  0xB4, 0x41, 0x89, 0x11, 0x03, 0x14, 0x44, 0x78, 0x20, 0x84, 0x00, 0x02, 0x28, 0x74, 0x41, 0x8A,
  0x11, 0x01, 0x44, 0x58, 0x85, 0x00, 0x02, 0x28, 0x54, 0x41, 0x8A, 0x11, 0x01, 0x44, 0x5A, 0x85,
  0x00, 0x02, 0x2A, 0x54, 0x41, 0x8A, 0x11, 0x01, 0x44, 0x5A, 0x85, 0x00, 0x02, 0x08, 0x54, 0x41,
  0x8A, 0x11, 0x01, 0x44, 0x50, 0x85, 0x00, 0x02, 0x08, 0x74, 0x41, 0x8A, 0x11, 0x01, 0x44, 0x50,
  0x85, 0x00, 0x02, 0x02, 0x74, 0x41, 0x89, 0x11, 0x02, 0x14, 0x44, 0x50, 0x86, 0x00, 0x01, 0x64,
  0x41, 0x89, 0x11, 0x01, 0x14, 0x44, 0x87, 0x00, 0x01, 0x85, 0x44, 0x89, 0x11, 0x01, 0x14, 0x44,
  0x87, 0x00, 0x01, 0x0B, 0x44, 0x89, 0x11, 0x01, 0x44, 0x40, 0x87, 0x00, 0x02, 0x08, 0x74, 0x41,
  0x88, 0x11, 0x01, 0x44, 0x40, 0x88, 0x00, 0x03, 0xC9, 0x99, 0xED, 0xF1, 0x83, 0x11, 0x03, 0x1F,
  0xDE, 0x99, 0x9E, 0x87, 0x00, 0x00, 0x03, 0x83, 0x33, 0x01, 0x39, 0xD1, 0x81, 0x11, 0x01, 0x1D,
  0x93, 0x83, 0x33, 0x00, 0x30, 0x85, 0x00, 0x85, 0x33, 0x03, 0x39, 0xF1, 0x11, 0x93, 0x85, 0x33,
  0x00, 0xC0, 0x83, 0x00, 0x00, 0x03, 0x86, 0x33, 0x01, 0xE1, 0x1E, 0x86, 0x33, 0x00, 0x30, 0x83,
  0x00, 0x00, 0x03, 0x86, 0x33, 0x01, 0x91, 0x1E, 0x87, 0x33, 0x83, 0x00, 0x00, 0x03, 0x86, 0x33,
  0x01, 0xC6, 0x6B, 0x86, 0x33, 0x00, 0x30, 0x84, 0x00, 0x85, 0x33, 0x00, 0x30, 0x81, 0x00, 0x00,
  0x03, 0x85, 0x33, 0x86, 0x00, 0x00, 0x03, 0x82, 0x33, 0x00, 0x30, 0x84, 0x00, 0x82, 0x33, 0x00,
  0x30, 0x83, 0x00,
};

const RleImage<Indexed4, ProgMemPtr>& penguin() {
  static Palette palette = Palette::ReadOnly(penguin_palette, 16);
  static RleImage<Indexed4, ProgMemPtr> value(
      Box(8, 3, 51, 41), Box(0, 0, 59, 44),
      penguin_data, Indexed4(&palette));
  return value;
}

void loop() {
  DrawingContext dc(display);
  dc.draw(penguin(), kCenter | kMiddle);
}
```

![img28](images/img28.png)

We made the image small to make the example convenient to copy-paste, but this technique works for larger images as well.

The image size, in pixels, is 60x45, which would mean 1350 bytes without compression (note that we're using 4bpp, i.e. 2 pixels per byte). The RLE compression reduced it to 547 bytes, i.e. by almost 60%.

##### Pixel formats

Both the `Raster` class and the `RleImage` class support a wide range of color modes, namely:

* ARGB 8888
* RGBA 8888
* RGB 888
* ARGB 6666
* ARGB 4444
* RGB 565
* RGB 565 with 1-color transparency
* Gray8 (8-bit grayscale)
* Gray4 (4-bit grayscale)
* GrayAlpha8 (8-bit grayscale with 8-bit alpha channel)
* Alpha8 (monochrome, 8-bit alpha)
* Alpha4 (monochrome, 4-bit alpha)
* Indexed8 (8bpp, 256-color palette)
* Indexed4 (4bpp, 16-color palette)
* Indexed2 (2pp, 4-color palette)
* Indexed1 (1bpp, 2-color palette)
* Monochrome (similar to Indexed1)

Some color modes are stateful, which means that you need to provide extra parameters when constructing rasters and images. Specifically:

* for indexed color modes, you need to provide the color palette, as shown in the examples above,
* for Monochrome, which is similar to Indexed1, you need to provide the foreground and the background color,
* for Alpha8 and Alpha4, you need to specify the foreground color,
* for RGB 565 with transparency, you need to provide the color index that is supposed to map to transparency.

The parameterized color mode can be passed as an argument to raster and image constructors. All color mode instances are designed to be copyable and lightweight. In particular, indexed color modes reference their palette via pointer. Consequently, you need to make sure that the palette object outlives any raster or image that uses it.

Additionally, for multiple-bytes-per-pixel modes, you can parameterize the `Raster` template class by specifying the byte order (big endian or little endian), and for sub-byte pixel modes (e.g. Gray4, Alpha4, Indexed4, Indexed2, Indexed1), you can specify the pixel order within byte (most-significant-first or least-significant-first). See the documentation of the `Raster` template for details.

This flexibility means that if you happen to have some existing uncompressed image data, there are good chances that you will be able to render it with `roo_display`. Moreover, if your color mode is unsupported, writing your own is not very hard. For example, the [implementation of RGBA 8888](https://github.com/dejwk/roo_display/blob/896a4ab04b8a4442e0aa9340ae235796a5b7067c/roo_display/color/color_modes.h#L46) fits in 20 lines of code. As long as your custom color mode uses 1, 2, 4, 8, 16, 24, or 32 bits per pixel, the `Offscreen`, `Raster` and the `RleImage` template classes will automatically support it.

In addition to the general-purpose `RleImage`, the library supports an alternative RLE encoding format, `RleImage4bppxBiased`, designed specifically for 4bpp color modes. This format is intended for cases when the 'boundary' colors, encoded as 0x0 and 0xF, occur significantly more frequently than the 'intermediate' colors 0x1 - 0xE. The main application is to compress Alpha4-encoded, monochrome, antialiased content, such as font glyphs and icons. In these cases, 0x0 represents the fully transparent 'background' and 0xF represents the fully opaque 'foreground', and the intermediate values correspond to different grades of translucency, used for antialiased 'edges'.

In fact, use of this format with the Alpha4 color mode is so common that the library defines a convenience typedef `Pictogram` to simplify it.

##### Importing images

You can use a companion utility [roo_display_image_importer](https://github.com/dejwk/roo_display_image_importer) to convert arbitrary images to the `roo_display`'s built-in image format:

```
wget https://github.com/dejwk/roo_display_image_importer/releases/download/1.2/roo_display_image_importer.zip
unzip roo_display_image_importer.zip
./roo_display_image_importer/bin/roo_display_image_importer -c RLE -e ARGB8888 penguin.png
```

See the importer's [documentation](https://github.com/dejwk/roo_display_image_importer) for details.

#### Using the Material Icons collection

If you want to use modern high-quality icons, try the companion library [roo_icons](https://github.com/dejwk/roo_icons). It contains over 34000 'Material Design' icons, divided into 4 styles, 18 categories, and 4 sizes (18x18, 24x24, 36x36, and 48x48). These free icons are well known from Google products and Android. You can browse them [here](https://fonts.google.com/icons?icon.set=Material+Icons).

The icons are monochrome, anti-aliased, and represented in the Alpha4 color mode. You can draw them in any color, using any background. Here is a small example:

```cpp
#include "roo_icons/round/device.h"

// ...

void drawIcon(DrawingContext& dc, const Pictogram& icon, int x, int y, Color color) {
  Pictogram my_icon = icon;
  my_icon.color_mode().setColor(color);
  dc.draw(Tile(&my_icon, my_icon.anchorExtents(), kNoAlign), x, y);
}

void loop() {
  DrawingContext dc(display);
  dc.setBackgroundColor(color::Wheat);
  drawIcon(dc, ic_round_18_device_1x_mobiledata(), 0, 0, color::Black);
  drawIcon(dc, ic_round_18_device_30fps(), 18, 0, color::Black);
  drawIcon(dc, ic_round_18_device_4g_mobiledata(), 36, 0, color::Black);
  drawIcon(dc, ic_round_18_device_4g_plus_mobiledata(), 54, 0, color::Black);
  drawIcon(dc, ic_round_18_device_60fps(), 72, 0, color::Black);
  drawIcon(dc, ic_round_18_device_access_alarm(), 90, 0, color::Black);
  drawIcon(dc, ic_round_18_device_access_alarms(), 108, 0, color::Black);
  drawIcon(dc, ic_round_18_device_access_time_filled(), 126, 0, color::Black);
  drawIcon(dc, ic_round_18_device_access_time(), 144, 0, color::Black);
  drawIcon(dc, ic_round_18_device_add_alarm(), 162, 0, color::Black);
  drawIcon(dc, ic_round_18_device_add_to_home_screen(), 180, 0, color::Black);
  drawIcon(dc, ic_round_18_device_ad_units(), 198, 0, color::Black);
  drawIcon(dc, ic_round_18_device_airplanemode_active(), 216, 0, color::Black);
  drawIcon(dc, ic_round_18_device_airplanemode_inactive(), 234, 0, color::Black);
  drawIcon(dc, ic_round_18_device_airplane_ticket(), 252, 0, color::Black);
  drawIcon(dc, ic_round_18_device_air(), 270, 0, color::Black);
  drawIcon(dc, ic_round_18_device_aod(), 288, 0, color::Black);
  drawIcon(dc, ic_round_18_device_battery_0_bar(), 306, 0, color::Black);
  dc.setBackgroundColor(color::BlueViolet);
  drawIcon(dc, ic_round_18_device_battery_std(), 0, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_battery_unknown(), 18, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bloodtype(), 36, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bluetooth_connected(), 54, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bluetooth_disabled(), 72, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bluetooth_drive(), 90, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bluetooth(), 108, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_bluetooth_searching(), 126, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_brightness_auto(), 144, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_brightness_high(), 162, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_brightness_low(), 180, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_brightness_medium(), 198, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_cable(), 216, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_cameraswitch(), 234, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_credit_score(), 252, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_dark_mode(), 270, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_data_saver_off(), 288, 18, color::Bisque);
  drawIcon(dc, ic_round_18_device_data_saver_on(), 306, 18, color::Bisque);
  dc.setBackgroundColor(color::Seashell);
  drawIcon(dc, ic_round_24_device_dvr(), 0, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_edgesensor_high(), 24, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_edgesensor_low(), 48, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_e_mobiledata(), 72, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_flashlight_off(), 96, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_flashlight_on(), 120, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_flourescent(), 144, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_fluorescent(), 168, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_fmd_bad(), 192, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_fmd_good(), 216, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_g_mobiledata(), 240, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_gpp_bad(), 264, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_gpp_good(), 288, 36, color::Crimson);
  drawIcon(dc, ic_round_24_device_gpp_maybe(), 312, 36, color::Crimson);
  dc.setBackgroundColor(color::DarkSlateGray);
  drawIcon(dc, ic_round_24_device_grid_goldenratio(), 0, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_auto(), 24, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_auto_select(), 48, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_off_select(), 72, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_on_select(), 96, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_h_mobiledata(), 120, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_h_plus_mobiledata(), 144, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lan(), 168, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lens_blur(), 192, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_light_mode(), 216, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_location_disabled(), 240, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_location_searching(), 264, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lte_mobiledata(), 288, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lte_plus_mobiledata(), 312, 60, color::LavenderBlush);
  dc.setBackgroundColor(color::LightPink);
  drawIcon(dc, ic_round_36_device_mobile_friendly(), 0, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_mobile_off(), 36, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_mode_night(), 72, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_mode_standby(), 108, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_monitor_heart(), 144, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_monitor_weight(), 180, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_nearby_error(), 216, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_nearby_off(), 252, 84, color::Navy);
  drawIcon(dc, ic_round_36_device_network_cell(), 288, 84, color::Navy);
  dc.setBackgroundColor(color::Maroon);
  drawIcon(dc, ic_round_36_device_nightlight(), 0, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_note_alt(), 36, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_password(), 72, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_phishing(), 108, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_pin(), 144, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_play_lesson(), 180, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_price_change(), 216, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_price_check(), 252, 120, color::PapayaWhip);
  drawIcon(dc, ic_round_36_device_punch_clock(), 288, 120, color::PapayaWhip);
  dc.setBackgroundColor(color::LightSteelBlue);
  drawIcon(dc, ic_round_48_device_reviews(), 0, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_r_mobiledata(), 48, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_rsvp(), 96, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_lock_landscape(), 144, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_lock_portrait(), 192, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_lock_rotation(), 240, 156, color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_rotation(), 288, 156, color::DarkBlue);
  dc.setBackgroundColor(color::DarkSlateBlue);
  drawIcon(dc, ic_round_48_device_sd_storage(), 0, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update_good(), 48, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update(), 96, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update_warning(), 144, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_sell(), 192, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_send_to_mobile(), 240, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_settings_suggest(), 288, 204, color::LightCoral);
}
```

![img29](images/img29.png)

Note that the icons usually need to be drawn tiled, because their extents may be smaller than their anchor extents. (You can rely on the anchor extents to be matching the 'nominal' icon size, e.g. `Box(0, 0, 17, 17)` for 18x18 icons, and so on).

> Note: due to the sheer volume, full compilation of icon files may take a few minutes. (No worries, though: only the icons that you _actually use_ get linked into your binary). If the compilation times bother you, delete icon files that you are not going to use.

### Using off-screen buffers

The `Offscreen` class allows you to draw to a memory buffer, instead of directly to the screen. It can be particularly useful in a couple of scenarios:

* You draw overlapping primitives, and you want to avoid flicker and SPI overheads resulting from overwriting the pixels.
* You want to perform real alpha-blending (which is not widely supported by display device drivers).

As a rule of thumb, drawing to an offscreen buffer is about an order of magnitude faster than drawing to a hardware device. In fact, using offscreens may improve the overall performance. But they can be memory hungry.

Let's see a simple example:

```cpp
#include "roo_display/core/offscreen.h"
#include "roo_fonts/NotoSerif_Italic/27.h"
#include "roo_display/ui/text_label.h"
#include "roo_display/shape/smooth.h"

// ...

void loop() {
  Offscreen<Rgb565> offscreen(200, 140, color::White);
  {
    DrawingContext dc(offscreen);
    dc.draw(SmoothFilledCircle({150, 50}, 40, color::Navy));
    dc.draw(SmoothFilledCircle({80, 80}, 70, color::Red.withA(0x90)));
    dc.draw(TextLabel("Oh, my!", font_NotoSerif_Italic_27(),
                      color::Yellow),
            70, 86);
  }
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(2, 2));
  dc.draw(offscreen, kCenter | kMiddle);
}
```

![img30](images/img30.png)

Note how the circles blend over each other, and how the text is correctly anti-aliased over this varying background. (We used magnification to make the anti-aliasing effect more easily visible). This effect is difficult to achieve without offscreens (although clip masks can help; see below.)

There is a number of important observations to make based on this example. First, you can specify the offscreen's color mode. Second, you can obtain a drawing context for the Offscreen, and draw anything to it, just as it if was a real device. Third, the offscreen is itself drawable, so you can easily draw it on the screen (or even to another offscreen) when you need to.

#### Buffer allocation

When created, the offscreen tries to allocate a memory buffer on the heap. The amount of memory needed is implied by the offscreen dimensions and the color mode. In our example, the buffer needs 200 _140_ 2 = 56000 bytes. If you specify the background color in the constructor, that buffer will be initialized (the offscreen will be cleared using that background color). Otherwise, the buffer will remain un-initialized; you will need to update all the offscreen's pixels manually.

You can also use an offscreen with a preallocated external buffer:

```cpp
uint8_t buffer[56000];

// ...

void loop() {
  Offscreen<Rgb565>(200, 140, buffer);
}
```

Note that in this case, the buffer is _not_ cleared in the constructor.

#### Pixel formats

You can parameterize the offscreen in exactly the same way as you can parameterize a raster. That is, you can specify the color mode, as well as byte order (for multi-byte color modes) or pixel order (for sub-byte color modes). Essentially, you can think of an offscreen as a DRAM raster that you can draw to. In fact, you can retrieve that raster, by calling `offscreen.raster()`.

Usually, you will want to use the Rgb565 color mode, as it is what most displays natively support, and it only uses two bytes per pixel. In some special situations, however, different color modes can be extremely useful. For example, if you know you only draw grayscale, you may as well use Grayscale8 (or even Grayscale4), significantly reducing the offscreen's footprint. On the other hand, if you want to preserve the transparency information in the result, you will want to use one of the 'A' color modes, e.g. Argb8888, Argb6666, or Argb4444.

Byte order and pixel order are rarely relevant (so, stick to the defaults). They may matter if you plan to export the resulting raster outside of the library and you need to adhere to a specific pixel format.

##### Indexed color modes

When using offscreens with indexed color modes, you need to specify the color palette. You have two options:

* provide the specific list of palette colors up front, or
* allow the library to dynamically build the palette as you draw the content.

###### Using a specific palette

Using an explicit palette with an offscreen is similar to using it with a raster, except for one important difference: the palette must be created as `ReadWrite`:

```cpp
#include "roo_display/color/color_mode_indexed.h"

// ...

static constexpr Color c = color::Cyan.withA(0x99);
static constexpr Color m = color::Magenta.withA(0x99);
static constexpr Color y = color::Yellow.withA(0x99);

Color colors[] = {color::Transparent,
                  c,
                  m,
                  y,
                  AlphaBlend(c, m),
                  AlphaBlend(m, y),
                  AlphaBlend(c, y),
                  AlphaBlend(AlphaBlend(c, m), y)};

// Create a palette suitable for drawing to an offscreen, consisting of the
// specifed colors.
Palette palette = Palette::ReadWrite(colors, 8);

// ...

void loop() {
  Offscreen<Indexed4> offscreen(150, 150, color::Transparent,
                                Indexed4(&palette));
  {
    DrawingContext dc(offscreen);
    dc.draw(FilledCircle::ByRadius(50, 50, 50, c));
    dc.draw(FilledCircle::ByRadius(90, 50, 50, m));
    dc.draw(FilledCircle::ByRadius(70, 80, 50, y));
  }
  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(10000);
}
```

![img36](images/img36.png)

Note that even though we only drew three colors explicitly (cyan, magenta, and yellow), the picture correctly captured also the implicit 'blended' colors. All we needed to do is to anticipate and declare them in the palette. This works because `roo_display` internally uses ARGB consistently. The drawables aren't bothered with palettes and color modes. THe conversion from ARGB to the indexed colors is performed late, in the `Offscreen` itself, using a hashmap embedded in the 'read-write' palette.

If you do try to draw a color that is not in the palette, the first color of the palette is used as a replacement.

###### Using a dynamic palette

Anticipating all the colors you'll use can be a little tedious. A simpler way is to let `roo_display` build the palette for you automatically, using the colors that you actually use when drawing:

```cpp
static constexpr Color c = color::Cyan.withA(0x99);
static constexpr Color m = color::Magenta.withA(0x99);
static constexpr Color y = color::Yellow.withA(0x99);

// ...
void loop() {
  // Declare a dynamic palette with up-to 16 colors.
  Palette palette = Palette::Dynamic(16);
  Offscreen<Indexed4> offscreen(150, 150, color::Transparent, Indexed4(&palette));
  {
    DrawingContext dc(offscreen);
    dc.draw(FilledCircle::ByRadius(50, 50, 50, c));
    dc.draw(FilledCircle::ByRadius(90, 50, 50, m));
    dc.draw(FilledCircle::ByRadius(70, 80, 50, y));
  }
  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(10000);
}
```

The colors are added to the palette in the order you use them. When you try to draw using a color that is not in the palette, and the current size of the palette is smaller than the declared maximum size, the new color is added at the next available position. Otherwise, drawing defaults to the first color in the palette (i.e. the color with index zero).

> Note: the implication is that drawing to an offscreen mutates your palette.

> Note: antialiasing doesn't work well with small palettes. Tonal gradients need a lot of colors.

#### Clip masks

In an earlier section, we saw how to use clip masks to restrict drawing to non-rectangular, complex areas. A clip mask, however, is essentially a 1-bit-per-pixel raster. It means that we can offscreens to construct bit masks!

In fact, the library provides a convenience subclass `BitMaskOffscreen` to make it straightforward:

```cpp
// ...

// 320x60 pixels.
roo::byte mask_data[320 * 60 / 8];

void setup() {
  // ...

  // We initialize the clip mask here.
  BitMaskOffscreen offscreen(320, 60, mask_data);
  {
    DrawingContext dc(offscreen);
    dc.clear();
    for (int i = 0; i < 320; ++i) {
      int y = (int)(10 * sin(i / 10.0)) + 30;
      // Anything but color::Transparent is interpreted as 'bit set'.
      dc.draw(Line(i, 0, i, y, color::Black));
    }
  }
}

void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  DrawingContext dc(display);
  int w = dc.width();
  int h = dc.height();
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  ClipMask mask(mask_data, Box(0, 90, 319, 149));
  dc.setClipMask(&mask);
  label.setColor(color::Crimson);
  dc.draw(label, kCenter | kMiddle);
  mask.setInverted(true);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipMask(nullptr);  // To be on the safe side.

  delay(10000);
}
```

![img31](images/img31.png)

As you can see, clip masks can be useful for drawing complex geometry with minimum RAM footprint. We just need to draw the primitives multiple times, applying different masks. It will usually be slower than preparing everything in a 'real' Rgb565 offscreen and drawing it out, but it only needs 9600 bytes for the entire 320x200 screen, as opposed to over 153 KB for Rgb565 (that we may not be able to allocate).

#### Offscreens with transparency: BLENDING_MODE_SOURCE

Ocassionally, you may want to use an Offscreen with an alpha channel, that is, using one of the color modes that support transparency, such as ARGB8888, ARGB6666, ARGB4444, Alpha4, etc. This way you can prepare translucent contents in the offscreen buffer, and later alpha-blend it over something else. It poses an interesting challenge: how do you actually draw translucent contents to such an offscreen? Whatever you draw, will, by default, be alpha-blended over the existing content. The issue is similar to painting on glass: the more paint you put on the glass, the more opaque it becomes, no matter how tranlucent the paint is. How do you _replace_ the underlying color with translucency, effectively 'erasing' the glass before you put the new paint on?

The solution it to call `setBlendingMode(BLENDING_MODE_SOURCE)` on the DrawingContext, changing from the default `BLENDING_MODE_SOURCE_OVER` (alpha-blending) to simple replacement.

> Note: when drawing an opaque color, BLENDING_MODE_SOURCE is effectively identical to BLENDING_MODE_SOURCE_OVER - the new color will replace the previous one.

> Note: you don't need to use alpha channel just to draw translucent content to an offscreen, as long as the result itself can be opaque.

Blending mode can be adjusted for any drawing context, not just for the offscreens. It is rarely useful for physical devices, though, because they operate in RGB (e.g. RGB565), without alpha, so there is rarely any need to output translucent colors to them and expect anything else than alpha-blending over the background bolor (or existing contents if they support that). We will look at other use cases of blending modes later, in a [dedicated section](programming_guide.md#blending-modes).

#### Extents and alignment

So far, for the offscreens we created, the only geometry we specified was width and height. Such an offscreen has both extents and anchor extents equal to `(0, 0, width - 1, height - 1)`.

An offscreen can have an arbitrary extents rectangle, however. You can specify these extents in the constructor:

```cpp
void loop() {
  Offscreen<Rgb565> offscreen(Box(50, 50, 269, 189));
  {
    DrawingContext dc(offscreen);
    dc.fill(color::SkyBlue);
    dc.setClipBox(70, 70, 249, 169);
    dc.fill(color::DarkBlue);
  }
  DrawingContext dc(display);
  dc.draw(offscreen);

  delay(10000);
}
```

![img38](images/img38.png)

Furthermore, you can set the anchor extents (effective when the offscreen itself is drawn) by calling `offscreen.setAnchorExtents()`.

Finally, there is a convenience offscreen constructor that accepts an arbitrary drawable, and effectively makes a copy of it in memory:

```cpp
  Offscreen<Rgb565> copy(drawable);
```

In the next section, we will see why it may be sometimes useful (if you can afford the memory).

### Backgrounds

Not all drawables are created equal.

The baseline contract of a `Drawable` is to be able to draw itself to a `Surface` provided by a `DrawingContext`. But some drawables offer more flexibility, captured by two further virtual classes: `Streamable` (which extends `Drawable`), and `Rasterizable` (which extents `Streamable`):

* a `Streamable` is able to generate a sequential stream of pixels that fill up its extents rectangle, possibly clipped;
* a `Rasterizable` is able to report the color of any pixel within its extents rectangle, given the coordinates.

Out of the drawable classes that we encountered so far, `Raster`, `Offscreen`, and all the smooth shapes, are rasterizable, and `RleImage` and `RleImage4bppBiased` are streamable.

In this section, we will focus on rasterizables, leaving streamables for the later section on [dynamic composition](programming_guide.md#dynamic-composition).

So why are rasterizables useful? The most immediate benefit is that you can use them as backgrounds. Anything you draw on top of them will be anti-aliased and alpha-blended correctly.

Let's look at a simple example of using a stretched raster as a gradient:

```cpp
#include "roo_display/color/hsv.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/90.h"

/// ...

// 1x240 Rgb565 pixels.
roo::byte background_data[240 * 2];

void setup() {
  // ...

  // Since we're filling full screen, we can use the 'non-clearning'
  // init here.
  display.init();
  display.setBackgroundColor(Graylevel(0xF0));

  // ...

  // We initialize the raster here.
  Offscreen<Rgb565> offscreen(1, 240, background_data);
  {
    DrawingContext dc(offscreen);
    dc.drawPixels([](PixelWriter& writer) {
      for (int i = 0; i < 240; ++i) {
        writer.writePixel(0, i, HsvToRgb(360.0 / 240 * i, 0.5, 0.9));
      }
    });
  }
}

void loop() {
  DrawingContext dc(display);
  // Creates a single vertical line raster.
  ConstDramRaster<Rgb565> raster(1, 240, background_data);
  // Repeats the raster pattern infinitely.
  auto bg = MakeTiledRaster(&raster);
  dc.setBackground(&bg);
  // Actually fill the screen with the background.
  // (Alternatively, experiment with FILL_MODE_RECTANGLE).
  dc.clear();
  // Magnify to emphasize anti-aliasing.
  dc.setTransformation(Transformation().scale(3, 3));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(10000);
}
```

![img32](images/img32.png)

We used a little trick here to keep memory overhead small: the raster consists of a single vertical line, and then we use a utility method to expand it by repeating the pattern infinitely.

If you can generate a color for a point given its (x, y) coordinates with a function (or a function-like object), a rasterizable can be created really easily from it by using the `MakeRasterizable()` factory:

```cpp
void loop() {
  DrawingContext dc(display);
  auto bg = MakeRasterizable(Box(60, 60, 259, 179), [](int16_t x, int16_t y) {
    return ((x / 40) - (y / 40)) % 2 ? color::White : color::MistyRose;
  }, TRANSPARENCY_NONE);
  dc.setBackground(&bg);
  dc.clear();

  dc.setTransformation(Transformation().scale(3, 3));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(10000);
}
```

![img33](images/img33.png)

Note `TRANSPARENCY_NONE`, the third argument to `MakeRasterizable()`. It is an optional hint: we are telling the library that the colors of the background rasterizable will always be fully opaque. Providing this hint speeds up rendering.

We specified the bounds of our background. We did it primarily to show that it is actually possible, and that the combination of the rasterizable background and the background color works as you might have expected (the rasterizable is drawn 'in front of' the background color). If your rasterizable is unbounded, you can specify `Box::MaximumBox()` as the extents (or just use `display.extents()`).

In fact, your rasterizable can be semi-transparent, and it will then blend over the background color. In the example below, we create a background that will gradually tint the underlying color towards blue:

```cpp
void loop() {
  DrawingContext dc(display);
  auto bg = MakeRasterizable(display.extents(), [](int16_t x, int16_t y) {
    return color::RoyalBlue.withA(y);
  });
  dc.setBackground(&bg);
  dc.setBackgroundColor(color::Violet);
  dc.clear();

  dc.setTransformation(Transformation().scale(5, 5));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(10000);
}
```

![img34](images/img34.png)

Finally, you can set a background, just like a background color, as a display default:

```cpp

auto bg = MakeRasterizable(Box::MaximumBox(), [](int16_t x, int16_t y) {
  return ((x / 20) - (y / 20)) % 2 ? color::White : color::MistyRose;
}, TRANSPARENCY_NONE);

void setup() {
  // ...
  display.setBackground(&bg);
  display.clear();
}

void loop() {
  DrawingContext dc(display);
  dc.draw(TextLabel("Hello!", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);
  for (int i = 0; i < 255; i++) {
    dc.draw(Line(i + 40, 190, i+40, 229, color::Blue.withA(i)));
  }
  delay(10000);
}
```

![img35](images/img35.png)

Now you can practically forget that the background is there - it will be painted no matter what you draw.

### Gradients

In the previous section, we saw how to define simple horizontal or vertical color gradients. You will find support for more sophisticated gradients in `roo_display/color/gradient.h`. You can define angular, radial, and arbitrary linear gradients, and all of them can be multi-point, and possibly periodic:

```cpp
#include "roo_display/color/gradient.h"
#include "roo_display/color/hsv.h"

// ...

void loop() {
  int w = display.width();
  int h = display.height();
  auto centered = kMiddle | kCenter;
  {
    // Radial gradient.
    DrawingContext dc(display, 0, 0, Box(0, 0, w / 2 - 1, h / 2 - 1));
    auto gradient = RadialGradientSq(
        {w / 4, h / 4}, ColorGradient({{0, HsvToRgb(60, 0.8, 0.99)},
                                       {50 * 50, HsvToRgb(0, 0.8, 0.95)},
                                       {51 * 51, color::Transparent}}));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Periodic, angular gradient.
    // Note that we're repeating the same color at both ends of the gradient,
    // to make the period equal to 120 degrees (2 * Pi / 3) and to smoothly
    // oscillate between red and yellow.
    DrawingContext dc(display, w / 2, 0, Box(0, 0, w / 2 - 1, h / 2 - 1));
    auto gradient =
        AngularGradient({w / 4, h / 4},
                        ColorGradient({{0, HsvToRgb(60, 0.8, 0.95)},
                                       {M_PI / 3, HsvToRgb(0, 0.8, 0.99)},
                                       {M_PI / 1.5, HsvToRgb(60, 0.8, 0.95)}},
                                      ColorGradient::PERIODIC),
                        Box(20, 20, w / 2 - 21, h / 2 - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Multi-node vertical gradient.
    DrawingContext dc(display, 0, h / 2, Box(0, 0, w / 2 - 1, h / 2 - 1));
    float v = 0.9;
    float s = 0.7;
    auto gradient = VerticalGradient(10, 1,
                                     ColorGradient({{0, HsvToRgb(0, s, v)},
                                                    {20, HsvToRgb(60, s, v)},
                                                    {40, HsvToRgb(120, s, v)},
                                                    {60, HsvToRgb(180, s, v)},
                                                    {80, HsvToRgb(240, s, v)},
                                                    {100, HsvToRgb(300, s, v)},
                                                    {120, HsvToRgb(360, s, v)}},
                                                   ColorGradient::PERIODIC),
                                     Box(20, 20, w / 2 - 21, h / 2 - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Multi-node skewed linear gradient.
    DrawingContext dc(display, w / 2, h / 2, Box(0, 0, w / 2 - 1, h / 2 - 1));
    float v = 0.9;
    float s = 0.7;
    auto gradient = LinearGradient({10, 10}, 0.5, 0.3,
                                   ColorGradient({{0, HsvToRgb(0, s, v)},
                                                  {20, HsvToRgb(60, s, v)},
                                                  {40, HsvToRgb(120, s, v)},
                                                  {60, HsvToRgb(180, s, v)},
                                                  {80, HsvToRgb(240, s, v)},
                                                  {100, HsvToRgb(300, s, v)},
                                                  {120, HsvToRgb(360, s, v)}},
                                                 ColorGradient::PERIODIC),
                                   Box(20, 20, w / 2 - 21, h / 2 - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }

  delay(10000);
}
```

![img56](images/img56.png)

> Note: large-area gradients may slow down the rendering noticeably. Multicolor areas are generally slower to render than unicolor areas, and, on top of that, gradient computation is CPU-intensive. For example, the angular gradient requires calculation of a trigonometric function for every pixel. Use with caution.

### Touch

In this section, we will look at support for touch-screen devices.

#### Configuration

Touch configuration tends to be quite independent from the display. Some devices may use the same SPI bus for both (with common SCK, MOSI used for the display, and MISO used for touch). Some others may have completely separate SPI pins for touch. (That gives more flexibility, but you can still attach these to the same SPI bus if you want to). Yet others may use different bus types for display and touch whatsoever; e.g. parallel interface for display, and a 2-Wire for touch.

Drivers for touch are independent from display, too. If you have a resistive touch panel, there is a good chance that it uses XPT 2046. Capacitive screens may use different drivers.

In case of an SPI-based driver, like XPT 2046, you need to connect at least 3 signals: SPI SCK, SPI MISO, and touch CS (chip select). The SCK and MISO can be usually be shared with other devices on the same SPI bus.

A simple raw setup with an ILI 9431 display and a XPT 2046 touch may look as follows:

```cpp
#include "Arduino.h"
#include "SPI.h"
#include "roo_display.h"
#include "roo_display/touch/calibration.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"

using namespace roo_display;

Ili9341spi<5, 17, 27> display_device;
TouchXpt2046<2> touch_device;

Display display(display_device, touch_device,
                TouchCalibration(269, 249, 3829, 3684,
                                 Orientation::LeftDown()));

void setup() {
  // ...
  SPI.begin();
}
```

The 'touch CS' pin is specified as a template argument to the touch driver. `SPI.begin()` handles both the display and touch, since they are connected to the same bus (using default pins) in this case.

#### Calibration

The native coordinate system used by your touch device can be completely different than your display. For example, you may have a 320x240 screen, but the XPT 2046 will report coordinates in the range [0..4095], and they may even be flipped with respect to the screen.

`TouchCalibration` brings these coordinate systems together. It specifies a bounding box of touch coordinates that correspond to screen extents, plus the information about relative orientation of touch vs the display.

How do you figure out the right values? First of all, check the `roo_display/products` subdirectory for your specific hardware unit - if you are in luck, and your device is explicitly supported, the touch will be tentatively pre-calibrated.

When using raw drivers, or if the default calibration is off, you can measure the necessary calibration parameters for your specific unit by using the helper script at `roo_display/examples/calibrate_touch.ino`. The script will guide you through the steps.

#### Usage

Once configured and calibrated, touch is simple to use. Specifically, the `Display` class provides the following method:

```cpp
class Display {

  // ...

  // If touched, returns true and sets the touch (x, y) coordinates (in device
  // coordinates). If not touched, returns false and does not modify (x, y).
  bool getTouch(int16_t &x, int16_t &y);

  // ...
}
```

Putting it all together, the following example draw a draggable cross-hairs at the touch point, for as long as the display is touched:

```cpp
#include "SPI.h"
#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"

#include "roo_display/shape/basic.h"

using namespace roo_display;

Ili9341spi<5, 17, 27> display_device;
TouchXpt2046<2> touch_device;

Display display(display_device, touch_device,
                TouchCalibration(269, 249, 3829, 3684,
                                 Orientation::LeftDown()));

void setup() {
  SPI.begin();
  display.init(color::DarkGray);
}

int16_t x = -1;
int16_t y = -1;
bool was_touched = false;

void loop(void) {
  int16_t old_x = x;
  int16_t old_y = y;
  bool touched = display.getTouch(x, y);
  if (touched) {
    was_touched = true;
    DrawingContext dc(display);
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
    if (x != old_x) {
      dc.draw(Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
    }
    if (y != old_y) {
      dc.draw(Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
    }
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
  } else {
    if (was_touched) {
      was_touched = false;
      DrawingContext dc(display);
      dc.draw(Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
      dc.draw(Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
    }
  }
}
```

Touch coordinates always correspond to the display coordinates, which is to say, they respond to the display orientation settings. Basically, if you draw a point at (x, y) and touch it, the touch coordinates are (x, y) as well.

> XPT 2046 output can be very noisy. The driver implements denoising and smoothing. For best results, call `getTouch` frequently. (The call is quite cheap; in particular, it never idle-waits, as many other XPT 2046 drivers do).

#### Multi-touch

Some capacitive screens support multi-touch. In particular, devices based on a GT911 controller can report up to 5 touch points simultaneously.

To detect multi-touch, use another variant of the `getTouch` method of the `Display` class:

```cpp
class Display {
  // ...

  // If touch has not been registered, returns {.touch_points = 0 } and does not
  // modify `points'. If k touch points have been registered, sets max(k,
  // max_points) `points`, and returns {.touch_points = k}. In both cases,
  // returned timestamp_us specifies the micros() corresponding to the
  // detection time.
  TouchResult getTouch(TouchPoint *points, int max_points) {
    return touch_.getTouch(points, max_points);
  }
};
```

> You can use this method also with single-touch devices; it will simply never return more than 1 touch point.

Compared to the simpler alternative above, this method additionally provides access to the touch detection timestamp, which can be useful in advanced use cases such as gesture detection.

### Implementing your own drawables

In the earlier section, we had a quick peek at the `Drawable` interface. Let's now look at it in more detail:

```cpp
class Drawable {
 public:
  virtual ~Drawable() {}

  // Returns the bounding box encompassing all pixels that need to be drawn.
  virtual Box extents() const = 0;

  // Returns the boundaries to be used when aligning this drawable. By default,
  // equivalent to extents(). Some drawables, notably text labels, may want to
  // use different bounds for alignment.
  virtual Box anchorExtents() const { return extents(); }

 private:
  friend void Surface::drawObject(const Drawable &object) const;

  // Draws this object's content, respecting the fill mode. That is, if
  // s.fill_mode() == FILL_MODE_RECTANGLE, the method must fill the entire
  // (clipped) extents() rectangle (using s.bgcolor() for transparent parts).
  //
  // The default implementation fills the clipped extents() rectangle
  // with bgcolor, and then calls drawInteriorTo. That causes flicker, so you
  // should try to override this method if a better implementation is possible.
  //
  // The implementation must also respect the other surface's parameters,
  // particularly the clip_box - i.e. it is not allowed to draw to output
  // outside of the clip_box.
  //
  // The surface's clip-box is always pre-clipped to fit within this drawable's
  // extents().
  //
  virtual void drawTo(const Surface &s) const;

  // Draws this object's content, ignoring s.fill_mode(). See drawTo().
  //
  // The implementation most respect the other surface's parameters,
  // particularly the clip_box - i.e. it is not allowed to draw to output
  // outside of the clip_box.
  virtual void drawInteriorTo(const Surface &s) const {}
};
```

We already discussed `extents()` and `anchorExtents()` before. The former specifies the minimum bounding box for any pixels drawn, and the latter defines bounds for the purpose of alignment.

#### Using DrawingContext

In order to implement the actual drawing, you override either `drawTo()` or `drawInteriorTo()`. Implementing the latter is a little easier, so we will start with that.

The method takes a `Surface` argument that your drawable is supposed to draw itself to. Before we delve into the details of the surface, let's take a look at a very useful trick that lets you define complex drawables without even dealing with the surface directly. That trick is: grab a `DrawingContext` for the surface, and just draw to it as you would draw to a screen:

```cpp
#include "roo_display/shape/point.h"

class Polygon : public Drawable {
 public:
  Polygon(std::vector<Point> corners, Color color)
      : corners_(std::move(corners)), color_(color) {
    assert(corners_.size() >= 3);
    extents_ = FilledTriangle(corners_[0].x, corners_[0].y, corners_[1].x,
                              corners_[1].y, corners_[2].x, corners_[2].y,
                              color::Black)
                   .extents();
    for (int i = 3; i < corners_.size(); ++i) {
      extents_ = Box::Extent(
          extents_, FilledTriangle(corners_[0].x, corners_[0].y,
                                   corners_[i - 1].x, corners_[i - 1].y,
                                   corners_[i].x, corners_[i].y, color::Black)
                        .extents());
    }
  }

  Box extents() const override { return extents_; }

 private:
  void drawInteriorTo(const Surface& s) const override {
    DrawingContext dc(s);
    for (int i = 2; i < corners_.size(); ++i) {
      dc.draw(FilledTriangle(corners_[0].x, corners_[0].y, corners_[i - 1].x,
                             corners_[i - 1].y, corners_[i].x, corners_[i].y,
                             color_));
    }
  }

  std::vector<Point> corners_;
  Color color_;
  Box extents_;
};

void loop() {
  DrawingContext dc(display);
  dc.draw(
      Polygon({{40, 70}, {60, 88}, {69, 140}, {14, 140}}, color::OrangeRed));
  dc.draw(Polygon({{150, 70}, {170, 88}, {170, 140}, {134, 140}, {120, 100}},
                  color::Navy), kCenter | kBottom.shiftBy(-20));
  dc.draw(Polygon(
      {{250, 70}, {278, 88}, {285, 110}, {270, 140}, {234, 140}, {220, 100}},
      color::SeaGreen));

  delay(10000);
}
```

![img37](images/img37.png)

Note that the drawing context will take care of any offsets due to alignment, so you don't need to worry about that - just draw using your drawables 'native' coordinates, corresponding to its extents. (It is emphasized in the example above by drawing the second polygon as aligned at the bottom-center). Background colors are handled automatically as well.

Also, you can apply any paremeterization supported by the drawing context. For example, you can add backgrounds or transformations, apply clip masks, and set background colors.

##### Handling FILL_MODE_RECTANGLE

If you looked carefully at the definition of the `Drawable` class before, you may have noticed that the difference between `drawTo()` and `drawInteriorTo` is that the former needs to respect the drawing context's fill mode. That is, in case of FILL_MODE_RECTANGLE, `drawTo()` needs to draw all pixels of the `extents()` rectangle - even those that are fully transparent. In some cases it is simple or even trivial,  e.g. for objects that are naturally rectangular, such as images. In other cases it is difficult - which is why the default implementation is provided in the first place. The rule of thumb: if it is simple enough, override `drawTo()`; otherwise, override `drawInteriorTo()`.

#### Drawing to the surface

Using `DrawingContext` is simple and powerful, but it adds some CPU and memory overhead. In particular, the `DrawingContext` object is over 0.5KB in size, so it may add some pressure on your call stack usage.

If all you need is to draw some existing objects without applying any transformations, additional clipping, backgrounds, or any other features provided by the drawing context, you can use  `surface.drawObject()` instead:

```cpp
  void drawInteriorTo(const Surface& s) const override {
    for (int i = 2; i < corners_.size(); ++i) {
      s.drawObject(FilledTriangle(corners_[0].x, corners_[0].y,
                                  corners_[i - 1].x, corners_[i - 1].y,
                                  corners_[i].x, corners_[i].y, color_));
    }
  }
```

This method will still correctly handle any translations, clipping, and the inherited background color, so you don't need to worry about any of that.

#### Drawing directly to the driver

If your drawable is drawn frequently and if it consists of many small parts, the techniques described above may add too much overhead.  In this case, you need to go to a lower level, and write more directly to `s.out()`, which typically is going to be your display's driver. You will then need to manually handle some aspects of the drawable contract. Specifically:

* any logical coordinates must be adjusted by `(s.dx(), s.dy())`;
* you must not draw outside of the `s.clip_box()` rectangle, expressed in the device coordinates (i.e. clipping needs to be applied _after_ the coordinate adjustment);
* if the content contains any transparency, it needs to be alpha-blended over `s.bgcolor()` before drawing;
* you need to pass `s.blending_mode()` to the underlying device.

With these in mind, you can call the methods on `s.out()` to output rectangles or pixels via the driver API.

##### Driver API

The driver API, represented by the `DisplayOutput` class, is quite small; it consists of just 6 'core' methods:

* `writePixels`: writes a batch of pixels with specified coordinates and colors;
* `fillPixels`: writes a batch of pixels with specified coordinates and the same color;
* `writeRects`: writes a batch of rectangles with specified coordinates and colors;
* `fillRects`: writes a batch of rectangles with specified coordinates and the same color;
* `setAddress`/`write`: fill a single rectangle (or a part of it) with subsequent colors, going top-to-bottom and left-to-right.

As you can see, the API is _vectorized_, in that it operates on _batches_ of items. Vectorization is key to achieving good performance. Batches do not need to be very large; 32 to 64 items is good enough - but calling these APIs to draw individual pixels or rectangles will add noticeable overhead.

In most cases, optimal use of the batch API does not come naturally, so you may want to use buffered drawing utilities.

##### Buffered drawing utilities

Helper classes defined in `roo_display/core/buffered_drawing.h` are adapters that allow drawing individual primitives such as pixels, lines, or rectangles, grouping them in batches before sending them to the driver. When you instantiate such an adapter, it allocates a small buffer on the stack, and uses that buffer to build up batches that are then flushed to the driver.

Basic shapes, such as lines or circles, use buffered drawing utilities a lot, so they are a good case study. See `roo_display/shape/basic.cpp`.

The names of those classes follow a convention, so it is easy to understand what they do - you just need to remember that 'write' means using distinct colors, and 'fill' means using the same color:

* `BufferedPixelWriter` buffers calls to `writePixels`,
* `BufferedPixelFiller` buffers calls to `fillPixels`,
* `BufferedRectWriter` buffers calls to `writeRects`,
* `BufferedRectFiller` buffers calls to `fillRects`.

Then, there are 'Clipping' variants of the above, which do the same, but respect a specified clip rectangle. That is, pixels or rectangles that are outside of the clip rectangle are ignored, and rectangles that intersect the clip rectangle are clipped.

Next, we have:

* `BufferedHLineFiller` - for drawing horizontal lines of the same color,
* `BufferedVLineFiller` - for drawing vertical lines of the same color.

Again, there are also 'Clipping' variants of the above.

Finally, there is `BufferedColorWriter` that buffers calls to `write`.

Checking for clipping at every item adds some overhead, so it is better to optimize it out if possible. For example, the built-in implementation of `Line::drawTo()` will first check if the line fits entirely in the clip rectangle, and if so, it uses the 'non-clipping' variant of the HLineFiller or the VLineFiller (depending on slope).

## Part 3: Advanced topics

### Blending modes

By default, when you are drawing to an offscreen, or to a device with a framebuffer, everything you draw is alpha-blended over pre-existing content. In the section about [offscreens](programming_guide.md#using-off-screen-buffers), we saw how to override this behavior by setting the blending mode to `BLENDING_MODE_SOURCE`, causing the new content to simply replace pre-existing content.

What else can you do with blending modes?

THe `roo_display` library supports 12 blending modes, defined in `roo_display/color/blending.h`, corresponding to the [alpha-compositing](https://en.wikipedia.org/wiki/Alpha_compositing) Porter-Duff operators.

There is a lot of excellent resources on Porter-Duff operators (see e.g. [Søren Sandmann Pedersen's blog](http://ssp.impulsetrain.com/porterduff.html), or the [interactive Wolfram's demo](https://demonstrations.wolfram.com/DuffPorterAlphaCompositingOperators/)). Let's jump right in to see what we can do with using a couple of new blending modes combined with some basic shapes and gradients:

```cpp
#include "roo_display/color/gradient.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/shape/smooth.h"

void setup() {
  // ...
  display.init(color::LightSeaGreen);
}

void loop() {
  auto circle = SmoothThickCircle({100, 100}, 71, 42, Color(0xFFEC6D44));
  auto rect1 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, M_PI / 4, color::White);
  auto rect2 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, -M_PI / 4, color::White);
  auto gradient = RadialGradientSq(
      {100, 100}, ColorGradient({{50*50, color::Black.withA(0x60)},
                                 {65*65, color::Black.withA(0x15)},
                                 {71*71, color::Transparent},
                                 {77*77, color::Black.withA(0x15)},
                                 {92*92, color::Black.withA(0x60)}}));
  auto shadow =
      SmoothThickCircle({100 - 5, 100 + 5}, 71, 42,
      color::Black.withA(0x80));

  Offscreen<Argb4444> offscreen(201, 201);
  {
    DrawingContext dc(offscreen);
    dc.clear();
    dc.draw(circle);
    dc.setBlendingMode(BLENDING_MODE_SOURCE_ATOP);
    dc.draw(rect1);
    dc.draw(rect2);
    dc.draw(gradient);
    dc.setBlendingMode(BLENDING_MODE_DESTINATION_OVER);
    dc.draw(shadow);
  }

  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(10000);
}
```

![img57](images/img57.png)

We used `BLENDING_MODE_SOURCE_ATOP` to draw two rectangles and a gradient in a way they are 'clipped' to the underlying ring. We then used `BLENDING_MODE_DESTINATION_OVER` to put the ring shadow under the picture.

Note that blending modes behave well on the edges of our (smooth) shapes - everything is nicely anti-aliased.

Note that this example required an offscreen that supported transparency. We used ARGB4444, rather than ARGB6666, to conserve RAM. (The image needs ~80 KB of heap space). It made the gradient look rather coarse. In the next section, we will look at ways to overcome this limitation.

> In the future, more blending modes will be added, e.g. using [ImageMagick](https://imagemagick.org/script/compose.php) as a source of inspiration.

### Dynamic composition

Wouldn't it be great if you could compose drawables using blending modes, as we did in the previous section, but without needing to render the result to a temporary offscreen?

Bad news is that you cannot do that with arbitrary drawables. Good news is that you can do that if your drawables are streamables or rasterizables.

Dynamic composition produces a drawable that combines multiple inputs, and is drawn in one pass, without any flicker.

If all the inputs are rasterizables (i.e. if they all extend `Rasterizable`), you can combine them into a logical 'stack' that is itself a rasterizable. During rendering, it needs only a little bit of stack memory (less than 1 KB) - i.e. no more need to preallocate an offscreen.

#### Rasterizable stack

Smooth shapes happen to all be rasterizable (that is, they inherit from the `Rasterizable` base class), which means that we can rewrite the previous example to use the `RasterizableStack`:

```cpp
#include "roo_display/composition/rasterizable_stack.h"

// ...

void loop() {
  auto circle = SmoothThickCircle({100, 100}, 71, 42, Color(0xFFEC6D44));
  auto rect1 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, M_PI / 4, color::White);
  auto rect2 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, -M_PI / 4, color::White);
  auto gradient = RadialGradientSq(
      {100, 100}, ColorGradient({{50*50, color::Black.withA(0x60)},
                                 {65*65, color::Black.withA(0x15)},
                                 {71*71, color::Transparent},
                                 {77*77, color::Black.withA(0x15)},
                                 {92*92, color::Black.withA(0x60)}}));
  auto shadow =
      SmoothThickCircle({100 - 5, 100 + 5}, 71, 42,
      color::Black.withA(0x80));

  RasterizableStack stack(Box(0, 0, 200, 200));
  stack.addInput(&circle);
  stack.addInput(&rect1).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&rect2).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&gradient).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&shadow).withMode(BLENDING_MODE_DESTINATION_OVER);
  {
    DrawingContext dc(display);
    dc.draw(stack, kCenter | kMiddle);
  }
  delay(10000);
}
```

![img58](images/img58.png)

Not only did we save 80 KB; the quality is now much better, too, because we no longer needed to compromise on bits-per-pixel. And it renders a bit faster, too. (Still somewhat slow, because radial gradients are expensive - calculating the gradient takes about half of the total rendering time).

The `RasterizableStack` is itself rasterizable, so you can use the result for further composition. Let's now compose several levels of stacks in order to dynamically render temperature gradients in a water heater:

```cpp
class HeatingSpiralContent : public RasterizableStack {
 public:
  HeatingSpiralContent()
      : RasterizableStack(Box(0, 0, 99, 219)), thickness_(8) {
    for (int i = 0; i < 12; ++i) {
      elements_r_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_, color::Black);
      addInput(&elements_r_[i]);
    }
    for (int i = 0; i < 12; ++i) {
      elements_l_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_, color::Black, ENDING_FLAT);
      addInput(&elements_l_[i]);
    }
    outlet_ = SmoothThickLine({90, 25}, {100, 25}, thickness_, color::Black);
    addInput(&outlet_);
    inlet_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                             thickness_, color::Black);
    addInput(&inlet_);
  }

 private:
  float thickness_;
  SmoothShape elements_l_[12];
  SmoothShape elements_r_[12];
  SmoothShape outlet_;
  SmoothShape inlet_;
};

class HeatingSpiralBody : public RasterizableStack {
 public:
  HeatingSpiralBody()
      : RasterizableStack(Box(0, 0, 99, 219)), thickness_outer_(10), thickness_inner_(7) {
    outlet_ = SmoothThickLine({90, 25}, {100, 25}, thickness_outer_, color::Black);
    addInput(&outlet_);
    for (int i = 0; i < 12; ++i) {
      elements_r_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_outer_, color::Black);
      addInput(&elements_r_[i]);
    }
    inlet_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                             thickness_outer_, color::Black);
    addInput(&inlet_);
    outlet_i_ = SmoothThickLine({90, 25}, {100, 25}, thickness_inner_, color::Black);
    addInput(&outlet_i_).withMode(BLENDING_MODE_DESTINATION_OUT);
    for (int i = 0; i < 12; ++i) {
      elements_ri_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_inner_, color::White);
      addInput(&elements_ri_[i]).withMode(BLENDING_MODE_DESTINATION_OUT);
    }
    for (int i = 0; i < 12; ++i) {
      elements_l_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_outer_, color::Black, ENDING_FLAT);
      addInput(&elements_l_[i]);
      elements_li_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_inner_, color::White);
      addInput(&elements_li_[i]).withMode(BLENDING_MODE_DESTINATION_OUT);
    }
    inlet_i_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                             thickness_inner_, color::Black);
    addInput(&inlet_i_).withMode(BLENDING_MODE_DESTINATION_OUT);
  }

 private:
  float thickness_outer_;
  float thickness_inner_;
  SmoothShape elements_l_[12];
  SmoothShape elements_li_[12];
  SmoothShape elements_r_[12];
  SmoothShape elements_ri_[12];
  SmoothShape outlet_;
  SmoothShape outlet_i_;
  SmoothShape inlet_;
  SmoothShape inlet_i_;
};

class Tank : public RasterizableStack {
 public:
  Tank()
      : RasterizableStack(Box(0, 0, 99, 219)),
        casing_(Box(0, 0, 99, 219)),
        casing_body_(SmoothFilledRoundRect(0, 0, 99, 219, 23, color::Black)),
        casing_gradient_({0, 0}, 0, 1, ColorGradient({{0, color::Purple}, {219, color::Blue}})),
        casing_wall_(SmoothThickRoundRect(0, 0, 99, 219, 23, 1, color::Black)),
        heated_spiral_(Box(0, 0, 99, 219)),
        heated_spiral_gradient_(
            {0, 0}, 0, 1,
            ColorGradient({{0, color::Blue}, {219, color::Red}})),
        heated_spiral_content_(),
        heating_spiral_body_() {
    casing_.addInput(&casing_body_);
    casing_.addInput(&casing_gradient_).withMode(BLENDING_MODE_SOURCE_ATOP);
    casing_.addInput(&casing_wall_);
    addInput(&casing_);
    heated_spiral_.addInput(&heated_spiral_content_);
    heated_spiral_.addInput(&heated_spiral_gradient_)
        .withMode(BLENDING_MODE_SOURCE_ATOP);
    addInput(&heated_spiral_);
    addInput(&heating_spiral_body_);
  }

 private:
  RasterizableStack casing_;
  SmoothShape casing_body_;
  LinearGradient casing_gradient_;
  SmoothShape casing_wall_;
  RasterizableStack heated_spiral_;
  LinearGradient heated_spiral_gradient_;
  HeatingSpiralContent heated_spiral_content_;
  HeatingSpiralBody heating_spiral_body_;
};

void loop() {
  DrawingContext dc(display);
  std::unique_ptr<Tank> tank(new Tank());
  dc.draw(*tank, kCenter | kMiddle);
}
```

![img60](images/img60.png)

Note that we create the tank object on the heap. This is because the total number of inputs in the preceding example creates a significant pressure on the stack memory. A single smooth shape has a footprint of 136 bytes (on ESP32), and we used about 30 of them, which in practice is sufficient to exceed the default stack size (causing crashes). But it works just fine if we allocate those few KB temporarily on the heap.

#### Streamable stack

In practice, it is often desirable to compose objects that are not rasterizable. In particular, you may want to use composition with imported images that use run-length encoding to reduce footprint. In this case, because those images are `Streamable`, you can use a `StreamableStack`, which has an API very similar to the `RasterizableStack`, except that it accepts streamables, not rasterizables (and it produces a streamable, not a rasterizable). As an example, we will use a JPG file, imported as a RLE-enoded image:

```
wget https://github.com/dejwk/roo_display_image_importer/releases/download/1.1/roo_display_image_importer.zip
unzip roo_display_image_importer.zip
./roo_display_image_importer/bin/roo_display_image_importer --output_dir=src -c RLE -e RGB565 roo2.jpg
```

```cpp
#include "roo2.h"
#include "roo_display/composition/streamable_stack.h"

// ...

void loop() {
  StreamableStack stack(Box(0, 0, 199, 199));
  auto c = SmoothFilledCircle({100, 100}, 79, color::Black);
  stack.addInput(&roo2(), Box(0, 0, 177, 177), 11, 11);
  stack.addInput(&c).withMode(BLENDING_MODE_DESTINATION_ATOP);
  DrawingContext dc(display);
  dc.draw(stack, kCenter | kMiddle);
}
```

![img59](images/img59.png)

As you can see, compositing with 'destination-atop' is a bit similar to putting a clip mask over a drawn contents - with an important difference that our mask can now have anti-aliased edges. (Note that the circle-cropped image is properly anti-aliased).

> Note: every rasterizable is streamable, so you can use rasterizables with the `StreamableStack`.

> Note: JPG and PNG images cannot be directly used with the `StreamableStack`, because, unfortunately, those formats do not satisfy the `Streamable` interface. Use the image importer to convert your images to a RLE-encoded internal image format.

Going back to the water heater example, if you tried it on a microcontroller, you might have noticed that it renders a bit slow, needing 270 milliseconds on ESP32 + SPI ILI9341 @40 MHz. We could improve upon it by pre-rendering parts of it and storing as an image in PROGMEM. There is no super-easy way of capturing rendered images (yet), but one can do so by using the [roo_testing](https://github.com/dejwk/roo_testing) emulator, outputting the images to a Linux window, and then taking a screenshot and an [image importer](https://github.com/dejwk/roo_display_image_importer).

> In case you were curious, images in this guide has been captured using [roo_testing](https://github.com/dejwk/roo_testing).

```
./roo_display_image_importer/bin/roo_display_image_importer -c RLE -e ALPHA4 --output-dir=src heater_spiral_content.png heater_spiral_body.png
```

```cpp
#include "heater_spiral_content.h"
#include "heater_spiral_body.h"

// ...

class Tank : public StreamableStack {
 public:
  Tank()
      : StreamableStack(Box(0, 0, 99, 219)),
        casing_(Box(0, 0, 99, 219)),
        casing_body_(SmoothFilledRoundRect(0, 0, 99, 219, 23, color::Black)),
        casing_gradient_(
            {0, 0}, 0, 1,
            ColorGradient({{0, color::Purple}, {219, color::Blue}})),
        casing_wall_(SmoothThickRoundRect(0, 0, 99, 219, 23, 1, color::Black)),
        heated_spiral_(Box(0, 0, 99, 219)),
        heated_spiral_gradient_(
            {0, 0}, 0, 1,
            ColorGradient({{0, color::Blue}, {219, color::Red}})) {
    casing_.addInput(&casing_body_);
    casing_.addInput(&casing_gradient_).withMode(BLENDING_MODE_SOURCE_ATOP);
    casing_.addInput(&casing_wall_);
    addInput(&casing_);
    heated_spiral_.addInput(&heater_spiral_content(), 5, 21);
    heated_spiral_.addInput(&heated_spiral_gradient_)
        .withMode(BLENDING_MODE_SOURCE_ATOP);
    addInput(&heated_spiral_);
    addInput(&heater_spiral_body(), 5, 21);
  }

 private:
  RasterizableStack casing_;
  SmoothShape casing_body_;
  LinearGradient casing_gradient_;
  SmoothShape casing_wall_;
  StreamableStack heated_spiral_;
  LinearGradient heated_spiral_gradient_;
};

void loop() {
  std::unique_ptr<Tank> tank(new Tank());
  dc.draw(*tank, kCenter | kMiddle);
  delay(1000);
}
```

This sketch produces the same output as before, but it takes only 160 ms (down from 270 ms), at the expense of about 6 KB PROGMEM space needed to store the pre-rendered images.

### Using rasterizable overlays and color filters

Dynamic composition, discussed in the previous section, requires that your inputs are rasterizables or streamables. If you draw content that is neither (e.g. text), you are not completely out of luck - you can still inject some dynamic composition.

In fact, we have already seen some examples of it, in the form of backgrounds.

The way backgrounds internally work is by intercepting and augmenting calls to the undelying device driver - in this case, using the `BackgroundFilter` device class. This technique can be used to implement other filters. For example, a `ForegroundFilter` applies a given rasterizable on top of the content drawn, rather than behind it, which can be useful in drawing overlaid graphics or animations:

```cpp
#include "roo_display/filter/foreground.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Bold/27.h"

// ...

class PressAnimationOverlay : public Drawable {
 public:
  PressAnimationOverlay(const Drawable* contents)
      : contents_(contents), xc_(0), yc_(0), r_(0) {}

  Box extents() const override { return contents_->extents(); }

  void set(int16_t xc, int16_t yc, int16_t r) {
    xc_ = xc;
    yc_ = yc;
    r_ = r;
  }

 private:
  void drawTo(const Surface& s) const override {
    Surface my_s(s);
    my_s.set_bgcolor(AlphaBlend(s.bgcolor(), color::Purple.withA(0x20)));
    auto circle = SmoothFilledCircle({xc_, yc_}, r_, color::Purple.withA(0x30));
    // By using the surface's offset, we ensure that the filter is positioned
    // relative to the object being drawn.
    ForegroundFilter fg(s.out(), &circle, s.dx(), s.dy());
    if (r_ != 0) {
      my_s.set_out(&fg);
    }
    my_s.drawObject(*contents_);
  }

  const Drawable* contents_;
  int16_t xc_, yc_, r_;
};

void loop() {
  auto tile = MakeTileOf(
      TextLabel("Hello, World!", font_NotoSans_Bold_27(), color::Black),
      Box(0, 0, 200, 50), kCenter | kMiddle);
  PressAnimationOverlay overlaid(&tile);
  if ((millis() / 1000) % 2 == 0) {
    // Center at (+120, +25) relative to the top-left corner of the rectangle.
    overlaid.set(120, 25, (millis() % 1000) / 2);
  }
  DrawingContext dc(display);
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(overlaid, kCenter | kMiddle);
}
```

![img55](images/img55.png)

This technique can also be used to draw 'sprites':

```cpp
// ...

class JumpingPedroSpriteOverlay : public Drawable {
 public:
  JumpingPedroSpriteOverlay(const Drawable* contents)
      : contents_(contents), xc_(0), yc_(0), jumping_(false) {}

  Box extents() const override { return contents_->extents(); }

  void set(int16_t xc, int16_t yc, bool jumping) {
    xc_ = xc;
    yc_ = yc;
    jumping_ = jumping;
  }

 private:
  void drawTo(const Surface& s) const override {
    static const uint8_t pedro_standing_data[] PROGMEM = {
        0x01, 0x00, 0x05, 0x40, 0x05, 0x40, 0x06, 0x40, 0x09, 0x80, 0x15,
        0x50, 0x15, 0x55, 0x6A, 0xAA, 0x95, 0x55, 0x5F, 0x34, 0x1F, 0xF0,
        0x0F, 0xC0, 0x65, 0x64, 0x59, 0x94, 0xD6, 0x5C, 0xFD, 0x5F, 0xFD,
        0x5F, 0x7D, 0x50, 0x55, 0x50, 0xF0, 0xF0, 0xFC, 0xFC, 0x3C, 0xFC,
    };

    static const uint8_t pedro_jumping_data[] PROGMEM = {
        0x01, 0x00, 0x05, 0x40, 0x05, 0x40, 0x06, 0x40, 0x09, 0x90, 0x05,
        0x54, 0x15, 0x54, 0x5A, 0xAA, 0x65, 0x55, 0x9F, 0x34, 0x1F, 0xF0,
        0xCF, 0xC3, 0xF9, 0x9F, 0xF6, 0x5F, 0xF5, 0x5C, 0x55, 0x50, 0x55,
        0x57, 0x15, 0x5F, 0x15, 0x3F, 0x0F, 0x0C, 0x0F, 0xC0, 0x0F, 0xC0,
    };

    static Color colors[] PROGMEM = {color::Transparent, Color(0xFF7A262C),
                                     Color(0xFFF8C02C), Color(0xFFFAA6AC)};

    static Palette palette = Palette::ReadOnly(colors, 4);

    Surface my_s(s);
    auto pedro_raster = ProgMemRaster<Indexed2>(
        8, 22, jumping_ ? pedro_jumping_data : pedro_standing_data, &palette);
    auto pedro_scaled = MakeRasterizable(
        Box(0, 0, 8 * 4 - 1, 22 * 2 - 1),
        [&](int16_t x, int16_t y) { return pedro_raster.get(x / 4, y / 2); });
    ForegroundFilter fg(s.out(), &pedro_scaled, xc_ + s.dx(), yc_ + s.dy());
    my_s.set_out(&fg);
    my_s.drawObject(*contents_);
  }

  const Drawable* contents_;
  int16_t xc_, yc_;
  bool jumping_;
};

Box invalid(display.extents());

void setup() {
  // ...
  display.init(color::Black);
}

void loop() {
  auto tile = MakeTileOf(
      TextLabel("Hello, World!", font_NotoSans_Bold_27(), color::SkyBlue),
      dc.bounds(), kCenter | kMiddle);
  JumpingPedroSpriteOverlay overlaid(&tile);
  int y = 140;
  bool jumping = (millis() / 500) % 4 == 0;
  if (jumping) {
    int progress = millis() % 500;
    int dy = (progress - 250) / 10;
    y = 140 - (625 - (dy * dy)) / 13;
  }
  overlaid.set(180, y, jumping);
  Box pedro = Box(0, 0, 8 * 4 - 1, 22 * 2 - 1).translate(180, y);
  invalid = Box::Extent(invalid, pedro);
  DrawingContext dc(display);
  dc.setClipBox(invalid);
  invalid = pedro;
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(overlaid, kCenter | kMiddle);
}
```

![img61](images/img61.png)

[![sprite_video_snapshot](images/sprite_video_snapshot.png)](https://youtu.be/RyOe2b5x9Q0)

If you look at the implementation of the `BackgroundFilter` or the `ForegroundFilter`, you will notice that they are specialization of the `BlendingFilter` class, defined in `roo_display/filter/blender.h`:

```cpp
using BackgroundFilter = BlendingFilter<BlendOp<BLENDING_MODE_SOURCE_OVER>>;
```

From this definition, you can see how to specialize `BlendingFilter` to use any blending mode you need. You can also chain more than one filter, thus creating complex rasterizable composition stacks.

### Using 'write-once' mode (experimental)

You can switch a drawing context into a write-once mode, by calling the `setWriteOnce()` method. In the write-once mode, any pixel can be drawn only one time. Subsequent attempts to overdraw it (via the same drawing context, or a descendant context) will do nothing.

The write-once mode can help avoid flicker. For example, rather than drawing a solid background first, and drawing the content later, which normally leads to flicker, you can enable the write-once mode, and then draw the content first, and the solid background later. The background will not touch the previously drawn 'content' pixels.

Once enabled, the 'write-once' mode cannot be turned off - it is active until the drawing context is destroyed.

Implementation-wise, the 'write-once' mode uses a clip-mask-based filter that internally 'masks out' pixels that has already been drawn to. The clip mask is allocated when `setWriteOnce()` is called for the first time. Consequently, the feature has a memory footprint of 1 bit per pixel of the affected drawing context.

### Advanced example: animated analog gauge

We now put together some of the techniques discussed so far, including: the write-once mode, clip masks, and smooth primitives, to write a complete analog gauge widget with a smoothly animated, anti-aliased needle:

```cpp
#include "roo_io/text/string_printf.h"
// ...

static const float kRadius = 120;
static const float kWidth = 30;
static const FpPoint kGaugeCenter{160, 190};
static const Box kTextExtents(kGaugeCenter.x - 45,
                              kGaugeCenter.y - kRadius / 2 - 20,
                              kGaugeCenter.x + 44,
                              kGaugeCenter.y - kRadius / 2);

// Helper class that generates the needle drawable, given the value it should
// indicate.
class Needle {
 public:
  // Value expected to swing between 0 and 1, inclusive.
  Needle(float value) : value_(value) {}

  SmoothShape getShape() {
    float needle_radius = kRadius - kWidth / 2 + 20;
    float needle_angle = (value_ * 2.0f / 3 - 1.0f / 3) * M_PI;
    float needle_x = sinf(needle_angle);
    float needle_y = -cosf(needle_angle);
    return SmoothWedgedLine({kGaugeCenter}, 10,
                            {kGaugeCenter.x + needle_x * needle_radius,
                             kGaugeCenter.y + needle_y * needle_radius},
                            0, color::Red);
  }

 private:
  float value_;
};

// Drawable that prints the numeric value in 'front' of the indicator, with the
// needle behind it.
class ValueIndicator : public Drawable {
 public:
  // Value expected to swing between 0 and 1, inclusive.
  ValueIndicator(float value) : value_(value) {}

  // The extents area covers the entire indicator, so that the needle (set as a
  // background) gets drawn fully, regardless of its position. To make sure we
  // don't overwrite the scale, it is drawn first (in the write-once mode).
  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2 + 10,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20 + 10,
               kGaugeCenter.x + kRadius + kWidth / 2 - 10,
               kGaugeCenter.y - 15 - 10);
  }

 private:
  void drawTo(const Surface& s) const override {
    Needle needle(value_);
    auto needle_shape = needle.getShape();
    DrawingContext dc(s);
    dc.setBackground(&needle_shape);
    dc.draw(MakeTileOf(TextLabel(StringPrintf("%0.1f%%", value_ * 100),
                                 font_NotoSans_Bold_27(), color::Black),
                       kTextExtents, kCenter | kMiddle));
    // We're expecting to be called in the write-once mode, when everything else
    // (e.g. the scale) is drawn. Calling clear() at the very end to make sure
    // that we get all of the background needle.
    dc.clear();
  }

  float value_;
};

// Drawable that prints the radial scale of our indicator, with the needle
// overlaid in front of it.
class Scale : public Drawable {
 public:
  Scale(float value) : value_(value) {}

  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2 + 10,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20 + 10,
               kGaugeCenter.x + kRadius + kWidth / 2 - 10,
               kGaugeCenter.y - 15 - 10);
  }

 private:
  void drawTo(const Surface& s) const override {
    Needle needle(value_);
    auto needle_shape = needle.getShape();
    Surface my_s = s;
    ForegroundFilter filter(s.out(), &needle_shape, s.dx(), s.dy());
    my_s.set_out(&filter);
    DrawingContext dc(my_s);
    // Note: we expect my_s to be write-once.
    dc.setBackgroundColor(color::LightGray);
    for (int i = 0; i < 51; ++i) {
      float angle = (i / 50.0f * 2 / 3 - 1.0f / 3.0f) * M_PI;
      float x = sinf(angle);
      float y = -cosf(angle);
      float tick_inner_radius = kRadius - kWidth / 2 + 5;
      float tick_len = (i % 5 == 0 ? 20 : 10);
      float tick_radius = tick_inner_radius + tick_len;
      dc.draw(SmoothLine({x * tick_inner_radius, y * tick_inner_radius},
                         {x * tick_radius, y * tick_radius}, color::Black),
              kGaugeCenter.x, kGaugeCenter.y);
    }
    dc.setBackgroundColor(color::White);
    dc.draw(SmoothThickArc(kGaugeCenter, kRadius, kWidth, -M_PI * 1 / 3 - 0.05,
                           M_PI * 1 / 3 + 0.05, color::LightGray, ENDING_FLAT));
  }

  float value_;
};

// Puts it all together.
class Gauge : public Drawable {
 public:
  Gauge() : value_(0), full_redraw_(true) {}

  void setValue(float value) {
    previous_value_ = value_;
    value_ = value;
  }

  // If true, redraws the entire content. Otherwise, assumes that setValue()
  // has been called to update the value, and draws the minimum amount of pixels
  // for an incremental update.
  void setFullRedraw(bool full_redraw) { full_redraw_ = full_redraw; }

  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20,
               kGaugeCenter.x + kRadius + kWidth / 2, kGaugeCenter.y - 15);
  }

 private:
  void drawTo(const Surface& s) const override {
    Box box = extents();
    if (full_redraw_) {
      s.drawObject(SmoothThickRoundRect(box.xMin() + 3, box.yMin() + 3,
                                        box.xMax() - 3, box.yMax() - 3, 8, 6,
                                        color::Orange));
    }
    DrawingContext dc(s);
    dc.setWriteOnce();
    if (full_redraw_) {
      // Simple case: we just draw the scale and the value indicator. Due to
      // write-once, value indicator will not overwrite the scale.
      dc.draw(Scale(value_));
      dc.draw(ValueIndicator(value_));
    } else {
      // Incremental case is more interesting. We need to redraw all pixels that
      // are affected by the union of the old and the new needle position, and
      // we need to update the text value.
      Needle old_needle(previous_value_);
      Needle new_needle(value_);
      auto old_needle_shape = old_needle.getShape();
      auto new_needle_shape = new_needle.getShape();
      Box extents = Box::Extent(
          Box::Intersect(dc.getClipBox(), old_needle_shape.extents()),
          Box::Intersect(dc.getClipBox(), new_needle_shape.extents()));
      extents = Box::Extent(extents, kTextExtents);
      // Using bitmask to narrow down to just the pixels that absolutely need to
      // be updated.
      BitMaskOffscreen bitmask(extents);
      DrawingContext bitmask_dc(bitmask);
      bitmask_dc.clear();
      // We need to redraw the text. (For simplicity, we're assuming that the
      // entire text tile needs to be invalidated).
      bitmask_dc.draw(FilledRect(kTextExtents, color::Black));
      // We also need to redraw the pixels affected by the old and the new
      // needle. They can be determined simply by drawing the needles to the
      // bitmask offscreen.
      bitmask_dc.draw(old_needle_shape);
      bitmask_dc.draw(new_needle_shape);

      // Now, let's apply the clip mask and the clip box to our context, and
      // just draw the content as before. Note that we're inverting the mask,
      // because the '1's now indicate pixels that *should* be drawn.
      dc.setClipBox(extents);
      ClipMask mask(bitmask.buffer(), extents);
      dc.setClipMask(&mask);
      mask.setInverted(true);
      dc.draw(Scale(value_));
      dc.draw(ValueIndicator(value_));
    }
  }

  float previous_value_;
  float value_;
  bool full_redraw_;
};

Gauge gauge;

void loop() {
  gauge.setValue((sinf(millis() / 5000.0) + 1) / 2);
  DrawingContext dc(display);
  dc.draw(gauge);
  gauge.setFullRedraw(false);
  delay(20);
}
```

![img62](images/img62.png)

[![img62](images/gauge_video_snapshot.png)](https://youtu.be/kkTuxA0qa34)

Note that:

* Owing to overlays and backgrounds, everything is anti-aliased correctly: the needle over the scale and ticks, and the text over the needle.
* Owing to write-once mode, there is no flicker.
* Owing to clip masks, incremental updates touch few pixels, which yields high frame rate and very smooth animation.

### Using the background-fill optimizer

The `BackgroundFillOptimizer` filter can very significantly improve performance of applications in which large solid areas are repetitively redrawn.

A common scenario is like this: you clear a rectangular area to draw some new content in it, paying no attention to whatever content was there before. But it is often the case that many pixels already had the correct (bacgkround) color and didn't really need to be overwritten. If only you knew which those were, you could have skipped them over, saving the SPI bandwidth.

A canonical solution to achieve that would be to use a framebuffer, keeping a copy of each pixel in RAM. The problem with framebuffers, however, is that they eat up a lot of RAM (e.g. 480x320x2 = 307 KB for a ILI9486 display).

`BackgroundFillOptimizer` is akin to a reduced-resolution framebuffer, using only 1/4 of a bit per pixel, rather than 16 bits per pixel (rounding up to 4.8 KB for the same ILI9486 display). Specifically, the filter assigns 4 bits to each 4x4 rectangle, and uses it to encode whether that rectangle is solid-filled with one of the predefined 15 'fill' colors (which you need to specify when you instantiate the filter). These data are used to optimize out rectangle fills using these predefined colors. Only those 4x4 sub-rectangles are drawn that are not already solid-filled with that given color.

The most natural way to use `BackgroundFillOptimizer` is to put it directly in front of a device driver, as in the following example:

```cpp

// ...

#include "roo_display/filter/background_fill_optimizer.h"
#include "roo_display/driver/ili9341.h"

using namespace roo_display;

Ili9341spi<5, 17, 27> display_device(Orientation().rotateLeft());
BackgroundFillOptimizerDevice fill_optimizer(display_device);

Display display(fill_optimizer);

void setup() {
  // ...

  // For simplicity, only using 1-color palette. (You can use up to 15 colors).
  fill_optimizer.setPalette({color::White});
  display.init(color::White);
}

void loop() {
  DrawingContext dc(display);
  dc.clear();
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          rand() % display.width(), rand() % display.height());
  delay(5);
}
```

If you run this example, you will notice that the frame rate is so high that it is hard to believe that you're looking at a single glyph at any given time. (Increase the delay if you need to convince yourself of that.)

### Adding new device drivers

TL;DR: there are a lot of really similar devices around. Try the age-old technique first: if you see a device similar to yours that is already supported, try tweaking its existing driver.

#### DisplayDevice

A display driver is represented by the abstract `DisplayDevice` class. To implement a driver, you need to implement the pure virtual methods in this class:

* `setAddress()`: set a rectangular window that will be filled by subsequent calls to `write`;
* `write()`: writes to the subsequent pixels in the address window;
* `writePixels()`: draws the specifed batch of pixels;
* `fillPixels()`: draws the specified batch of pixels of the same color;
* `writeRects()`: draws the specified batch of rectangles;
* `fillRects()`: draws the specified batch of rectangles of the same color.

Additionally, it is usually necessary to override these virtual methods as well:

* `init()`: initializes the display;
* `begin()`: starts a write transaction;
* `end()`: ends a write transaction;
* `orientationUpdated()`: applies the specified screen orientation;
* `setBgColorHint()`: set the specified color as the default background.

All colors are expressed in the 32-bit ARGB. The driver needs to perform the conversion to the hardware color mode.

The `setAddress()` and `{write/fill}x{Pixels/Rects}` methods accept a `BlendingMode` argument. Most blending modes expect the driver to mix the provided color with the pre-existing content, or, if it is not supported (which is the common case), over the color specified by `setBgColorHint()`. Drivers should use `ApplyBlending()` and related routines from `roo_display/color/blending.h` to implement that.

If a destination color is non-opaque and the driver does not support the 'alpha' channel, the driver should simply ignore the alpha and treat the color as opaque.

#### The AddrWindowDevice template class

Many display devices are internally implemented using the same underlying paradigm, in which all drawing is essentially reduced to `setAddress()` and `write()`. The AddrWindowDevice helper base class provides a common implementation of this paradigm. The class is parameterized using a `Target` class. Essentially, instead of implementing the `DisplayDevice`, you now need to implement a `Target`. The number of methods to implement there is actually larger, but they are simpler. You no longer need to worry about rectangles, pixels, and ARGB conversion; instead, you need to implement basic primitives to set the address window in the X/Y direction, and to write raw pixel data in the native format (often RGB565). Additionally, the base class provides an optimized baseline implementation of `writePixels` and `fillPixels`, detecting neighboring pixels in a batch and minimizing the number of calls to 'set window' methods. This optimization can considerably reduce communication volume and thus significantly improve pixel drawing performance.

The implementation of most of the existing drivers, e.g. ILI9486, ILI9488, ILI9431, ST7789, and ST7735, use `AddWindowDevice` as the base class. They include an implementation of the appropriate `Target`, plus some typedefs to put it together with the `AddWindowDevice`. The `Target` implements the core device-specific part, such as the init sequence and the commands to set the window and write the data.

> Note that template parameterization is used rather than inheritance, to facilitate inlining and avoid the cost of virtual method calls.

#### Transport abstraction

In order to support hardware-optimized, microcontroller-specific SPI and GPIO implementations, and to provide extensibility towards non-SPI transports, many drivers are templated on `Transport` and `Gpio` classes. The `Transport` provides the following methods:

* `beginReadWriteTransaction()`, `beginWriteOnlyTransaction`, `endTransaction()`,
* `write()`, `write16()`, `write16be()`, `write32()`, `write32be()`, `writeBytes()`, `fill16()`, `fill24be()`, and some asynchronous variants.

SPI drivers are specializations that use the `DefaultSpi` transport by default, which resolves to the platform-optimized implementation if it exists, and falls back to the default Arduino SPI interface otherwise.

> Again, templates are used to maximize inlining and bring the performance close to 'bare metal'.

#### Non-SPI transports

The library supports 8-bit parallel displays using the I80 interface on ESP32-S3, using the `ParallelLcd8bit` transport. You can find an example of its use at `roo_display/products/lilygo/t_display_s3.h`, where it is used as a transport (alternative for SPI) in combination with a 'generic' ST7789 driver.

There is also support for DMA-based parallel displays with ESP32-S3, using a framebuffer stored in PSRAM, via the `ParallelRgb565Buffered` device. (In this case, we're not using the `AddrWindowDevice`; instead, the interface is quite similar to an offscreen device). An example of its use can be found in `roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive_43.h`, which illustrates how it can be configured for other devices.

## Conclusion

That's all, folks!

I hope that you will find this library useful. If you find anything missing, consider filing a feature request, or, better yet, contribute!
