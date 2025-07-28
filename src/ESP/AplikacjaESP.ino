#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <MPU6050_light.h>
#include <AccelStepper.h>
#include <BluetoothSerial.h>
#include <FS.h>
#include <SPIFFS.h>

#include "HomingAdvanced.h"
#include "SphericalTracker.h"

// --- I2C pins ---
#define SDA_PIN         25
#define SCL_PIN         26

// --- Motor pins ---
#define PAN_STEP_PIN    18
#define PAN_DIR_PIN     19
#define PAN_ENABLE_PIN  5
#define PAN_ENDSTOP_PIN 32

#define TILT_STEP_PIN    21
#define TILT_DIR_PIN     22
#define TILT_ENABLE_PIN  4
#define TILT_ENDSTOP_PIN 33

// --- Mechanical parameters ---
const float STEP_ANGLE_DEG  = 1.8;
const int   MICROSTEPS      = 8;
const float GEAR_MOTOR      = 14.0;
const float GEAR_AXIS_PAN   = 180.0;
const float GEAR_AXIS_TILT  = 84.0;

const float gearRatioPan    = GEAR_MOTOR / GEAR_AXIS_PAN;
const float gearRatioTilt   = GEAR_MOTOR / GEAR_AXIS_TILT;

const float degPerMicroPan  = (STEP_ANGLE_DEG/MICROSTEPS) * gearRatioPan;
const float degPerMicroTilt = (STEP_ANGLE_DEG/MICROSTEPS) * gearRatioTilt;

// --- Homing params ---
const float HOME_SPEED    = 500.0;
const long  BACKOFF       = 100;

// --- Sensors & Steppers ---
Adafruit_HMC5883_Unified mag(12345);
MPU6050                    mpu(Wire);
AccelStepper               panStp(AccelStepper::DRIVER, PAN_STEP_PIN, PAN_DIR_PIN);
AccelStepper               tiltStp(AccelStepper::DRIVER, TILT_STEP_PIN, TILT_DIR_PIN);

bool waitingForPrep = false;
bool prepExecuted = false;

HomingAdvanced homing(
  panStp, PAN_ENDSTOP_PIN, PAN_ENABLE_PIN, PAN_DIR_PIN, PAN_STEP_PIN,
  tiltStp, TILT_ENDSTOP_PIN, TILT_ENABLE_PIN, TILT_DIR_PIN, TILT_STEP_PIN,
  degPerMicroPan, degPerMicroTilt,
  mag, mpu,
  HOME_SPEED, BACKOFF
);

// --- Bluetooth ---
BluetoothSerial SerialBT;
const char*      btName    = "ESP32-Tracker";
const char*      btPin     = "1234";
bool             wasClient = false;

// --- Trajectory buffer ---
constexpr int MAX_POINTS = 1024;
static TrackPoint trajBuf[MAX_POINTS];
static uint16_t   expectPts = 0, recvPts = 0;
static uint32_t   T0_unix   = 0;
static bool       inRxTraj  = false;

// --- Tracker instance ---
SphericalTracker tracker(panStp, tiltStp, degPerMicroPan, degPerMicroTilt, MAX_POINTS);

// --- Manual drive flags & speeds ---
bool  movingPan  = false;
bool  movingTilt = false;
float speedPan   = 0;
float speedTilt  = 0;


void processCmd(const String &raw, bool viaBT) {
  String cmd = raw;
  cmd.trim();

  if (cmd == "HOME") {
    homing.homeAll();
    if (viaBT) SerialBT.println("HOMED"); else Serial.println("HOMED");
  }
  else if (cmd == "PING") {
    if (viaBT) SerialBT.println("PONG"); else Serial.println("PONG");
  }
  else if (cmd == "BREAK") {
    // tracker.stop();
    if (viaBT) SerialBT.println("TRACK_STOPPED"); else Serial.println("TRACK_STOPPED");
  }
  // Single-step manual
  else if (cmd == "UP") {
    tiltStp.enableOutputs();
    tiltStp.move(-5);
    tiltStp.runToPosition();
    tiltStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "DOWN") {
    tiltStp.enableOutputs();
    tiltStp.move(+5);
    tiltStp.runToPosition();
    tiltStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "LEFT") {
    panStp.enableOutputs();
    panStp.move(-5);
    panStp.runToPosition();
    panStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "RIGHT") {
    panStp.enableOutputs();
    panStp.move(+5);
    panStp.runToPosition();
    panStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  // Continuous manual start
  else if (cmd == "UP_START") {
    movingTilt = true;
    speedTilt  = -500;
    tiltStp.enableOutputs();
    tiltStp.setSpeed(speedTilt);
  }
  else if (cmd == "DOWN_START") {
    movingTilt = true;
    speedTilt  = +500;
    tiltStp.enableOutputs();
    tiltStp.setSpeed(speedTilt);
  }
  else if (cmd == "LEFT_START") {
    movingPan  = true;
    speedPan   = -1000;
    panStp.enableOutputs();
    panStp.setSpeed(speedPan);
  }
  else if (cmd == "RIGHT_START") {
    movingPan  = true;
    speedPan   = +1000;
    panStp.enableOutputs();
    panStp.setSpeed(speedPan);
  }else if (cmd == "FUP") {
    tiltStp.enableOutputs();
    tiltStp.move(-1);
    tiltStp.runToPosition();
    tiltStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "FDOWN") {
    tiltStp.enableOutputs();
    tiltStp.move(+1);
    tiltStp.runToPosition();
    tiltStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "FLEFT") {
    panStp.enableOutputs();
    panStp.move(-1);
    panStp.runToPosition();
    panStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "FRIGHT") {
    panStp.enableOutputs();
    panStp.move(+1);
    panStp.runToPosition();
    panStp.disableOutputs();
    if (viaBT) SerialBT.println("OK"); else Serial.println("OK");
  }
  else if (cmd == "STOP") {
    movingPan = movingTilt = false;
    panStp.setSpeed(0);
    tiltStp.setSpeed(0);
    if (viaBT) SerialBT.println("MAN_STOPPED"); else Serial.println("MAN_STOPPED");
  }
  else if (cmd == "PREP") {
    waitingForPrep = true;
    prepExecuted = false;
    if (viaBT) SerialBT.println("PREP_OK"); else Serial.println("PREP_OK");
  }
  else if (waitingForPrep && !prepExecuted && cmd.startsWith("STEP ")) {
    prepExecuted = true;
    waitingForPrep = false;
    int space1 = cmd.indexOf(' ');
    int space2 = cmd.indexOf(' ', space1 + 1);

    if (space1 == -1 || space2 == -1) {
      if (viaBT) SerialBT.println("PREP_BAD_FORMAT");
      else       Serial.println("PREP_BAD_FORMAT");
      return;
    }

    int deltaPan  = cmd.substring(space1 + 1, space2).toInt();
    int deltaTilt = cmd.substring(space2 + 1).toInt();

    Serial.printf("[ESP] PREP Step: Pan=%d, Tilt=%d\n", deltaPan, deltaTilt);

    panStp.enableOutputs();
    tiltStp.enableOutputs();
    panStp.move(deltaPan/2);
    tiltStp.move(-deltaTilt/2);
    while (panStp.isRunning() || tiltStp.isRunning()) {
      panStp.run();
      tiltStp.run();
    }
    panStp.disableOutputs();
    tiltStp.disableOutputs();

    if (viaBT) SerialBT.println("PREP_DONE"); else Serial.println("PREP_DONE");
  }
  else if (cmd.startsWith("STEP ")) {
    Serial.println("[ESP] Rejected repeated STEP");
  }
  else {
    if (viaBT) SerialBT.printf("UNKNOWN:%s\n", cmd.c_str());
    else        Serial.printf("UNKNOWN:%s\n", cmd.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);

  // Bluetooth init
  SerialBT.begin(btName, true);
  SerialBT.setPin(btPin, strlen(btPin));
  SerialBT.enableSSP();
  Serial.printf("BT \"%s\" PIN %s\n", btName, btPin);

  // Homing & steppers config
  homing.begin();
  panStp.setMaxSpeed(4000);
  panStp.setAcceleration(4000);
  tiltStp.setMaxSpeed(2000);
  tiltStp.setAcceleration(1000);
}

void loop() {
  // BT client tracking
  bool client = SerialBT.hasClient();
  if (client && !wasClient) {
    Serial.println("CLIENT_CONNECTED");
    SerialBT.println("CLIENT_CONNECTED");
  }
  wasClient = client;

  if (client && SerialBT.available()) {
    size_t avail = SerialBT.available();
    String line = SerialBT.readStringUntil('\n');
    line.trim();
    Serial.printf("[ESP] Recived raw: '%s'\n", line.c_str());
    processCmd(line, true);
  }

  // Read USB
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    processCmd(line, false);
  }
  // Read BT
  if (client && SerialBT.available()) {
    String line = SerialBT.readStringUntil('\n');
    processCmd(line, true);
  }

  uint32_t nowSec = uint32_t(time(nullptr));

  if (tracker.isTracking()) {
    // automatic tracking
    tracker.update(nowSec);
    tracker.runSteppers();
  } else {
    // manual continuous
    if (movingPan)   panStp.runSpeed(); else panStp.disableOutputs();
    if (movingTilt)  tiltStp.runSpeed(); else tiltStp.disableOutputs();
  }

  delay(10);
}
