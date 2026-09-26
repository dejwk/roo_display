# roo_display 3.3.1

- Upgrade dependencies to `roo_collections` 1.4.8, `roo_io` 2.4.0, `roo_icons` 1.2.5, `roo_fonts_basic` 1.0.5, and `roo_testing` 2.3.0.
- Add explicit icon and font dependencies to Arduino and PlatformIO package metadata.
- Automatically select the ESP-IDF configuration when running ESP-IDF examples through the Bazel wrapper.
- Refresh contributor guidance and fix stale documentation references.

---

# roo_display 3.3.0

- Migrated FT6x36 and GT911 touch drivers to the shared `roo_io` I2C API, retaining compatibility aliases in existing headers. ESP-IDF device registrations are now released when drivers are destroyed; externally owned buses must outlive their drivers.
- Extended FT6x36 touch-coordinate tests to Arduino and ESP-IDF backends.
- Updated dependencies to `roo_io` 2.3.0, `roo_backport` 1.2.4, `roo_collections` 1.4.7, `roo_testing` 2.1.2, `roo_icons` 1.2.4, and `roo_fonts_basic` 1.0.4. Added icons and basic fonts to Arduino and PlatformIO dependency declarations.
- Updated Bazel rules and shared CI tooling.
- Added consolidated release history and documented a deferred glyph-stream optimization.

---

# [roo_display 3.2.2](https://github.com/dejwk/roo_display/releases/tag/3.2.2)

Published 2026-08-30.

This maintenance release updates the `roo_testing` dependency to 2.1.1, improving ESP-IDF host emulation:

- Fixed emulated delays and automatic system-time progression.
- Fixed FreeRTOS shutdown crashes after successful tests.
- Improved AddressSanitizer stability while retaining emulated task stacks.
- Returning from `app_main()` now keeps the emulator running, matching ESP-IDF behavior.
- Bazel test failures now print their logs inline.

There are no public `roo_display` API changes in this release.

**Full changelog:** https://github.com/dejwk/roo_display/compare/3.2.1...3.2.2

---

# [roo_display 3.2.1](https://github.com/dejwk/roo_display/releases/tag/3.2.1)

Published 2026-08-30.

### Highlights

- Added a new `multiple_displays` example showing an ILI9341 and ST7789 sharing one SPI bus.
- Added ESP-IDF host-emulation support and an ILI9341 ESP-IDF example, with SPI display smoke-test coverage.
- Made all examples runnable through the emulator and documented Arduino/ESP-IDF host-build commands.
- Updated dependencies, including `roo_testing` 2.1.0.

### Fixes

- Avoid compiling the ESP32-S3-specific driver on unsupported ESP32 targets.
- Resolved compilation warnings across examples.
- Removed an unused transformed-raster interpolator.
- Improved SPI transport compilation for displays without DC or reset pins.

### Tooling

- Modernized CI, GitHub Pages, Dependabot, Bazelisk, and AddressSanitizer configuration.

**Full Changelog:** https://github.com/dejwk/roo_display/compare/3.2.0...3.2.1

---

# [roo_display 3.2.0](https://github.com/dejwk/roo_display/releases/tag/3.2.0)

Published 2026-08-07.

New features
* Added support for basic letter tracking (i.e. additional spacing between glyphs)
* Added experimental support for GC9a01 displays
* Re-generated builtin fonts to include small subscripts (2-5).
* Added a method to FontMetrics to retrieve the default space width.

Bug fixes
* Fixed rendering artifacts when the default DeviceOutput::fill() is called. The function now re-generates fill buffer before emiting fill chunks, since the device is allowed to modify the buffer.
* Fixed rendering artifacts in BlendingFilter::fill().
* Fixed rendering artifacts in SmoothRoundRect when is clipped and skip() is called.
* Support for IRQ touch on XPT2046: reactivate PENIRQ after TS read by @Gamadril in https://github.com/dejwk/roo_display/pull/19.
* Marked default color constructor as constexpr.


**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.1.2...3.2.0

---

# [roo_display 3.1.2](https://github.com/dejwk/roo_display/releases/tag/3.1.2)

Published 2026-06-04.

New features:
* Added support for drawing smooth round rectangles with unequal corner radii.

Bug fixes:
* Fixed thread stack overflow issues in the PNG decoder. (thanks Gamadril for the contribution).
* Multiple fixes for smooth round rectangles:
  * Incorrect clipping,
  * Rendering of rounded rectangles with very thin outlines,
  * Rendering or thick rounded rectangles whose inner radius degrades to zero,
  * Interpolation near boundary.
* Pulled the MemoryResouce fix in roo_io.

Performance improvements in dynamic composition:
* Added a streamable version of smooth round rect (which also covers circles, filled circles, and 'tracks'). Significant speedup, ~2.5x, when using StreamableStack compositions.
* Rasterizable stack: drop to streamable stack when a stream is requested, taking advantage of that former optimization.
* Blender: several optimizations, focused on detecting long runs, minimizing the calls to alpha-blending code.
* RectUnion: significantly reduced SPI calls and CPU overhead. Observed 2-3x speedup for medium-to-large writes.
* RasterizableStack: skip partially overlapping layers that happen to be purely transparent.
* Various small optimizations in the StreamableStack.

Tests:
* Added coverage for the PNG and JPEG decoders,
* Improved debuggability of background_fill_optimizer

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.1.1...3.1.2

---

# [roo_display 3.1.1](https://github.com/dejwk/roo_display/releases/tag/3.1.1)

Published 2026-04-25.

This release brings significant performance improvements in a few usage scenarios (particularly when using offscreens, rasterizable overlays and stacks, the 'turbo' mode, and the framebuffer-based devices, e.g. parallel RGB565 with ESP32-S3), and some new experimental features.

## Performance

Direct raw rectangle copy:
* Added dedicated fast functions for direct-copying raw-formatted rectangles from RAM (or flash memory), and, for devices using frame buffer, also for copying ractangular areas of the display.

DMA (experimental):
* Added a directDrawRectAsync, which uses DMA when possible (e.g. for ESP32S3-based RGB displays, or with SPI if enabled)
* Added optional support for DMA and async drawing to the SPI drivers on ESP32 chips. (Can be enabled by setting the flag roo_display_esp32_spi_async; see [hal/esp32/spi_config.h](https://github.com/dejwk/roo_display/blob/master/src/roo_display/hal/esp32/spi_config.h) ).

Async drawing may often have slightly worse performance than regular sync/blocking (although the 'DMA pipeline' mode comes very close and is sometimes faster, due to better pipelining). The real strength of async modes (and DMA in particular) is that they offload the main CPU, allowing it do perform other work, which can make your application more responsive or power-efficient.

Significant improvements to the 'turbo' mode (background-fill-optimizer):
* faster detection of unchanged buffers (-30% in text_scroll benchmark) 
* uniform color areas in the raster are detected, thus improving cache hit ratio,
* implemented the raw-copy overrides (directDrawRect, directDrawRectAsync, blitCopy), delegating to the underlying device but only updating the affected screen area.

Significant improvements in the Offscreen performance:
* Implemented bulk write and bulk fill operators, which improves performance of large fills spectacularly (random_fill64 benchmark shows 4.75x improvement - yes, nearly five times faster).
* implemented support for very fast scrim (writing any color with 50% opacity on top of existing content) on offscreens and framebuffer-based devices. Integer arithmetics, no alpha-blending -> several times faster.

Faster rasterizables that have sub-areas of uniform color:
* Introduced a new method in the Rasterizable interface to quickly test for uniform color areas;
* Implemented the new method in smooth shapes - which should now perform better e.g. when used as overlays or when stacked together using RasterizableStack.

Other improvements:
* copying rectangles from offscreens to devices, when the color mode is the same, is now automatically performed used raw data copy, which is 2-3 times faster than before;
* improved string rendering performance on solid background a bit, by pre-calculating 16-element color palettes and avoiding a lot of alpha-blending;
* implemented fast-path in clip_include_rects when rectangles are drawn that don't intersect the mask (common e.g. for small areas like glyphs);
* optimization in the blender to fast-path areas with uniform color. 

With these improvements, roo_display is a very good choice as a rendering engine for lvgl. In particular, when using slow SPI displays, the boost provided by the 'turbo' mode in roo_display is probably currently unparalleled. See [roo_display_lvgl](https://github.com/dejwk/roo_display_lvgl), and experimental library aiming to make that easy, by automating the setup for everything on the basis of a roo_display::Device.

(When using roo_display with lgvl for parallel RGB devices using PSRAM, enable the DMA pipeline mode for best performance).

## Bug fixes
* implemented a workaround a pesky SD/display conflict on the ILI9488-based Makerfabs display (which causes display corruption due to interference from the SD reader),
* made it possible to call init() multiple times in case of the parallel RGB driver,

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.0.4...3.1.1

---

# [roo_display 3.0.4](https://github.com/dejwk/roo_display/releases/tag/3.0.4)

Published 2026-03-07.

* Fixing compilation issues in Arduino IDE (related to inclusion of roo_backport);
* Some new font-related low-level public APIs: drawing single glyph, and getting kerning information for a pair of glyphs.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.0.3...3.0.4

---

# [roo_display 3.0.3](https://github.com/dejwk/roo_display/releases/tag/3.0.3)

Published 2026-02-28.

Finally, new release ready for prime time!

Please see the 3.0.0 - 3.0.2 release notes for major new changes since 2.0. This version brings some additional changes:

* added support for perspective transformations, with a cool new example;
* added 'rainbow text' examples, and the discussion in the programming guide. (The funtionality has been there before, but not easy to discover).
* fixed a bug in the gt911 driver, not correctly selecting the right I2C address. (Kudos @p43lz3r for finding the bug!)
* updates in the examples and benchmarks.
* the enum APIs have been migrated to the new 'enum class' style, for stylistic consistency across the roo library suite. You may see deprecation warnings if you used those enums directly. Nothing should break, hopefully.
* new font collection available as a companion library: http://github.com/dejwk/roo_fonts_material.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.0.2...3.0.3

---

# [roo_display 3.0.2](https://github.com/dejwk/roo_display/releases/tag/3.0.2)

Published 2026-02-26. Pre-release.

* Finalizing the API around enums;
* Updated the programming guide, correcting some inaccuracies and mentioning the display.enableTurbo() option.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.0.1...3.0.2

---

# [roo_display 3.0.1](https://github.com/dejwk/roo_display/releases/tag/3.0.1)

Published 2026-02-25. Pre-release.

Minor fixes and cleanups; making the examples work, refactoring testing setup (BUILD).

**Full Changelog**: https://github.com/dejwk/roo_display/compare/3.0.0...3.0.1

---

# [roo_display 3.0.0](https://github.com/dejwk/roo_display/releases/tag/3.0.0)

Published 2026-02-25. Pre-release.

This is a major feature release, with lots of new and exciting functionality and improved performance.

## New features

* Now works on esp-idf (not just Arduino). Kudos to Daniel Wiese for consulting on and helping out with this port.
* New device supported: Waveshare ESP32-S3 touch. Thanks p43lz3r for the contibution!
* Significantly improved font format (borrowing a few good ideas from lgvl). Font now use much less space, particularly kerning tables. The average size reduction ~30%.
* Background-fill-optimizer is now easier to use. Just call display.enableTurbo(). No need to specify palette. It tracks usage of colors on its own.
* API documentation massively revamped, and made doxygen-compliant.

## Performance improvements

* Hardware-optimized SPI drivers for the rest of the ESP32 family (C3, C6, S2, S3).
* Tuned the background fill optimizer.
* Performance optimization in the SPI drivers, to improve CPU/IO pipelining.
* Fonts render a couple percent faster (mainly due to reduced footprint).
* New benchmarks added (text rendering, background fill optimizer, direct draw).
* Added new 'direct draw' functionality to the drivers, allowing pass-through rectangle drawing using driver-native color format, not translated through ARGB.

## Bug fixes

* UB in the touch driver,
* Not calling setOrientation() until the display is actually initialized,
* Improved unit test coverage: added tests for device drivers, background fill optimizer, and some others.

## API changes
 
* IMPORTANT: most font files have been moved to a companion library, http://github.com/dejwk/roo_fonts_basic/. This is motivated by the desire to improve build times, as fonts are the dominant contributor to it. Now only 4 fonts are shipped with the core library: NotoSand_Regular, NotoSans_Bold, NotoSerif_Italic, and NotoSansMono_Regular. To use the remaining fonts, import the companion library.
* Use roo_time::Uptime, rather than possibly overflowing uint32_t, in TouchResult,
* Enums reformatted to kCamelCase for consistency (but aliases left behind for compatibility).
* Refactored low-level SPI APIs.
* Exposing raw color modes from devices and offscreens.
* Made offscreen movable.
* GT911 touch driver now supports IO via port extenders (used e.g. by the Waveshare combo device).
 
## Other

* Cleaned up build warnings.

For now, this release is considered 'pre-release', as I have not yet tested it extensively on various hardware.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/2.3.2...3.0.0

---

# [roo_display 2.3.2](https://github.com/dejwk/roo_display/releases/tag/2.3.2)

Published 2026-01-07.

This release brings a minor reduction of the footprint of small fonts. Previously, glyphs have been always using RLE compression. As it turns out, for some tiny glyphs, the uncompressed representation might be a byte or two smaller than the RLE-compressed representation. Now, these small ghyphs are indeed stored uncompressed.

There's more to be saved there in the future by reducing the size of kerning tables. Stay tuned!


---

# [roo_display 2.3.1](https://github.com/dejwk/roo_display/releases/tag/2.3.1)

Published 2025-10-31.

New features:
* 'Virtual canvases': it is now possible to shift a drawing context when it gets constructed. With pre-existing functionality, it allows creation of drawing contexts that behave as sub-views of the underlying device; e.g. you can split up the screen in two sections, and draw to them using the same code as if you had two separate devices with half the resolution. See the [updated documentation](https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#alignment).
* Updated the 'backlit' interface so that it works properly with ESP32 Adruino v3+.
* Added a color gradient example.
* Added support for Makerfabs ESP32S3 combo at both 800x480 and 1024x600 resolution.
* Better continuous integration.

Fixes:
* GT911 touch driver initialization problems (seen at least on the Makerfabs devices) have been fixed. The driver now performs initialization asynchronously, so that it doesn't block the rest of the application.
* Documentation has been revised, and all examples were updated. Some small compilation issues in the examples have been resolved.
* Removed some compilation warnings.
* Some fixes have been made in the dependency libraries (e.g., roo_io).
* Small fix in the clock rendering example (using Tile to make sure there's complete redraw).
* Fixed a regression in the LilyGo T-Display-S3 display driver.
* Fixes in the st7789 drivers for higher-resolution devices (above 256x256).

**Full Changelog**: https://github.com/dejwk/roo_display/compare/2.2.2...2.3.1

---

# [roo_display 2.2.2](https://github.com/dejwk/roo_display/releases/tag/2.2.2)

Published 2025-10-14.

* Improvements and bug fixes in the dependency library (roo_io).
* Fixed SD card CS pin specification for Makerfabs capacitive display combo.

---

# [roo_display 2.2.1](https://github.com/dejwk/roo_display/releases/tag/2.2.1)

Published 2025-09-19.

Updated dependencies (bugfix in roo_collections).


---

# [roo_display 2.2.0](https://github.com/dejwk/roo_display/releases/tag/2.2.0)

Published 2025-08-10.

Added CI/CD integration (unit tests will now run on push).

Made roo_display a Bazel module, which makes it possible to use in unit tests of depending code, automatically pulling in all the necessary deps.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/2.1.1...2.2.0

---

# [roo_display 2.1.1](https://github.com/dejwk/roo_display/releases/tag/2.1.1)

Published 2025-08-01.

Bugfix: fixing backlit compilation on Arduino 3.x. This makes parallel drivers for ESP32 S3 work under Arduino IDE.

---

# [roo_display 2.1.0](https://github.com/dejwk/roo_display/releases/tag/2.1.0)

Published 2025-07-04.

New drivers:

* ST7796S,
* SD7786S 'black' combo

Fixes:

* Various compilation fixes, particularly with Arduino 3 and the Arduino IDE,
* Performance bug fixed when drawing filled arcs.


---

# [roo_display 2.0.0](https://github.com/dejwk/roo_display/releases/tag/2.0.0)

Published 2024-12-29.

The I/O functionality of the library, namely: reading image and raster data from progmem and files, reading and decoding font data from progmem, reading from and writing to memory behind offscreen buffers and rasters, encoding/decoding UTF-8, endianness support, and string formatting, has been factored out to a separate library, dejwk/roo_io. This change is mostly backwards-compatible. Almost all documented functionality and examples work without changes, except for some advanced features such as drawing JPEG images stored in PROGMEM that might require small changes. Also, applications that depend on undocumented functionality may need some small updates.

This change makes the library more flexible (e.g. easier to support various sources of image data) and secure (e.g. improved UTF-8 decoder that better handles invalid input). The downside of less than 100% backwards compatibility is the main reason for major version increase.

Additionally, this release brings a number of bug fixes, and some small features:
* Fixed a deadlock in the JPEG and PNG decoders when the display and an SD card shared the same SPI bus.
* Fixed JPEG and PNG clipping.
* Support for a 7'' combo display from Makerfabs.
* Compatibility with ESP32C3 and ESP32S3.
* Fix for an offscreen construction from translucent drawable, which previously would not clear background properly.
* Fixed crashes on multi-touch.
* Fixed crashes for parallel DMA displays on ESP32S3.
* Bug fixes in several examples.


---

# [roo_display 1.4.1](https://github.com/dejwk/roo_display/releases/tag/1.4.1)

Published 2024-08-06.

* Added experimental support for dynamically transforming (scaling, shearing, rotating) rasters.
* Minor fixes and improvements.

**Full Changelog**: https://github.com/dejwk/roo_display/compare/1.4.0...1.4.1

---

# [roo_display 1.4.0](https://github.com/dejwk/roo_display/releases/tag/1.4.0)

Published 2024-01-03.

Fixes:

* Many bugfixes in smooth arc rendering. Now works well for corner-cases, and is suitable for animations. Also, tuned its performance somewhat.
* Added missing FS dependency, which could cause trouble in some builds.
* Some tunings in background_fill_optimizer.
* Alignment objects are now comparable for equality.
* Directory rename: for simplicity, roo_smooth_fonts has been renamed to roo_fonts. The old headers are left there for backwards compability.

---

# [roo_display 1.3.0](https://github.com/dejwk/roo_display/releases/tag/1.3.0)

Published 2023-12-04.

* Made compatible with Arduino IDE
* Added examples corresponding to the developer guide

---

# [roo_display 1.1](https://github.com/dejwk/roo_display/releases/tag/1.1)

Published 2023-11-10.

Small bug fixes, as well as rendering performance improvements.

---

# [roo_display 1.0](https://github.com/dejwk/roo_display/releases/tag/1.0)

Published 2023-06-16.

Initial release.

---

