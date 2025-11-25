#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ESP32Servo.h>
#include <MAVLink.h>
SSD1306Wire myDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
Servo esc;
#define ESC_PIN 19        // Белый провод → GPIO19
#define PRG_PIN 0         // Кнопка PRG на SLAVE
HardwareSerial FC(2);  // UART2 для Pixhawk (RX=46, TX=45)


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
  //-------------------------------------------------------------------
  FC.begin(115200, SERIAL_8N1, 46, 45);
  Serial.println("Ожидание MAVLink-сообщений от Pixhawk...");
  //-------------------------------------------------------------------
  pinMode(PRG_PIN, INPUT_PULLUP);  // Кнопка на SLAVE
  VextON();
  delay(100);
    
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  myDisplay.init();
  myDisplay.setContrast(255);
  myDisplay.clear();
  myDisplay.drawString(0, 0, "WAITING...");
  myDisplay.drawString(0, 15, "PRG = 1200us TEST");
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
  static mavlink_message_t msg;
  static mavlink_status_t status;

  static uint8_t buffer[280];
  static uint16_t index = 0;

// ===================== MAVLINK V2 ПАРСЕР =====================
  while (FC.available() > 0) {
    uint8_t c = FC.read();

    // парсимо байти через бібліотеку
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      // коли отримали повний пакет
      switch (msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
          Serial.println("✅ HEARTBEAT отримано!");
          break;
        }

        case MAVLINK_MSG_ID_VIBRATION: {
          mavlink_vibration_t vibration;
          mavlink_msg_vibration_decode(&msg, &vibration);

          Serial.println("📊 VIBRATION:");
          Serial.print("  Vibration X: "); Serial.println(vibration.vibration_x, 6);
          Serial.print("  Vibration Y: "); Serial.println(vibration.vibration_y, 6);
          Serial.print("  Vibration Z: "); Serial.println(vibration.vibration_z, 6);
          myDisplay.clear();
          myDisplay.drawString(70, 30, "Vib x: " + String(vibration.vibration_x, 3));
          myDisplay.drawString(70, 40, "Vib y: " + String(vibration.vibration_y, 3));
          myDisplay.drawString(70, 50, "Vib z: " + String(vibration.vibration_z, 3));
          myDisplay.display();
          if(vibration.clipping_0 > 0 || vibration.clipping_1 > 0 || vibration.clipping_2 > 0){
            Serial.print("  Clipping 0: "); Serial.println(vibration.clipping_0);
            Serial.print("  Clipping 1: "); Serial.println(vibration.clipping_1);
            Serial.print("  Clipping 2: "); Serial.println(vibration.clipping_2);
          }
          
          break;
        }

        default:
          // інші повідомлення можна обробляти тут
          break;
      }
    }
  }
  // ============================================================

  // Постоянный ШИМ 900 мкс, если мотор не работает
  if (!motorRunning && !localTest) {
    esc.writeMicroseconds(900);
  }

  // Кнопка PRG — локальный тест
  if (digitalRead(PRG_PIN) == LOW && !motorRunning) {
    delay(20);
    if (digitalRead(PRG_PIN) == LOW) {
      setMotorTest();
      while (digitalRead(PRG_PIN) == LOW) delay(10);
    }
  }

  delay(10);
}

