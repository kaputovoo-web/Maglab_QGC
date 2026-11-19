/****************************************************************************
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "QGCMAVLink.h"

class LinkInterface;

/// Legacy diagnostic display only. Never inject synthetic GPS into vehicles.
class SingletonTest : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString test1 READ test1 WRITE setTest1 NOTIFY test1Changed)
    Q_PROPERTY(QString test2 READ test2 WRITE setTest2 NOTIFY test2Changed)
    Q_PROPERTY(bool isboat READ isboat WRITE setIsboat NOTIFY isboatChanged)

public:
    explicit SingletonTest(QObject* parent = nullptr);

    QString test1() const { return _test1; }
    QString test2() const { return _test2; }
    bool isboat() const { return _isboat; }
    void setTest1(const QString& value);
    void setTest2(const QString& value);
    void setIsboat(bool value);

signals:
    void test1Changed();
    void test2Changed();
    void isboatChanged();

private:
    void _mavlinkMessageReceived(LinkInterface* link, const mavlink_message_t& message);

    QString _test1;
    QString _test2;
    bool _isboat = false;
};
