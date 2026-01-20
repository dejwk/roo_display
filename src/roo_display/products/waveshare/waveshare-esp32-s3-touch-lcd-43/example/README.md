# Waveshare ESP32-S3-Touch-LCD-4.3 Test Sketch

Basic test application for the Waveshare ESP32-S3-Touch-LCD-4.3 display using the roo_display library.

## Hardware overview

- Waveshare ESP32-S3-Touch-LCD-4.3
  - 800x480 RGB parallel display
  - GT911 touch controller
  - CH422G I/O expander
  - ESP32-S3 with PSRAM

## Features

- Display initialization and configuration test
- Touch controller functionality test
- Six geometric shapes demonstrating drawing capabilities
- Touch coordinate logging to serial console
- Touch visualization with yellow markers
- Reset button for device restart
- Memory information display

## Serial Output

The sketch provides some debugging information:
- Display resolution
- Free heap and PSRAM
- Touch coordinates with timestamps
- Time between consecutive touches
- Screen region detection

## Usage

1. Upload sketch to ESP32-S3
2. Open serial monitor (115200 baud)
3. Touch the screen to see coordinate output
4. Touch reset button (top-right) to restart device

6. Display buffer setup

## License

Include your license information here.
