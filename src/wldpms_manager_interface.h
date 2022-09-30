// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WLDPMS_MANAGER_INTERFACE_H
#define WLDPMS_MANAGER_INTERFACE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtDBus/QtDBus>
#include <QtDBus/QDBusConnection>

#include <registry.h>
#include <dpms.h>

using namespace KWayland::Client;

const QString DPMS_MANAGER_PATH = "/com/deepin/daemon/KWayland/DpmsManager";

class WlDpmsManagerInterface : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.daemon.KWayland.DpmsManager")

public:
    explicit WlDpmsManagerInterface(DpmsManager *manager);
    virtual ~WlDpmsManagerInterface();
    void dpmsDbusAdd(const QString& dbus);
    void dpmsDbusRemove(const QString& dbus);

public Q_SLOTS:
    QList<QVariant> dpmsList() const;

private:
    QList<QVariant> m_dpmsDbusPath;
};

#endif // WLDPMS_MANAGER_INTERFACE_H
