#ifndef SPHERICALTRACKER_H
#define SPHERICALTRACKER_H

#include <AccelStepper.h>
#include <cstdint>

// Struktura definiująca pojedynczy punkt trajektorii
typedef struct {
    uint32_t t;    // od T0 w sekundach
    float    az;   // azymut [deg]
    float    el;   // elewacja [deg]
} TrackPoint;

class SphericalTracker {
public:
    /**
     * Konstruktor
     * @param panStp        Referencja do sterownika silnika PAN
     * @param tiltStp       Referencja do sterownika silnika TILT
     * @param degPerStepPan Ilość stopni na jeden mikro-krok osi PAN
     * @param degPerStepTilt Ilość stopni na jeden mikro-krok osi TILT
     * @param maxPoints     Maksymalna liczba punktów trajektorii
     */
    SphericalTracker(AccelStepper &panStp,
                     AccelStepper &tiltStp,
                     float degPerStepPan,
                     float degPerStepTilt,
                     int maxPoints);

    /**
     * Załaduj trajektorię
     * @param traj      Wskaźnik do tablicy punktów
     * @param nPts      Liczba punktów w tablicy
     * @param T0_unix   Czas startu śledzenia w sekundach unix
     */
    void loadTrajectory(const TrackPoint *traj, int nPts, uint32_t T0_unix);

    /**
     * Przygotowanie do śledzenia (ustawienie startMillis i flagi)
     */
    void prepare();

    /**
     * Sprawdź, czy aktualnie trwa śledzenie
     */
    bool isTracking() const;

    /**
     * Aktualizacja trybu śledzenia; wywoływane w loop
     * @param nowSec Aktualny czas w sekundach UNIX
     */
    void update(uint32_t nowSec);

    /**
     * Wykonanie kroków; wywoływane w loop
     */
    void runSteppers();
    void stop();


private:
    AccelStepper &panStp_;
    AccelStepper &tiltStp_;
    float degPerStepPan_;
    float degPerStepTilt_;
    int maxPoints_;
    TrackPoint *buffer_;    // dynamicznie alokowany bufor
    int numPoints_;
    uint32_t T0_unix_;
    int currentIndex_;
    bool tracking_;
    uint32_t startMillis_;
    int executedPan_;
    int executedTilt_;

    /**
     * Oblicza różnicę kątową sferycznie (-180, +180]
     */
    float angularDiff(float target, float origin);
};

#endif // SPHERICALTRACKER_H
