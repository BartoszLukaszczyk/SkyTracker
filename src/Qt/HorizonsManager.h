#pragma once

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "bluetoothmanager.h"
#include <QTimer>

// Ephemeris point: UTC time and azimuth/elevation
struct EphemPoint {
    QDateTime utc;
    double az;   // degrees: azimuth
    double el;   // degrees: elevation
};

// Temporary structure for parsing RA/Dec from Horizons
struct EphemRD {
    QDateTime utc;
    double    raDeg;  // degrees
    double    decDeg; // degrees
};

// Single motor step record to send to ESP
struct StepRecord {
    uint32_t offset_ms;
    int16_t  deltaPan;
    int16_t  deltaTilt;
};

class HorizonsManager : public QObject {
    Q_OBJECT
public:
    explicit HorizonsManager(QObject *parent = nullptr);

    /**
     * Downloads observer-based ephemeris data from JPL Horizons
     * @param objectId  Horizons object ID (e.g. "301")
     * @param center    observer location (latitude, longitude)
     * @param start     local time range start
     * @param end       local time range end
     * @param stepSec   interpolation time step in seconds
     */
    void fetchEphemeris(const QString &objectId,
                        const QGeoCoordinate &center,
                        const QDateTime &start,
                        const QDateTime &end,
                        int stepSec);

    /**
     * Sends a trajectory of motor steps to the ESP, based on latest ephemeris
     * @param bt            pointer to BluetoothManager
     * @param degPerStepPan degrees per step for pan axis
     * @param degPerStepTilt degrees per step for tilt axis
     */
    void sendTrajectorySteps(BluetoothManager *bt,
                             double degPerStepPan,
                             double degPerStepTilt);
    void stopLiveTracking();

signals:
    void ephemerisReady(const QVector<EphemPoint> &traj);
    void ephemerisError(const QString &errorString);

private slots:
    void onNetworkFinished(QNetworkReply *reply);

private:
    QGeoCoordinate m_center;

    // Step 1: parse Horizons response to RA/Dec
    QVector<EphemRD> parseHorizonsText(const QString &txt) const;
    // Step 2: convert RA/Dec to topocentric Alt/Az
    QVector<EphemPoint> radecToAltAz(const QVector<EphemRD> &in,
                                     double latDeg,
                                     double lonDeg) const;
    // Step 3: interpolate trajectory with resolution m_stepSec
    QVector<EphemPoint> interpolateTrajectory(const QVector<EphemPoint> &in) const;

    // Helper functions
    double julianDay(const QDateTime &dtUtc) const;
    double gmstDeg(const QDateTime &dtUtc) const;

    // Debug CSV logs
    void dumpRawCsv(const QString &raw) const;
    void dumpParsedCsv(const QVector<EphemRD> &parsed) const;
    void dumpTopoCsv(const QVector<EphemPoint> &topo) const;
    void dumpDebugCsv(const QVector<EphemPoint> &traj) const;
    void dumpStepsCsv(const QVector<StepRecord> &steps) const;

    QNetworkAccessManager m_manager;
    QVector<EphemPoint>   m_fullTraj;
    int                   m_stepSec = 60;

    QTimer *m_liveTimer = nullptr;
    int *m_stepIdx = nullptr;
};
