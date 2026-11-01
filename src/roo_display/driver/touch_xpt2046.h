#pragma once
#include <stdlib.h>

#include "roo_display/core/device.h"
#include "roo_display/driver/common/basic_touch.h"
#include "roo_display/driver/common/touch_spi.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/spi.h"
#include "roo_display/hal/spi_settings.h"

namespace roo_display {
static const int kSpiTouchFrequency = 2500000;
using TouchXpt2046SpiSettings =
    SpiSettings<kSpiTouchFrequency, kSpiMsbFirst, kSpiMode0>;

// Reject sample pairs further apart than this raw ADC distance in either axis.
// This filters noise; reducing the distance too far can cause missed touches.
static const int kMaxRawSettlingDistance = 200;

// Bound conversion/pressure settling attempts; reject reads that cannot obtain
// kMinSettledConversions within this budget instead of holding the bus forever.
static const int kMaxConversionAttempts = 100;
static const int kMinSettledConversions = 8;

// Minimum pressure required to count as touch.
static const int kInitialTouchZThreshold = 400;

/// Resistive XPT2046 touch driver with bounded settling and shared SPI scopes.
/// Standard buses use roo_io; custom SPI/GPIO policies retain the legacy path.
template <int pinCS, typename Spi = DefaultSpi, typename Gpio = DefaultGpio>
class TouchXpt2046 : public BasicTouchDevice<1> {
 public:
  /// Borrows the application-initialized bus supplied by @p spi. Keep its
  /// underlying bus alive until the driver is destroyed; initTouch() configures
  /// the device/CS and must precede reads. Existing default/explicit Spi
  /// constructors and custom SPI/GPIO policy types remain supported.
  explicit TouchXpt2046(Spi spi = Spi());

  /// Configures touch transport without initializing or taking over the bus.
  /// Failed setup disables reads; call again to retry after fixing bus setup.
  void initTouch() override { initialized_ = device_.init(); }

 protected:
  int readTouch(TouchPoint* points) override;

 private:
  using Device =
      internal::TouchSpiDevice<pinCS, Spi, Gpio, TouchXpt2046SpiSettings>;
  Device device_;
  bool initialized_ = false;
};

namespace internal {
enum class XptConversion { kUntouched, kTouched, kUnsettled, kError };

// Responses are big-endian words containing a 12-bit ADC sample shifted by 3.
inline uint16_t DecodeXptSample(const uint8_t* bytes) {
  return (static_cast<uint16_t>(bytes[0]) << 8 | bytes[1]) >> 3;
}

// Pipeline three samples per axis and retain the final settled conversion.
template <typename Transaction>
bool ReadXptXY(Transaction& spi, uint16_t& x, uint16_t& y) {
  const uint8_t tx[] = {0xd3, 0,    0xd3, 0,    0xd3, 0, 0x93,
                        0,    0x93, 0,    0x93, 0,    0};
  uint8_t rx[sizeof(tx)];
  if (!spi.transfer(tx, rx, sizeof(tx))) return false;
  x = DecodeXptSample(rx + 5);
  y = DecodeXptSample(rx + 11);
  return x <= 4095 && y <= 4095;
}

// Read both pressure channels in one exchange, rejecting malformed ADC words.
template <typename Transaction>
bool ReadXptZ(Transaction& spi, uint16_t& z) {
  const uint8_t tx[] = {0xb1, 0, 0xc1, 0, 0};
  uint8_t rx[sizeof(tx)];
  if (!spi.transfer(tx, rx, sizeof(tx))) return false;
  const uint16_t z1 = DecodeXptSample(rx + 1);
  const uint16_t z2 = DecodeXptSample(rx + 3);
  if (z1 > 4095 || z2 > 4095) return false;
  z = 4095 + z1 - z2;
  return true;
}

// Require stable pressure and a nearby pair of coordinate samples. Every loop
// has a finite budget, including a controller that never settles.
template <typename Transaction>
XptConversion ReadXptConversion(Transaction& spi, uint16_t threshold,
                                uint16_t& x, uint16_t& y, uint16_t& z) {
  uint16_t z1 = 1;
  uint16_t z2 = 0;
  for (int i = 0; i < kMaxConversionAttempts && z1 > z2; ++i) {
    z2 = z1;
    if (!ReadXptZ(spi, z1)) return XptConversion::kError;
  }
  if (z1 > z2) return XptConversion::kUnsettled;
  if (z1 <= threshold) return XptConversion::kUntouched;
  uint16_t x1;
  uint16_t y1;
  uint16_t x2;
  uint16_t y2;
  if (!ReadXptXY(spi, x1, y1) || !ReadXptXY(spi, x2, y2)) {
    return XptConversion::kError;
  }
  if (abs(x1 - x2) > kMaxRawSettlingDistance ||
      abs(y1 - y2) > kMaxRawSettlingDistance) {
    return XptConversion::kUnsettled;
  }
  if (!ReadXptZ(spi, z)) return XptConversion::kError;
  if (z <= threshold) return XptConversion::kUnsettled;
  x = (x1 + x2) / 2;
  y = (y1 + y2) / 2;
  return XptConversion::kTouched;
}
}  // namespace internal

template <int pinCS, typename Spi, typename Gpio>
TouchXpt2046<pinCS, Spi, Gpio>::TouchXpt2046(Spi spi)
    : BasicTouchDevice<1>(
          typename BasicTouchDevice<1>::Config{.min_sampling_interval_ms = 5,
                                               .touch_intertia_ms = 30,
                                               .smoothing_factor = 0.8}),
      device_(spi) {}

template <int pinCS, typename Spi, typename Gpio>
int TouchXpt2046<pinCS, Spi, Gpio>::readTouch(TouchPoint* point) {
  if (!initialized_) return 0;
  typename Device::Transaction spi = device_.transaction();
  if (!spi.isOpen()) return 0;
  using internal::ReadXptConversion;
  using internal::XptConversion;
  uint16_t x;
  uint16_t y;
  uint16_t z;
  bool ok = true;
  // Discard initial conversions so the sensor settles.
  for (int i = 0; i < 5; ++i) {
    if (ReadXptConversion(spi, kInitialTouchZThreshold, x, y, z) ==
        XptConversion::kError) {
      ok = false;
      break;
    }
  }
  int settled = 0;
  int count = 0;
  int32_t x_sum = 0;
  int32_t y_sum = 0;
  uint16_t z_max = 0;
  for (int i = 0;
       ok && i < kMaxConversionAttempts && settled < kMinSettledConversions;
       ++i) {
    const XptConversion result =
        ReadXptConversion(spi, kInitialTouchZThreshold, x, y, z);
    if (result == XptConversion::kError) {
      ok = false;
      break;
    }
    if (result == XptConversion::kUnsettled) continue;
    ++settled;
    if (result == XptConversion::kTouched) {
      ++count;
      x_sum += x;
      y_sum += y;
      if (z_max < z) z_max = z;
    }
  }
  // Re-enable PENIRQ while CS is still low. Attempt protocol cleanup even
  // after an exchange error, then always release CS and the bus reservation.
  const uint8_t power_down[] = {0x82, 0, 0};
  const bool powered_down =
      spi.transfer(power_down, nullptr, sizeof(power_down));
  const bool closed = spi.close();
  if (!ok || !powered_down || !closed || settled < kMinSettledConversions ||
      count == 0) {
    return 0;
  }
  point->id = 0;
  point->x = 4095 - x_sum / count;
  point->y = 4095 - y_sum / count;
  point->z = z_max;
  return 1;
}
}  // namespace roo_display
