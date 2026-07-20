/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

/// AVIX 雲台二進位協定（TCP Port 2000）的 encode/decode。純函式，不做任何 I/O，
/// 方便寫 unit test。封包格式見 docs/avix-gimbal/gimbal_icd.txt 1.1 節：
///
///   Offset 0-1  Header      0xAB 0xCD
///   Offset 2    Protocol Version
///   Offset 3    From Device ID
///   Offset 4    To Device ID
///   Offset 5    Sequence
///   Offset 6    Length（Data 長度）
///   Offset 7    Message ID
///   Offset 8..  Data[Length]
///   Offset 8+Length  Checksum
///
/// 待確認事項（SRD docs/avix-gimbal 第6節）：
/// - #3 Checksum 範圍：ICD 只寫「Sum of all bytes」，這裡採最literal 讀法，從 offset 0
///   （含 0xAB 0xCD）累加到 Data 結尾。header 欄位排列已用實機抓包（雲台送出的 0xF0 狀態
///   封包）驗證過（sync/version/from/to/length/msgid 全部吻合），但 checksum 那個 byte
///   本身尚未逐 byte 手動核對過，範圍假設仍待更多抓包驗證。
/// - Protocol Version 欄位：2026-07 實機抓包確認雲台送出的封包這個欄位是 0x01，
///   已改成這個值（原本假設 0x00 是錯的，這很可能就是先前雲台不理會指令的原因）。
/// - #2 Sequence/0x01 ACK 配對機制不明確：這裡先假設同步、單一 pending-command，
///   sequence 只是單調遞增，不做任何配對驗證（由呼叫端 AvixGimbalLink 決定要不要用）。
namespace AvixGimbalProtocol
{
    enum class DeviceId : quint8 {
        Gimbal = 0x01,
        PC = 0xA0,
    };

    enum class MessageId : quint8 {
        MessageResponse = 0x01,
        FollowRandomHead = 0x31,
        ControlGimbalVelocity = 0x32,
        TransferToSpecifyAngle = 0x33,
        CameraControl = 0x3A,
    };

    enum class ZoomDirection : qint8 {
        Out = -1,
        Stop = 0,
        In = 1,
    };

    struct DecodedPacket {
        quint8 fromDeviceId = 0;
        quint8 toDeviceId = 0;
        quint8 sequence = 0;
        quint8 messageId = 0;
        QByteArray data;
    };

    /// Sum of all bytes（simple 8-bit 累加，非 CRC），見 gimbal_icd.txt 1.4 節
    quint8 calculateChecksum(const QByteArray &bytes);

    /// 組出一個完整封包（含 header/checksum）。From/To Device ID 固定 PC→Gimbal。
    QByteArray encodePacket(MessageId messageId, const QByteArray &data, quint8 sequence);

    /// 0x32 速度模式（deg/s）
    QByteArray encodeVelocity(float yawRateDegS, float pitchRateDegS, bool followNose, quint8 sequence);

    /// 0x33 角度模式（deg）
    QByteArray encodeAngle(float yawDeg, float pitchDeg, bool enableYaw, bool enablePitch, bool followNose, quint8 sequence);

    /// 0x3A Mode=0（可見光 Zoom）
    QByteArray encodeZoom(ZoomDirection direction, quint8 sequence);

    /// 從 buffer 開頭嘗試解出一個完整封包：掃描 0xAB 0xCD sync、確認長度足夠、驗證 checksum。
    /// 成功時回傳 true，並把已消耗的 bytes（含丟棄的雜訊/壞封包）從 buffer 移除。
    /// 資料還沒收齊時回傳 false 且 buffer 不變，等下次有更多資料再呼叫。
    bool tryDecodePacket(QByteArray &buffer, DecodedPacket &outPacket);
}
