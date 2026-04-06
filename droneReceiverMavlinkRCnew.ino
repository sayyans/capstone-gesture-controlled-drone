#include <WiFi.h>
#include <esp_now.h>
#include <Arduino.h>
#include <MAVLink.h> // protocol: acts as an interface between ESP32 and flight controller (allowing us to override the joystick inputs while still using the flight controllers built in stabilization)
#include <ESP32Servo.h>

Servo clawServo; // servo for gripper

// flight controller settings
static const int FC_TX_PIN = 2;          // ESP32 TX - FC RX
static const uint32_t FC_BAUD = 57600;
static const uint32_t USB_BAUD = 115200;

// MAVLink IDs
static const uint8_t MY_SYS_ID = 255;       //esp
static const uint8_t MY_COMP_ID = MAV_COMP_ID_TELEMETRY_RADIO;
static const uint8_t TARGET_SYS_ID = 1;     //flight controller
static const uint8_t TARGET_COMP_ID = 1;

HardwareSerial FCSerial(2);

// data received from glove
typedef struct {
  char command[20];
} GestureData;

GestureData receivedData;

// RC channel values (neutral = 1500)
uint16_t rollCmd = 1500;
uint16_t pitchCmd = 1500;
uint16_t throttleCmd = 1100;   // start low but not too low
uint16_t yawCmd = 1500;

// timing
uint32_t lastHeartbeatMs = 0;
uint32_t lastRcMs = 0;
uint32_t lastPacketMs = 0;

// flag to check if drone is armed
bool armedState = false;

// "sends message" to the flight controller that the system is active
void sendHeartbeat() {
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_heartbeat_pack(
    MY_SYS_ID,
    MY_COMP_ID,
    &msg,
    MAV_TYPE_GCS,
    MAV_AUTOPILOT_INVALID,
    0,
    0,
    MAV_STATE_ACTIVE
  );

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  FCSerial.write(buf, len);
}

// arm/disarm drone motors
void sendArmCommand(bool arm) {
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_command_long_pack(
    MY_SYS_ID,
    MY_COMP_ID,
    &msg,
    TARGET_SYS_ID,
    TARGET_COMP_ID,
    MAV_CMD_COMPONENT_ARM_DISARM,
    0,
    arm ? 1.0f : 0.0f,
    0, 0, 0, 0, 0, 0
  );

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  FCSerial.write(buf, len);

  Serial.println(arm ? "ARM command sent" : "DISARM command sent");
}

// sends control signals to drone of RC channel type
void sendRCOverride(uint16_t roll, uint16_t pitch, uint16_t throttle, uint16_t yaw) {
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_rc_channels_override_pack(
    MY_SYS_ID,
    MY_COMP_ID,
    &msg,
    TARGET_SYS_ID,
    TARGET_COMP_ID,
    roll,
    pitch,
    throttle,
    yaw,
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
    UINT16_MAX, UINT16_MAX
  );

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  FCSerial.write(buf, len);
}

// returns controls to neutral position
void neutralAxes() {
  rollCmd = smooth(rollCmd, 1500);
  pitchCmd = smooth(pitchCmd, 1500);
  yawCmd = smooth(yawCmd, 1500);
}

void printRCState() {
  Serial.print("RC - Roll: ");
  Serial.print(rollCmd);
  Serial.print(" Pitch: ");
  Serial.print(pitchCmd);
  Serial.print(" Throttle: ");
  Serial.print(throttleCmd);
  Serial.print(" Yaw: ");
  Serial.println(yawCmd);
}

// smoothing sudden changes so it doesn't jerk 
uint16_t smooth(uint16_t current, uint16_t target, int step = 10) {
  if (current < target) return min(current + step, target);
  if (current > target) return max(current - step, target);
  return current;
}

// data received from glove - map commands to rc channel drone values
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len != sizeof(GestureData)) {
    Serial.print("Received size mismatch: ");
    Serial.println(len);
    return;
  }

  memcpy(&receivedData, incomingData, sizeof(receivedData));
  lastPacketMs = millis();

  String cmd = String(receivedData.command);
  cmd.trim();
  cmd.toUpperCase();

  Serial.print("Received gesture: [");
  Serial.print(cmd);
  Serial.println("]");

  // arm
  if (cmd == "ARM") {
    neutralAxes();
    throttleCmd = smooth(throttleCmd, 1200);   // keep above low-idle so FC stays armed
    sendArmCommand(true);
    armedState = true;
    printRCState();
    return;
  }

  // disarm
  if (cmd == "DISARM") {
    neutralAxes();
    throttleCmd = smooth(throttleCmd, 1000);
    sendArmCommand(false);
    armedState = false;
    printRCState();
    return;
  }

  if (!armedState) {
    Serial.println("Ignoring movement: FC not armed");
    return;
  }

  // can delete if causing blocky behaviour
  //neutralAxes();

  // directional movement
  if (cmd == "MOVE_FORWARD") {
    pitchCmd = smooth(pitchCmd, 1600);
  }

  else if (cmd == "MOVE_BACKWARD") {
    pitchCmd = smooth(pitchCmd, 1400);
  }

  else if (cmd == "MOVE_RIGHT") {
    rollCmd = smooth(rollCmd, 1600);
    pitchCmd = smooth(pitchCmd, 1500);
    yawCmd = smooth(yawCmd, 1500);
  }

  else if (cmd == "MOVE_LEFT") {
    rollCmd = smooth(rollCmd, 1400);
    pitchCmd = smooth(pitchCmd, 1500);
    yawCmd = smooth(yawCmd, 1500);
  }

  else if (cmd == "MOVE_UP") {
    throttleCmd = min<uint16_t>(throttleCmd + 40, 1250);
  }

  else if (cmd == "MOVE_DOWN") {
    throttleCmd = max<uint16_t>(throttleCmd - 40, 1150);
  }

  // gripper
  else if (cmd == "OPEN_GRIPPER") {
    clawServo.writeMicroseconds(2000);
  }

  else if (cmd == "CLOSE_GRIPPER") {
    clawServo.writeMicroseconds(1000);
  }

  else if (cmd == "NONE") {
    // keep throttle as-is, just neutralize roll/pitch/yaw
    neutralAxes();
    Serial.println("Stabilizing...");
  }

  else {
    Serial.println("Unknown gesture");
  }

  printRCState();
}

void setup() {
  Serial.begin(USB_BAUD);
  delay(1000);

  FCSerial.begin(FC_BAUD, SERIAL_8N1, -1, FC_TX_PIN);

  WiFi.mode(WIFI_STA);
  delay(200);

  // setup servo for gripper
  clawServo.setPeriodHertz(50);          // standard servo frequency
  clawServo.attach(27, 1000, 2000);      // pin, min pulse, max pulse
  clawServo.writeMicroseconds(1500);     // center


  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  neutralAxes();
  throttleCmd = 1000;

  Serial.println("Gesture MAVLink receiver ready");
  Serial.println("Send ARM gesture to arm");
}

void loop() {
  uint32_t now = millis();

  // keep MAVLink alive (send every 1 sec)
  if (now - lastHeartbeatMs >= 1000) {
    sendHeartbeat();
    lastHeartbeatMs = now;
  }

  // failsafe: instead of 2000 (2secs), changed to 5000 (5 secs): this was probably why the drone ran for a few seconds then stopped
  if (now - lastPacketMs > 5000) {  
    neutralAxes();

    if (armedState) {
      // keep current throttle while armed
      static uint32_t lastPrintMs = 0;
      if (now - lastPrintMs > 1000) {
        Serial.println("Failsafe: no gesture packets");
        printRCState();
        lastPrintMs = now;
      }
    } else {
      throttleCmd = smooth(throttleCmd, 1000);
    }
  }

  // continuously send RC signals (every 20ms)
  if (now - lastRcMs >= 20) {
    sendRCOverride(rollCmd, pitchCmd, throttleCmd, yawCmd);
    lastRcMs = now;
  }
}

