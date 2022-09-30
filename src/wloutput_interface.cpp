// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
static inline double dmin(double x, double y)
{
    return x < y ? x : y;
}

static const float blackbodyColor[] = {
    1.00000000, 0.18172716, 0.00000000, /* 1000K */
    1.00000000, 0.25503671, 0.00000000, /* 1100K */
    1.00000000, 0.30942099, 0.00000000, /* 1200K */
    1.00000000, 0.35357379, 0.00000000, /*  ...  */
    1.00000000, 0.39091524, 0.00000000,
    1.00000000, 0.42322816, 0.00000000,
    1.00000000, 0.45159884, 0.00000000,
    1.00000000, 0.47675916, 0.00000000,
    1.00000000, 0.49923747, 0.00000000,
    1.00000000, 0.51943421, 0.00000000,
    1.00000000, 0.54360078, 0.08679949, /* 2000K */
    1.00000000, 0.56618736, 0.14065513,
    1.00000000, 0.58734976, 0.18362641,
    1.00000000, 0.60724493, 0.22137978,
    1.00000000, 0.62600248, 0.25591950,
    1.00000000, 0.64373109, 0.28819679,
    1.00000000, 0.66052319, 0.31873863,
    1.00000000, 0.67645822, 0.34786758,
    1.00000000, 0.69160518, 0.37579588,
    1.00000000, 0.70602449, 0.40267128,
    1.00000000, 0.71976951, 0.42860152, /* 3000K */
    1.00000000, 0.73288760, 0.45366838,
    1.00000000, 0.74542112, 0.47793608,
    1.00000000, 0.75740814, 0.50145662,
    1.00000000, 0.76888303, 0.52427322,
    1.00000000, 0.77987699, 0.54642268,
    1.00000000, 0.79041843, 0.56793692,
    1.00000000, 0.80053332, 0.58884417,
    1.00000000, 0.81024551, 0.60916971,
    1.00000000, 0.81957693, 0.62893653,
    1.00000000, 0.82854786, 0.64816570, /* 4000K */
    1.00000000, 0.83717703, 0.66687674,
    1.00000000, 0.84548188, 0.68508786,
    1.00000000, 0.85347859, 0.70281616,
    1.00000000, 0.86118227, 0.72007777,
    1.00000000, 0.86860704, 0.73688797, /* 4500K */
    1.00000000, 0.87576611, 0.75326132,
    1.00000000, 0.88267187, 0.76921169,
    1.00000000, 0.88933596, 0.78475236,
    1.00000000, 0.89576933, 0.79989606,
    1.00000000, 0.90198230, 0.81465502, /* 5000K */
    1.00000000, 0.90963069, 0.82838210,
    1.00000000, 0.91710889, 0.84190889,
    1.00000000, 0.92441842, 0.85523742,
    1.00000000, 0.93156127, 0.86836903,
    1.00000000, 0.93853986, 0.88130458,
    1.00000000, 0.94535695, 0.89404470,
    1.00000000, 0.95201559, 0.90658983,
    1.00000000, 0.95851906, 0.91894041,
    1.00000000, 0.96487079, 0.93109690,
    1.00000000, 0.97107439, 0.94305985, /* 6000K */
    1.00000000, 0.97713351, 0.95482993,
    1.00000000, 0.98305189, 0.96640795,
    1.00000000, 0.98883326, 0.97779486,
    1.00000000, 0.99448139, 0.98899179,
    1.00000000, 1.00000000, 1.00000000, /* 6500K */
    0.98947904, 0.99348723, 1.00000000,
    0.97940448, 0.98722715, 1.00000000,
    0.96975025, 0.98120637, 1.00000000,
    0.96049223, 0.97541240, 1.00000000,
    0.95160805, 0.96983355, 1.00000000, /* 7000K */
    0.94303638, 0.96443333, 1.00000000,
    0.93480451, 0.95923080, 1.00000000,
    0.92689056, 0.95421394, 1.00000000,
    0.91927697, 0.94937330, 1.00000000,
    0.91194747, 0.94470005, 1.00000000,
    0.90488690, 0.94018594, 1.00000000,
    0.89808115, 0.93582323, 1.00000000,
    0.89151710, 0.93160469, 1.00000000,
    0.88518247, 0.92752354, 1.00000000,
    0.87906581, 0.92357340, 1.00000000, /* 8000K */
    0.87315640, 0.91974827, 1.00000000,
    0.86744421, 0.91604254, 1.00000000,
    0.86191983, 0.91245088, 1.00000000,
    0.85657444, 0.90896831, 1.00000000,
    0.85139976, 0.90559011, 1.00000000,
    0.84638799, 0.90231183, 1.00000000,
    0.84153180, 0.89912926, 1.00000000,
    0.83682430, 0.89603843, 1.00000000,
    0.83225897, 0.89303558, 1.00000000,
    0.82782969, 0.89011714, 1.00000000, /* 9000K */
    0.82353066, 0.88727974, 1.00000000,
    0.81935641, 0.88452017, 1.00000000,
    0.81530175, 0.88183541, 1.00000000,
    0.81136180, 0.87922257, 1.00000000,
    0.80753191, 0.87667891, 1.00000000,
    0.80380769, 0.87420182, 1.00000000,
    0.80018497, 0.87178882, 1.00000000,
    0.79665980, 0.86943756, 1.00000000,
    0.79322843, 0.86714579, 1.00000000,
    0.78988728, 0.86491137, 1.00000000, /* 10000K */
    0.78663296, 0.86273225, 1.00000000,
    0.78346225, 0.86060650, 1.00000000,
    0.78037207, 0.85853224, 1.00000000,
    0.77735950, 0.85650771, 1.00000000,
    0.77442176, 0.85453121, 1.00000000,
    0.77155617, 0.85260112, 1.00000000,
    0.76876022, 0.85071588, 1.00000000,
    0.76603147, 0.84887402, 1.00000000,
    0.76336762, 0.84707411, 1.00000000,
    0.76076645, 0.84531479, 1.00000000, /* 11000K */
    0.75822586, 0.84359476, 1.00000000,
    0.75574383, 0.84191277, 1.00000000,
    0.75331843, 0.84026762, 1.00000000,
    0.75094780, 0.83865816, 1.00000000,
    0.74863017, 0.83708329, 1.00000000,
    0.74636386, 0.83554194, 1.00000000,
    0.74414722, 0.83403311, 1.00000000,
    0.74197871, 0.83255582, 1.00000000,
    0.73985682, 0.83110912, 1.00000000,
    0.73778012, 0.82969211, 1.00000000, /* 12000K */
    0.73574723, 0.82830393, 1.00000000,
    0.73375683, 0.82694373, 1.00000000,
    0.73180765, 0.82561071, 1.00000000,
    0.72989845, 0.82430410, 1.00000000,
    0.72802807, 0.82302316, 1.00000000,
    0.72619537, 0.82176715, 1.00000000,
    0.72439927, 0.82053539, 1.00000000,
    0.72263872, 0.81932722, 1.00000000,
    0.72091270, 0.81814197, 1.00000000,
    0.71922025, 0.81697905, 1.00000000, /* 13000K */
    0.71756043, 0.81583783, 1.00000000,
    0.71593234, 0.81471775, 1.00000000,
    0.71433510, 0.81361825, 1.00000000,
    0.71276788, 0.81253878, 1.00000000,
    0.71122987, 0.81147883, 1.00000000,
    0.70972029, 0.81043789, 1.00000000,
    0.70823838, 0.80941546, 1.00000000,
    0.70678342, 0.80841109, 1.00000000,
    0.70535469, 0.80742432, 1.00000000,
    0.70395153, 0.80645469, 1.00000000, /* 14000K */
    0.70257327, 0.80550180, 1.00000000,
    0.70121928, 0.80456522, 1.00000000,
    0.69988894, 0.80364455, 1.00000000,
    0.69858167, 0.80273941, 1.00000000,
    0.69729688, 0.80184943, 1.00000000,
    0.69603402, 0.80097423, 1.00000000,
    0.69479255, 0.80011347, 1.00000000,
    0.69357196, 0.79926681, 1.00000000,
    0.69237173, 0.79843391, 1.00000000,
    0.69119138, 0.79761446, 1.00000000, /* 15000K */
    0.69003044, 0.79680814, 1.00000000,
    0.68888844, 0.79601466, 1.00000000,
    0.68776494, 0.79523371, 1.00000000,
    0.68665951, 0.79446502, 1.00000000,
    0.68557173, 0.79370830, 1.00000000,
    0.68450119, 0.79296330, 1.00000000,
    0.68344751, 0.79222975, 1.00000000,
    0.68241029, 0.79150740, 1.00000000,
    0.68138918, 0.79079600, 1.00000000,
    0.68038380, 0.79009531, 1.00000000, /* 16000K */
    0.67939381, 0.78940511, 1.00000000,
    0.67841888, 0.78872517, 1.00000000,
    0.67745866, 0.78805526, 1.00000000,
    0.67651284, 0.78739518, 1.00000000,
    0.67558112, 0.78674472, 1.00000000,
    0.67466317, 0.78610368, 1.00000000,
    0.67375872, 0.78547186, 1.00000000,
    0.67286748, 0.78484907, 1.00000000,
    0.67198916, 0.78423512, 1.00000000,
    0.67112350, 0.78362984, 1.00000000, /* 17000K */
    0.67027024, 0.78303305, 1.00000000,
    0.66942911, 0.78244457, 1.00000000,
    0.66859988, 0.78186425, 1.00000000,
    0.66778228, 0.78129191, 1.00000000,
    0.66697610, 0.78072740, 1.00000000,
    0.66618110, 0.78017057, 1.00000000,
    0.66539706, 0.77962127, 1.00000000,
    0.66462376, 0.77907934, 1.00000000,
    0.66386098, 0.77854465, 1.00000000,
    0.66310852, 0.77801705, 1.00000000, /* 18000K */
    0.66236618, 0.77749642, 1.00000000,
    0.66163375, 0.77698261, 1.00000000,
    0.66091106, 0.77647551, 1.00000000,
    0.66019791, 0.77597498, 1.00000000,
    0.65949412, 0.77548090, 1.00000000,
    0.65879952, 0.77499315, 1.00000000,
    0.65811392, 0.77451161, 1.00000000,
    0.65743716, 0.77403618, 1.00000000,
    0.65676908, 0.77356673, 1.00000000,
    0.65610952, 0.77310316, 1.00000000, /* 19000K */
    0.65545831, 0.77264537, 1.00000000,
    0.65481530, 0.77219324, 1.00000000,
    0.65418036, 0.77174669, 1.00000000,
    0.65355332, 0.77130560, 1.00000000,
    0.65293404, 0.77086988, 1.00000000,
    0.65232240, 0.77043944, 1.00000000,
    0.65171824, 0.77001419, 1.00000000,
    0.65112144, 0.76959404, 1.00000000,
    0.65053187, 0.76917889, 1.00000000,
    0.64994941, 0.76876866, 1.00000000, /* 20000K */
    0.64937392, 0.76836326, 1.00000000,
    0.64880528, 0.76796263, 1.00000000,
    0.64824339, 0.76756666, 1.00000000,
    0.64768812, 0.76717529, 1.00000000,
    0.64713935, 0.76678844, 1.00000000,
    0.64659699, 0.76640603, 1.00000000,
    0.64606092, 0.76602798, 1.00000000,
    0.64553103, 0.76565424, 1.00000000,
    0.64500722, 0.76528472, 1.00000000,
    0.64448939, 0.76491935, 1.00000000, /* 21000K */
    0.64397745, 0.76455808, 1.00000000,
    0.64347129, 0.76420082, 1.00000000,
    0.64297081, 0.76384753, 1.00000000,
    0.64247594, 0.76349813, 1.00000000,
    0.64198657, 0.76315256, 1.00000000,
    0.64150261, 0.76281076, 1.00000000,
    0.64102399, 0.76247267, 1.00000000,
    0.64055061, 0.76213824, 1.00000000,
    0.64008239, 0.76180740, 1.00000000,
    0.63961926, 0.76148010, 1.00000000, /* 22000K */
    0.63916112, 0.76115628, 1.00000000,
    0.63870790, 0.76083590, 1.00000000,
    0.63825953, 0.76051890, 1.00000000,
    0.63781592, 0.76020522, 1.00000000,
    0.63737701, 0.75989482, 1.00000000,
    0.63694273, 0.75958764, 1.00000000,
    0.63651299, 0.75928365, 1.00000000,
    0.63608774, 0.75898278, 1.00000000,
    0.63566691, 0.75868499, 1.00000000,
    0.63525042, 0.75839025, 1.00000000, /* 23000K */
    0.63483822, 0.75809849, 1.00000000,
    0.63443023, 0.75780969, 1.00000000,
    0.63402641, 0.75752379, 1.00000000,
    0.63362667, 0.75724075, 1.00000000,
    0.63323097, 0.75696053, 1.00000000,
    0.63283925, 0.75668310, 1.00000000,
    0.63245144, 0.75640840, 1.00000000,
    0.63206749, 0.75613641, 1.00000000,
    0.63168735, 0.75586707, 1.00000000,
    0.63131096, 0.75560036, 1.00000000, /* 24000K */
    0.63093826, 0.75533624, 1.00000000,
    0.63056920, 0.75507467, 1.00000000,
    0.63020374, 0.75481562, 1.00000000,
    0.62984181, 0.75455904, 1.00000000,
    0.62948337, 0.75430491, 1.00000000,
    0.62912838, 0.75405319, 1.00000000,
    0.62877678, 0.75380385, 1.00000000,
    0.62842852, 0.75355685, 1.00000000,
    0.62808356, 0.75331217, 1.00000000,
    0.62774186, 0.75306977, 1.00000000, /* 25000K */
    0.62740336, 0.75282962, 1.00000000};

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
                        // dde-session-daemon 监听此信号
                        Q_EMIT ButtonPress(kwaylandId, pos.x(), pos.y());
                    }
                );
                connect(m_ddeTouch, &DDETouch::touchMotion, this, [this] (int32_t kwaylandId, const QPointF &pos) {
                    // 更新滑动位置
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

            //创建dpms
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

void wloutput_interface::SetBrightness(QString uuid, const int brightness)
{
    auto itFind = uuid2OutputDevice.find(uuid);
    if (itFind != uuid2OutputDevice.end())
    {
        auto dev = itFind.value();
        setColorCurvesBrightness(dev, brightness / 100.0);
    } else {
        qDebug() << "uuid is not found";
    }
}

void wloutput_interface::SetColorTemperature(QString uuid, const int temperature)
{
    auto itFind = uuid2OutputDevice.find(uuid);
    if (itFind != uuid2OutputDevice.end())
    {
        auto dev = itFind.value();
        setColorCurvesTemperature(dev, temperature);
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

        //创建output
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

 void wloutput_interface::setColorCurvesBrightness(OutputDevice *outputDevice, const float brightness)
 {
    OutputDevice::ColorCurves colorCurves = outputDevice->colorCurves();
    int rampsize = colorCurves.red.size();
    QVector<quint16> vecRed;
    QVector<quint16> vecGreen;
    QVector<quint16> vecBlue;

    for (int i = 0; i < rampsize; i++)
    {
        if (abs(brightness - 1.0) < 0.00001) {
            quint16 value = (double)i / (double)(rampsize - 1) * (double)UINT16_MAX;
            vecRed.append(value);
            vecGreen.append(value);
            vecBlue.append(value);
        } else {
            quint16 value = dmin(pow((double)i / (double)(rampsize - 1), 1.0) * brightness, 1.0) * (double)UINT16_MAX;
            vecRed.append(value);
            vecGreen.append(value);
            vecBlue.append(value);
        }
    }
    qDebug() << "setColorCurvesBrightness, gamma size is " << rampsize;
    if (m_pConf) {
        m_pConf->setColorCurves(outputDevice, vecRed, vecGreen, vecBlue);
        m_pConf->apply();
    }
 }

 void wloutput_interface::setColorCurvesTemperature(OutputDevice *outputDevice, const int temperature)
 {
    OutputDevice::ColorCurves colorCurves = outputDevice->colorCurves();
    int rampsize = colorCurves.red.size();
    QVector<quint16> vecRed;
    QVector<quint16> vecGreen;
    QVector<quint16> vecBlue;

    // linear default state
    for (int i = 0; i < rampsize; i++)
    {
        quint16 value = (double)i / rampsize * (UINT16_MAX + 1);
        vecRed.push_back(value);
        vecGreen.push_back(value);
        vecBlue.push_back(value);
    }

    // approximate white point
    float whitePoint[3];
    float alpha = (temperature % 100) / 100.0;
    int bbCIndex = ((temperature - 1000) / 100) * 3;
    whitePoint[0] = (1. - alpha) * blackbodyColor[bbCIndex] + alpha * blackbodyColor[bbCIndex + 3];
    whitePoint[1] = (1. - alpha) * blackbodyColor[bbCIndex + 1] + alpha * blackbodyColor[bbCIndex + 4];
    whitePoint[2] = (1. - alpha) * blackbodyColor[bbCIndex + 2] + alpha * blackbodyColor[bbCIndex + 5];

    for (int i = 0; i < rampsize; i++)
    {
        vecRed[i] = (double)vecRed[i] / (UINT16_MAX + 1) * whitePoint[0] * (UINT16_MAX + 1);
        vecGreen[i] = (double)vecGreen[i] / (UINT16_MAX + 1) * whitePoint[1] * (UINT16_MAX + 1);
        vecBlue[i] = (double)vecBlue[i] / (UINT16_MAX + 1) * whitePoint[2] * (UINT16_MAX + 1);
    }
    qDebug() << "setColorCurvesTemperature, gamma size is " << rampsize;
    if (m_pConf) {
        m_pConf->setColorCurves(outputDevice, vecRed, vecGreen, vecBlue);
        m_pConf->apply();
    }
 }
