#pragma once
// =============================================================================
//  RC CAR — TRANSMITTER CONFIG  (ESP8266 NodeMCU v3)
// =============================================================================

// --- ESP-NOW -----------------------------------------------------------------
// Receiver'ın MAC adresini buraya yazın (receiver Serial'dan basar)
#define RX_MAC   { 0x5E, 0xCF, 0x7F, 0x87, 0x14, 0x54 }  // <-- değiştirin!

#define ESPNOW_CHANNEL  1   // Receiver AP kanalıyla aynı olmalı!

// --- JOYSTICK PİNLERİ --------------------------------------------------------
#define ADC_PIN         A0
#define MUX_SEL_PIN     D5      // GPIO14 — LOW: Gaz, HIGH: Yön

#define JOY_THROTTLE_CENTER  0   // 0 = otomatik
#define JOY_STEER_CENTER     0   // 0 = otomatik

#define JOY_ADC_MAX   1023
#define JOY_ADC_MIN   0

// --- POTANSİYOMETRE (TRIM) ---------------------------------------------------
#define TRIM_SOURCE_POT      1
#define TRIM_SOURCE_BUTTONS  0
#define TRIM_SOURCE   TRIM_SOURCE_POT
#define MUX_SEL2_PIN  D6    // GPIO12

#define TRIM_BTN_LEFT_PIN   D3
#define TRIM_BTN_RIGHT_PIN  D7
#define TRIM_BTN_RESET_PIN  D8
#define TRIM_MIN   -30
#define TRIM_MAX    30
#define TRIM_STEP    1

// --- GÖNDERIM HIZI -----------------------------------------------------------
#define SEND_INTERVAL_MS  20    // 50 Hz

// --- DEADBAND ----------------------------------------------------------------
#define THROTTLE_DEADBAND  5
#define STEER_DEADBAND     3
#define JOYSTICK_DEADBAND  5

// --- DURUM LED ---------------------------------------------------------------
#define STATUS_LED_PIN  LED_BUILTIN

// =============================================================================
//  GYRO AYAR PARAMETRELERİ
// =============================================================================

// --- Gyro Gain Potansiyometresi -----------------------------------------------
//  Transmitter'da bir pot ile gyro gain (0-100) ayarlanabilir.
//  MUX üzerinden okunur (kanal 3).
//  GYRO_GAIN_SOURCE_POT = 1 → Pot kullan
//  GYRO_GAIN_SOURCE_POT = 0 → Sabit değer kullan (GYRO_GAIN_FIXED)
#define GYRO_GAIN_SOURCE_POT   0   // 1=pot, 0=sabit
#define MUX_SEL3_PIN           D8  // GPIO15 — Gyro gain pot için MUX SEL bit2

#define GYRO_GAIN_FIXED        50  // 0–100 (GYRO_GAIN_SOURCE_POT=0 iken)

// --- Gyro Direction Butonu ---------------------------------------------------
//  Bu pini LOW'a çekmek gyro yönünü tersine çevirir.
//  GYRO_DIR_BTN_ENABLED = 1 → Buton kullan
//  GYRO_DIR_BTN_ENABLED = 0 → Sabit yön kullan (GYRO_DIR_FIXED)
#define GYRO_DIR_BTN_ENABLED   0      // 1=buton, 0=sabit
#define GYRO_DIR_BTN_PIN       D0     // GPIO16
#define GYRO_DIR_FIXED         1      // +1 veya -1
