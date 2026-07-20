/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AvixGimbalLink.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AvixGimbalLinkLog, "qgc.avixgimbal.avixgimballink")

AvixGimbalLink::AvixGimbalLink(QObject *parent)
    : QObject(parent)
{
    (void) connect(&_socket, &QTcpSocket::connected, this, &AvixGimbalLink::_onConnected);
    (void) connect(&_socket, &QTcpSocket::disconnected, this, &AvixGimbalLink::_onDisconnected);
    (void) connect(&_socket, &QTcpSocket::readyRead, this, &AvixGimbalLink::_onReadyRead);
    (void) connect(&_socket, &QTcpSocket::errorOccurred, this, &AvixGimbalLink::_onSocketError);

    _reconnectTimer.setInterval(kReconnectIntervalMs);
    (void) connect(&_reconnectTimer, &QTimer::timeout, this, &AvixGimbalLink::_attemptReconnect);
}

AvixGimbalLink::~AvixGimbalLink()
{
    // 明確停掉 timer、切斷 socket signal 再關閉連線，避免物件解構到一半時
    // event queue 裡還卡著 timeout/readyRead/disconnected 事件，事後觸發呼叫到已銷毀的 this。
    _reconnectTimer.stop();
    _socket.disconnect(this);
    _socket.abort();
}

void AvixGimbalLink::connectToGimbal(const QHostAddress &address, quint16 port)
{
    _gimbalAddress = address;
    _gimbalPort = port;
    _rxBuffer.clear();
    _socket.connectToHost(address, port);
    _reconnectTimer.start();
}

void AvixGimbalLink::disconnectFromGimbal()
{
    _reconnectTimer.stop();
    _socket.disconnectFromHost();
}

void AvixGimbalLink::_attemptReconnect()
{
    if (isConnected() || _socket.state() != QAbstractSocket::UnconnectedState) {
        // 已連上，或上一次嘗試還在進行中（Connecting/Closing），這次先不重複發起
        return;
    }

    qCDebug(AvixGimbalLinkLog) << "Attempting to reconnect to gimbal at" << _gimbalAddress.toString() << _gimbalPort;
    _socket.connectToHost(_gimbalAddress, _gimbalPort);
}

bool AvixGimbalLink::isConnected() const
{
    return _socket.state() == QAbstractSocket::ConnectedState;
}

quint8 AvixGimbalLink::_nextSequence()
{
    return _sequence++;
}

void AvixGimbalLink::_sendPacket(const QByteArray &packet)
{
    if (!isConnected()) {
        qCDebug(AvixGimbalLinkLog) << "Dropping packet, not connected to gimbal";
        return;
    }

    _socket.write(packet);
}

void AvixGimbalLink::sendVelocity(float yawRateDegS, float pitchRateDegS, bool followNose)
{
    _sendPacket(AvixGimbalProtocol::encodeVelocity(yawRateDegS, pitchRateDegS, followNose, _nextSequence()));
}

void AvixGimbalLink::sendAngle(float yawDeg, float pitchDeg, bool enableYaw, bool enablePitch, bool followNose)
{
    _sendPacket(AvixGimbalProtocol::encodeAngle(yawDeg, pitchDeg, enableYaw, enablePitch, followNose, _nextSequence()));
}

void AvixGimbalLink::sendZoom(AvixGimbalProtocol::ZoomDirection direction)
{
    _sendPacket(AvixGimbalProtocol::encodeZoom(direction, _nextSequence()));
}

void AvixGimbalLink::_onConnected()
{
    qCDebug(AvixGimbalLinkLog) << "Connected to gimbal";
    _reconnectTimer.stop();
    emit connected();
}

void AvixGimbalLink::_onDisconnected()
{
    qCDebug(AvixGimbalLinkLog) << "Disconnected from gimbal";
    if (!_reconnectTimer.isActive()) {
        _reconnectTimer.start();
    }
    emit disconnected();
}

void AvixGimbalLink::_onReadyRead()
{
    _rxBuffer.append(_socket.readAll());

    AvixGimbalProtocol::DecodedPacket packet;
    while (AvixGimbalProtocol::tryDecodePacket(_rxBuffer, packet)) {
        if (packet.messageId == static_cast<quint8>(AvixGimbalProtocol::MessageId::MessageResponse) && !packet.data.isEmpty()) {
            emit ackReceived(static_cast<quint8>(packet.data.at(0)));
        }
    }
}

void AvixGimbalLink::_onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qCWarning(AvixGimbalLinkLog) << "Socket error:" << _socket.errorString();
    emit errorOccurred(_socket.errorString());
}
