#pragma once
#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <utility>

#include "roo_backport/byte.h"
#include "roo_display/hal/gpio.h"
#include "roo_io/spi/spi.h"

namespace roo_display {
namespace internal {

// Third-party SPI/GPIO policies keep their old protocol. Standard buses expose
// a borrowed roo_io handle and use the shared, error-aware implementation.
template <typename Spi, typename Gpio, typename = void>
struct UseSharedTouchSpi : std::false_type {};

template <typename Spi, typename Gpio>
struct UseSharedTouchSpi<
    Spi, Gpio, std::void_t<decltype(std::declval<Spi&>().busHandle())>>
    : std::is_same<Gpio, DefaultGpio> {};

/// Retains the original SPI/GPIO policy protocol for existing custom drivers.
template <int pinCS, typename Device, typename Gpio>
class LegacyTouchTransaction {
 public:
  /// Opens @p device using its existing transaction and GPIO policy.
  explicit LegacyTouchTransaction(Device& device) : device_(&device) {
    device_->beginReadWriteTransaction();
    Gpio::template setLow<pinCS>();
  }

  /// Releases the original device/CS transaction.
  ~LegacyTouchTransaction() { close(); }

  /// A legacy reservation has one owner.
  LegacyTouchTransaction(const LegacyTouchTransaction&) = delete;

  /// Legacy reservations cannot be assigned.
  LegacyTouchTransaction& operator=(const LegacyTouchTransaction&) = delete;

  /// Transfers ownership of the current legacy reservation.
  LegacyTouchTransaction(LegacyTouchTransaction&& other) noexcept
      : device_(other.device_) {
    other.device_ = nullptr;
  }

  /// Reports whether this scope still owns its reservation.
  bool isOpen() const { return device_ != nullptr; }

  /// Exchanges bytes in wire order; the legacy API has no transport status.
  bool transfer(const uint8_t* tx, uint8_t* rx, size_t len) const {
    if (device_ == nullptr || len == 0 || (tx == nullptr && rx == nullptr)) {
      return false;
    }
    for (size_t i = 0; i < len; ++i) {
      const uint8_t value = static_cast<uint8_t>(
          device_->transfer(static_cast<roo::byte>(tx == nullptr ? 0 : tx[i])));
      if (rx != nullptr) rx[i] = value;
    }
    return true;
  }

  /// Releases CS and the bus; repeated calls are successful no-ops.
  bool close() {
    if (device_ == nullptr) return true;
    Gpio::template setHigh<pinCS>();
    device_->endTransaction();
    device_ = nullptr;
    return true;
  }

 private:
  Device* device_;
};

/// Adapts a custom pre-existing SPI/GPIO policy to the touch buffer interface.
template <int pinCS, typename Spi, typename Gpio, typename Settings,
          bool shared = UseSharedTouchSpi<Spi, Gpio>::value>
class TouchSpiDevice {
 public:
  using Device = typename Spi::template Device<Settings>;
  using Transaction = LegacyTouchTransaction<pinCS, Device, Gpio>;

  /// Borrows the underlying bus supplied by @p spi.
  explicit TouchSpiDevice(Spi& spi) : device_(spi) {}

  /// Configures/registers the device; the caller initializes the bus.
  bool init() {
    Gpio::setOutput(pinCS);
    Gpio::template setHigh<pinCS>();
    device_.init();
    return true;
  }

  /// Opens one touch exchange with CS held until scope cleanup.
  Transaction transaction() { return Transaction(device_); }

 private:
  Device device_;
};

/// Uses roo_io for standard buses while borrowing the application's bus.
template <int pinCS, typename Spi, typename Gpio, typename Settings>
class TouchSpiDevice<pinCS, Spi, Gpio, Settings, true> {
 public:
  using Transaction = roo_io::SpiTransaction;

  /// Borrows the underlying bus supplied by @p spi.
  explicit TouchSpiDevice(Spi& spi)
      : device_(spi.busHandle(), pinCS, Settings::clock, Settings::data_mode) {}

  /// Configures/registers the device; the caller initializes the bus.
  bool init() { return device_.init(); }

  /// Opens one touch exchange with CS held until scope cleanup.
  Transaction transaction() { return device_.transaction(); }

 private:
  roo_io::SpiSlaveDevice device_;
};
}  // namespace internal
}  // namespace roo_display
