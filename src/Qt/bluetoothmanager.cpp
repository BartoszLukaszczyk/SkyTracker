#include "bluetoothmanager.h"
#include <QDebug>
#include <QDateTime>

BluetoothManager::BluetoothManager(QObject* parent)
    : QObject(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_socket(new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this))
{
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothManager::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BluetoothManager::onScanFinished);

    connect(m_socket, &QBluetoothSocket::connected,
            this, &BluetoothManager::onSocketConnected);
    connect(m_socket, &QBluetoothSocket::disconnected,
            this, &BluetoothManager::onSocketDisconnected);
    connect(m_socket, &QBluetoothSocket::errorOccurred,
            this, &BluetoothManager::onSocketError);
    connect(m_socket, &QBluetoothSocket::readyRead,
            this, &BluetoothManager::onReadyRead);
}

BluetoothManager::~BluetoothManager()
{
    if (m_socket->isOpen()) {
        m_socket->disconnectFromService();
        m_socket->abort();
    }
}

void BluetoothManager::startScan()
{
    m_discoveryAgent->start();
}

void BluetoothManager::onDeviceDiscovered(const QBluetoothDeviceInfo& info)
{
    emit deviceDiscovered(info);
}

void BluetoothManager::onScanFinished()
{
    qDebug() << "Scan finished, found devices.";
}

void BluetoothManager::connectToDevice(const QBluetoothAddress& address)
{
    m_socket->abort();
    QBluetoothUuid sppUuid(QBluetoothUuid::ServiceClassUuid::SerialPort);
    m_socket->connectToService(address, sppUuid, QIODevice::ReadWrite);
}

void BluetoothManager::onSocketConnected()
{
    emit connected();
    // Synchronize ESP time with host
    qint64 utcMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    QByteArray cmd = QByteArray("SYNC_TIME ") + QByteArray::number(utcMs) + "\n";
    sendCommand(cmd);
}

void BluetoothManager::onSocketDisconnected()
{
    emit disconnected();
}

void BluetoothManager::onSocketError(QBluetoothSocket::SocketError err)
{
    Q_UNUSED(err)
    emit errorOccurred(m_socket->errorString());
}

void BluetoothManager::sendCommand(const QByteArray& data)
{
    // Improved connection state check
    if (m_socket->state() == QBluetoothSocket::SocketState::ConnectedState) {
        m_socket->write(data);
    } else {
        emit errorOccurred(QStringLiteral("Not connected"));
    }
}

void BluetoothManager::onReadyRead()
{
    QByteArray fromEsp = m_socket->readAll();
    emit dataReceived(fromEsp);
}

QBluetoothSocket* BluetoothManager::socket() const {
    return m_socket;
}
