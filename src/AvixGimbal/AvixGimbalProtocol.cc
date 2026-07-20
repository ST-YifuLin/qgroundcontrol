/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AvixGimbalProtocol.h"

#include <QtCore/QtEndian>

#include <cstring>

namespace AvixGimbalProtocol
{

namespace {
    constexpr quint8 kSyncByte0 = 0xAB;
    constexpr quint8 kSyncByte1 = 0xCD;
    // 2026-07 實機抓包確認：雲台自己送出的 0xF0 狀態封包這個欄位是 0x01（不是原本假設的 0x00）。
    constexpr quint8 kProtocolVersion = 0x01;
    constexpr int kHeaderLength = 8;          // offset 0..7，Data 從 offset 8 開始

    void appendFloatLE(QByteArray &out, float value)
    {
        quint32 bits;
        std::memcpy(&bits, &value, sizeof(bits));
        bits = qToLittleEndian(bits);
        out.append(reinterpret_cast<const char *>(&bits), sizeof(bits));
    }
}

quint8 calculateChecksum(const QByteArray &bytes)
{
    quint8 crc = 0;
    for (const char byte : bytes) {
        crc = static_cast<quint8>(crc + static_cast<quint8>(byte));
    }
    return crc;
}

QByteArray encodePacket(MessageId messageId, const QByteArray &data, quint8 sequence)
{
    QByteArray packet;
    packet.append(static_cast<char>(kSyncByte0));
    packet.append(static_cast<char>(kSyncByte1));
    packet.append(static_cast<char>(kProtocolVersion));
    packet.append(static_cast<char>(DeviceId::PC));
    packet.append(static_cast<char>(DeviceId::Gimbal));
    packet.append(static_cast<char>(sequence));
    packet.append(static_cast<char>(data.size()));
    packet.append(static_cast<char>(messageId));
    packet.append(data);
    packet.append(static_cast<char>(calculateChecksum(packet)));
    return packet;
}

QByteArray encodeVelocity(float yawRateDegS, float pitchRateDegS, bool followNose, quint8 sequence)
{
    QByteArray data;
    appendFloatLE(data, yawRateDegS);
    appendFloatLE(data, pitchRateDegS);
    const quint8 flags = followNose ? 0x01 : 0x00;
    data.append(static_cast<char>(flags));
    return encodePacket(MessageId::ControlGimbalVelocity, data, sequence);
}

QByteArray encodeAngle(float yawDeg, float pitchDeg, bool enableYaw, bool enablePitch, bool followNose, quint8 sequence)
{
    QByteArray data;
    appendFloatLE(data, yawDeg);
    appendFloatLE(data, pitchDeg);
    quint8 flags = 0;
    if (enableYaw) {
        flags |= 0x01;
    }
    if (enablePitch) {
        flags |= 0x02;
    }
    if (followNose) {
        flags |= 0x04;
    }
    data.append(static_cast<char>(flags));
    return encodePacket(MessageId::TransferToSpecifyAngle, data, sequence);
}

QByteArray encodeZoom(ZoomDirection direction, quint8 sequence)
{
    quint8 zoomCtrl = 0;
    switch (direction) {
    case ZoomDirection::In:
        zoomCtrl = 1;
        break;
    case ZoomDirection::Out:
        zoomCtrl = 2;
        break;
    case ZoomDirection::Stop:
        zoomCtrl = 0;
        break;
    }

    QByteArray data;
    data.append(static_cast<char>(0)); // Mode = 0 (Zoom control)
    data.append(static_cast<char>(zoomCtrl));
    data.append(static_cast<char>(0)); // FocusCtrl，本指令不使用
    data.append(static_cast<char>(0)); // RecSwitch，本指令不使用
    return encodePacket(MessageId::CameraControl, data, sequence);
}

bool tryDecodePacket(QByteArray &buffer, DecodedPacket &outPacket)
{
    static const QByteArray kSync(QByteArray("\xAB\xCD", 2));

    while (true) {
        const int syncIndex = buffer.indexOf(kSync);
        if (syncIndex < 0) {
            // 沒找到完整 sync，留下可能是半個 sync 的最後一個 byte，其餘視為雜訊丟棄
            if (!buffer.isEmpty() && static_cast<quint8>(buffer.back()) == kSyncByte0) {
                buffer = buffer.right(1);
            } else {
                buffer.clear();
            }
            return false;
        }

        if (syncIndex > 0) {
            buffer.remove(0, syncIndex); // 丟掉 sync 前的雜訊
        }

        if (buffer.size() < kHeaderLength) {
            return false; // header 還沒收齊，等更多資料
        }

        const quint8 length = static_cast<quint8>(buffer.at(6));
        const int totalPacketLength = kHeaderLength + length + 1; // +1 checksum byte
        if (buffer.size() < totalPacketLength) {
            return false; // 資料還沒到齊
        }

        const QByteArray packetBytes = buffer.left(totalPacketLength);
        const quint8 expectedChecksum = calculateChecksum(packetBytes.left(totalPacketLength - 1));
        const quint8 actualChecksum = static_cast<quint8>(packetBytes.at(totalPacketLength - 1));

        if (expectedChecksum != actualChecksum) {
            // checksum 不合，這個 sync 可能是誤判到 Data 裡剛好出現的 0xAB 0xCD，跳過 1 byte 重新找
            buffer.remove(0, 1);
            continue;
        }

        outPacket.fromDeviceId = static_cast<quint8>(packetBytes.at(3));
        outPacket.toDeviceId = static_cast<quint8>(packetBytes.at(4));
        outPacket.sequence = static_cast<quint8>(packetBytes.at(5));
        outPacket.messageId = static_cast<quint8>(packetBytes.at(7));
        outPacket.data = packetBytes.mid(kHeaderLength, length);

        buffer.remove(0, totalPacketLength);
        return true;
    }
}

}
