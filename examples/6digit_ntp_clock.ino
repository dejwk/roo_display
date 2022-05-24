#include <Arduino.h>

#include <string>

#include "roo_display.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSans_Regular/40.h"
#include "roo_smooth_fonts/NotoSans_Bold/60.h"
#include <WiFi.h>
#include <time.h>

using namespace roo_display;

//********* ENTER YOUR WIFI CREDENTIALS ********** 
const char *ssid     = "*******";
const char *password = "**********";

//********* NTP TIME  ********** 
// Set TZ_INFO for your location:  https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* TZ_INFO    = "PST8PDT,M3.2.0,M11.1.0";  //e.g Los Angeles US Pacific and DST details
// Specify your nearest NTP server
const char* NTP_SERVER = "0.pool.ntp.org";

tm timeinfo;
time_t now;
bool us_datefmt = true; // mon/day/year, set to false for day/mon/year format

// 
// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_172x320<5, 2, 4> device;
//St7789spi_240x240<5, 2, 4> device;

Display display(device);

// Return Horizontal size of font/string combo (font.h)
GlyphMetrics getAsciiStringMetrics(const Font& font, const std::string& ascii) {
  return font.getHorizontalStringMetrics((const uint8_t*)ascii.c_str(),
                                         ascii.size());
}

//FWD Declarations
void initNTP(std::string s);
bool getNTPtime(int sec);
void stringifyTime(tm localTime, char* time, char* date);
void bootMsg(std::string s);
void initWiFi(std::string s);




void setup() {
  Serial.begin(9600);

  SPI.begin();  // Use default SPI pins, or specify your own here.

  display.init(color::AliceBlue);
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
    display.setBackground(color::BlanchedAlmond);

  display.clear();
  
  DrawingContext dc(display);
  const Font& font = font_NotoSans_Regular_40();
 
  initWiFi("Init Wifi");
  initNTP("Init NTP");
  display.clear();

  // split screen background color vertically
  dc.draw(FilledRect(0,display.height() / 2  +15, 
               display.width(), display.height(),
               color::BlueViolet));
  dc.draw(Line(0,display.height() / 2  +15, 
               display.width(), display.height() / 2  +15,
               color::Black));             
}

void loop() {

  display.setBackground(color::BlanchedAlmond);

  char time[9] = "";
  char date[11] = "";
  const Font& timeFont = font_NotoSans_Bold_60();
  const Font& dateFont = font_NotoSans_Regular_40();

  GlyphMetrics timeMetrics = getAsciiStringMetrics(timeFont, "00:00:00");
  Box timeBox = timeMetrics.screen_extents();
  GlyphMetrics dateMetrics = getAsciiStringMetrics(dateFont, "00/00/0000");
  Box dateBox = dateMetrics.screen_extents();

  getNTPtime(10);
  stringifyTime(timeinfo,time,date);

  auto t_label = ClippedTextLabel(timeFont, time, color::BlueViolet);
  auto t_tile = MakeTileOf(t_label, timeBox, HAlign::None(), VAlign::None(), 
                        color::Transparent, FillMode::FILL_MODE_RECTANGLE);

  auto d_label = ClippedTextLabel(dateFont, date, color::BlanchedAlmond);
  auto d_tile = MakeTileOf(d_label, dateBox, HAlign::None(), VAlign::None(), 
                        color::Transparent, FillMode::FILL_MODE_RECTANGLE);

  DrawingContext dc(display);
  // Translate dc to center of display
  dc.setTransform(Transform().translate(display.width() / 2, 
                                        display.height() / 2) );
  int16_t timeDx = -timeBox.width() / 2;
  dc.draw(t_tile, timeDx, 0);                       // draw Time

  dc.setBackground(color::BlueViolet);             
  int16_t dateDx = -dateBox.width() / 2;
  dc.draw(d_tile, dateDx, timeBox.height() *3/2);   // draw Date
  delay(100);
}


// ntp methods (see https://github.com/SensorsIot/NTP-time-for-ESP8266-and-ESP32)
bool getNTPtime(int sec) {
  {
    uint32_t start = millis();
    do {
      time(&now);
      localtime_r(&now, &timeinfo);
      delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
    if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
  }
  return true;
}

void initNTP(std::string s) {
  bootMsg(s);
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TZ_INFO, 1);

  if (getNTPtime(10)) {  // wait up to 10sec to sync
  } else {
    Serial.println("Time not set");
    ESP.restart();
  }
}

// Time/Date stringifier
void stringifyTime(tm localTime, char* time, char* date) {
  char int_st[32];
    sprintf(int_st, "%02d",(us_datefmt ? localTime.tm_mon+1 : localTime.tm_mday));
    strcat(date,int_st);
    strcat(date,"/");
    sprintf(int_st, "%02d",(us_datefmt ? localTime.tm_mday : localTime.tm_mon+1));
    strcat(date,int_st);
    strcat(date,"/");
    sprintf(int_st, "%04d",localTime.tm_year+1900);
    strcat(date,int_st); 

    if(localTime.tm_hour > 12) {  // PM
      localTime.tm_hour -= 12;
    }
    sprintf(int_st, "%02d",localTime.tm_hour);
    strcat(time,int_st);
    strcat(time,":");
    sprintf(int_st, "%02d",localTime.tm_min);
    strcat(time,int_st);
    strcat(time,":");
    sprintf(int_st, "%02d",localTime.tm_sec);
    strcat(time,int_st);
}

// Show short message during boot in vertical center of display
void bootMsg(std::string s){
  const Font& font = font_NotoSans_Regular_40();
  DrawingContext dc(display);
  display.clear();
  dc.draw(TextLabel(font, s, color::Black), 10, display.height()/2);
}

// Connect to WiFi
void initWiFi(std::string s) {
  bootMsg(s);

  WiFi.begin();
  while ( WiFi.status() == WL_DISCONNECTED ){
    delay (50);
  }
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    s.append(".");
    bootMsg(s);
    Serial.print ( "." );
  }
  WiFi.begin(ssid, password);
}


