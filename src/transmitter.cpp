// =============================================================================
//  RC CAR — TRANSMITTER  (ESP8266 NodeMCU v3)
//
//  Donanım:
//    • Arduino joystick modülü (X=Yön, Y=Gaz) — analog MUX ile A0'dan okunur
//    • Potansiyometre veya butonlar — Trim
//    • ESP-NOW → Receiver'a gönderir
//    • ESP-NOW ← Receiver'dan ACK/telemetri alır
//
//  Startup kalibrasyonu:
//    İlk açılışta joystick orta değerleri ADC'den otomatik öğrenilir.
//    Joystickleri boşta bırakın — LED 3 kez yanıp söner, sonra hazır.
// =============================================================================

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "config.h"

// -----------------------------------------------------------------------------
//  ORTAK VERİ YAPILARI
// -----------------------------------------------------------------------------
struct __attribute__((packed)) RCPacket {
  int8_t  throttle;
  int8_t  steer;
  uint8_t seq;
};

struct __attribute__((packed)) TelemetryPacket {
  uint8_t ack_seq;
  int8_t  ack_throttle;   // -100 … +100
  int8_t  ack_steer;      // -100 … +100
  float   ack_vBat;      // pil voltajı
  uint8_t ack_rssi;
};

// -----------------------------------------------------------------------------
//  DURUM
// -----------------------------------------------------------------------------
uint8_t  rxMac[6]      = RX_MAC;
uint8_t  seqCounter    = 0;
uint8_t  lastAckSeq    = 0;
uint32_t lastAckMs     = 0;
bool     rxConnected   = false;

// Joystick kalibrasyon (startup'ta ölçülür)
int joyThrottleCenter = 512;
int joySteerCenter    = 512;

// Trim
int trimValue = 0;

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
//  sel1=LOW,  sel2=LOW  → Kanal 0 = Gaz (Joystick Y)
//  sel1=HIGH, sel2=LOW  → Kanal 1 = Yön (Joystick X)
//  sel1=LOW,  sel2=HIGH → Kanal 2 = Trim Pot
// -----------------------------------------------------------------------------
int readMuxChannel(int ch) {
  // ch: 0=Gaz, 1=Yön, 2=Trim
  bool sel1 = (ch & 0x01) != 0;
  bool sel2 = (ch & 0x02) != 0;
  digitalWrite(MUX_SEL_PIN, sel1 ? HIGH : LOW);
#if TRIM_SOURCE == TRIM_SOURCE_POT
  digitalWrite(MUX_SEL2_PIN, sel2 ? HIGH : LOW);
#endif
  delayMicroseconds(50);   // MUX yerleşme süresi
  return analogRead(ADC_PIN);
}

// -----------------------------------------------------------------------------
//  JOYSTICK → -100..+100
// -----------------------------------------------------------------------------
int readJoystick(int kanal = 0) {
  int raw = readMuxChannel(kanal);
  int center = joyThrottleCenter;
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
  int raw = readMuxChannel(2);
  // Pot orta = trim 0, tam sola = TRIM_MIN, tam sağa = TRIM_MAX
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
//  ESP-NOW gönderim callback
// -----------------------------------------------------------------------------
void onSendCB(uint8_t* mac, uint8_t status) {
  // status 0 = başarılı delivery
  if (status == 0) ledOn();
  else             ledOff();
}

// -----------------------------------------------------------------------------
//  ESP-NOW alma callback (telemetri / ACK)
// -----------------------------------------------------------------------------

TelemetryPacket tp;

void onRecvCB(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len != sizeof(TelemetryPacket)) return;
  memcpy(&tp, data, sizeof(tp));
  lastAckSeq   = tp.ack_seq;
  lastAckMs    = millis();
  rxConnected  = true;
}

// -----------------------------------------------------------------------------
//  STARTUP KALİBRASYONU
// -----------------------------------------------------------------------------
void calibrateJoystick() {
  Serial.println("[CAL] Joystick kalibrasyonu — boşta bırakın...");
  ledBlink(1, 500);

  // 10 örnek ortalaması
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

  // WiFi station modu (ESP-NOW için gerekli, gerçek AP'ye bağlanmayacak)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // MAC adresini yazdır (receiver config.h'a yazılacak)
  Serial.printf("[TX] MAC: %s\n", WiFi.macAddress().c_str());

  // Joystick kalibrasyon
  calibrateJoystick();

  // ESP-NOW başlat
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Init HATA — yeniden başlatılıyor");
    ledBlink(10, 50);
    ESP.restart();
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onSendCB);
  esp_now_register_recv_cb(onRecvCB);

  // Receiver peer ekle
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

  // --- Trim güncelle ---
  updateTrim();

  // --- Belirli aralıkta gönder ---
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;

    RCPacket pkt;
    pkt.throttle = (int8_t) readJoystick(0);
    pkt.steer    = (int8_t) constrain((int8_t) readJoystick(1) + (int8_t) trimValue, -100, 100);
    pkt.seq      = ++seqCounter;

    esp_now_send(rxMac, (uint8_t*)&pkt, sizeof(pkt));

    Serial.printf("TX → t:%4d / %4d  s:%4d / %4d  trim:%3d  seq:%3d / %3d  %s\n",
      pkt.throttle, tp.ack_throttle, pkt.steer, tp.ack_steer, (int8_t) trimValue, 
      pkt.seq,tp.ack_seq, rxConnected ? "ACK✓" : "---");
  }

  // --- Bağlantı kontrolü: 1 sn ACK gelmezse bağlantı yok ---
  if (rxConnected && (now - lastAckMs > 1000)) {
    rxConnected = false;
    Serial.println("[TX] Receiver yanıt vermiyor!");
  }

  // --- LED: bağlı → hızlı yanıp sönme, değil → yavaş ---
  if (now - lastLedMs > (rxConnected ? 100 : 500)) {
    lastLedMs = now;
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }

  yield();
}
