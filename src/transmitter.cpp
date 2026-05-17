// =============================================================================
//  RC CAR — TRANSMITTER
//  Desteklenen kartlar: ESP8266, ESP32
//
//  config.h'den seçim:
//    BOARD_TYPE → BOARD_ESP8266 / BOARD_ESP32
//
//  ESP8266: Analog MUX (CD4051) ile tek ADC üzerinden joystick + trim + gain
//  ESP32  : Her kanal için ayrı ADC pini (MUX gerekmez)
//
//  ESP-NOW → Receiver'a RCPacket gönderir
//  ESP-NOW ← Receiver'dan TelemetryPacket alır (ACK + pil + gyro durumu)
// =============================================================================

#include <Arduino.h>
#include "config.h"

// ─── Platform include'ları ───────────────────────────────────────────────────
#if BOARD_TYPE == BOARD_ESP8266
  #include <ESP8266WiFi.h>
  #include <espnow.h>
#elif BOARD_TYPE == BOARD_ESP32
  #include <WiFi.h>
  #include <esp_now.h>
#endif

// =============================================================================
//  VERİ YAPILARI  (Receiver ile aynı olmalı!)
// =============================================================================
struct __attribute__((packed)) RCPacket {
  int8_t  throttle;
  int8_t  steer;
  uint8_t seq;
  int8_t  gyroGain;
  int8_t  gyroDir;
};

struct __attribute__((packed)) TelemetryPacket {
  uint8_t ack_seq;
  int8_t  ack_throttle;
  int8_t  ack_steer;
  float   ack_vBat;
  uint8_t ack_rssi;
  int8_t  ack_gyroGain;
  int8_t  ack_gyroDir;
  uint8_t ack_cells;
  uint8_t ack_lowVoltage;
};

// =============================================================================
//  DURUM
// =============================================================================
uint8_t rxMac[6]    = RX_MAC;
uint8_t seqCounter  = 0;
uint32_t lastAckMs  = 0;
bool rxConnected    = false;

int joyThrottleCenter = JOY_ADC_MAX / 2;
int joySteerCenter    = JOY_ADC_MAX / 2;
int trimValue = 0;
int gyroGain  = GYRO_GAIN_FIXED;
int gyroDir   = GYRO_DIR_FIXED;

TelemetryPacket lastTelemetry = {};

#if TRIM_SOURCE == TRIM_SOURCE_BUTTONS
uint32_t lastTrimMs = 0;
#endif

// =============================================================================
//  LED
// =============================================================================
void ledOn()  { digitalWrite(STATUS_LED_PIN, LOW);  }
void ledOff() { digitalWrite(STATUS_LED_PIN, HIGH); }
void ledBlink(int n, int ms = 150) {
  for (int i = 0; i < n; i++) {
    ledOn();  delay(ms);
    ledOff(); delay(ms);
  }
}

// =============================================================================
//  ADC OKUMA — Platform soyutlaması
//
//  ESP8266: MUX kanalı seç → analogRead(A0)
//  ESP32  : Doğrudan ilgili pini oku
//
//  Kanal eşlemesi:
//    0 → Throttle (gaz)
//    1 → Steer (yön)
//    2 → Trim
//    3 → Gyro Gain
// =============================================================================
int readAnalogChannel(int ch) {
#if BOARD_TYPE == BOARD_ESP8266
  // MUX SEL pinleri: SEL1=bit0, SEL2=bit1
  bool sel1 = (ch & 0x01) != 0;
  bool sel2 = (ch & 0x02) != 0;
  digitalWrite(MUX_SEL_PIN, sel1 ? HIGH : LOW);
  #if TRIM_SOURCE == TRIM_SOURCE_POT
    if (MUX_SEL2_PIN >= 0) digitalWrite(MUX_SEL2_PIN, sel2 ? HIGH : LOW);
  #endif
  delayMicroseconds(50);
  return analogRead(ADC_PIN);

#elif BOARD_TYPE == BOARD_ESP32
  // ESP32: Her kanal kendi pininden doğrudan okunur
  switch (ch) {
    case 0: return analogRead(ADC_THROTTLE_PIN);
    case 1: return analogRead(ADC_STEER_PIN);
    case 2: return analogRead(ADC_TRIM_PIN);
    case 3: return analogRead(ADC_GYRO_GAIN_PIN);
    default: return JOY_ADC_MAX / 2;
  }
#endif
}

// =============================================================================
//  JOYSTICK → -100..+100
// =============================================================================
int readJoystick(int kanal) {
  int raw    = readAnalogChannel(kanal);
  int center = (kanal == 0) ? joyThrottleCenter : joySteerCenter;
  int val;
  if (raw >= center)
    val = map(raw, center, JOY_ADC_MAX, 0, 100);
  else
    val = map(raw, JOY_ADC_MIN, center, -100, 0);
  val = constrain(val, -100, 100);
  if (abs(val) <= JOYSTICK_DEADBAND) val = 0;
  return val;
}

// =============================================================================
//  TRIM OKUMA
// =============================================================================
void updateTrim() {
#if TRIM_SOURCE == TRIM_SOURCE_POT
  int raw = readAnalogChannel(2);
  trimValue = map(raw, JOY_ADC_MIN, JOY_ADC_MAX, TRIM_MIN, TRIM_MAX);
  trimValue = constrain(trimValue, TRIM_MIN, TRIM_MAX);

#elif TRIM_SOURCE == TRIM_SOURCE_BUTTONS
  if (millis() - lastTrimMs < (uint32_t)TRIM_REPEAT_MS) return;
  bool l = (digitalRead(TRIM_BTN_L_PIN)   == LOW);
  bool r = (digitalRead(TRIM_BTN_R_PIN)   == LOW);
  bool z = (digitalRead(TRIM_BTN_RST_PIN) == LOW);
  if (z)      trimValue = 0;
  else if (l) trimValue = constrain(trimValue - TRIM_STEP, TRIM_MIN, TRIM_MAX);
  else if (r) trimValue = constrain(trimValue + TRIM_STEP, TRIM_MIN, TRIM_MAX);
  if (l || r || z) lastTrimMs = millis();
#endif
}

// =============================================================================
//  GYRO GAIN / DIRECTION
// =============================================================================
void updateGyroGain() {
#if GYRO_GAIN_SOURCE_POT
  int raw  = readAnalogChannel(3);
  gyroGain = constrain(map(raw, JOY_ADC_MIN, JOY_ADC_MAX, 0, 100), 0, 100);
#else
  gyroGain = GYRO_GAIN_FIXED;
#endif
}

void updateGyroDir() {
#if GYRO_DIR_BTN_ENABLED
  gyroDir = (digitalRead(GYRO_DIR_BTN_PIN) == LOW) ? -1 : 1;
#else
  gyroDir = GYRO_DIR_FIXED;
#endif
}

// =============================================================================
//  ESP-NOW CALLBACK'LER
// =============================================================================
#if BOARD_TYPE == BOARD_ESP8266
void onSendCB(uint8_t* mac, uint8_t status) {
  if (status == 0) ledOn(); else ledOff();
}
void onRecvCB(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len != sizeof(TelemetryPacket)) return;
  memcpy(&lastTelemetry, data, sizeof(lastTelemetry));
  lastAckMs = millis(); rxConnected = true;
}

#elif BOARD_TYPE == BOARD_ESP32
void onSendCB(const uint8_t* mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) ledOn(); else ledOff();
}
void onRecvCB(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(TelemetryPacket)) return;
  memcpy(&lastTelemetry, data, sizeof(lastTelemetry));
  lastAckMs = millis(); rxConnected = true;
}
#endif

// =============================================================================
//  JOYSTICK KALİBRASYONU
// =============================================================================
void calibrateJoystick() {
  Serial.println("[CAL] Joystick kalibrasyon — bosta birakin...");
  ledBlink(1, 500);
  long sumT = 0, sumS = 0;
  for (int i = 0; i < 16; i++) {
    sumT += readAnalogChannel(0);
    sumS += readAnalogChannel(1);
    delay(20);
  }
  joyThrottleCenter = (JOY_THROTTLE_CENTER != 0) ? JOY_THROTTLE_CENTER : (int)(sumT / 16);
  joySteerCenter    = (JOY_STEER_CENTER    != 0) ? JOY_STEER_CENTER    : (int)(sumS / 16);
  Serial.printf("[CAL] Gaz merkez: %d  Yon merkez: %d\n",
                joyThrottleCenter, joySteerCenter);
  ledBlink(3, 100);
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== RC TRANSMITTER (%s) ===\n",
    (BOARD_TYPE == BOARD_ESP32) ? "ESP32" : "ESP8266");

  pinMode(STATUS_LED_PIN, OUTPUT);
  ledOff();

  // Trim & Gyro pinleri
#if TRIM_SOURCE == TRIM_SOURCE_BUTTONS
  pinMode(TRIM_BTN_L_PIN,   INPUT_PULLUP);
  pinMode(TRIM_BTN_R_PIN,   INPUT_PULLUP);
  if (TRIM_BTN_RST_PIN >= 0) pinMode(TRIM_BTN_RST_PIN, INPUT_PULLUP);
#endif
#if GYRO_DIR_BTN_ENABLED
  pinMode(GYRO_DIR_BTN_PIN, INPUT_PULLUP);
#endif

// ESP8266: MUX SEL pinlerini ayarla
#if BOARD_TYPE == BOARD_ESP8266
  if (MUX_SEL_PIN >= 0) { pinMode(MUX_SEL_PIN, OUTPUT); digitalWrite(MUX_SEL_PIN, LOW); }
  #if TRIM_SOURCE == TRIM_SOURCE_POT
    if (MUX_SEL2_PIN >= 0) { pinMode(MUX_SEL2_PIN, OUTPUT); digitalWrite(MUX_SEL2_PIN, LOW); }
  #endif
#endif

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.printf("[TX] MAC: %s\n", WiFi.macAddress().c_str());

  calibrateJoystick();

  // ESP-NOW başlat
#if BOARD_TYPE == BOARD_ESP8266
  if (esp_now_init() != 0) { Serial.println("[ESP-NOW] HATA"); ledBlink(10, 50); ESP.restart(); }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onSendCB);
  esp_now_register_recv_cb(onRecvCB);
  esp_now_add_peer(rxMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

#elif BOARD_TYPE == BOARD_ESP32
  if (esp_now_init() != ESP_OK) { Serial.println("[ESP-NOW] HATA"); ESP.restart(); }
  esp_now_register_send_cb(onSendCB);
  esp_now_register_recv_cb(onRecvCB);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, rxMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
#endif

  Serial.println("[TX] Hazir.");
  ledBlink(2, 200);
}

// =============================================================================
//  LOOP
// =============================================================================
static uint32_t lastSendMs = 0;
static uint32_t lastLedMs  = 0;

void loop() {
  uint32_t now = millis();

  updateTrim();
  updateGyroGain();
  updateGyroDir();

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;

    RCPacket pkt;
    pkt.throttle = (int8_t) readJoystick(0);
    pkt.steer    = (int8_t) constrain(readJoystick(1) + trimValue, -100, 100);
    pkt.seq      = ++seqCounter;
    pkt.gyroGain = (int8_t) gyroGain;
    pkt.gyroDir  = (int8_t) gyroDir;

#if BOARD_TYPE == BOARD_ESP8266
    esp_now_send(rxMac, (uint8_t*)&pkt, sizeof(pkt));
#elif BOARD_TYPE == BOARD_ESP32
    esp_now_send(rxMac, (const uint8_t*)&pkt, sizeof(pkt));
#endif

    // Düşük voltaj / bağlantı log
    static uint32_t lastLogMs = 0;
    if (now - lastLogMs >= 500) {
      lastLogMs = now;
      Serial.printf(
        "TX t:%4d s:%4d tr:%3d gg:%3d gd:%+2d | ACK t:%4d s:%4d bat:%.2fV %dS lv:%u | %s\n",
        (int)pkt.throttle, (int)pkt.steer, trimValue, gyroGain, gyroDir,
        (int)lastTelemetry.ack_throttle, (int)lastTelemetry.ack_steer,
        lastTelemetry.ack_vBat, (int)lastTelemetry.ack_cells,
        (unsigned)lastTelemetry.ack_lowVoltage,
        rxConnected ? "OK" : "---"
      );
    }
  }

  if (rxConnected && (now - lastAckMs > 1000)) {
    rxConnected = false;
    Serial.println("[TX] Receiver yanit vermiyor!");
  }

  // LED: bağlı = hızlı, değil = yavaş
  if (now - lastLedMs > (uint32_t)(rxConnected ? 100 : 500)) {
    lastLedMs = now;
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }

#if BOARD_TYPE == BOARD_ESP8266
  yield();
#endif
}
