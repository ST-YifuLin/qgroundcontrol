/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AvixGimbalController.h"
#include "AvixGimbalSettings.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"

#include <QtCore/qapplicationstatic.h>
#include <QtQml/qqml.h>

QGC_LOGGING_CATEGORY(AvixGimbalControllerLog, "qgc.avixgimbal.avixgimbalcontroller")

Q_APPLICATION_STATIC(AvixGimbalController, _avixGimbalControllerInstance);

namespace {
    // 見 docs/avix-gimbal SRD 第2節「系統介面」
    const QHostAddress kGimbalAddress(QStringLiteral("192.168.144.200"));
    constexpr quint16 kGimbalPort = 2000;
}

AvixGimbalController::AvixGimbalController(QObject *parent)
    : QObject(parent)
    , _link(this)
{
    const int savedSource = SettingsManager::instance()->avixGimbalSettings()->ControlSource()->rawValue().toInt();
    _activeControlSource = static_cast<ControlSource>(savedSource);

    _watchdogTimer.setSingleShot(true);
    _watchdogTimer.setInterval(kWatchdogTimeoutMs);
    (void) connect(&_watchdogTimer, &QTimer::timeout, this, &AvixGimbalController::_watchdogTimeout);

    _link.connectToGimbal(kGimbalAddress, kGimbalPort);
}

AvixGimbalController::~AvixGimbalController()
{
}

AvixGimbalController *AvixGimbalController::instance()
{
    return _avixGimbalControllerInstance();
}

void AvixGimbalController::registerQmlTypes()
{
    qmlRegisterSingletonInstance<AvixGimbalController>("AvixGimbal", 1, 0, "AvixGimbalController", AvixGimbalController::instance());
}

bool AvixGimbalController::_isActiveSource(ControlSource source) const
{
    return source == _activeControlSource;
}

void AvixGimbalController::requestControlSource(ControlSource source)
{
    if (source == _activeControlSource) {
        return;
    }

    qCDebug(AvixGimbalControllerLog) << "Switching control source from" << static_cast<int>(_activeControlSource) << "to" << static_cast<int>(source);

    // 交接前先送停止/中性指令，避免舊來源殘留的非零速度指令繼續生效
    _sendStopCommand();

    _activeControlSource = source;
    SettingsManager::instance()->avixGimbalSettings()->ControlSource()->setRawValue(static_cast<int>(source));
    emit activeControlSourceChanged();
}

void AvixGimbalController::setVelocity(ControlSource source, double yawRateDegS, double pitchRateDegS)
{
    if (!_isActiveSource(source)) {
        qCDebug(AvixGimbalControllerLog) << "setVelocity dropped, source" << static_cast<int>(source) << "is not active source" << static_cast<int>(_activeControlSource);
        return;
    }

    _lastYawRateDegS = yawRateDegS;
    _lastPitchRateDegS = pitchRateDegS;

    _sendVelocityToGimbal(yawRateDegS, pitchRateDegS);
    _armWatchdog();
}

void AvixGimbalController::setAngle(ControlSource source, double yawDeg, double pitchDeg)
{
    if (!_isActiveSource(source)) {
        qCDebug(AvixGimbalControllerLog) << "setAngle dropped, source" << static_cast<int>(source) << "is not active source" << static_cast<int>(_activeControlSource);
        return;
    }

    _sendAngleToGimbal(yawDeg, pitchDeg);
}

void AvixGimbalController::setZoom(ControlSource source, int direction)
{
    if (!_isActiveSource(source)) {
        qCDebug(AvixGimbalControllerLog) << "setZoom dropped, source" << static_cast<int>(source) << "is not active source" << static_cast<int>(_activeControlSource);
        return;
    }

    _sendZoomToGimbal(direction);
}

void AvixGimbalController::emergencyStop(ControlSource source)
{
    setVelocity(source, 0, 0);
    setZoom(source, 0);
}

void AvixGimbalController::_watchdogTimeout()
{
    if (_lastYawRateDegS != 0.0 || _lastPitchRateDegS != 0.0) {
        qCWarning(AvixGimbalControllerLog) << "Velocity command watchdog timeout, sending stop";
        _sendStopCommand();
    }
}

void AvixGimbalController::_armWatchdog()
{
    _watchdogTimer.start();
}

void AvixGimbalController::_sendStopCommand()
{
    _lastYawRateDegS = 0.0;
    _lastPitchRateDegS = 0.0;
    _watchdogTimer.stop();
    _sendVelocityToGimbal(0.0, 0.0);
}

void AvixGimbalController::_sendVelocityToGimbal(double yawRateDegS, double pitchRateDegS)
{
    _link.sendVelocity(static_cast<float>(yawRateDegS), static_cast<float>(pitchRateDegS), /*followNose=*/ false);
}

void AvixGimbalController::_sendAngleToGimbal(double yawDeg, double pitchDeg)
{
    _link.sendAngle(static_cast<float>(yawDeg), static_cast<float>(pitchDeg), /*enableYaw=*/ true, /*enablePitch=*/ true, /*followNose=*/ false);
}

void AvixGimbalController::_sendZoomToGimbal(int direction)
{
    AvixGimbalProtocol::ZoomDirection zoomDirection = AvixGimbalProtocol::ZoomDirection::Stop;
    if (direction > 0) {
        zoomDirection = AvixGimbalProtocol::ZoomDirection::In;
    } else if (direction < 0) {
        zoomDirection = AvixGimbalProtocol::ZoomDirection::Out;
    }

    _link.sendZoom(zoomDirection);
}
