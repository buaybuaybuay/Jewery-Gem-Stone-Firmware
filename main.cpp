#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>
#include <time.h>
#if __has_include("../secrets.h")
  #include "../secrets.h"
#else
  #include "../secrets.example.h"
#endif

// SAFETY: Keep both switches false until the physical board, wiring,
// power supply, servo model, stop behavior, and GPIO have been verified.
namespace {
constexpr char FIRMWARE_VERSION[] = "0.1.0";
constexpr char MODULE_MODEL[] = "ESP32-S3-WROOM-1";
constexpr char MODULE_VARIANT[] = "N16R8";
constexpr char BOARD_PROFILE[] = "custom-esp32-s3-wroom-1-n16r8";
constexpr bool REMOTE_CONTROL_ENABLED = false;
#if defined(WOKWI_SIMULATION)
constexpr bool MOTOR_OUTPUT_ENABLED = true;
constexpr int SERVO_PIN = 18;
constexpr int HC_SR04_TRIG_PIN = 1;
constexpr int HC_SR04_ECHO_PIN = 17;
constexpr int DETECTION_DISTANCE_CM = 50;
constexpr uint32_t SERVO_PWM_RESOLUTION_BITS = 14;
constexpr uint32_t SERVO_PWM_MAX_DUTY = (1UL << SERVO_PWM_RESOLUTION_BITS) - 1;
#else
constexpr bool MOTOR_OUTPUT_ENABLED = false;
constexpr int SERVO_PIN = -1;               // Set only after wiring is verified.
#endif
constexpr int SERVO_STOP_US = 1500;         // Calibrate for the actual servo.
constexpr int SERVO_ROTATE_US = 1600;       // Calibrate for the actual servo.
constexpr uint32_t SERVO_ROTATION_MS = 1000; // Not a verified 360-degree turn.
constexpr uint32_t MAX_ROTATION_MS = 10000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000;
constexpr uint32_t COMMAND_POLL_INTERVAL_MS = 2000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;

Servo turntableServo;
bool motorReady = false, rotationActive = false, remoteControlReady = false;
uint32_t rotationStartedAt = 0, lastHeartbeatAt = 0;
uint32_t lastCommandPollAt = 0, lastWifiAttemptAt = 0;
String activeCommandId;

#if defined(WOKWI_SIMULATION)
bool personDetected = false;
bool servoIncreasing = true;
int servoAngle = 90;
bool servoPwmReady = false;
uint32_t lastDistanceReadAt = 0, lastServoStepAt = 0, lastDistanceLogAt = 0;

void writeWokwiServoAngle(int angle) {
  if (!servoPwmReady) return;
  uint32_t pulseUs = 500 + (2000UL * angle) / 180;
  uint32_t duty = (pulseUs * SERVO_PWM_MAX_DUTY) / 20000UL;
  ledcWrite(SERVO_PIN, duty);
}

int measureDistanceCm() {
  digitalWrite(HC_SR04_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_SR04_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_SR04_TRIG_PIN, LOW);
  unsigned long duration = pulseIn(HC_SR04_ECHO_PIN, HIGH, 30000UL);
  return duration ? duration / 58 : 401;
}

void updateWokwiSimulation() {
  uint32_t now = millis();
  if (now - lastDistanceReadAt >= 100) {
    lastDistanceReadAt = now;
    int distanceCm = measureDistanceCm();
    if (!personDetected && distanceCm <= DETECTION_DISTANCE_CM) {
      personDetected = true;
      Serial.printf("{\"event\":\"person_detected\",\"distanceCm\":%d}\n", distanceCm);
    } else if (personDetected && distanceCm > DETECTION_DISTANCE_CM + 5) {
      personDetected = false;
      servoAngle = 90;
      writeWokwiServoAngle(servoAngle);
      Serial.printf("{\"event\":\"person_left\",\"distanceCm\":%d}\n", distanceCm);
    }
    if (now - lastDistanceLogAt >= 500) {
      lastDistanceLogAt = now;
      Serial.printf("{\"event\":\"distance\",\"distanceCm\":%d}\n", distanceCm);
    }
  }

  if (!personDetected || now - lastServoStepAt < 20) return;
  lastServoStepAt = now;
  if (servoIncreasing) {
    if (servoAngle >= 180) servoIncreasing = false;
    else ++servoAngle;
  } else {
    if (servoAngle <= 0) servoIncreasing = true;
    else --servoAngle;
  }
  writeWokwiServoAngle(servoAngle);
}
#endif

bool secretsConfigured() {
  return strlen(WIFI_SSID) && strlen(WIFI_PASSWORD) &&
         strlen(WORKER_BASE_URL) && strlen(DEVICE_TOKEN) &&
         strlen(CLOUDFLARE_ROOT_CA);
}
void reportCommandResult(const String&, const char*, const char*);
void stopMotor() {
  if (MOTOR_OUTPUT_ENABLED) {
#if defined(WOKWI_SIMULATION)
    writeWokwiServoAngle(90);
#else
    if (turntableServo.attached())
    turntableServo.writeMicroseconds(SERVO_STOP_US);
#endif
  }
  rotationActive = false;
}
void startRotation(const String& id) {
  if (!MOTOR_OUTPUT_ENABLED || !motorReady || rotationActive) {
    Serial.println("{\"event\":\"rotate_rejected\",\"reason\":\"motor_not_ready\"}");
    return;
  }
  activeCommandId = id;
  rotationStartedAt = millis();
  rotationActive = true;
  turntableServo.writeMicroseconds(SERVO_ROTATE_US);
  Serial.println("{\"event\":\"rotation_started\"}");
}
void serviceMotor() {
  if (!rotationActive) return;
  bool timeout = millis() - rotationStartedAt >= MAX_ROTATION_MS;
  bool done = millis() - rotationStartedAt >= SERVO_ROTATION_MS;
  bool disconnected = WiFi.status() != WL_CONNECTED;
  if (timeout || done || disconnected) {
    String id = activeCommandId;
    stopMotor();
    if (timeout || disconnected) {
      if (WiFi.status() == WL_CONNECTED && id.length())
        reportCommandResult(id, "failed", timeout ? "motor_timeout" : "wifi_disconnected");
      Serial.println("{\"event\":\"rotation_stopped\",\"status\":\"failed\"}");
    } else {
      if (id.length()) reportCommandResult(id, "completed", "timed_profile_finished");
      Serial.println("{\"event\":\"rotation_stopped\",\"status\":\"completed\"}");
    }
    activeCommandId = "";
  }
}
bool waitForClock() {
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  uint32_t start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 15000) delay(250);
  return time(nullptr) >= 1700000000;
}
bool beginSecureRequest(WiFiClientSecure& client, HTTPClient& http, const char* path) {
  if (!remoteControlReady || !waitForClock()) return false;
  String base = WORKER_BASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  if (!base.startsWith("https://")) return false;
  client.setCACert(CLOUDFLARE_ROOT_CA);
  if (!http.begin(client, base + path)) return false;
  http.setTimeout(4000);
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
  http.addHeader("Content-Type", "application/json");
  return true;
}
void sendHeartbeat() {
  if (!remoteControlReady || rotationActive || WiFi.status() != WL_CONNECTED ||
      millis() - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS) return;
  lastHeartbeatAt = millis();
  WiFiClientSecure client; HTTPClient http;
  if (!beginSecureRequest(client, http, "/device/heartbeat")) return;
  JsonDocument body;
  body["motorReady"] = motorReady && MOTOR_OUTPUT_ENABLED;
  body["pirDetected"] = nullptr; // Set to a real PIR reading only if PIR is installed.
  body["firmware"] = FIRMWARE_VERSION;
  String payload; serializeJson(body, payload);
  int code = http.POST(payload); http.end();
  if (code < 200 || code >= 300)
    Serial.printf("{\"event\":\"heartbeat_failed\",\"httpStatus\":%d}\n", code);
}
void reportCommandResult(const String& id, const char* status, const char* message) {
  WiFiClientSecure client; HTTPClient http;
  if (!beginSecureRequest(client, http, "/device/result")) return;
  JsonDocument body;
  body["commandId"] = id; body["status"] = status; body["message"] = message;
  String payload; serializeJson(body, payload);
  int code = http.POST(payload); http.end();
  if (code < 200 || code >= 300)
    Serial.printf("{\"event\":\"result_report_failed\",\"httpStatus\":%d}\n", code);
}
void pollCommand() {
  if (!remoteControlReady || !motorReady || rotationActive ||
      WiFi.status() != WL_CONNECTED ||
      millis() - lastCommandPollAt < COMMAND_POLL_INTERVAL_MS) return;
  lastCommandPollAt = millis();
  WiFiClientSecure client; HTTPClient http;
  if (!beginSecureRequest(client, http, "/device/next")) return;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return; }
  JsonDocument response;
  auto error = deserializeJson(response, http.getString());
  http.end();
  if (error || response["command"].isNull()) return;
  String command = response["command"]["command"] | "";
  String id = response["command"]["id"] | "";
  if (command != "rotate_360_once" || !id.length()) {
    reportCommandResult(id, "failed", "unsupported_command");
    return;
  }
  startRotation(id);
}
void connectWifi() {
  if (!strlen(WIFI_SSID) || WiFi.status() == WL_CONNECTED ||
      millis() - lastWifiAttemptAt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttemptAt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("{\"event\":\"wifi_connecting\"}");
}
void readSerialCommands() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n'); command.trim();
  if (command.equalsIgnoreCase("IDENTIFY")) {
    Serial.printf("{\"event\":\"identify\",\"protocolVersion\":1,"
      "\"chipModel\":\"%s\",\"chipRevision\":%u,\"chipCores\":%u,"
      "\"flashBytes\":%u,\"psramBytes\":%u,"
      "\"moduleModel\":\"%s\",\"moduleVariant\":\"%s\","
      "\"boardProfile\":\"%s\",\"firmware\":\"%s\"}\n",
      ESP.getChipModel(), (unsigned)ESP.getChipRevision(),
      (unsigned)ESP.getChipCores(), (unsigned)ESP.getFlashChipSize(),
      (unsigned)ESP.getPsramSize(), MODULE_MODEL, MODULE_VARIANT,
      BOARD_PROFILE, FIRMWARE_VERSION);
  } else if (command.equalsIgnoreCase("STATUS")) {
    Serial.printf("{\"event\":\"status\",\"firmware\":\"%s\","
      "\"chipModel\":\"%s\",\"moduleModel\":\"%s\","
      "\"moduleVariant\":\"%s\",\"boardProfile\":\"%s\","
      "\"wifiConnected\":%s,\"motorReady\":%s,"
      "\"remoteControlEnabled\":%s,\"motorOutputEnabled\":%s}\n",
      FIRMWARE_VERSION, ESP.getChipModel(), MODULE_MODEL, MODULE_VARIANT,
      BOARD_PROFILE,
      WiFi.status() == WL_CONNECTED ? "true" : "false",
      motorReady ? "true" : "false",
      remoteControlReady ? "true" : "false",
      MOTOR_OUTPUT_ENABLED ? "true" : "false");
  } else if (command.equalsIgnoreCase("STOP")) {
    String id = activeCommandId; stopMotor();
    if (id.length() && WiFi.status() == WL_CONNECTED)
      reportCommandResult(id, "failed", "manual_stop");
    activeCommandId = "";
    Serial.println("{\"event\":\"stop_requested\"}");
  } else {
    Serial.println("{\"error\":\"supported_commands_are_IDENTIFY_STATUS_STOP\"}");
  }
}
} // namespace

void setup() {
  Serial.begin(115200); Serial.setTimeout(50); delay(300);
#if defined(WOKWI_SIMULATION)
  pinMode(HC_SR04_TRIG_PIN, OUTPUT);
  pinMode(HC_SR04_ECHO_PIN, INPUT);
  digitalWrite(HC_SR04_TRIG_PIN, LOW);
  servoPwmReady = ledcAttach(SERVO_PIN, 50, SERVO_PWM_RESOLUTION_BITS);
  motorReady = servoPwmReady;
  if (motorReady) {
    writeWokwiServoAngle(servoAngle);
    Serial.println("{\"event\":\"wokwi_servo_pwm_ready\",\"gpio\":18}");
  } else {
    Serial.println("{\"error\":\"wokwi_servo_pwm_attach_failed\",\"gpio\":18}");
  }
#else
  if (MOTOR_OUTPUT_ENABLED && SERVO_PIN >= 0) {
    turntableServo.setPeriodHertz(50);
    turntableServo.attach(SERVO_PIN, 1000, 2000);
    turntableServo.writeMicroseconds(SERVO_STOP_US);
    motorReady = turntableServo.attached() && SERVO_ROTATION_MS > 0 &&
                 SERVO_ROTATION_MS <= MAX_ROTATION_MS;
  }
#endif
  remoteControlReady = REMOTE_CONTROL_ENABLED && secretsConfigured();
  if (remoteControlReady) connectWifi();
  Serial.printf("{\"event\":\"boot\",\"firmware\":\"%s\","
    "\"motorOutputEnabled\":%s,\"remoteControlEnabled\":%s}\n",
    FIRMWARE_VERSION, MOTOR_OUTPUT_ENABLED ? "true" : "false",
    remoteControlReady ? "true" : "false");
#if defined(WOKWI_SIMULATION)
  if (!motorReady)
    Serial.println("{\"error\":\"wokwi_servo_attach_failed\"}");
#endif
}
void loop() {
  readSerialCommands();
  serviceMotor();
#if defined(WOKWI_SIMULATION)
  updateWokwiSimulation();
#endif
  if (!remoteControlReady) {
    delay(10);
    return;
  }
  connectWifi(); sendHeartbeat(); pollCommand();
  delay(10);
}
