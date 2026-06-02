#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

#define NAU_I2C_ADDR  0x1A
#define NAU_SCL_PIN   25
#define NAU_SDA_PIN   33
#define I2S_BCLK_PIN  0
#define I2S_LRCK_PIN  13
#define I2S_DO_PIN    32

#define SAMPLE_RATE   48000
#define SINE_LEN      960  // 960 = 20 * 48 (1kHz), kein Phasensprung

static int16_t sine_buf[SINE_LEN];

// NAU88C22 I2C: 7-bit reg addr + 9-bit value packed into 2 bytes
void nau_write(uint8_t reg, uint16_t val) {
    Wire.beginTransmission(NAU_I2C_ADDR);
    Wire.write((reg << 1) | ((val >> 8) & 1));
    Wire.write(val & 0xFF);
    Wire.endTransmission();
}

uint16_t nau_read(uint8_t reg) {
    Wire.beginTransmission(NAU_I2C_ADDR);
    Wire.write(reg << 1);
    Wire.endTransmission(false);
    if (Wire.requestFrom((int)NAU_I2C_ADDR, 2) == 2) {
        uint8_t b0 = Wire.read();
        uint8_t b1 = Wire.read();
        return ((uint16_t)(b0 & 1) << 8) | b1;
    }
    return 0xFFFF;
}

void nau_dump(uint8_t reg) {
    Wire.beginTransmission(NAU_I2C_ADDR);
    Wire.write(reg << 1);
    Wire.endTransmission(false);
    if (Wire.requestFrom((int)NAU_I2C_ADDR, 2) == 2) {
        uint8_t b0 = Wire.read();
        uint8_t b1 = Wire.read();
        uint16_t val = ((uint16_t)(b0 & 1) << 8) | b1;
        Serial.printf("  R[0x%02X]=0x%03X (%02X %02X)\n", reg, val, b0, b1);
    }
}

void nau_scan() {
    Serial.println("I2C Scan:");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X\n", addr);
        }
    }
}

void nau88c22_init() {
    Serial.println("Init NAU88C22...");

    // Reset
    nau_write(0x00, 0x0000);
    delay(20);

    Serial.printf("  Device ID: 0x%03X\n", nau_read(0x3F));

    // Bias startup (from Linux driver)
    nau_write(0x01, 0x000F); // IOBUF + ABIAS + REFIMP=3K
    delay(100);
    nau_write(0x01, 0x000E); // IOBUF + ABIAS + REFIMP=300K
    delay(10);

    // === PLL from BCLK ===
    // BCLK = 3.072MHz (32-bit slot) * 8 = 24.576MHz VCO
    // VCO/2 = 12.288MHz SYSCLK = 256×48kHz

    // Audio Interface: I2S, 16-bit, slave, PLL Input = BCLK
    nau_write(0x04, 0x0000);

    // PLL OFF
    nau_write(0x01, nau_read(0x01) & ~0x20);

    // PLL_N: N=8
    nau_write(0x24, 0x0008);

    // Kein fractional
    nau_write(0x25, 0x0000);
    nau_write(0x26, 0x0000);
    nau_write(0x27, 0x0000);

    // Clocking: VCO/2 (bit8), slave
    nau_write(0x06, 0x0100);

    // PLL ON
    nau_write(0x01, nau_read(0x01) | 0x20);
    delay(10);

    // Sample rate: 48kHz
    nau_write(0x07, 0x0000);

    // PM3: all audio blocks on (LDAC|RDAC|LMIX|RMIX|BIASGEN?|RSPK|LSPK)
    nau_write(0x03, 0x007F);
    delay(10);

    // Output Control
    nau_write(0x31, 0x0002);

    // Mixer: DAC to output
    nau_write(0x32, 0x0001);
    nau_write(0x33, 0x0001);

    // DAC volume (max)
    nau_write(0x0B, 0x00FF);
    nau_write(0x0C, 0x00FF);

    // Speaker volume
    nau_write(0x36, 0x0039);
    nau_write(0x37, 0x0039);

    // Register dump
    nau_dump(0x01);
    nau_dump(0x03);
    nau_dump(0x04);
    nau_dump(0x06);
    nau_dump(0x24); // PLL_N
    nau_dump(0x0E); // ADC Control
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\nNAU88C22 Audio Test v14 - PLL from BCLK v2");

    Wire.begin(NAU_SDA_PIN, NAU_SCL_PIN, 100000);
    delay(100);

    nau_scan();

    // Build sine BEFORE I2S init
    for (int i = 0; i < SINE_LEN; i++) {
        sine_buf[i] = (int16_t)(20000.0 * sin(2.0 * M_PI * 1000.0 * i / SAMPLE_RATE));
    }

    // I2S config
    i2s_config_t i2s_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256
    };

    i2s_pin_config_t pin_cfg = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DO_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("I2S install error: %d\n", err);
        return;
    }
    i2s_set_pin(I2S_NUM_0, &pin_cfg);
    Serial.println("I2S ready");

    // Init codec AFTER I2S (so BCLK is running for PLL)
    nau88c22_init();

    Serial.println("Playing 1kHz sine...");
}

void loop() {
    int16_t stereo[SINE_LEN * 2];
    for (int i = 0; i < SINE_LEN; i++) {
        stereo[i * 2] = sine_buf[i];
        stereo[i * 2 + 1] = sine_buf[i];
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, stereo, sizeof(stereo), &written, portMAX_DELAY);
}
