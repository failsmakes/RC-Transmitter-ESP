#pragma once
// =============================================================================
//  RC CAR — TRANSMITTER CONFIG
//  ESP8266 ve ESP32 kartlarını destekler.
// =============================================================================

// =============================================================================
//  1. KART SEÇİMİ  (Receiver config.h ile aynı enum değerleri)
// =============================================================================
#define BOARD_ESP8266   1
#define BOARD_ESP32     2

#define BOARD_TYPE      BOARD_ESP8266   // ← buradan değiştir

// =============================================================================
//  2. ESP-NOW
// =============================================================================
//  Receiver'ın MAC adresini buraya yazın (receiver Serial'dan basar)
#define RX_MAC         { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }  // ← değiştirin!
#define ESPNOW_CHANNEL  1     // Receiver AP kanalıyla aynı olmalı!

// =============================================================================
//  3. PIN TANIMLARI
// =============================================================================

#if BOARD_TYPE == BOARD_ESP8266
  // ── ESP8266 NodeMCU v3 ────────────────────────────────────────────────────
  #define ADC_PIN          A0
  #define MUX_SEL_PIN      14    // D5 = GPIO14
  #define MUX_SEL2_PIN     12    // D6 = GPIO12
  #define MUX_SEL3_PIN     15    // D8 = GPIO15 (Gyro gain pot için, opsiyonel)
  #define TRIM_BTN_L_PIN    0    // D3 = GPIO0
  #define TRIM_BTN_R_PIN   13    // D7 = GPIO13
  #define TRIM_BTN_RST_PIN  3    // opsiyonel
  #define GYRO_DIR_BTN_PIN 16    // D0 = GPIO16
  #define STATUS_LED_PIN   LED_BUILTIN
  #define JOY_ADC_MAX      1023
  #define JOY_ADC_MIN      0

#elif BOARD_TYPE == BOARD_ESP32
  // ── ESP32 DevKit v1 ───────────────────────────────────────────────────────
  //  ESP32'de analogRead birden fazla pin destekler.
  //  Joystick doğrudan pinlere bağlanır (MUX gerekmeyebilir).
  #define ADC_THROTTLE_PIN  34   // GPIO34 — ADC1_CH6 (giriş-only)
  #define ADC_STEER_PIN     35   // GPIO35 — ADC1_CH7 (giriş-only)
  #define ADC_TRIM_PIN      32   // GPIO32 — ADC1_CH4 (trim pot)
  #define ADC_GYRO_GAIN_PIN 33   // GPIO33 — ADC1_CH5 (gyro gain pot, opsiyonel)
  // ESP32'de MUX gerekmez — her kanal doğrudan okunur
  #define MUX_SEL_PIN       -1   // kullanılmıyor
  #define TRIM_BTN_L_PIN    18
  #define TRIM_BTN_R_PIN    19
  #define TRIM_BTN_RST_PIN  23
  #define GYRO_DIR_BTN_PIN  5
  #define STATUS_LED_PIN     2   // Dahili LED (bazı ESP32 boardlarında farklı olabilir)
  #define JOY_ADC_MAX      4095
  #define JOY_ADC_MIN      0

#endif

// =============================================================================
//  4. JOYSTICK
// =============================================================================
//  0 = otomatik kalibrasyon (açılışta merkezi oku)
#define JOY_THROTTLE_CENTER  0
#define JOY_STEER_CENTER     0
#define JOYSTICK_DEADBAND    5

// =============================================================================
//  5. TRIM
// =============================================================================
//  TRIM_SOURCE_POT     → Potansiyometre (MUX kanal 2 / ESP32 ADC_TRIM_PIN)
//  TRIM_SOURCE_BUTTONS → Butonlar
#define TRIM_SOURCE_POT      1
#define TRIM_SOURCE_BUTTONS  2

#define TRIM_SOURCE   TRIM_SOURCE_POT

#define TRIM_MIN    -30
#define TRIM_MAX     30
#define TRIM_STEP     1
#define TRIM_REPEAT_MS  200   // Buton tekrar süresi

// =============================================================================
//  6. GÖNDERIM HIZI
// =============================================================================
#define SEND_INTERVAL_MS   20    // 50 Hz

// =============================================================================
//  7. GYRO AYAR PARAMETRELER
// =============================================================================
//  GYRO_GAIN_SOURCE_POT = 1 → pot (MUX kanal 3 / ESP32 ADC_GYRO_GAIN_PIN)
//  GYRO_GAIN_SOURCE_POT = 0 → sabit değer
#define GYRO_GAIN_SOURCE_POT   0
#define GYRO_GAIN_FIXED        50

//  GYRO_DIR_BTN_ENABLED = 1 → buton ile direction toggle
//  GYRO_DIR_BTN_ENABLED = 0 → sabit direction
#define GYRO_DIR_BTN_ENABLED   0
#define GYRO_DIR_FIXED         1

// =============================================================================
//  8. DEADBAND
// =============================================================================
#define THROTTLE_DEADBAND  5
#define STEER_DEADBAND     3
