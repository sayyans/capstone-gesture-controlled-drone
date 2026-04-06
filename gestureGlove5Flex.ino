// https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
// install: esp32 by espressif systems

// 5 flex sensor version

#include <Wire.h>
#include <Adafruit_ICM20X.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <esp_now.h>

// hardware
Adafruit_ICM20948 imuSensor;

// 5 flex sensor pins
const int flexPins[5] = {32, 33, 34, 35, 39}; 

// moving average filtering with 5 flex sensors
const int SMOOTHING = 5;
float flexHistory[5][SMOOTHING] = {0};
int flexIndex = 0;
float flex[5];

uint8_t receiverAddress[] = {0x88, 0x57, 0x21, 0xAD, 0x25, 0x24};

typedef struct {
  char command[20];
} GestureData;

GestureData data;

// arm/disarm hold timing
const unsigned long ARM_HOLD_MS = 700;
unsigned long armStartMs = 0;
unsigned long disarmStartMs = 0;

// detect the flex sensor portion (hand pattern)
String detectHandPattern() {
  float straight = 0.7*10;  
  float bent = 0.7*10;      

  bool openHand = true;
  bool fist = true;

  for (int i = 0; i < 5; i++) {
    if (flex[i] * 10 < straight) {
      openHand = false;
    }

    if (flex[i] * 10 > bent) {
      fist = false;
    }
  }

  if (openHand) return "OPEN_HAND";
  if (fist) return "FIST";

  // index + middle fingers outward, others inward
  if (flex[1] > straight && flex[2] > straight &&
      flex[0] < bent && flex[3] < bent && flex[4] < bent)
    return "TWO_FINGERS_OUT";

  return "OTHER";
}

// combines hand pattern to gesture
String detectGesture(float pitch, float roll, bool isStill) {
  String hand = detectHandPattern();

  bool armPose = isStill && (hand == "FIST") && roll > 30;
  bool disarmPose = isStill && (hand == "FIST") && roll < -30;

  unsigned long now = millis();

  if (armPose) {
    if (armStartMs == 0) armStartMs = now;
    if (now - armStartMs >= ARM_HOLD_MS) {
      disarmStartMs = 0;
      return "ARM";
    }
  } else {
    armStartMs = 0;
  }

  if (disarmPose) {
    if (disarmStartMs == 0) disarmStartMs = now;
    if (now - disarmStartMs >= ARM_HOLD_MS) {
      armStartMs = 0;
      return "DISARM";
    }
  } else {
    disarmStartMs = 0;
  }

  if (hand == "OPEN_HAND" && pitch > 60) return "MOVE_UP";
  if (hand == "OPEN_HAND" && pitch < -60) return "MOVE_DOWN";

  if (hand == "OPEN_HAND" && pitch < -25) return "MOVE_FORWARD";
  if (hand == "OPEN_HAND" && pitch > 25)  return "MOVE_BACKWARD";

  if (hand == "OPEN_HAND" && roll > 45)  return "MOVE_RIGHT";
  if (hand == "OPEN_HAND" && roll < -45) return "MOVE_LEFT";

  if (hand == "FIST" && isStill) return "CLOSE_GRIPPER";
  if (hand == "OPEN_HAND" && isStill) return "OPEN_GRIPPER";

  return "NONE";
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!imuSensor.begin_I2C()) {
    Serial.println("Failed to find ICM20948 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("IMU initialized");

  WiFi.mode(WIFI_STA);
  delay(200);

  Serial.print("Sender MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Transmitter Ready");
}

void loop() {
  sensors_event_t accel, gyro, temp, mag;
  imuSensor.getEvent(&accel, &gyro, &temp, &mag);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  float gx = gyro.gyro.x;
  float gy = gyro.gyro.y;
  float gz = gyro.gyro.z;

  float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float roll  = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;

  bool isStill = abs(gx) < 15 && abs(gy) < 15 && abs(gz) < 15;

  // read & smooth flex sensors (moving average filter for flex sensors)
  for (int i = 0; i < 5; i++) {
    float raw = analogRead(flexPins[i]) / 409.50;
    flexHistory[i][flexIndex] = raw;

    float sum = 0;
    for (int j = 0; j < SMOOTHING; j++) sum += flexHistory[i][j];
    flex[i] = sum / SMOOTHING;
  }
  flexIndex = (flexIndex + 1) % SMOOTHING;

  // detect gesture
  String gesture = detectGesture(pitch, roll, isStill);
  String hand = detectHandPattern();

  // send ESP NOW every loop so receiver does not time out
  memset(data.command, 0, sizeof(data.command));
  gesture.toCharArray(data.command, sizeof(data.command));
  esp_now_send(receiverAddress, (uint8_t *)&data, sizeof(data));

  // printing values for debugging/visual
  Serial.print("Sent: [");
  Serial.print(data.command);
  Serial.println("]");

  Serial.print("Pitch: "); Serial.print(pitch, 2);
  Serial.print(" | Roll: "); Serial.print(roll, 2);
  Serial.print(" | Hand: "); Serial.print(hand);
  Serial.print(" | Smoothed Flex: [");
  for (int i = 0; i < 5; i++) {
    Serial.print(flex[i], 2);
    if (i < 4) Serial.print(", ");
  }
  Serial.print("] | Gesture: "); Serial.println(gesture);

  delay(50);
}