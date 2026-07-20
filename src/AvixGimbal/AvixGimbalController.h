/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "AvixGimbalLink.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QTimer>

Q_DECLARE_LOGGING_CATEGORY(AvixGimbalControllerLog)

/// AvixGimbalController 是唯一持有 AVIX 雲台 TCP 連線、真正對雲台下指令的角色。
/// App 生命週期 singleton（Q_APPLICATION_STATIC，見 .cc），不是 Vehicle 的一部分：
/// 這顆雲台不講 MAVLink、沒有 System ID/Component ID，跟任何 Vehicle 的存在與否無關。
///
/// 內建 ControlSource 仲裁（見 docs/avix-gimbal SRD 第5節、CLAUDE.md 2.1）：
/// - 下指令前檢查呼叫來源是否為目前的 activeControlSource，不是的話丟棄並記 log
/// - 切換來源時先送停止/中性指令，確認後才交接，避免舊來源殘留的非零速度繼續生效
/// - velocity 模式下有 watchdog：逾時未收新指令且上次非零速度則自動送停止
///
class AvixGimbalController : public QObject
{
    Q_OBJECT

public:
    enum class ControlSource {
        Native = 0,        ///< QGC 原生 QML 控制面板
        MavlinkBridge = 1, ///< 預留給未來 MAVLink shim，MVP 未實作
    };
    Q_ENUM(ControlSource)

    Q_PROPERTY(ControlSource activeControlSource READ activeControlSource NOTIFY activeControlSourceChanged)

    explicit AvixGimbalController(QObject *parent = nullptr);
    ~AvixGimbalController();

    static AvixGimbalController *instance();
    static void registerQmlTypes();

    ControlSource activeControlSource() const { return _activeControlSource; }

    /// 請求切換目前的控制來源。會先對雲台送停止/中性指令，才把 activeControlSource 切過去。
    /// 已經是目前來源時無動作。
    Q_INVOKABLE void requestControlSource(AvixGimbalController::ControlSource source);

    /// Yaw/Pitch 速度模式指令（deg/s，正負代表方向）。source 不是目前 activeControlSource 時整包丟棄。
    Q_INVOKABLE void setVelocity(AvixGimbalController::ControlSource source, double yawRateDegS, double pitchRateDegS);

    /// Yaw/Pitch 角度模式指令（deg）。source 不是目前 activeControlSource 時整包丟棄。
    Q_INVOKABLE void setAngle(AvixGimbalController::ControlSource source, double yawDeg, double pitchDeg);

    /// 可見光鏡頭縮放指令。direction: -1 = Zoom Out, 0 = Stop, 1 = Zoom In。
    /// source 不是目前 activeControlSource 時整包丟棄。
    Q_INVOKABLE void setZoom(AvixGimbalController::ControlSource source, int direction);

    /// 緊急停止：同時把移動速度跟縮放都送 0（等同 setVelocity(0,0) + setZoom(0)）。
    /// source 不是目前 activeControlSource 時整包丟棄（沿用 setVelocity/setZoom 各自的判斷）。
    Q_INVOKABLE void emergencyStop(AvixGimbalController::ControlSource source);

signals:
    void activeControlSourceChanged();

private slots:
    void _watchdogTimeout();

private:
    bool _isActiveSource(ControlSource source) const;

    /// 送停止/中性指令：速度模式 yaw/pitch 歸零。用於切換來源交接與 watchdog 逾時。
    void _sendStopCommand();

    void _armWatchdog();

    void _sendVelocityToGimbal(double yawRateDegS, double pitchRateDegS);
    void _sendAngleToGimbal(double yawDeg, double pitchDeg);
    void _sendZoomToGimbal(int direction);

    ControlSource _activeControlSource = ControlSource::Native;

    AvixGimbalLink _link;

    QTimer _watchdogTimer;
    double _lastYawRateDegS = 0.0;
    double _lastPitchRateDegS = 0.0;

    static constexpr int kWatchdogTimeoutMs = 400;
};
