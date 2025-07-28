#include "SphericalTracker.h"
#include <Arduino.h>
#include <cmath>

SphericalTracker::SphericalTracker(AccelStepper &panStp,
                                   AccelStepper &tiltStp,
                                   float degPerStepPan,
                                   float degPerStepTilt,
                                   int maxPoints)
    : panStp_(panStp)
    , tiltStp_(tiltStp)
    , degPerStepPan_(degPerStepPan)
    , degPerStepTilt_(degPerStepTilt)
    , maxPoints_(maxPoints)
    , buffer_(nullptr)
    , numPoints_(0)
    , T0_unix_(0)
    , currentIndex_(0)
    , tracking_(false)
    , executedPan_(0)
    , executedTilt_(0)
{
    // Alokuj bufor trajektorii
    buffer_ = new TrackPoint[maxPoints_];
}

void SphericalTracker::loadTrajectory(const TrackPoint *traj, int nPts, uint32_t T0_unix) {
    numPoints_ = (nPts > maxPoints_) ? maxPoints_ : nPts;
    for (int i = 0; i < numPoints_; ++i) {
        buffer_[i] = traj[i];
    }
    T0_unix_ = T0_unix;
}

void SphericalTracker::prepare() {
    startMillis_  = millis();
    currentIndex_ = 0;
    executedPan_  = 0;
    executedTilt_ = 0;
    tracking_     = true;
}

bool SphericalTracker::isTracking() const {
    return tracking_ && (currentIndex_ < numPoints_);
}

float SphericalTracker::angularDiff(float target, float origin) {
    // Różnica kąta w przedziale (-180,180]
    float diff = fmod(target - origin + 540.0f, 360.0f) - 180.0f;
    return diff;
}

void SphericalTracker::update(uint32_t nowSec) {
    if (!tracking_) return;
    // Oblicz upływ milisekund od T0
    uint32_t elapsedMs = (nowSec - T0_unix_) * 1000ul;
    // Dla każdego punktu, którego czas już minął, ustaw prędkości
    while (currentIndex_ < numPoints_ && buffer_[currentIndex_].t * 1000u <= elapsedMs) {
        // --- Pan ---
        float rawDeltaPan   = angularDiff(buffer_[currentIndex_].az, buffer_[0].az);
        int   desiredPan    = lround(rawDeltaPan / degPerStepPan_);
        int   stepPan       = desiredPan - executedPan_;
        executedPan_       += stepPan;
        panStp_.setSpeed((stepPan >= 0) ? 1000 : -1000);

        // --- Tilt ---
        float rawDeltaTilt  = buffer_[currentIndex_].el - buffer_[0].el;
        int   desiredTilt   = lround(rawDeltaTilt / degPerStepTilt_);
        int   stepTilt      = desiredTilt - executedTilt_;
        executedTilt_      += stepTilt;
        tiltStp_.setSpeed((stepTilt >= 0) ? 500 : -500);

        ++currentIndex_;
    }
}

void SphericalTracker::runSteppers() {
    if (isTracking()) {
        panStp_.runSpeed();
        tiltStp_.runSpeed();
    }
}
void SphericalTracker::stop() {
    tracking_ = false;
    panStp_.setSpeed(0);
    tiltStp_.setSpeed(0);
}