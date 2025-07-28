#pragma once

#include <QObject>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QBluetoothSocket>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothUuid>
#include <QtBluetooth/QBluetoothServiceInfo>

class BluetoothManager : public QObject {
    Q_OBJECT
public:
    explicit BluetoothManager(QObject* parent = nullptr);
    ~BluetoothManager();

    void startScan();
    void connectToDevice(const QBluetoothAddress& address);
    void sendCommand(const QByteArray& data);
    QBluetoothSocket* socket() const;

signals:
    void deviceDiscovered(const QBluetoothDeviceInfo& info);
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void dataReceived(const QByteArray& data);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo& info);
    void onScanFinished();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QBluetoothSocket::SocketError err);
    void onReadyRead();

private:
    QBluetoothDeviceDiscoveryAgent* m_discoveryAgent;
    QBluetoothSocket* m_socket;
};

