#pragma once

// Waveshare ESP32-S3-Touch-LCD-4.3
// Hardware configuration for roo_display library

#include <Wire.h>
#include "roo_display.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/touch/calibration.h"

namespace roo_display {

// ============================================================================
// CH422G I/O Expander Konfiguration
// ============================================================================

#define CH422G_I2C_ADDR 0x20
#define CH422G_I2C_SDA GPIO_NUM_8
#define CH422G_I2C_SCL GPIO_NUM_9

// CH422G Bit-Definitionen
#define CH422G_LCD_BL  (1 << 2)  // Backlight auf Bit 2
#define CH422G_LCD_RST (1 << 3)  // Reset auf Bit 3

// CH422G Hilfsfunktionen - Nutzt Wire statt Low-Level I2C
namespace {

void writeCH422G(uint8_t value) {
    Wire.beginTransmission(CH422G_I2C_ADDR);
    Wire.write(value);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("CH422G write failed: error %d\n", error);
    }
}

bool initCH422G() {
    Serial.println("Initializing CH422G I/O expander...");
    
    // Wire wurde bereits im Konstruktor initialisiert!
    
    // GT911 Reset-Sequenz über CH422G
    Serial.println("Resetting GT911 touch controller...");
    
    // 1. Alles low (Reset aktiv)
    writeCH422G(0x00);
    delay(10);
    
    // 2. Backlight an, Reset noch low
    writeCH422G(CH422G_LCD_BL);
    delay(10);
    
    // 3. Reset high, GT911 startet
    writeCH422G(CH422G_LCD_RST | CH422G_LCD_BL);
    delay(200);  // GT911 braucht ~200ms zum Booten
    
    // Test: CH422G antwortet?
    Wire.beginTransmission(CH422G_I2C_ADDR);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("CH422G initialized successfully");
        return true;
    } else {
        Serial.printf("CH422G init failed: error %d\n", error);
        return false;
    }
}

} // anonymous namespace

// ============================================================================
// Display Konfiguration
// ============================================================================

namespace {
constexpr esp32s3_dma::Config kWaveshareConfig = {
    .width = 800,
    .height = 480,
    
    // Control Pins
    .de = 5,      // Data Enable
    .hsync = 46,  // Horizontal Sync
    .vsync = 3,   // Vertical Sync
    .pclk = 7,    // Pixel Clock
    
    // Horizontal Timing
    .hsync_pulse_width = 4,
    .hsync_back_porch = 8,
    .hsync_front_porch = 8,
    .hsync_polarity = 0,
    
    // Vertical Timing (spezifisch für 4.3" Modell)
    .vsync_pulse_width = 4,
    .vsync_back_porch = 16,   // 4.3" nutzt 16 (nicht 8 wie 7" Modell)
    .vsync_front_porch = 16,  // 4.3" nutzt 16 (nicht 8 wie 7" Modell)
    .vsync_polarity = 0,
    
    // Clock Konfiguration
    .pclk_active_neg = 1,  // entspricht pclk_idle_high = 1 in LovyanGFX
    .prefer_speed = 16000000,  // 16 MHz Pixel Clock
    
    // RGB Data Pins - Blue (5 Bits)
    .r0 = 1,   // R3
    .r1 = 2,   // R4
    .r2 = 42,  // R5
    .r3 = 41,  // R6
    .r4 = 40,  // R7
    
    // RGB Data Pins - Green (6 Bits)
    .g0 = 39,  // G2
    .g1 = 0,   // G3
    .g2 = 45,  // G4
    .g3 = 48,  // G5
    .g4 = 47,  // G6
    .g5 = 21,  // G7
    
    // RGB Data Pins - Red (5 Bits)
    .b0 = 14,  // B3
    .b1 = 38,  // B4
    .b2 = 18,  // B5
    .b3 = 17,  // B6
    .b4 = 10,  // B7
    
    .bswap = false
};
} // anonymous namespace

// ============================================================================
// Combo Device Klasse
// ============================================================================

class WaveshareEsp32s3TouchLcd43 {
public:
    WaveshareEsp32s3TouchLcd43(Orientation orientation = Orientation())
        : wire_(Wire),
          display_(kWaveshareConfig),
          touch_(wire_, -1, -1) {  // Polling-Modus (kein INT), RST über CH422G
        // WICHTIG: Wire MUSS vor TouchGt911-Konstruktor initialisiert sein!
        // Daher hier im Konstruktor, NICHT erst in init()
        wire_.begin(CH422G_I2C_SDA, CH422G_I2C_SCL, 400000);
        display_.setOrientation(orientation);
    }
    
    // Initialisierung - MUSS aufgerufen werden!
    bool init() {
        Serial.println("=== Waveshare ESP32-S3-Touch-LCD-4.3 ===");
        
        // PSRAM prüfen
        if (!psramFound()) {
            Serial.println("ERROR: PSRAM not found!");
            return false;
        }
        Serial.printf("PSRAM found: %d bytes free\n", ESP.getFreePsram());
        
        // Wire wurde bereits im Konstruktor initialisiert!
        
        // CH422G initialisieren (resettet GT911)
        if (!initCH422G()) {
            Serial.println("ERROR: CH422G initialization failed!");
            return false;
        }
        
        // Test: GT911 antwortet?
        Serial.println("Checking GT911 touch controller...");
        Wire.beginTransmission(0x5D);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.println("GT911 found at 0x5D!");
        } else {
            Serial.printf("WARNING: GT911 not responding (error: %d)\n", error);
        }
        
        // Touch explizit initialisieren (NACH CH422G Reset!)
        Serial.println("Initializing TouchGt911...");
        touch_.initTouch();
        delay(50);  // GT911 braucht kurze Zeit nach init
        
        Serial.println("Hardware initialized successfully!");
        return true;
    }
    
    // Zugriff auf Display und Touch
    DisplayDevice& display() { return display_; }
    TouchDevice& touch() { return touch_; }
    
    // Touch Kalibrierung (bei Bedarf anpassen)
    TouchCalibration touchCalibration() {
        return TouchCalibration(0, 0, 800, 480);
    }

private:
    decltype(Wire)& wire_;
    esp32s3_dma::ParallelRgb565Buffered display_;
    TouchGt911 touch_;
};

} // namespace roo_display