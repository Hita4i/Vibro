#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

SSD1306Wire myDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

#define RF_FREQUENCY            868000000
#define TX_OUTPUT_POWER         20
#define LORA_BANDWIDTH          0
#define LORA_SPREADING_FACTOR   12
#define LORA_CODINGRATE         1
#define LORA_PREAMBLE_LENGTH    8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON    false
#define RX_TIMEOUT_VALUE        3000
#define BUFFER_SIZE             64                     // збільшили буфер

char txpacket[BUFFER_SIZE];      // ← ОДИН ЄДИНИЙ БУФЕР!
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone();
void OnTxTimeout();
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

bool buttonPressed = false;
int speedPercent = 0;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void OnTxDone() {
  Serial.println("SPEED SENT");
  myDisplay.clear();
  myDisplay.drawString(0, 0, "MASTER");
  myDisplay.drawString(0, 15, "SENT: " + String(speedPercent) + "%");
  myDisplay.display();
  Radio.Rx(0);
}

void OnTxTimeout() {
  Radio.Sleep();
  Serial.println("TX TIMEOUT");
  Radio.Rx(0);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Serial.printf("RX: %s | RSSI: %d\n", rxpacket, rssi);

  myDisplay.clear();
  myDisplay.drawString(0, 0, "MASTER");
  myDisplay.drawString(0, 15, "RX: " + String(rxpacket));
  myDisplay.drawString(0, 30, "RSSI: " + String(rssi));
  myDisplay.display();

  Radio.Rx(0);
}

void setup() {

  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);

  VextON();
  delay(100);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  myDisplay.init();
  myDisplay.setContrast(255);
  myDisplay.clear();
  myDisplay.drawString(0, 0, "MASTER READY");
  myDisplay.drawString(0, 20, "PC CONTROL ACTIVE");
  myDisplay.display();

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    0, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
                    LORA_IQ_INVERSION_ON, true);

  Radio.Rx(0);

  Serial.println("");
  Serial.print("MASTER ГОТОВИЙ — ЧЕКАЮ КОМАНД З ПК");
  Serial.println("");
}

void loop() {
  Radio.IrqProcess();

  // ─────── ПРИЙОМ КОМАНД З ПК ───────
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;

    Serial.println("");
    Serial.println("╔══════════════════════════════════════╗");
    Serial.printf ("║   ПРИЙНЯТО З ПК: %-22s ║\n", cmd.c_str());
    Serial.println("╚══════════════════════════════════════╝");

    if (cmd.indexOf("ТЕСТОВЫЙ СТАРТ") >= 0 || cmd.indexOf("ТЕСТОВИЙ СТАРТ") >= 0) {
      Serial.println("ТЕСТОВИЙ ЗАПУСК З КОМП'ЮТЕРА!");
    }

    if (cmd.startsWith("SPEED:")) {
      int val = cmd.substring(6).toInt();
      if (val >= 0 && val <= 100) {
        speedPercent = val;

        sprintf(txpacket, "SPEED:%d", speedPercent);
        Serial.printf("НАДІСЛАНО ПО LORA → %s\n", txpacket);
        Radio.Send((uint8_t*)txpacket, strlen(txpacket));   // ← правильний буфер!

        Serial.printf("ДВИГУН ВСТАНОВЛЕНО НА %d%%\n", speedPercent);
        Serial.flush();                    // дочекаємося відправки
        delay(50);                         // маленька пауза для CH340
        Serial.println("OK");             // ← ОТВЕТ, КОТОРЫЙ C# ЖДЕТ!
      }
    }
  }

  // ─────── КНОПКА НА ПЛАТІ (залишаємо) ───────
  if (digitalRead(0) == LOW && !buttonPressed) {
    delay(50);
    if (digitalRead(0) == LOW) {
      buttonPressed = true;
      speedPercent = (speedPercent + 25) % 125;
      if (speedPercent > 100) speedPercent = 0;

      sprintf(txpacket, "SPEED:%d", speedPercent);
      Serial.printf("КНОПКА → LORA: %s\n", txpacket);
      Radio.Send((uint8_t*)txpacket, strlen(txpacket));
    }
  }
  if (digitalRead(0) == HIGH) buttonPressed = false;

  delay(10);
}
