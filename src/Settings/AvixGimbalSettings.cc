/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AvixGimbalSettings.h"

#include <QQmlEngine>
#include <QtQml>

DECLARE_SETTINGGROUP(AvixGimbal, "AvixGimbal")
{
    qmlRegisterUncreatableType<AvixGimbalSettings>("QGroundControl.SettingsManager", 1, 0, "AvixGimbalSettings", "Reference only");
}

DECLARE_SETTINGSFACT(AvixGimbalSettings, ControlSource)
