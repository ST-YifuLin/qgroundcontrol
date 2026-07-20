#pragma once

#include "UnitTest.h"

class AvixGimbalProtocolTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testChecksum();
    void _testEncodeVelocity();
    void _testEncodeAngle();
    void _testEncodeZoom();
    void _testDecodeRoundTrip();
    void _testDecodePartialPacket();
    void _testDecodeSkipsGarbagePrefix();
    void _testDecodeSkipsBadChecksum();
};
