#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "roo_backport/byte.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_io/data/byte_order.h"
#include "roo_io/memory/fill.h"

namespace roo_display {
namespace esp32 {

// ESP-IDF SPI transport used by roo_testing. The production ESP32 transport
// accesses peripheral registers directly, which the host emulator deliberately
// does not model. This implementation stays on ESP-IDF's public SPI master API,
// whose behavior roo_testing shims at the bus/device boundary.
template <uint8_t spi_port, typename SpiSettings>
class IdfEmulatorSpiDevice;

template <uint8_t spi_port>
class IdfEmulatorSpi {
 public:
  template <typename SpiSettings>
  using Device = IdfEmulatorSpiDevice<spi_port, SpiSettings>;

  IdfEmulatorSpi() : host_(static_cast<spi_host_device_t>(spi_port - 1)) {}

  void init() { init(-1, -1, -1); }

  void init(int sck, int miso, int mosi) {
    spi_bus_config_t config = {};
    config.mosi_io_num = mosi;
    config.miso_io_num = miso;
    config.sclk_io_num = sck;
    config.quadwp_io_num = -1;
    config.quadhd_io_num = -1;
    config.max_transfer_sz = 4096;
    ESP_ERROR_CHECK(spi_bus_initialize(host_, &config, SPI_DMA_CH_AUTO));
  }

  void deinit() {
    esp_err_t err = spi_bus_free(host_);
    if (err != ESP_ERR_INVALID_STATE) {
      ESP_ERROR_CHECK(err);
    }
  }

 private:
  template <uint8_t, typename>
  friend class IdfEmulatorSpiDevice;

  spi_host_device_t host_;
};

template <uint8_t spi_port, typename SpiSettings>
class IdfEmulatorSpiDevice {
 public:
  explicit IdfEmulatorSpiDevice(IdfEmulatorSpi<spi_port>& spi)
      : host_(spi.host_) {}

  IdfEmulatorSpiDevice(const IdfEmulatorSpiDevice&) = delete;
  IdfEmulatorSpiDevice& operator=(const IdfEmulatorSpiDevice&) = delete;

  IdfEmulatorSpiDevice(IdfEmulatorSpiDevice&& other) noexcept
      : host_(other.host_), device_(other.device_) {
    other.device_ = nullptr;
  }

  ~IdfEmulatorSpiDevice() {
    if (device_ != nullptr) {
      ESP_ERROR_CHECK(spi_bus_remove_device(device_));
    }
  }

  void init() {
    spi_device_interface_config_t config = {};
    config.mode = SpiSettings::data_mode;
    config.clock_speed_hz = SpiSettings::clock;
    config.spics_io_num = -1;
    config.flags =
        SpiSettings::bit_order == kSpiLsbFirst ? SPI_DEVICE_BIT_LSBFIRST : 0;
    config.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(host_, &config, &device_));
  }

  void beginReadWriteTransaction() {
    ESP_ERROR_CHECK(
        spi_device_acquire_bus(device_, static_cast<uint32_t>(portMAX_DELAY)));
  }

  void beginWriteOnlyTransaction() { beginReadWriteTransaction(); }

  void endTransaction() { spi_device_release_bus(device_); }

  void flush() {}

  void write(uint8_t data) { transmit(&data, sizeof(data)); }

  void write16(uint16_t data) {
    data = roo_io::htobe(data);
    transmit(&data, sizeof(data));
  }

  void write16x2(uint16_t a, uint16_t b) {
    uint16_t data[] = {roo_io::htobe(a), roo_io::htobe(b)};
    transmit(data, sizeof(data));
  }

  void writeBytes(const roo::byte* data, uint32_t len) { transmit(data, len); }

  void fill16(const roo::byte* data, uint32_t repetitions) {
    fill<2, 64>(data, repetitions);
  }

  void fill24(const roo::byte* data, uint32_t repetitions) {
    fill<3, 96>(data, repetitions);
  }

  void fill16once(const roo::byte* data, uint32_t repetitions) {
    fill16(data, repetitions);
  }

  void fill24once(const roo::byte* data, uint32_t repetitions) {
    fill24(data, repetitions);
  }

  void asyncBlit(const roo::byte* data, size_t row_stride_bytes,
                 size_t row_bytes, size_t row_count) {
    if (data == nullptr || row_bytes == 0 || row_count == 0) return;
    if (row_stride_bytes == row_bytes) {
      transmit(data, row_bytes * row_count);
      return;
    }
    for (size_t row = 0; row < row_count; ++row) {
      transmit(data, row_bytes);
      data += row_stride_bytes;
    }
  }

  roo::byte transfer(roo::byte data) {
    uint8_t tx = static_cast<uint8_t>(data);
    uint8_t rx = 0;
    transmit(&tx, sizeof(tx), &rx);
    return static_cast<roo::byte>(rx);
  }

  uint16_t transfer16(uint16_t data) {
    uint16_t tx = roo_io::htobe(data);
    uint16_t rx = 0;
    transmit(&tx, sizeof(tx), &rx);
    return roo_io::betoh(rx);
  }

 private:
  template <size_t pattern_size, size_t buffer_size>
  void fill(const roo::byte* data, uint32_t repetitions) {
    roo::byte buffer[buffer_size];
    constexpr uint32_t kPatternsPerBuffer = buffer_size / pattern_size;
    roo_io::PatternFill<pattern_size>(buffer, kPatternsPerBuffer, data);
    while (repetitions >= kPatternsPerBuffer) {
      transmit(buffer, sizeof(buffer));
      repetitions -= kPatternsPerBuffer;
    }
    transmit(buffer, repetitions * pattern_size);
  }

  void transmit(const void* tx, size_t bytes, void* rx = nullptr) {
    if (bytes == 0) return;
    spi_transaction_t transaction = {};
    transaction.length = bytes * 8;
    transaction.rxlength = rx == nullptr ? 0 : bytes * 8;
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;
    ESP_ERROR_CHECK(spi_device_polling_transmit(device_, &transaction));
  }

  spi_host_device_t host_;
  spi_device_handle_t device_ = nullptr;
};

#if CONFIG_IDF_TARGET_ESP32
using IdfEmulatorFspi = IdfEmulatorSpi<1>;
using IdfEmulatorHspi = IdfEmulatorSpi<2>;
using IdfEmulatorVspi = IdfEmulatorSpi<3>;
#else
using IdfEmulatorFspi = IdfEmulatorSpi<2>;
#endif

}  // namespace esp32
}  // namespace roo_display
