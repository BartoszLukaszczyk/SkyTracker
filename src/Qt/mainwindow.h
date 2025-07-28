#pragma once

#include <QMainWindow>
#include <QtBluetooth/QBluetoothAddress>
#include <QTimer>                // <-- dodane
#include <QGeoPositionInfoSource>
#include <QGeoCoordinate>
#include "HorizonsManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class BluetoothManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_BT_Connect_clicked();

    // --- manual control handlers ---
    void onManualPressed();
    void onManualReleased();
    void onRepeatTimeout();

    void onPositionUpdated(const QGeoPositionInfo &info);
    void onObjectButtonClicked();

    // --- Ephemeris handlers ---
    void onEphemerisReady(const QVector<EphemPoint> &traj);
    void onEphemerisError(const QString &errorString);

private:
    Ui::MainWindow *ui;
    BluetoothManager *m_bt;

    void populatePairedDevices();
#ifdef Q_OS_ANDROID
    void requestAndroidPermissions();
#endif

    // --- manual control state ---
    enum Direction { None, Up, Down, Left, Right };
    QTimer    *repeatTimer    = nullptr;
    Direction  currentDir     = None;

    Direction directionForSender(QObject *s) const;

    // GPS/Wi-Fi position source
    QGeoPositionInfoSource *m_posSource = nullptr;
    QGeoCoordinate          m_currentCenter;

    // Ephemeris manager
    HorizonsManager        *m_horizonsMgr = nullptr;
};
