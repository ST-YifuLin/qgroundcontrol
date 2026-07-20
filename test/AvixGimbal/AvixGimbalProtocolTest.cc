#include "AvixGimbalProtocolTest.h"
#include "AvixGimbalProtocol.h"

#include <QtTest/QTest>

#include <cstring>

using namespace AvixGimbalProtocol;

namespace {
    quint8 referenceChecksum(const QByteArray &bytes)
    {
        quint8 sum = 0;
        for (const char byte : bytes) {
            sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
        }
        return sum;
    }
}

void AvixGimbalProtocolTest::_testChecksum()
{
    QCOMPARE(calculateChecksum(QByteArray("\x01\x02\x03", 3)), quint8(6));
    // 驗證 8-bit 溢位會回繞（0xFF + 0xFF = 0x1FE，取低 8 位 = 0xFE）
    QCOMPARE(calculateChecksum(QByteArray("\xFF\xFF", 2)), quint8(0xFE));
    QCOMPARE(calculateChecksum(QByteArray()), quint8(0));
}

void AvixGimbalProtocolTest::_testEncodeVelocity()
{
    const QByteArray packet = encodeVelocity(10.0f, -5.0f, /*followNose=*/ true, /*sequence=*/ 5);

    // Header(2) + Version(1) + From(1) + To(1) + Seq(1) + Length(1) + MsgId(1) + Data(9) + Checksum(1)
    QCOMPARE(packet.size(), 18);
    QCOMPARE(static_cast<quint8>(packet.at(0)), quint8(0xAB));
    QCOMPARE(static_cast<quint8>(packet.at(1)), quint8(0xCD));
    QCOMPARE(static_cast<quint8>(packet.at(3)), static_cast<quint8>(DeviceId::PC));
    QCOMPARE(static_cast<quint8>(packet.at(4)), static_cast<quint8>(DeviceId::Gimbal));
    QCOMPARE(static_cast<quint8>(packet.at(5)), quint8(5));
    QCOMPARE(static_cast<quint8>(packet.at(6)), quint8(9));
    QCOMPARE(static_cast<quint8>(packet.at(7)), static_cast<quint8>(MessageId::ControlGimbalVelocity));

    float yaw = 0.0f;
    float pitch = 0.0f;
    memcpy(&yaw, packet.constData() + 8, sizeof(float));
    memcpy(&pitch, packet.constData() + 12, sizeof(float));
    QCOMPARE(yaw, 10.0f);
    QCOMPARE(pitch, -5.0f);
    QCOMPARE(static_cast<quint8>(packet.at(16)), quint8(0x01)); // followNose flag

    QCOMPARE(static_cast<quint8>(packet.at(17)), referenceChecksum(packet.left(17)));
}

void AvixGimbalProtocolTest::_testEncodeAngle()
{
    const QByteArray packet = encodeAngle(90.0f, -30.0f, /*enableYaw=*/ true, /*enablePitch=*/ true, /*followNose=*/ false, /*sequence=*/ 7);

    QCOMPARE(packet.size(), 18);
    QCOMPARE(static_cast<quint8>(packet.at(7)), static_cast<quint8>(MessageId::TransferToSpecifyAngle));

    float yaw = 0.0f;
    float pitch = 0.0f;
    memcpy(&yaw, packet.constData() + 8, sizeof(float));
    memcpy(&pitch, packet.constData() + 12, sizeof(float));
    QCOMPARE(yaw, 90.0f);
    QCOMPARE(pitch, -30.0f);
    QCOMPARE(static_cast<quint8>(packet.at(16)), quint8(0x03)); // bit0 enableYaw | bit1 enablePitch

    QCOMPARE(static_cast<quint8>(packet.at(17)), referenceChecksum(packet.left(17)));
}

void AvixGimbalProtocolTest::_testEncodeZoom()
{
    const QByteArray zoomIn = encodeZoom(ZoomDirection::In, 1);
    const QByteArray zoomOut = encodeZoom(ZoomDirection::Out, 1);
    const QByteArray zoomStop = encodeZoom(ZoomDirection::Stop, 1);

    QCOMPARE(zoomIn.size(), 13); // header(8) + data(4) + checksum(1)
    QCOMPARE(static_cast<quint8>(zoomIn.at(7)), static_cast<quint8>(MessageId::CameraControl));
    QCOMPARE(static_cast<quint8>(zoomIn.at(8)), quint8(0));  // Mode = Zoom
    QCOMPARE(static_cast<quint8>(zoomIn.at(9)), quint8(1));  // ZoomCtrl = Zoom in
    QCOMPARE(static_cast<quint8>(zoomOut.at(9)), quint8(2)); // ZoomCtrl = Zoom out
    QCOMPARE(static_cast<quint8>(zoomStop.at(9)), quint8(0)); // ZoomCtrl = Stop
}

void AvixGimbalProtocolTest::_testDecodeRoundTrip()
{
    QByteArray buffer = encodeVelocity(1.5f, -2.5f, false, 42);
    DecodedPacket packet;

    QVERIFY(tryDecodePacket(buffer, packet));
    QCOMPARE(packet.fromDeviceId, static_cast<quint8>(DeviceId::PC));
    QCOMPARE(packet.toDeviceId, static_cast<quint8>(DeviceId::Gimbal));
    QCOMPARE(packet.sequence, quint8(42));
    QCOMPARE(packet.messageId, static_cast<quint8>(MessageId::ControlGimbalVelocity));
    QCOMPARE(packet.data.size(), 9);
    QVERIFY(buffer.isEmpty());
}

void AvixGimbalProtocolTest::_testDecodePartialPacket()
{
    const QByteArray full = encodeVelocity(1.0f, 1.0f, false, 1);
    QByteArray buffer = full.left(full.size() - 3); // 拿掉最後幾個 byte，模擬還沒收完整
    DecodedPacket packet;

    QVERIFY(!tryDecodePacket(buffer, packet));
    QCOMPARE(buffer, full.left(full.size() - 3)); // 資料不足時 buffer 不應被消耗
}

void AvixGimbalProtocolTest::_testDecodeSkipsGarbagePrefix()
{
    QByteArray buffer;
    buffer.append(QByteArray("\x00\x01\x02", 3)); // 雜訊，不是有效封包
    buffer.append(encodeAngle(1.0f, 2.0f, true, true, false, 9));

    DecodedPacket packet;
    QVERIFY(tryDecodePacket(buffer, packet));
    QCOMPARE(packet.sequence, quint8(9));
    QVERIFY(buffer.isEmpty());
}

void AvixGimbalProtocolTest::_testDecodeSkipsBadChecksum()
{
    QByteArray buffer = encodeVelocity(1.0f, 1.0f, false, 1);
    buffer[buffer.size() - 1] = static_cast<char>(static_cast<quint8>(buffer.at(buffer.size() - 1)) ^ 0xFF); // 打壞 checksum

    DecodedPacket packet;
    QVERIFY(!tryDecodePacket(buffer, packet)); // 唯一的 sync 校驗失敗，找不到其他候選封包，應回傳 false 且不會卡死
}
