// =============================================================================
//  RC CAR — TRANSMITTER  (ESP8266 NodeMCU v3)
//
//  Donanım:
//    • Arduino joystick modülü (X=Yön, Y=Gaz) — analog MUX ile A0'dan okunur
//    • Potansiyometre veya butonlar — Trim
//    • Potansiyometre (opsiyonel) — Gyro Gain
//    • Buton (opsiyonel) — Gyro Direction Reverse
//    • ESP-NOW → Receiver'a RCPacket gönderir
//    • ESP-NOW ← Receiver'dan TelemetryPacket (ACK + gyro durumu) alır
//
//  RCPacket yapısı:
//    throttle : -100..+100
//    steer    : -100..+100 (trim dahil)
//    seq      : paket sayacı
//    gyroGain : 0–100
//    gyroDir  : +1 veya -1
//
//  Serial çıkış (115200 baud):
//    TX → t / ack_t  s / ack_s  trim  gain  dir  seq / ack_seq  bağlantı durumu
// =============================================================================

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "config.h"

// -----------------------------------------------------------------------------
//  VERİ YAPILARI  (Receiver ile aynı olmalı!)
// -----------------------------------------------------------------------------
struct __attribute__((packed)) RCPacket {
  int8_t  throttle;
  int8_t  steer;
  uint8_t seq;
  int8_t  gyroGain;   // 0–100
  int8_t  gyroDir;    // +1 veya -1
};

struct __attribute__((packed)) TelemetryPacket {
  uint8_t ack_seq;
  int8_t  ack_throttle;
  int8_t  ack_steer;
  float   ack_vBat;
  uint8_t ack_rssi;
  int8_t  ack_gyroGain;
  int8_t  ack_gyroDir;
};

// -----------------------------------------------------------------------------
//  DURUM
// -----------------------------------------------------------------------------
uint8_t  rxMac[6]      = RX_MAC;
uint8_t  seqCounter    = 0;
uint32_t lastAckMs     = 0;
bool     rxConnected   = false;

int joyThrottleCenter = 512;
int joySteerCenter    = 512;

int  trimValue  = 0;
int  gyroGain   = GYRO_GAIN_FIXED;
int  gyroDir    = GYRO_DIR_FIXED;

TelemetryPacket lastTelemetry = {};

#if TRIM_SOURCE == TRIM_SOURCE_BUTTONS
uint32_t lastTrimMs = 0;
#define TRIM_REPEAT_MS 200
#endif

// -----------------------------------------------------------------------------
//  LED
// -----------------------------------------------------------------------------
void ledOn()  { digitalWrite(STATUS_LED_PIN, LOW);  }
void ledOff() { digitalWrite(STATUS_LED_PIN, HIGH); }
void ledBlink(int n, int ms = 150) {
  for (int i = 0; i < n; i++) {
    ledOn();  delay(ms);
    ledOff(); delay(ms);
  }
}

// -----------------------------------------------------------------------------
//  MUX OKUMA
//  Kanal → SEL pinleri: sel1=bit0, sel2=bit1, sel3=bit2
//    0: Gaz  (sel1=0, sel2=0, sel3=0)
//    1: Yön  (sel1=1, sel2=0, sel3=0)
//    2: Trim Pot (sel1=0, sel2=1, sel3=0)
//    3: Gyro Gain Pot (sel1=1, sel2=1, sel3=0) — sel3 gerekirse ayrı pin
// -----------------------------------------------------------------------------
int readMuxChannel(int ch) {
  bool sel1 = (ch & 0x01) != 0;
  bool sel2 = (ch & 0x02) != 0;
  digitalWrite(MUX_SEL_PIN, sel1 ? HIGH : LOW);
#if TRIM_SOURCE == TRIM_SOURCE_POT
  digitalWrite(MUX_SEL2_PIN, sel2 ? HIGH : LOW);
#endif
#if GYRO_GAIN_SOURCE_POT
  // sel3 için ek pin (MUX_SEL3_PIN) — kanal 3'te gyro gain pot okunur
  bool sel3 = (ch & 0x04) != 0;
  // sel3 pini varsa buraya ekle:
  // digitalWrite(MUX_SEL3_PIN, sel3 ? HIGH : LOW);
  (void)sel3;
#endif
  delayMicroseconds(50);
  return analogRead(ADC_PIN);
}

// -----------------------------------------------------------------------------
//  JOYSTICK → -100..+100
// -----------------------------------------------------------------------------
int readJoystick(int kanal) {
  int raw    = readMuxChannel(kanal);
  int center = (kanal == 0) ? joyThrottleCenter : joySteerCenter;
  int val;
  if (raw >= center) {
    val = map(raw, center, JOY_ADC_MAX, 0, 100);
  } else {
    val = map(raw, JOY_ADC_MIN, center, -100, 0);
  }
  val = constrain(val, -100, 100);
  if (abs(val) <= JOYSTICK_DEADBAND) val = 0;
  return val;
}

// -----------------------------------------------------------------------------
//  TRIM OKUMA
// -----------------------------------------------------------------------------
void updateTrim() {
#if TRIM_SOURCE == TRIM_SOURCE_POT
  int raw   = readMuxChannel(2);
  trimValue = map(raw, JOY_ADC_MIN, JOY_ADC_MAX, TRIM_MIN, TRIM_MAX);
  trimValue = constrain(trimValue, TRIM_MIN, TRIM_MAX);

#elif TRIM_SOURCE == TRIM_SOURCE_BUTTONS
  if (millis() - lastTrimMs < TRIM_REPEAT_MS) return;
  bool left  = (digitalRead(TRIM_BTN_LEFT_PIN)  == LOW);
  bool right = (digitalRead(TRIM_BTN_RIGHT_PIN) == LOW);
  bool reset = (digitalRead(TRIM_BTN_RESET_PIN) == LOW);
  if (reset)      trimValue = 0;
  else if (left)  trimValue = constrain(trimValue - TRIM_STEP, TRIM_MIN, TRIM_MAX);
  else if (right) trimValue = constrain(trimValue + TRIM_STEP, TRIM_MIN, TRIM_MAX);
  if (left || right || reset) lastTrimMs = millis();
#endif
}

// -----------------------------------------------------------------------------
//  GYRO GAIN OKUMA
// -----------------------------------------------------------------------------
void updateGyroGain() {
#if GYRO_GAIN_SOURCE_POT
  // MUX kanal 3'ten pot oku
  int raw = readMuxChannel(3);
  gyroGain = map(raw, JOY_ADC_MIN, JOY_ADC_MAX, 0, 100);
  gyroGain = constrain(gyroGain, 0, 100);
#else
  gyroGain = GYRO_GAIN_FIXED;
#endif
}

// -----------------------------------------------------------------------------
//  GYRO DIRECTION OKUMA
// -----------------------------------------------------------------------------
void updateGyroDir() {
#if GYRO_DIR_BTN_ENABLED
  // Buton basılıyken yön ters
  gyroDir = (digitalRead(GYRO_DIR_BTN_PIN) == LOW) ? -1 : 1;
#else
  gyroDir = GYRO_DIR_FIXED;
#endif
}

// -----------------------------------------------------------------------------
//  ESP-NOW CALLBACK'LER
// -----------------------------------------------------------------------------
void onSendCB(uint8_t* mac, uint8_t status) {
  if (status == 0) ledOn();
  else             ledOff();
}

void onRecvCB(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len != sizeof(TelemetryPacket)) return;
  memcpy(&lastTelemetry, data, sizeof(lastTelemetry));
  lastAckMs   = millis();
  rxConnected = true;
}

// -----------------------------------------------------------------------------
//  STARTUP KALİBRASYONU
// -----------------------------------------------------------------------------
void calibrateJoystick() {
  Serial.println("[CAL] Joystick kalibrasyonu — boşta bırakın...");
  ledBlink(1, 500);

  long sumT = 0, sumS = 0;
  for (int i = 0; i < 10; i++) {
    sumT += readMuxChannel(0);
    sumS += readMuxChannel(1);
    delay(20);
  }
  joyThrottleCenter = (JOY_THROTTLE_CENTER != 0) ? JOY_THROTTLE_CENTER : (int)(sumT / 10);
  joySteerCenter    = (JOY_STEER_CENTER    != 0) ? JOY_STEER_CENTER    : (int)(sumS / 10);

  Serial.printf("[CAL] Gaz merkez: %d  Yön merkez: %d\n",
                joyThrottleCenter, joySteerCenter);
  ledBlink(3, 100);
}

// -----------------------------------------------------------------------------
//  SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(STATUS_LED_PIN, OUTPUT);
  ledOff();

  pinMode(MUX_SEL_PIN, OUTPUT);
  digitalWrite(MUX_SEL_PIN, LOW);

#if TRIM_SOURCE == TRIM_SOURCE_POT
  pinMode(MUX_SEL2_PIN, OUTPUT);
  digitalWrite(MUX_SEL2_PIN, LOW);
#elif TRIM_SOURCE == TRIM_SOURCE_BUTTONS
  pinMode(TRIM_BTN_LEFT_PIN,  INPUT_PULLUP);
  pinMode(TRIM_BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode(TRIM_BTN_RESET_PIN, INPUT_PULLUP);
#endif

#if GYRO_DIR_BTN_ENABLED
  pinMode(GYRO_DIR_BTN_PIN, INPUT_PULLUP);
#endif

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.printf("[TX] MAC: %s\n", WiFi.macAddress().c_str());

  calibrateJoystick();

  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Init HATA — yeniden başlatılıyor");
    ledBlink(10, 50);
    ESP.restart();
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onSendCB);
  esp_now_register_recv_cb(onRecvCB);
  esp_now_add_peer(rxMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

  Serial.println("[TX] Hazır — gönderim başlıyor.");
  ledBlink(2, 200);
}

// -----------------------------------------------------------------------------
//  LOOP
// -----------------------------------------------------------------------------
static uint32_t lastSendMs = 0;
static uint32_t lastLedMs  = 0;

void loop() {
  uint32_t now = millis();

  // Trim, Gyro Gain, Gyro Dir güncelle
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

    esp_now_send(rxMac, (uint8_t*)&pkt, sizeof(pkt));

    Serial.printf(
      "TX → t:%4d/%4d  s:%4d/%4d  trim:%3d  gain:%3d  dir:%+2d  seq:%3d/%3d  %s\n",
      (int)pkt.throttle, (int)lastTelemetry.ack_throttle,
      (int)pkt.steer,    (int)lastTelemetry.ack_steer,
      trimValue, gyroGain, gyroDir,
      (int)pkt.seq, (int)lastTelemetry.ack_seq,
      rxConnected ? "ACK✓" : "---"
    );
  }

  // Bağlantı kontrolü
  if (rxConnected && (now - lastAckMs > 1000)) {
    rxConnected = false;
    Serial.println("[TX] Receiver yanıt vermiyor!");
  }

  // LED: bağlı → hızlı, değil → yavaş yanıp sönme
  if (now - lastLedMs > (rxConnected ? 100 : 500)) {
    lastLedMs = now;
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }

  yield();
}
