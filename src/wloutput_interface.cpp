#include "wloutput_interface.h"
#include <QDebug>
#include <QtDBus/QDBusMessage>
#include <outputdevice.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDBusInterface>
#include <event_queue.h>
#include <plasmawindowmanagement.h>
#include "plasma_window_interface.h"
#include "window_manager_interface.h"
#include "wldpms_manager_interface.h"

static QMap<QString, OutputDevice*> uuid2OutputDevice;

wloutput_interface::wloutput_interface(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);

    m_wlIdleInterface = new WlIdleInterface(parent);
    m_pConnectThread = new ConnectionThread;
    m_pThread = new QThread(this);
    m_bConnected = false;
    m_pManager = nullptr;
}

wloutput_interface::~wloutput_interface()
{
    m_pThread->quit();
    m_pThread->wait();

    if (m_pConnectThread) {
        m_pConnectThread->deleteLater();
    }
    if(m_wlIdleInterface) {
        m_wlIdleInterface->deleteLater();
    }

    if (m_pRegisry)
        m_pRegisry->deleteLater();
    if (m_pConf)
        m_pRegisry->deleteLater();
    if (m_pManager)
        m_pManager->deleteLater();
}

OutputInfo wloutput_interface::GetOutputInfo(const OutputDevice* dev)
{
    OutputInfo stOutputInfo;
    stOutputInfo.model = dev->model();
    stOutputInfo.manufacturer = dev->manufacturer();
    stOutputInfo.uuid = dev->uuid();
    stOutputInfo.edid = dev->edid();

    qDebug() << "GetOutputInfo: " << dev->edid().length() << dev->edid().toBase64() << stOutputInfo.manufacturer;
    qDebug()<<"[Dev Get]: "<<dev->model()<<dev->uuid()<<dev->globalPosition()<<dev->geometry()<<dev->refreshRate()<<dev->pixelSize();
    switch(dev->enabled())
    {
        case OutputDevice::Enablement::Disabled:
            stOutputInfo.enabled = 0;
            break;
        case OutputDevice::Enablement::Enabled:
            stOutputInfo.enabled = 1;
            break;

    }


    stOutputInfo.x = dev->geometry().x();
    stOutputInfo.y = dev->geometry().y();
    stOutputInfo.width = dev->geometry().width();
    stOutputInfo.height = dev->geometry().height();
    stOutputInfo.refresh_rate = dev->refreshRate();

    switch (dev->transform())
    {
        case OutputDevice::Transform::Normal:
            stOutputInfo.transform = 0;
            break;
        case OutputDevice::Transform::Rotated90:
            stOutputInfo.transform = 1;
            break;
        case OutputDevice::Transform::Rotated180:
            stOutputInfo.transform = 2;
            break;
        case OutputDevice::Transform::Rotated270:
            stOutputInfo.transform = 3;
            break;
        case OutputDevice::Transform::Flipped:
            stOutputInfo.transform = 4;
            break;
        case OutputDevice::Transform::Flipped90:
            stOutputInfo.transform = 5;
            break;
        case OutputDevice::Transform::Flipped180:
            stOutputInfo.transform = 6;
            break;
        case OutputDevice::Transform::Flipped270:
            stOutputInfo.transform = 7;
            break;
    }
    stOutputInfo.phys_width = dev->physicalSize().width();
    stOutputInfo.phys_height = dev->physicalSize().height();
    stOutputInfo.scale = dev->scale();
    auto modes = dev->modes();
    for (auto oIter = modes.begin(); oIter != modes.end(); ++oIter)
    {
        ModeInfo stModeInfo;
        stModeInfo.id = oIter->id;
        stModeInfo.width = oIter->size.width();
        stModeInfo.height = oIter->size.height();
        stModeInfo.refresh_rate = oIter->refreshRate;
        stModeInfo.flags = oIter->flags;
        stOutputInfo.lstModeInfos.push_back(stModeInfo);
    }
    return stOutputInfo;
}

QString wloutput_interface::OutputInfo2Json(QList<OutputInfo>& listOutputInfos)
{
    QString json;
    QJsonObject root;

    QJsonArray jsonOutputArray;
    auto oIterOutputInfo = listOutputInfos.begin();
    for (int i = 0; oIterOutputInfo != listOutputInfos.end(); ++oIterOutputInfo, ++i)
    {
        QJsonObject jsonOutputInfo;

        qDebug()<<"[To JSON]: "<<oIterOutputInfo->model<<oIterOutputInfo->manufacturer<<oIterOutputInfo->uuid<<oIterOutputInfo->enabled
               <<oIterOutputInfo->x<<oIterOutputInfo->y<<oIterOutputInfo->width<<oIterOutputInfo->height<<oIterOutputInfo->refresh_rate
              <<oIterOutputInfo->transform;
        jsonOutputInfo.insert("model", oIterOutputInfo->model);
        jsonOutputInfo.insert("manufacturer", oIterOutputInfo->manufacturer);
        jsonOutputInfo.insert("uuid", oIterOutputInfo->uuid);
        jsonOutputInfo.insert("enabled", oIterOutputInfo->enabled);
        jsonOutputInfo.insert("x", oIterOutputInfo->x);
        jsonOutputInfo.insert("y", oIterOutputInfo->y);
        jsonOutputInfo.insert("width", oIterOutputInfo->width);
        jsonOutputInfo.insert("height", oIterOutputInfo->height);
        jsonOutputInfo.insert("refresh_rate", oIterOutputInfo->refresh_rate);
        jsonOutputInfo.insert("transform", oIterOutputInfo->transform);
        jsonOutputInfo.insert("phys_width", oIterOutputInfo->phys_width);
        jsonOutputInfo.insert("phys_height", oIterOutputInfo->phys_height);
        jsonOutputInfo.insert("scale", oIterOutputInfo->scale);
        QByteArray edidBase64 = oIterOutputInfo->edid.toBase64();
        jsonOutputInfo.insert("edid_base64", QString(edidBase64));



        QJsonArray jsonModeInfoArray;
        auto oIterModeInfo = oIterOutputInfo->lstModeInfos.begin();
        for (int j = 0; oIterModeInfo != oIterOutputInfo->lstModeInfos.end(); ++oIterModeInfo, ++j) {

            QJsonObject jsonModeInfo;
            jsonModeInfo.insert("id", oIterModeInfo->id);
            jsonModeInfo.insert("width", oIterModeInfo->width);
            jsonModeInfo.insert("height", oIterModeInfo->height);
            jsonModeInfo.insert("refresh_rate", oIterModeInfo->refresh_rate);
            jsonModeInfo.insert("flags", oIterModeInfo->flags);
            jsonModeInfoArray.insert(j, jsonModeInfo);
        }

        jsonOutputInfo.insert("ModeInfo", jsonModeInfoArray);

        jsonOutputArray.insert(i, jsonOutputInfo);
    }
    root.insert("OutputInfo", jsonOutputArray);

    QJsonDocument rootDoc;
    rootDoc.setObject(root);
    //json = rootDoc.toJson(QJsonDocument::Compact);
    json = rootDoc.toJson(QJsonDocument::Indented);
    return  json;
}

QList<OutputInfo> wloutput_interface::json2OutputInfo(QString jsonString)
{
    QList<OutputInfo> listOutputInfo;
    QJsonParseError err;
    QJsonDocument  rootDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);
    if(err.error != QJsonParseError::NoError) {
        qDebug() << "json fromat error";
        return listOutputInfo;
    }
    else {

        QJsonObject root = rootDoc.object();
        QJsonValue outputInfoValue = root.value("OutputInfo");
        if (outputInfoValue.isArray()) {
            QJsonArray outputInfoArray = outputInfoValue.toArray();
            for (int i = 0; i < outputInfoArray.size(); ++i) {
                OutputInfo stOutputInfo;
                QJsonObject jsonOutputInfo = outputInfoArray.at(i).toObject();
                stOutputInfo.model = jsonOutputInfo.value("model").toString();
                stOutputInfo.manufacturer = jsonOutputInfo.value("manufacturer").toString();
                stOutputInfo.uuid = jsonOutputInfo.value("uuid").toString();
                stOutputInfo.enabled = jsonOutputInfo.value("enabled").toInt();
                stOutputInfo.x = jsonOutputInfo.value("x").toInt();
                stOutputInfo.y = jsonOutputInfo.value("y").toInt();
                stOutputInfo.width = jsonOutputInfo.value("width").toInt();
                stOutputInfo.height = jsonOutputInfo.value("height").toInt();
                stOutputInfo.refresh_rate = jsonOutputInfo.value("refresh_rate").toInt();
                stOutputInfo.transform = jsonOutputInfo.value("transform").toInt();
                stOutputInfo.phys_width = jsonOutputInfo.value("phys_width").toInt();
                stOutputInfo.phys_height = jsonOutputInfo.value("phys_height").toInt();
                stOutputInfo.scale = jsonOutputInfo.value("scale").toDouble();

                QString edidBase64 = jsonOutputInfo.value("edid_base64").toString();
                stOutputInfo.edid = QByteArray::fromBase64(edidBase64.toLatin1());

                QJsonValue modeInfoValue = jsonOutputInfo.value("ModeInfo");
                if (modeInfoValue.isArray()) {
                    QJsonArray jsonModeInfoArray = modeInfoValue.toArray();
                    for (int j = 0; j < jsonModeInfoArray.size(); ++j) {
                        ModeInfo stModeInfo;
                        QJsonObject jsonModeInfo = jsonModeInfoArray.at(i).toObject();
                        stModeInfo.id = jsonModeInfo.value("id").toInt();
                        stModeInfo.width = jsonModeInfo.value("width").toInt();
                        stModeInfo.height = jsonModeInfo.value("height").toInt();
                        stModeInfo.refresh_rate = jsonModeInfo.value("refresh_rate").toInt();
                        stModeInfo.flags = jsonModeInfo.value("flags").toInt();
                        stOutputInfo.lstModeInfos.push_back(stModeInfo);
                    }

                }

                listOutputInfo.push_back(stOutputInfo);

            }
        }

    }
    return listOutputInfo;
}

void wloutput_interface::onMangementAnnounced(quint32 name, quint32 version) {
    qDebug() << "onMangementAnnounced";

    m_pManager = m_pRegisry->createOutputManagement(name, version, this);
    if (!m_pManager || !m_pManager->isValid()) {
        qDebug() << "create manager is nullptr or not valid!";
        return;
    }

    m_pConf = m_pManager->createConfiguration();
    if (!m_pConf || !m_pConf->isValid()) {
        qDebug() << "create output configure is null or is not vaild";
        return;
    }
}

void wloutput_interface::createPlasmaWindowManagement(quint32 name, quint32 version)
{
    qDebug() << "on plasmaWindowManagerment Ann...";
    m_pWindowManager = m_pRegisry->createPlasmaWindowManagement(name, version, this);
    PlasmaWindowManagerInterface *wmanager_inter = new PlasmaWindowManagerInterface(m_pWindowManager);
    if(!QDBusConnection::sessionBus().registerObject(MANAGER_PATH, m_pWindowManager)) {
        qDebug() << "register wayland plasma window manager interface failed";
    }

    if (!m_pWindowManager || !m_pWindowManager->isValid()) {
        qDebug() << " create window manager error or not vaild!";
        return;
    }

    connect(m_pWindowManager, &PlasmaWindowManagement::windowCreated, this, [this, wmanager_inter](PlasmaWindow* plasmaWindow) {
        PlasmaWindowInterface *plasma_window_interface = new PlasmaWindowInterface(plasmaWindow);

        QString dbus_path = WINDOW_PATH + "_" + QString::number(plasmaWindow->internalId());
        if ( !QDBusConnection::sessionBus().registerObject(dbus_path, plasmaWindow)) {
            qDebug() << "register wayland plasma window interface failed " << plasmaWindow->title();
        }
        wmanager_inter->windowAdd(dbus_path);

        connect(plasmaWindow, &PlasmaWindow::unmapped, this, [wmanager_inter, dbus_path, plasma_window_interface](){
            wmanager_inter->windowRemove(dbus_path);
            if(plasma_window_interface != nullptr) plasma_window_interface->deleteLater();
        });
    });

//    qDebug() << m_pWindowManager->windows();
//    m_pWindowManager->setShowingDesktop(true);
}

void wloutput_interface::StartWork()
{

    QObject::connect(m_pConnectThread, &ConnectionThread::connected, this, [ this ] {
        m_eventQueue = new EventQueue(this);
        m_eventQueue->setup(m_pConnectThread);
        m_pRegisry = new Registry(this);

        QObject::connect(m_pRegisry, &Registry::outputManagementAnnounced, this, &wloutput_interface::onMangementAnnounced);
        QObject::connect(m_pRegisry, &Registry::plasmaWindowManagementAnnounced, this, &wloutput_interface::createPlasmaWindowManagement);
        QObject::connect(m_pRegisry, &Registry::outputDeviceAnnounced, this, &wloutput_interface::onDeviceRemove);
        QObject::connect(m_pRegisry, &Registry::outputDeviceRemoved, [](quint32 name) {});
        
        connect(m_pRegisry, &Registry::ddeSeatAnnounced, this,
            [ = ](quint32 name, quint32 version) {
                qDebug() << "create ddeseat";
                m_ddeSeat = m_pRegisry->createDDESeat(name, version, this);
            }
        );

        connect(m_pRegisry, &Registry::seatAnnounced, this, [ = ](quint32 name, quint32 version) {
            qDebug() << "create seat";
            m_seat = m_pRegisry->createSeat(name, version, this);
        });

        connect(m_pRegisry, &Registry::fakeInputAnnounced, this, [ = ](quint32 name, quint32 version) {
            qDebug() << "create fakeinput";
            m_fakeInput = m_pRegisry->createFakeInput(name, version, this);
            bool has = m_fakeInput->isValid();
            if (!has) {
                qDebug() << "create fakeinput failed!!!!!!";
            }
            qDebug() << "create fakeinput successed";
        });


        connect(m_pRegisry, &Registry::idleAnnounced, this, [ = ](quint32 name, quint32 version) {
            m_idle = m_pRegisry->createIdle(name, version, this);
        });
        connect(m_pRegisry, &Registry::interfacesAnnounced, this, [ this ] {
            if(m_wlIdleInterface != nullptr) m_wlIdleInterface->setData(m_seat, m_idle);

            if (m_ddeSeat) {
                m_ddePointer = m_ddeSeat->createDDePointer(this);
                connect(m_ddePointer, &DDEPointer::buttonStateChanged, this,
                    [this] (const QPointF &pos, quint32 button, KWayland::Client::DDEPointer::ButtonState state) {
                        if (state == DDEPointer::ButtonState::Released) {
                            Q_EMIT ButtonRelease(button, pos.x(), pos.y());
                        } else {
                            Q_EMIT ButtonPress(button, pos.x(), pos.y());
                        }
                    }
                );
                connect(m_ddePointer, &DDEPointer::motion, this, [this] (const QPointF &pos) {
                    Q_EMIT CursorMove(pos.x(), pos.y());
                }
                );
                connect(m_ddePointer, &DDEPointer::axisChanged, this, [this] (quint32 time, KWayland::Client::DDEPointer::Axis axis, qreal delta) {
                    Q_EMIT AxisChanged(int(axis), delta);
                });

                m_ddeTouch = m_ddeSeat->createDDETouch(this);
                connect(m_ddeTouch, &DDETouch::touchDown, this,
                    [this] (int32_t kwaylandId, const QPointF &pos) {
                        m_touchMap.insert(kwaylandId, pos);
                        // dde-session-daemon ???????????????
                        Q_EMIT ButtonPress(kwaylandId, pos.x(), pos.y());
                    }
                );
                connect(m_ddeTouch, &DDETouch::touchMotion, this, [this] (int32_t kwaylandId, const QPointF &pos) {
                    // ??????????????????
                    m_touchMap.insert(kwaylandId, pos);
                });

                connect(m_ddeTouch, &DDETouch::touchUp, this,
                    [this] (int32_t kwaylandId) {
                        if (m_touchMap.contains(kwaylandId)) {
                            QPointF pos = m_touchMap.take(kwaylandId);
                            Q_EMIT ButtonRelease(kwaylandId, pos.x(), pos.y());
                            m_touchMap.remove(kwaylandId);
                        }
                    }
                );
            }

            //??????dpms
            createDpmsManagement();
        });

        m_pRegisry->setEventQueue(m_eventQueue);
        m_pRegisry->create(m_pConnectThread);
        m_pRegisry->setup();
    });

    QObject::connect(m_pConnectThread, &ConnectionThread::failed, [ & ] {
          qDebug() << "connect failed to wayland at socket:" << m_pConnectThread->socketName();
          m_bConnected = true;

    });
    QObject::connect(m_pConnectThread, &ConnectionThread::connectionDied, [ & ] {
          qDebug() << "connect failed to wayland at socket:" << m_pConnectThread->socketName();
          if (m_pRegisry)
          {
              m_pRegisry->deleteLater();
          }
          if (m_pConf)
          {
              m_pConf->deleteLater();
          }

    });

    m_pConnectThread->moveToThread(m_pThread);
    m_pThread->start();
    m_pConnectThread->initConnection();
}


QString wloutput_interface::ListOutput()
{
    qDebug() << "in ListOutput()";
    QString rst;
    if(uuid2OutputDevice.empty())
    {
        return rst;
    }

    QList<OutputInfo> listOutputInfos;
    auto oIter = uuid2OutputDevice.begin();
    for (; oIter != uuid2OutputDevice.end(); ++oIter) {
        OutputInfo stOutputInfo = GetOutputInfo(oIter.value());
        //qDebug() << stOutputInfo.uuid;
        listOutputInfos.push_back(stOutputInfo);
    }

    return OutputInfo2Json(listOutputInfos);
}

QString wloutput_interface::GetOutput(QString uuid)
{
    qDebug() << "in GetOutput() uuid=" << uuid;
    QString rst;
    auto itFind = uuid2OutputDevice.find(uuid);
    if (itFind != uuid2OutputDevice.end()) {
        QList<OutputInfo> listOutputInfos;
        OutputInfo stOutputInfo = GetOutputInfo(itFind.value());
        listOutputInfos.push_back(stOutputInfo);
        rst = OutputInfo2Json(listOutputInfos);
    }
    return  rst;
}

void wloutput_interface::Apply(QString outputs)
{
    qDebug() << "in Apply()";
    //qDebug() <<  outputs;
    QList<OutputInfo> listOutputInfo = json2OutputInfo(outputs);
    if (!listOutputInfo.empty()) {

        auto itFind = uuid2OutputDevice.find(listOutputInfo.at(0).uuid);
        if (itFind != uuid2OutputDevice.end()) {
            OutputInfo stOutputInfo = listOutputInfo.at(0);
            auto dev = itFind.value();
            if (!stOutputInfo.lstModeInfos.empty())
            {

                for (auto m : dev->modes())
                {
//                    if (m.size.width() == stOutputInfo.lstModeInfos.at(0).width &&
//                            m.size.height() == stOutputInfo.lstModeInfos.at(0).height &&
//                            m.refreshRate == stOutputInfo.lstModeInfos.at(0).refresh_rate)
//                    {
                        qDebug() << "setmode id" << stOutputInfo.lstModeInfos.at(0).id;
                        m_pConf->setMode(dev, stOutputInfo.lstModeInfos.at(0).id);
//                    }
                }
            }
            m_pConf->setPosition(dev, QPoint(stOutputInfo.x, stOutputInfo.y));
            m_pConf->setEnabled(dev, OutputDevice::Enablement(stOutputInfo.enabled));
            qDebug() << "set output transform to " << stOutputInfo.transform;
            m_pConf->setTransform(dev, OutputDevice::Transform(stOutputInfo.transform));
            m_pConf->apply();

            QObject::connect(m_pConf, &OutputConfiguration::applied, [this]() {
                qDebug() << "Configuration applied!";
                m_bConnected = true;
            });
            QObject::connect(m_pConf, &OutputConfiguration::failed, [this]() {
                qDebug() << "Configuration failed!";
                m_bConnected = true;
            });

        }

    }
    else {
        qDebug() << "listOutputInfo is empty";
    }
}

void wloutput_interface::setBrightness(QString uuid, const int brightness)
{
    auto itFind = uuid2OutputDevice.find(uuid);
    if (itFind != uuid2OutputDevice.end()) {
        auto dev = itFind.value();
        m_pConf->setBrightness(dev, brightness);
        m_pConf->apply();
    } else {
        qDebug() << "uuid is not found";
    }
}

void wloutput_interface::WlSimulateKey(int keycode)
{
    if (m_fakeInput == nullptr) {
        qDebug() << "WlSimulateKey m_fakeInput is NULL!!!!!!";
        return ;
    }

    bool has = m_fakeInput->isValid();
    if (!has) {
        qDebug() << "WlSimulateKey fakeinput is invalid!!!!!!";
        return ;
    }

    qDebug() << "WlSimulateKey keycode:" << keycode;
    m_fakeInput->authenticate(QStringLiteral("test-case"), QStringLiteral("to test"));
    m_fakeInput->requestKeyboardKeyPress(keycode);  //KEY_NUMLOCK
    m_fakeInput->requestKeyboardKeyRelease(keycode);
    return ;
}

void wloutput_interface::onDeviceChanged(OutputDevice *dev)
{
    qDebug() << "onDeviceChanged";
    qDebug()<<"[Changed]: "<<dev->model()<<dev->uuid()<<dev->globalPosition()<<dev->geometry()<<dev->refreshRate()<<dev->pixelSize();
    QString uuid = dev->uuid();
    if (uuid2OutputDevice.find(uuid) == uuid2OutputDevice.end()) {
        uuid2OutputDevice.insert(uuid, dev);
        qDebug() << "OutputDevice::Added uuid=" << uuid;
        OutputInfo stOutputInfo = GetOutputInfo(dev);
        QList<OutputInfo> listOutputInfos;
        listOutputInfos.push_back(stOutputInfo);
        QString json = OutputInfo2Json(listOutputInfos);
//            qDebug() << json;
        Q_EMIT OutputAdded(json);
//                        QDBusMessage message = QDBusMessage::createSignal(PATH, INTERFACE, "OutputAdded");
//                        QList<QVariant> arg;
//                        message.setArguments(arg);
//                        QDBusConnection::sessionBus().send(message);
    }
    else {
        qDebug() << "OutputDevice::changed";
        OutputInfo stOutputInfo = GetOutputInfo(dev);
        QList<OutputInfo> listOutputInfos;
        listOutputInfos.push_back(stOutputInfo);
        QString json = OutputInfo2Json(listOutputInfos);
        //qDebug() << json;
        Q_EMIT OutputChanged(json);
//                        QDBusMessage message = QDBusMessage::createSignal(PATH, INTERFACE, "OutputChanged");
//                        QList<QVariant> arg;
//                        message.setArguments(arg);
//                        QDBusConnection::sessionBus().send(message);
    }

}

void wloutput_interface::onDeviceRemove(quint32 name, quint32 version) {
    qDebug() << "onDeviceRemove";

    auto dev = m_pRegisry->createOutputDevice(name, version);
    if (!dev || !dev->isValid())
    {
        qDebug() << "get dev is null or not valid!";
        return;
    }

    QObject::connect(dev, &OutputDevice::changed, this, [=]{
        this->onDeviceChanged(dev);
    });

    QObject::connect(dev, &OutputDevice::removed, this, [dev, this]{
       qDebug() << "OutputDevice::removed";
       OutputInfo stOutputInfo = GetOutputInfo(dev);
       QList<OutputInfo> listOutputInfos;
       listOutputInfos.push_back(stOutputInfo);
       QString json = OutputInfo2Json(listOutputInfos);
       //qDebug() << json;
       uuid2OutputDevice.remove(dev->uuid());
       Q_EMIT OutputRemoved(json);
//                   QDBusMessage message = QDBusMessage::createSignal(PATH, INTERFACE, "OutputRemoved");
//                   QList<QVariant> arg;
//                   message.setArguments(arg);
//                   QDBusConnection::sessionBus().send(message);
    });

}

void wloutput_interface::createDpmsManagement()
{
    const bool hasDpms = m_pRegisry->hasInterface(Registry::Interface::Dpms);
    if (hasDpms) {
        qDebug() << "create dpms manager";
        const auto dpmsData = m_pRegisry->interface(Registry::Interface::Dpms);
        m_dpmsManger = m_pRegisry->createDpmsManager(dpmsData.name, dpmsData.version);
        m_wldpms_Manager = new WlDpmsManagerInterface(m_dpmsManger);
        if(!QDBusConnection::sessionBus().registerObject(DPMS_MANAGER_PATH, m_dpmsManger)) {
            qDebug() << "register dpms manager interface failed";
        }

        //??????output
        const auto outputs = m_pRegisry->interfaces(Registry::Interface::Output);
        for (auto o : outputs) {
            Output *output = m_pRegisry->createOutput(o.name, o.version, m_pRegisry);
            registerDpmsDbus(output);
        }

        connect(m_pRegisry, &Registry::outputAnnounced, this, [=] (quint32 name, quint32 version){
            Output *output = m_pRegisry->createOutput(name, version, m_pRegisry);
            registerDpmsDbus(output);
        });

    } else {
        qDebug() << ("Compositor does not provide a DpmsManager");
    }
}

 void wloutput_interface::registerDpmsDbus(Output *output)
 {
     if (Dpms * dpms = m_dpmsManger->getDpms(output)) {
         wldpms_interface *dpmsinterface = new wldpms_interface(dpms);
         QString dbus_path;
         int count = 1;
         while(1) {
             dbus_path = DPMS_PATH + "_" + QString::number(count);
             if (m_wldpms_Manager->dpmsList().contains(dbus_path)) {
                  ++count;
             } else {
                  break;
             }
         }

         if ( !QDBusConnection::sessionBus().registerObject(dbus_path, dpms)) {
             qDebug() << "register dpms interface failed";
         } else {
             m_wldpms_Manager->dpmsDbusAdd(dbus_path);
         }

         connect(output, &Output::changed, this, [dpmsinterface, output] {
             dpmsinterface->setDpmsName(output->model());
         });

         connect(output, &Output::removed, this, [dpmsinterface, dbus_path, this] {
             QDBusConnection::sessionBus().unregisterObject(dbus_path);
             m_wldpms_Manager->dpmsDbusRemove(dbus_path);
             dpmsinterface->deleteLater();
         });
     }
 }
