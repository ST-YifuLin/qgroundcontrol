/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "AvixGimbalProtocol.h"

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpSocket>

Q_DECLARE_LOGGING_CATEGORY(AvixGimbalLinkLog)

/// AVIX 雲台 TCP 連線（Port 2000）：只負責連線生命週期與封包收發框架，
/// 不做 ControlSource 仲裁／watchdog（那是 AvixGimbalController 的責任，見該類別）。
///
/// 待確認事項（SRD docs/avix-gimbal 第6節 #1，尚未用實機驗證）：idle 斷線時間、keep-alive
/// 行為都未知。已於 2026-07 實機測試中發現：app 啟動當下若網路還沒通，QTcpSocket 連線
/// 失敗後不會自動重試，之後所有指令都會被 _sendPacket() 悄悄丟棄。因此加了最基本的
/// 重連 timer（固定週期重試，見 kReconnectIntervalMs），連線成功後自動停止。
class AvixGimbalLink : public QObject
{
    Q_OBJECT

public:
    explicit AvixGimbalLink(QObject *parent = nullptr);
    ~AvixGimbalLink();

    void connectToGimbal(const QHostAddress &address, quint16 port);
    void disconnectFromGimbal();
    bool isConnected() const;

    void sendVelocity(float yawRateDegS, float pitchRateDegS, bool followNose);
    void sendAngle(float yawDeg, float pitchDeg, bool enableYaw, bool enablePitch, bool followNose);
    void sendZoom(AvixGimbalProtocol::ZoomDirection direction);

signals:
    void connected();
    void disconnected();
    void ackReceived(quint8 respondedMessageId);
    void errorOccurred(const QString &errorString);

private slots:
    void _onConnected();
    void _onDisconnected();
    void _onReadyRead();
    void _onSocketError(QAbstractSocket::SocketError error);
    void _attemptReconnect();

private:
    quint8 _nextSequence();
    void _sendPacket(const QByteArray &packet);

    QTcpSocket _socket;
    QByteArray _rxBuffer;
    quint8 _sequence = 0;

    QHostAddress _gimbalAddress;
    quint16 _gimbalPort = 0;
    QTimer _reconnectTimer;

    static constexpr int kReconnectIntervalMs = 2000;
};
