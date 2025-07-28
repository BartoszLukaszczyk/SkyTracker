#ifndef HOMING_ADVANCED_H
#define HOMING_ADVANCED_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <MPU6050_light.h>
#include <AccelStepper.h>

class HomingAdvanced {
public:
  HomingAdvanced(
    AccelStepper& panStp, uint8_t panEndPin, uint8_t panEnablePin, uint8_t panDirPin, uint8_t panStepPin,
    AccelStepper& tiltStp, uint8_t tiltEndPin, uint8_t tiltEnablePin, uint8_t tiltDirPin, uint8_t tiltStepPin,
    float degPerMicroPan, float degPerMicroTilt,
    Adafruit_HMC5883_Unified& magSensor, MPU6050& mpuSensor,
    float homeSpeed, long backoffSteps
  );

  void begin();
  void homeAll();

private:
  void homeAxis(AccelStepper& stp, uint8_t endPin);
  void backoffBoth();
  void readOrientation(float &azDeg, float &elDeg);
  void reposition(float targetAz, float targetEl);
  void moveAxis(uint8_t dirPin, uint8_t stepPin, float deltaDeg, float degPerMicrostep);

  AccelStepper& panStepper;
  uint8_t panEndstopPin, panEnablePin, panDirPin, panStepPin;
  AccelStepper& tiltStepper;
  uint8_t tiltEndstopPin, tiltEnablePin, tiltDirPin, tiltStepPin;
  float degPerStepPan, degPerStepTilt;
  Adafruit_HMC5883_Unified& mag;
  MPU6050& mpu;
  float speedHome;
  long backoff;
};

#endif // HOMING_ADVANCED_H
