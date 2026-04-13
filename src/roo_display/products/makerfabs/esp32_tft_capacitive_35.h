#pragma once

// https://www.makerfabs.com/esp32-3-5-inch-tft-touch-capacitive-with-camera.html
// Maker: MakerFabs
// Product Code: ESPTFT35CA

#include "roo_display/hal/config.h"
#if !defined(ESP_PLATFORM) || !CONFIG_IDF_TARGET_ESP32
#warning "This driver requires ESP32 SoC."
#else

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/hal/spi.h"
#include "roo_display/products/combo_device.h"

#if defined(ARDUINO)
#include "roo_io/fs/esp32/arduino/sdspi.h"
#else
#include "roo_io/fs/esp32/esp-idf/sdspi.h"
#endif

namespace roo_display::products::makerfabs {

/// Makerfabs ESP32 3.5" TFT capacitive touch device.
///
/// NOTE: this device has a design flaw by using the same SPI bus for the
/// display and SD card, without a hardware switch to disconnect the SD card
/// during display updates. A newly inserted SD card powers up in SD-native
/// mode, which can cause interference with the display by driving the MOSI
/// line, even when the SD card's CS line is kept high. To work around this,
/// the driver attempts to switch the SD card to SPI mode at startup, by
/// calling checkMediaPresence() on the SD SPI driver. This only works, however,
/// until the card is removed and reinserted, since a newly inserted card powers
/// up in SD-native mode again.
///
/// If you're using this device, it is recommended not to swap SD card while the
/// display is active. If it is unavoidable, you can mitigate by calling
/// checkMediaPresence() on the SD SPI driver after each card insertion (perhaps
/// periodically polling for card presence), which will re-prime the card into
/// SPI mode.
class Esp32TftCapacitive35 : public ComboDevice {
 public:
#if defined(ARDUINO)
  // You can use non-default I2C interface by providing a custom instance of
  // TwoWire as the i2c parameter.
  Esp32TftCapacitive35(
      Orientation orientation = Orientation(),
      roo_display::I2cMasterBusHandle i2c = roo_display::I2cMasterBusHandle())
      : hspi_(HSPI), spi_(hspi_), i2c_(i2c), display_(spi_), touch_(i2c_) {
    display_.setOrientation(orientation);
  }
#else
  // You can use non-default I2C interface by providing a specific
  // i2c_port_num_t as the i2c parameter.
  Esp32TftCapacitive35(
      Orientation orientation = Orientation(),
      roo_display::I2cMasterBusHandle i2c = roo_display::I2cMasterBusHandle())
      : spi_(), i2c_(i2c), display_(spi_), touch_(i2c_) {
    display_.setOrientation(orientation);
  }
#endif

  /// Initialize SPI/I2C transport and SD card CS pin.
  ///
  /// If prime_sd_spi_mode is true (Arduino only), performs a quick SD init at
  /// low speed and immediately releases the card, so it is explicitly switched
  /// to SPI mode before display traffic starts.
  void initTransport(bool prime_sd_spi_mode = true) {
    spi_.init(14, 12, 13);
    i2c_.init(26, 27);
    // In case the SD card is unused, its CS pin should be held HIGH, so that
    // doesn't interfere with the display.
    DefaultGpio::setOutput(pin_sd_cs());
    DefaultGpio::setHigh(pin_sd_cs());

    if (prime_sd_spi_mode) {
      primeSdSpiMode();
    }
  }

  /// Re-primes the SD card into SPI mode.
  ///
  /// Call this after physical card reinsertions, since a newly inserted card
  /// powers up in SD-native mode again.
  void primeSdSpiMode() {
#if defined(ARDUINO)
    roo_io::SD_SPI.setSPI(hspi_);
    roo_io::SD_SPI.setCsPin(pin_sd_cs());
    roo_io::SD_SPI.setFrequency(20000000);
    roo_io::SD_SPI.checkMediaPresence();
#else
    roo_io::SDSPI.setSpiHost(SPI2_HOST);
    roo_io::SDSPI.setCsPin(pin_sd_cs());
    roo_io::SDSPI.setFrequency(20000000);
    roo_io::SDSPI.checkMediaPresence();
#endif
  }

  /// Return display device.
  DisplayDevice& display() override { return display_; }

  /// Return touch device.
  TouchDevice* touch() override { return &touch_; }

  roo_io::BaseEsp32VfsFilesystem& sd() {
#if defined(ARDUINO)
    return roo_io::SD_SPI;
#else
    return roo_io::SDSPI;
#endif
  }

  /// Return touch calibration.
  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 20, 309, 454, Orientation::RightDown());
  }

  /// SPI SCK pin.
  constexpr int8_t pin_sck() const { return 14; }
  /// SPI MISO pin.
  constexpr int8_t pin_miso() const { return 12; }
  /// SPI MOSI pin.
  constexpr int8_t pin_mosi() const { return 13; }
  /// I2C SDA pin.
  constexpr int8_t pin_sda() const { return 26; }
  /// I2C SCL pin.
  constexpr int8_t pin_scl() const { return 27; }

  /// SD card CS pin.
  constexpr int8_t pin_sd_cs() const { return 4; }

#if defined(ARDUINO)
  decltype(SPI)& spi() { return hspi_; }
#endif

 private:
#if defined(ARDUINO)
  decltype(SPI) hspi_;
#endif

  roo_display::esp32::Hspi spi_;
  roo_display::I2cMasterBusHandle i2c_;
  roo_display::Ili9488spi<15, 33, 26, esp32::Hspi> display_;
  roo_display::TouchFt6x36 touch_;
};

}  // namespace roo_display::products::makerfabs

#endif  // !defined(ESP_PLATFORM) || !CONFIG_IDF_TARGET_ESP32
