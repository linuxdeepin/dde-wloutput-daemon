// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WLDPMS_INTERFACE_H
#define WLDPMS_INTERFACE_H

#include <QObject>
#include <QtDBus/QtDBus>
#include <QtDBus/QDBusConnection>
#include <registry.h>
#include <dpms.h>
#include <output.h>

using namespace KWayland::Client;

const QString DPMS_PATH = "/com/deepin/daemon/KWayland/Dpms";

class wldpms_interface : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.daemon.KWayland.Dpms")
public:
    explicit wldpms_interface(Dpms *dpms);
    virtual ~wldpms_interface();
    bool InitDBus();
    void setDpmsName(QString str);
    void updateMode();
signals:

public Q_SLOTS:
    int getDpmsMode();
    QString getDpmsName();
    void setDpmsMode(int mode);
private:
    int m_mode;
    QString m_name;
    Dpms *m_Dpms{nullptr};
};

#endif // WLDPMS_INTERFACE_H
