/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Custom QGCCorePlugin Implementation
 */

#include "CustomPlugin.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QFile>
#include <QtCore/qapplicationstatic.h>
#include <QtQml/QQmlApplicationEngine>

QGC_LOGGING_CATEGORY(CustomLog, "gcs.custom.customplugin")

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomPlugin::CustomPlugin(QObject *parent)
    : QGCCorePlugin(parent)
{

}

CustomPlugin::~CustomPlugin()
{

}

QGCCorePlugin *CustomPlugin::instance()
{
    return _customPluginInstance();
}

void CustomPlugin::cleanup()
{
    if (_qmlEngine) {
        _qmlEngine->removeUrlInterceptor(_selector);
    }

    delete _selector;
}

QString CustomPlugin::brandImageIndoor(void) const
{
    return QStringLiteral("/Custom/res/QGCLogoFull.svg");
}

QString CustomPlugin::brandImageOutdoor(void) const
{
    return QStringLiteral("/Custom/res/QGCLogoFull.svg");
}

// We override this so we can install a URL interceptor that redirects the
// existing QGCLogoFull/White/Black.svg and qgroundcontrol.ico qrc lookups
// (used directly by several existing QML/C++ files) to our custom versions
// without having to touch those existing files.
QQmlApplicationEngine* CustomPlugin::createQmlApplicationEngine(QObject* parent)
{
    _qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);

    _selector = new CustomOverrideInterceptor();
    _qmlEngine->addUrlInterceptor(_selector);

    return _qmlEngine;
}

/*===========================================================================*/

CustomOverrideInterceptor::CustomOverrideInterceptor()
    : QQmlAbstractUrlInterceptor()
{

}

QUrl CustomOverrideInterceptor::intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type)
{
    switch (type) {
    using DataType = QQmlAbstractUrlInterceptor::DataType;
    case DataType::QmlFile:
    case DataType::UrlString:
        if (url.scheme() == QStringLiteral("qrc")) {
            const QString origPath = url.path();
            const QString overrideRes = QStringLiteral(":/Custom%1").arg(origPath);
            if (QFile::exists(overrideRes)) {
                const QString relPath = overrideRes.mid(2);
                QUrl result;
                result.setScheme(QStringLiteral("qrc"));
                result.setPath('/' + relPath);
                return result;
            }
        }
        break;
    default:
        break;
    }

    return url;
}
