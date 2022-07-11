#ifndef WLOUTPUT_INTERFACE_H
#define WLOUTPUT_INTERFACE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtDBus/QtDBus>
#include <QtDBus/QDBusConnection>
#include <QThread>

#include <outputdevice.h>
#include <registry.h>
#include <fakeinput.h>
#include <connection_thread.h>
#include <outputmanagement.h>
#include <QList>
#include <QTimer>
#include <QByteArray>
#include <outputconfiguration.h>
#include <dpms.h>
#include <output.h>

#include "wlidle_interface.h"
#include "wldpms_interface.h"
#include <ddeseat.h>
#include <pointer.h>
#include <linux/input.h>
#include "wldpms_manager_interface.h"

namespace KWayland {
namespace  Client{
    class PlasmaWindowModel;
}
}

using namespace KWayland::Client;

const QString SERVER = "com.deepin.daemon.KWayland";
const QString PATH = "/com/deepin/daemon/KWayland/Output";
const QString INTERFACE = "com.deepin.daemon.KWayland.Output";

typedef struct _mode_info
{
    int id;
    int width;
    int height;
    int refresh_rate;
    int flags; // avaliable values: [current, preferred]
}ModeInfo;


typedef  struct _output_info
{
    QString model;
    QString manufacturer;
    QString uuid;
    QByteArray edid;
    int enabled;
    int x;
    int y;
    int width;
    int height;
    int refresh_rate;
    int transform;
    int phys_width;
    int phys_height;
    double scale;
    QList<ModeInfo> lstModeInfos;
}OutputInfo;


class wloutput_interface : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.daemon.KWayland.Output")

public:
    explicit wloutput_interface(QObject *parent=nullptr);
    virtual ~wloutput_interface();
    bool InitDBus();
    void StartWork();
    QDBusAbstractAdaptor* idleObject() { return m_wlIdleInterface; }
    static OutputInfo GetOutputInfo(const OutputDevice* dev);
    static QString OutputInfo2Json(QList<OutputInfo>& listOutputInfos);
    static QList<OutputInfo> json2OutputInfo(QString jsonString);

signals:
    void OutputAdded(QString output);
    void OutputRemoved(QString output);
    void OutputChanged(QString output);
    void ButtonPress(quint32 button, quint32 x, quint32 y);
    void ButtonRelease(quint32 button, quint32 x, quint32 y);
    void CursorMove(quint32 x, quint32 y);
    void AxisChanged(int orientation, qreal delta);

public Q_SLOTS:
    QString ListOutput();
    QString GetOutput(QString uuid);
    void Apply(QString outputs);
    void WlSimulateKey(int keycode);
    void SetBrightness(const QString uuid, const int brightness);
    void SetColorTemperature(const QString uuid, const int temperature);

private:
    void onDeviceChanged(OutputDevice *dev);
    void onDeviceRemove(quint32 name, quint32 version) ;
    void onMangementAnnounced(quint32 name, quint32 version);
    void createPlasmaWindowManagement(quint32 name, quint32 version);
    void createDpmsManagement();
    void registerDpmsDbus(Output *output);
    void setColorCurvesBrightness(OutputDevice *outputDevice, const float brightness);
    void setColorCurvesTemperature(OutputDevice *outputDevice, const int temperature);

private:
    //QTimer m_Timer;
    ConnectionThread *m_pConnectThread{nullptr};
    QThread *m_pThread{nullptr};
    Registry *m_pRegisry{nullptr};
    OutputManagement *m_pManager{nullptr};
    OutputConfiguration *m_pConf{nullptr};
    EventQueue *m_eventQueue{nullptr};
    bool m_bConnected;
    PlasmaWindowManagement *m_pWindowManager{nullptr};
    WlIdleInterface *m_wlIdleInterface{nullptr};
    Idle *m_idle{nullptr};
    Seat *m_seat{nullptr};
    DDESeat *m_ddeSeat = nullptr;
    DDEPointer *m_ddePointer = nullptr;
    DDETouch *m_ddeTouch = nullptr;
    QMap<int32_t, QPointF> m_touchMap;  // wayland 窗管 touchUp 时没有位置信息，自行存储
    FakeInput *m_fakeInput{nullptr};
    QTimer *m_timer;
    WlDpmsManagerInterface *m_wldpms_Manager{nullptr};
    DpmsManager *m_dpmsManger{nullptr};
};

#endif // WLOUTPUT_INTERFACE_H
