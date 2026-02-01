#pragma once

#if defined(ESP_PLATFORM)

#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "roo_backport.h"
#include "roo_backport/byte.h"

namespace roo_display {

// I2C master bus reference, capturing the specified i2c_port.
class Esp32I2cMasterBusHandle {
 public:
  // Creates the I2C master bus handle that references the specified esp-idf bus
  // handle.
  Esp32I2cMasterBusHandle(i2c_port_num_t i2c_port = 0)
      : bus_handle_(0), frequency_(0), i2c_port_(i2c_port) {}

  // Should not be called after resolveBusHandle().
  void init(int sda, int scl, uint32_t frequency = 0) {
    i2c_master_bus_config_t config = {
        .i2c_port = i2c_port_,
        .sda_io_num = (gpio_num_t)sda,
        .scl_io_num = (gpio_num_t)scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus_handle_));
    frequency_ = frequency;
  }

 private:
  friend class Esp32I2cSlaveDevice;

  i2c_master_bus_handle_t resolveBusHandle() {
    if (bus_handle_ == 0) {
      ESP_ERROR_CHECK(i2c_master_get_bus_handle(i2c_port_, &bus_handle_));
    }
    return bus_handle_;
  }

  i2c_port_num_t i2c_port_;
  i2c_master_bus_handle_t bus_handle_;
  uint32_t frequency_;
};

class Esp32I2cSlaveDevice {
 public:
  Esp32I2cSlaveDevice(Esp32I2cMasterBusHandle& bus_handle,
                      uint8_t slave_address)
      : bus_handle_(bus_handle), slave_address_(slave_address) {}

  void init() {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = slave_address_,
        .scl_speed_hz =
            bus_handle_.frequency_ > 0 ? bus_handle_.frequency_ : 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle_.resolveBusHandle(),
                                              &dev_cfg, &dev_handle_));
  }

  bool transmit(const roo::byte* data, size_t len) {
    return i2c_master_transmit(dev_handle_, (const uint8_t*)data, len, -1) ==
           ESP_OK;
  }

  size_t receive(roo::byte* data, size_t len) {
    if (i2c_master_receive(dev_handle_, (uint8_t*)data, len, -1) != ESP_OK) {
      return 0;
    }
    return len;
  }

 private:
  Esp32I2cMasterBusHandle& bus_handle_;
  uint8_t slave_address_;
  i2c_master_dev_handle_t dev_handle_;
};

}  // namespace roo_display

#endif  // defined(ESP_PLATFORM)