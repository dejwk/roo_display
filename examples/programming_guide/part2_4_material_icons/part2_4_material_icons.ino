// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#using-the-material-icons-collection

// In order to build this example, import the roo_icons library:
// https://github.com/dejwk/roo_icons

#include "Arduino.h"
#include "roo_display.h"
#include "roo_icons.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 4;
static constexpr int kBlPin = 16;

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin, /* ledc channel */ 0);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

#include "roo_display/ui/tile.h"
#include "roo_icons/round/device.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void drawIcon(DrawingContext& dc, const Pictogram& icon, int x, int y,
              Color color) {
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
  drawIcon(dc, ic_round_18_device_airplanemode_inactive(), 234, 0,
           color::Black);
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
  drawIcon(dc, ic_round_18_device_bluetooth_searching(), 126, 18,
           color::Bisque);
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
  drawIcon(dc, ic_round_24_device_grid_goldenratio(), 0, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_auto(), 24, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_auto_select(), 48, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_off_select(), 72, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_hdr_on_select(), 96, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_h_mobiledata(), 120, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_h_plus_mobiledata(), 144, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lan(), 168, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lens_blur(), 192, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_light_mode(), 216, 60, color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_location_disabled(), 240, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_location_searching(), 264, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lte_mobiledata(), 288, 60,
           color::LavenderBlush);
  drawIcon(dc, ic_round_24_device_lte_plus_mobiledata(), 312, 60,
           color::LavenderBlush);
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
  drawIcon(dc, ic_round_48_device_screen_lock_landscape(), 144, 156,
           color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_lock_portrait(), 192, 156,
           color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_lock_rotation(), 240, 156,
           color::DarkBlue);
  drawIcon(dc, ic_round_48_device_screen_rotation(), 288, 156, color::DarkBlue);
  dc.setBackgroundColor(color::DarkSlateBlue);
  drawIcon(dc, ic_round_48_device_sd_storage(), 0, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update_good(), 48, 204,
           color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update(), 96, 204,
           color::LightCoral);
  drawIcon(dc, ic_round_48_device_security_update_warning(), 144, 204,
           color::LightCoral);
  drawIcon(dc, ic_round_48_device_sell(), 192, 204, color::LightCoral);
  drawIcon(dc, ic_round_48_device_send_to_mobile(), 240, 204,
           color::LightCoral);
  drawIcon(dc, ic_round_48_device_settings_suggest(), 288, 204,
           color::LightCoral);
}