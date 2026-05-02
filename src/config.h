#pragma once
// =============================================================================
//  RC CAR — TRANSMITTER CONFIG  (ESP8266 NodeMCU v3)
// =============================================================================

// --- ESP-NOW -----------------------------------------------------------------
// Receiver'ın MAC adresini buraya yazın (receiver Serial'dan basar)
#define RX_MAC   { 0x5E, 0xCF, 0x7F, 0x87, 0x14, 0x54 }  // <-- değiştirin!

#define ESPNOW_CHANNEL  1   // Receiver AP kanalıyla aynı olmalı!

// --- JOYSTICK PİNLERİ --------------------------------------------------------
//  NodeMCU v3'te tek ADC pini A0'dır.
//  İki ekseni okumak için analog multiplexer (4051/4052) önerilir.
//  VEYA: D-pad benzeri dijital joystick kullanılabilir.
//
//  Bu firmware analog MUX varsayar:
//    MUX_SEL_PIN → D5 (LOW=Gaz/Y ekseni, HIGH=Yön/X ekseni okunur)
//    ADC_PIN     → A0
//
//  Tek eksenli test için MUX_SEL_PIN'i tanımlamayın (sadece gaz okunur).
// -----------------------------------------------------------------------------
#define ADC_PIN         A0
#define MUX_SEL_PIN     D5      // GPIO14 — LOW: Gaz, HIGH: Yön

// Joystick orta (boşta) ADC değerleri — startup'ta otomatik kalibre edilir
// Manuel override için değer girin, 0 bırakırsanız startup'ta ölçülür:
#define JOY_THROTTLE_CENTER  0   // 0 = otomatik
#define JOY_STEER_CENTER     0   // 0 = otomatik

// Joystick ADC tam sapma (tipik 0-1023, NodeMCU için 0-1023)
#define JOY_ADC_MAX   1023
#define JOY_ADC_MIN   0

// --- POTANSİYOMETRE (TRIM) ---------------------------------------------------
//  MUX olmadan potansiyometre ayrı bir pin gerektirir.
//  ESP8266'da A0 tek ADC olduğundan pot için de MUX üzerinden okunur
//  ya da dijital trim butonu (D3, D4) kullanılır.
//
//  Bu firmware iki seçenek sunar — config'den seçin:
#define TRIM_SOURCE_POT      1   // Potansiyometre (MUX kanal 2 — D6=SEL2)
#define TRIM_SOURCE_BUTTONS  0   // Dijital butonlar

#define TRIM_SOURCE   TRIM_SOURCE_POT

// Pot MUX kanalı için ek SEL pin (TRIM_SOURCE_POT seçiliyse)
#define MUX_SEL2_PIN  D6    // GPIO12

// Buton pinleri (TRIM_SOURCE_BUTTONS seçiliyse)
#define TRIM_BTN_LEFT_PIN   D3   // GPIO0  — dahili pull-up
#define TRIM_BTN_RIGHT_PIN  D7   // GPIO13 — dahili pull-up
#define TRIM_BTN_RESET_PIN  D8   // GPIO15

// Trim sınırları
#define TRIM_MIN   -30
#define TRIM_MAX    30
#define TRIM_STEP    1

// --- GÖNDERIM HIZI -----------------------------------------------------------
#define SEND_INTERVAL_MS  20    // 50 Hz

// --- DEADBAND ----------------------------------------------------------------
#define THROTTLE_DEADBAND  5
#define STEER_DEADBAND     3
#define JOYSTICK_DEADBAND  5   // Hem gaz hem yön için ortak deadband

// --- DURUM LED ---------------------------------------------------------------
#define STATUS_LED_PIN  LED_BUILTIN   // NodeMCU dahili LED (aktif-LOW)
