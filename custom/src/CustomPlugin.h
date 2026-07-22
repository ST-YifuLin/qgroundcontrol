/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Custom QGCCorePlugin Declaration
 *
 * Minimal custom build: only overrides application branding (icons/logo).
 * See custom-example/src/CustomPlugin.h for the full-featured template this
 * was trimmed from (options/palette/vehicle overrides intentionally omitted).
 */

#pragma once

#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"

class QQmlApplicationEngine;

Q_DECLARE_LOGGING_CATEGORY(CustomLog)

class CustomPlugin : public QGCCorePlugin
{
    Q_OBJECT
public:
    explicit CustomPlugin(QObject *parent = nullptr);
    ~CustomPlugin();

    static QGCCorePlugin *instance();

    // Overrides from QGCCorePlugin
    void                    cleanup                    (void) final;
    QString                 brandImageIndoor           (void) const final;
    QString                 brandImageOutdoor          (void) const final;
    QQmlApplicationEngine*  createQmlApplicationEngine (QObject* parent) final;

private:
    QQmlApplicationEngine *_qmlEngine = nullptr;
    class CustomOverrideInterceptor *_selector = nullptr;
};

/*===========================================================================*/

class CustomOverrideInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    CustomOverrideInterceptor();

    QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type) final;
};
