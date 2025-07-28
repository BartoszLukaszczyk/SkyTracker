#include "HomingAdvanced.h"
#include <Arduino.h>
#include <Wire.h>  // inicjalizację usuniemy stąd

HomingAdvanced::HomingAdvanced(
    AccelStepper& panStp, uint8_t panEndPin, uint8_t panEn, uint8_t panDir, uint8_t panStep,
    AccelStepper& tiltStp, uint8_t tiltEndPin, uint8_t tiltEn, uint8_t tiltDir, uint8_t tiltStep,
    float degPerMicroPan, float degPerMicroTilt,
    Adafruit_HMC5883_Unified& magSensor, MPU6050& mpuSensor,
    float homeSpeed, long backoffSteps
  )
  : panStepper(panStp)
  , panEndstopPin(panEndPin)
  , panEnablePin(panEn)
  , panDirPin(panDir)
  , panStepPin(panStep)
  , tiltStepper(tiltStp)
  , tiltEndstopPin(tiltEndPin)
  , tiltEnablePin(tiltEn)
  , tiltDirPin(tiltDir)
  , tiltStepPin(tiltStep)
  , degPerStepPan(degPerMicroPan)
  , degPerStepTilt(degPerMicroTilt)
  , mag(magSensor)
  , mpu(mpuSensor)
  , speedHome(homeSpeed)
  , backoff(backoffSteps)
{}

void HomingAdvanced::begin() {
  // I²C-inicjalizację wykonaj w szkicu:
  // Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // Sensory
  if (!mag.begin()) {
    Serial.println("Error: HMC5883L not found");
    while (1);
  }
  if (mpu.begin() != 0) {
    Serial.println("Error: MPU6050 init failed");
    while (1);
  }
  mpu.calcGyroOffsets();
  Serial.println("Sensors initialized");

  // Krańcówki
  pinMode(panEndstopPin, INPUT_PULLUP);
  pinMode(tiltEndstopPin, INPUT_PULLUP);

  // Driver-enable
  pinMode(panEnablePin, OUTPUT);
  pinMode(tiltEnablePin, OUTPUT);
  digitalWrite(panEnablePin, LOW);
  digitalWrite(tiltEnablePin, LOW);

  // Steppery
  panStepper.setMaxSpeed(speedHome);
  panStepper.setAcceleration(speedHome);
  tiltStepper.setMaxSpeed(speedHome);
  tiltStepper.setAcceleration(speedHome);
}

void HomingAdvanced::homeAll() {
  Serial.println("=== Starting full homing ===");
  homeAxis(panStepper, panEndstopPin);
  Serial.println("Pan homed at 0");
  homeAxis(tiltStepper, tiltEndstopPin);
  Serial.println("Tilt homed at 0");
  backoffBoth();
  Serial.println("Repositioning to 180° azimuth & 45° elevation...");
  reposition(180.0f, 45.0f);
  Serial.println("Repositioning done");
  Serial.println("Drivers disabled – full homing complete");
}

void HomingAdvanced::homeAxis(AccelStepper& stp, uint8_t endPin) {
  stp.setSpeed(speedHome);
  while (digitalRead(endPin) == HIGH) {
    stp.runSpeed();
  }
  stp.setCurrentPosition(0);
}

void HomingAdvanced::backoffBoth() {
  Serial.printf("Backoff %ld steps\n", backoff);
  panStepper.move(-backoff);
  tiltStepper.move(-backoff);
  panStepper.runToPosition();
  tiltStepper.runToPosition();
  Serial.println("Drivers disabled");
}

void HomingAdvanced::readOrientation(float &azDeg, float &elDeg) {
  sensors_event_t magEvent;
  mag.getEvent(&magEvent);
  mpu.update();
  float ax = mpu.getAccX(), ay = mpu.getAccY(), az = mpu.getAccZ();

  float heading = atan2(magEvent.magnetic.y, magEvent.magnetic.x) * 180.0 / M_PI;
  if (heading < 0) heading += 360.0;
  azDeg = heading;

  elDeg = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
}

void HomingAdvanced::moveAxis(uint8_t dirPin, uint8_t stepPin, float deltaDeg, float degPerMicrostep) {
  long steps = lround(abs(deltaDeg) / degPerMicrostep);
  digitalWrite(dirPin, (deltaDeg >= 0) ? HIGH : LOW);
  for (long i = 0; i < steps; ++i) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(500);
  }
}

void HomingAdvanced::reposition(float targetAz, float targetEl) {
  float currAz, currEl;
  readOrientation(currAz, currEl);

  float rawAz = fmod(targetAz - currAz + 360.0, 360.0);
  float deltaAz = (rawAz > 0) ? rawAz - 360.0 : rawAz;
  float deltaEl = targetEl - currEl;

  Serial.printf("⏩ Reposition: AZ %.2f°, EL %.2f°\n", deltaAz, deltaEl);
  moveAxis(panDirPin,  panStepPin,  deltaAz, degPerStepPan);
  moveAxis(tiltDirPin, tiltStepPin, -deltaEl, degPerStepTilt);
}
