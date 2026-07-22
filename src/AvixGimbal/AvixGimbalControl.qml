/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette

import AvixGimbal

// AVIX 雲台移動/縮放控制面板（FR-2/FR-3）。獨立於既有 MAVLink GimbalController 面板，
// 直接 bind AvixGimbalController 的 property/slot，不共用既有相機/雲台元件的 class 階層。
Item {
    id: _root

    property real _velocityDegS:   30     // MVP 先用固定速率，範圍見 ICD：-100~100 deg/s
    property bool _isActiveSource: AvixGimbalController.activeControlSource === AvixGimbalController.Native
    property bool _minimized:      false

    property real _dpadButtonSize: ScreenTools.defaultFontPixelHeight * 2.0
    property real _stopButtonSize: ScreenTools.defaultFontPixelHeight * 2.4
    property real _dpadSpacing:    ScreenTools.defaultFontPixelHeight / 6

    // AvixGimbalController 的 watchdog 逾時是 400ms（見 AvixGimbalController.h kWatchdogTimeoutMs），
    // 逾時未收新指令且上次非零速度就會自動送停止。方向鍵按住不放時，靠這個 timer 每 200ms 重送一次
    // 同樣的速度指令，避免被 watchdog 誤判成斷線而自動停下來（比照既有 OnScreenGimbalController.qml
    // 的 sendRateTimer 做法）。
    function _startVelocity(yawRateDegS, pitchRateDegS) {
        velocityRepeatTimer.yawRateDegS = yawRateDegS
        velocityRepeatTimer.pitchRateDegS = pitchRateDegS
        AvixGimbalController.setVelocity(AvixGimbalController.Native, yawRateDegS, pitchRateDegS)
        velocityRepeatTimer.running = true
    }

    function _stopVelocity() {
        velocityRepeatTimer.running = false
        AvixGimbalController.setVelocity(AvixGimbalController.Native, 0, 0)
    }

    Timer {
        id:         velocityRepeatTimer
        interval:   200
        repeat:     true
        property real yawRateDegS:   0
        property real pitchRateDegS: 0
        onTriggered: AvixGimbalController.setVelocity(AvixGimbalController.Native, yawRateDegS, pitchRateDegS)
    }

    Rectangle {
        id:             panel
        anchors.left:   parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin:   ScreenTools.defaultFontPixelWidth
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 3 // 避開 QGC 地圖介面本身的縮放按鈕
        visible:        !_root._minimized
        width:          panelLayout.implicitWidth + ScreenTools.defaultFontPixelWidth * 1
        height:         panelLayout.implicitHeight + ScreenTools.defaultFontPixelWidth * 1
        radius:         ScreenTools.defaultFontPixelWidth / 2
        color:          "#80F0F0F0" // 半透明淡色底（ARGB，80≈50%不透明度）。用 color 的 alpha 通道而不是
                                     // Rectangle.opacity，因為 opacity 會連裡面的按鈕/文字一起變透明，
                                     // 這裡只要背景看得穿、按鈕文字要維持清楚可讀。
        enabled:        _root._isActiveSource

        ColumnLayout {
            id:                 panelLayout
            anchors.centerIn:   parent
            spacing:            ScreenTools.defaultFontPixelHeight / 4

            RowLayout {
                Layout.fillWidth:   true

                QGCLabel {
                    text:               qsTr("AVIX Gimbal")
                    color:              "black"
                    Layout.fillWidth:   true
                }

                QGCButton {
                    text:               "–"
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 1.2
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.2
                    onClicked:          _root._minimized = true
                }
            }

            // Yaw/Pitch 速度模式移動（0x32）：方向鍵按住送速度、放開歸零。
            // 中間是緊急停止（比周圍方向鍵大一號，避免跟一般移動鍵混淆）。
            // 改用 anchors 明確定位（不用 GridLayout 的自動 cell 排版），確保四個方向鍵
            // 一定對稱地貼齊中間緊急停止鈕，不會被 GridLayout 的 column/row 尺寸計算influence。
            Item {
                id:                 dpad
                Layout.alignment:   Qt.AlignHCenter
                implicitWidth:      _root._stopButtonSize + (_root._dpadButtonSize + _root._dpadSpacing) * 2
                implicitHeight:     implicitWidth

                QGCButton {
                    id:         stopButton
                    anchors.centerIn: parent
                    width:      _root._stopButtonSize
                    height:     width
                    pointSize:  ScreenTools.defaultFontPointSize
                    text:       qsTr("緊急\n停止")
                    fontWeight: Font.Bold
                    textColor:  stopButton.pressed ? "#FFFFFF" : "#D32F2F"   // 按下時背景變紅，文字改白色避免紅字疊紅底看不見
                    // 覆寫掉 QGCButton 內建 padding（隨字級縮放，見 QGCButton.qml:37-38）。
                    // 這顆按鈕被 _stopButtonSize 強制縮到比內建 padding 還小，不覆寫的話文字會被擠壓/溢出，
                    // 詳細分析見 docs/avix-gimbal/QGC_AvixGimbal_UI設計預覽SOP_工作報告_2026-07-22.md。
                    leftPadding:   0
                    rightPadding:  0
                    topPadding:    0
                    bottomPadding: 0
                    onClicked:  AvixGimbalController.emergencyStop(AvixGimbalController.Native)

                    // 按下整顆變紅，跟其他按鈕的一般 hover/按壓高亮區隔開，強調這是緊急停止。
                    // 只在這顆按鈕覆寫 background，不動共用的 QGCButton.qml，代價是失去原本的
                    // hover 高亮效果（見 docs/avix-gimbal/QGC_AvixGimbal_UI設計預覽SOP_工作報告_2026-07-22.md 第7節）。
                    background: Rectangle {
                        radius:       4
                        color:        stopButton.pressed ? "#D32F2F" : "#FFFFFF"
                        border.width: 1
                        border.color: "#D32F2F"
                    }
                }
                QGCButton {
                    text:                   "▲"
                    pointSize:              ScreenTools.defaultFontPointSize * 1.2
                    rotation:               0                       // 上：不轉
                    width:                  _root._dpadButtonSize
                    height:                 width
                    leftPadding:            0
                    rightPadding:           0
                    topPadding:             0
                    bottomPadding:          0
                    anchors.horizontalCenter: stopButton.horizontalCenter
                    anchors.bottom:         stopButton.top
                    anchors.bottomMargin:   _root._dpadSpacing
                    onPressed:              _root._startVelocity(0, _root._velocityDegS)
                    onReleased:             _root._stopVelocity()
                }
                QGCButton {
                    text:                   "▲"
                    pointSize:              ScreenTools.defaultFontPointSize * 1.2
                    rotation:               180                     // 下：轉180度
                    width:                  _root._dpadButtonSize
                    height:                 width
                    leftPadding:            0
                    rightPadding:           0
                    topPadding:             0
                    bottomPadding:          0
                    anchors.horizontalCenter: stopButton.horizontalCenter
                    anchors.top:            stopButton.bottom
                    anchors.topMargin:      _root._dpadSpacing
                    onPressed:              _root._startVelocity(0, -_root._velocityDegS)
                    onReleased:             _root._stopVelocity()
                }
                QGCButton {
                    text:                   "▲"
                    pointSize:              ScreenTools.defaultFontPointSize * 1.2
                    rotation:               270                     // 左：轉270度
                    width:                  _root._dpadButtonSize
                    height:                 width
                    leftPadding:            0
                    rightPadding:           0
                    topPadding:             0
                    bottomPadding:          0
                    anchors.verticalCenter: stopButton.verticalCenter
                    anchors.right:          stopButton.left
                    anchors.rightMargin:    _root._dpadSpacing
                    onPressed:              _root._startVelocity(-_root._velocityDegS, 0)
                    onReleased:             _root._stopVelocity()
                }
                QGCButton {
                    text:                   "▲"
                    pointSize:              ScreenTools.defaultFontPointSize * 1.2
                    rotation:               90                      // 右：轉90度
                    width:                  _root._dpadButtonSize
                    height:                 width
                    leftPadding:            0
                    rightPadding:           0
                    topPadding:             0
                    bottomPadding:          0
                    anchors.verticalCenter: stopButton.verticalCenter
                    anchors.left:           stopButton.right
                    anchors.leftMargin:     _root._dpadSpacing
                    onPressed:              _root._startVelocity(_root._velocityDegS, 0)
                    onReleased:             _root._stopVelocity()
                }
            }

            // 角度模式（0x33）：回到中心角度，Yaw=0/Pitch=0
            QGCButton {
                text:                   qsTr("Center")
                Layout.alignment:       Qt.AlignHCenter
                Layout.fillWidth:       true
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.3
                topPadding:             0   // 同 d-pad 按鈕的 padding 擠壓問題，覆寫成0讓文字能真正上下置中
                bottomPadding:          0
                onClicked:              AvixGimbalController.setAngle(AvixGimbalController.Native, 0, 0)
            }

            // 可見光鏡頭縮放（0x3A Mode=0）
            RowLayout {
                Layout.alignment:   Qt.AlignHCenter
                spacing:            ScreenTools.defaultFontPixelWidth / 2

                QGCButton {
                    text:                   qsTr("Zoom -")
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.3
                    topPadding:             0
                    bottomPadding:          0
                    onPressed:              AvixGimbalController.setZoom(AvixGimbalController.Native, -1)
                    onReleased:             AvixGimbalController.setZoom(AvixGimbalController.Native, 0)
                }
                QGCButton {
                    text:                   qsTr("Zoom +")
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.3
                    topPadding:             0
                    bottomPadding:          0
                    onPressed:              AvixGimbalController.setZoom(AvixGimbalController.Native, 1)
                    onReleased:             AvixGimbalController.setZoom(AvixGimbalController.Native, 0)
                }
            }
        }
    }

    QGCButton {
        id:             restoreButton
        visible:        _root._minimized
        anchors.left:   parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin:   ScreenTools.defaultFontPixelWidth
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 3
        width:          ScreenTools.defaultFontPixelHeight * 2.2
        height:         width
        text:           qsTr("G")
        onClicked:      _root._minimized = false
    }
}
