#include <Arduino.h>
#include <ESP32Servo.h>

namespace {
constexpr int DEMO_SERVO_PIN = 18;
constexpr int SERVO_CENTER_DEGREES = 90;
constexpr int SWEEP_STEP_DEGREES = 2;
constexpr uint32_t SWEEP_STEP_INTERVAL_MS = 50;

Servo demoServo;
int angle = 0;
int direction = 1;
bool sweepActive = false;
bool servoReady = false;
uint32_t lastStepAt = 0;

void startDemo() {
  if (sweepActive) return;
  if (!servoReady) {
    Serial.println("{\"error\":\"servo_attach_failed\"}");
    return;
  }
  angle = 0;
  direction = 1;
  sweepActive = true;
  lastStepAt = millis();
  demoServo.write(angle);
  Serial.println("{\"event\":\"demo_started\",\"mode\":\"servo_sweep_0_to_180\"}");
}

void stopDemo() {
  sweepActive = false;
  demoServo.write(SERVO_CENTER_DEGREES);
  Serial.println("{\"event\":\"demo_stopped\"}");
}

void readSerialCommands() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command.equalsIgnoreCase("DEMO")) {
    startDemo();
  } else if (command.equalsIgnoreCase("STOP")) {
    stopDemo();
  } else if (command.equalsIgnoreCase("STATUS")) {
    Serial.printf("{\"event\":\"status\",\"simulationOnly\":true,"
                  "\"servoAttached\":%s,\"sweepActive\":%s,\"angle\":%d}\n",
                  servoReady ? "true" : "false",
                  sweepActive ? "true" : "false", angle);
  } else {
    Serial.println("{\"error\":\"supported_commands_are_DEMO_STOP_STATUS\"}");
  }
}

void serviceDemo() {
  if (!sweepActive || millis() - lastStepAt < SWEEP_STEP_INTERVAL_MS) return;
  lastStepAt = millis();
  angle += direction * SWEEP_STEP_DEGREES;
  if (angle >= 180) {
    angle = 180;
    direction = -1;
  } else if (angle <= 0 && direction < 0) {
    angle = 0;
    direction = 1;
    Serial.println("{\"event\":\"demo_cycle_completed\",\"result\":\"simulated_sweep_not_360_degree_calibration\"}");
  }
  demoServo.write(angle);
}
} // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(300);
  demoServo.setPeriodHertz(50);
  demoServo.attach(DEMO_SERVO_PIN, 500, 2500);
  servoReady = demoServo.attached();
  demoServo.write(SERVO_CENTER_DEGREES);
  Serial.printf("{\"event\":\"simulator_ready\",\"simulationOnly\":true,"
                "\"servoPin\":18,\"servoAttached\":%s,"
                "\"physicalPinMappingVerified\":false}\n",
                servoReady ? "true" : "false");
  startDemo();
}

void loop() {
  readSerialCommands();
  serviceDemo();
  delay(1);
}
