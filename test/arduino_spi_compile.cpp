#include "roo_display/hal/arduino/spi.h"
#include "roo_display/hal/spi_settings.h"

// Verifies both real Arduino frontend APIs can instantiate every write path.
void CompileArduinoSpi() {
  roo_display::ArduinoSpi spi;
  spi.init();
  using Settings = roo_display::SpiSettings<1000000, roo_display::kSpiMsbFirst,
                                            roo_display::kSpiMode0>;
  roo_display::ArduinoSpiDevice<Settings> device(spi);
  const roo::byte data[2] = {roo::byte{0x12}, roo::byte{0x34}};
  device.beginReadWriteTransaction();
  device.write(0x12);
  device.write16(0x1234);
  device.write16x2(1, 2);
  device.writeBytes(data, 2);
  device.fill16(data, 2);
  device.fill16once(data, 2);
  device.endTransaction();
}
