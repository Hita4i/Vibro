#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ESP32Servo.h>

SSD1306Wire myDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
Servo esc;
#define ESC_PIN 19        // Белый провод → GPIO19
#define PRG_PIN 0         // Кнопка PRG на SLAVE

#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 20
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 12
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 3000
#define BUFFER_SIZE 32

char rxpacket[BUFFER_SIZE];
int currentSpeed = 0;
unsigned long motorStartTime = 0;
bool motorRunning = false;
bool localTest = false;

static RadioEvents_t RadioEvents;
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setMotorSpeed(int percent) {
  int pulse = 1100 + (percent * 9);  // 0% → 1100, 100% → 2000
  esc.writeMicroseconds(pulse);
  currentSpeed = percent;
  motorRunning = true;
  motorStartTime = millis();
  localTest = false;

  myDisplay.clear();
  myDisplay.drawString(0, 0, "SLAVE");
  myDisplay.drawString(0, 15, "MOTOR: " + String(percent) + "%");
  myDisplay.drawString(0, 30, "RUNNING...");
  myDisplay.display();
}

void setMotorTest() {
  esc.writeMicroseconds(1200);  // Фикс 1200 мкс
  motorRunning = true;
  motorStartTime = millis();
  localTest = true;

  myDisplay.clear();
  myDisplay.drawString(0, 0, "LOCAL TEST");
  myDisplay.drawString(0, 15, "1200 us");
  myDisplay.drawString(0, 30, "RUNNING...");
  myDisplay.display();
}

void stopMotor() {
  esc.writeMicroseconds(1100);  // Полный стоп
  motorRunning = false;

  myDisplay.clear();
  myDisplay.drawString(0, 0, "SLAVE");
  if (localTest) {
    myDisplay.drawString(0, 15, "LOCAL TEST OFF");
  } else {
    myDisplay.drawString(0, 15, "MOTOR OFF");
  }
  myDisplay.display();
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Serial.printf("RX: %s | RSSI: %d\n", rxpacket, rssi);

  if (strncmp(rxpacket, "SPEED:", 6) == 0) {
    int percent = atoi(rxpacket + 6);
    if (percent >= 0 && percent <= 100) {
      setMotorSpeed(percent);
    }
  }

  Radio.Rx(0);
}

void setup() {
  Serial.begin(115200);
  pinMode(PRG_PIN, INPUT_PULLUP);  // Кнопка на SLAVE

  VextON();
  delay(100);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  myDisplay.init();
  myDisplay.setContrast(255);
  myDisplay.clear();
  myDisplay.drawString(0, 0, "SLAVE");
  myDisplay.drawString(0, 15, "WAITING...");
  myDisplay.drawString(0, 30, "PRG = 1200us TEST");
  myDisplay.display();

  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(1100);  // Стоп

  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    0, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
                    LORA_IQ_INVERSION_ON, true);

  Radio.Rx(0);
}

void loop() {
  Radio.IrqProcess();
  // Постоянный ШИМ 900 мкс, если мотор не работает
  if (!motorRunning && !localTest) {
      esc.writeMicroseconds(900);
  }
  // === ЛОКАЛЬНАЯ ПРОВЕРКА ПО КНОПКЕ PRG НА SLAVE ===
  if (digitalRead(PRG_PIN) == LOW && !motorRunning) {
    delay(50);
    if (digitalRead(PRG_PIN) == LOW) {
      setMotorTest();
      while (digitalRead(PRG_PIN) == LOW);  // Ждём отпускания
    }
  }

  // === АВТООТКЛЮЧЕНИЕ ЧЕРЕЗ 20 СЕКУНД ===
  //if (motorRunning && (millis() - motorStartTime > 20000)) {
  //  stopMotor();
  //}

  delay(10);
}
