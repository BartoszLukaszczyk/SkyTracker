#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "bluetoothmanager.h"
#include <QTimer>
#include <QMessageBox>


#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_ANDROID
    requestAndroidPermissions();
#endif


    m_bt = new BluetoothManager(this);

    connect(ui->Homing, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->Page_Homing);
        ui->statusbar->showMessage("Send Home");
        m_bt->sendCommand("HOME");
    });
    connect(ui->Choose_Object, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->Page_Choose_Object);
    });
    connect(ui->Manual_Control, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->Page_Manual_Control);
    });
    connect(ui->Other, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->Page_Other);
        populatePairedDevices();
    });
    connect(ui->BT_Connect, &QPushButton::clicked, this, [=]() {
        this->on_BT_Connect_clicked();
    });
    // --- MANUAL CONTROL SETUP ---
    repeatTimer = new QTimer(this);
    repeatTimer->setInterval(200);
    connect(repeatTimer, &QTimer::timeout, this, &MainWindow::onRepeatTimeout);

    const auto btns = {
        ui->Up_Button,
        ui->Down_Button,
        ui->Left_Button,
        ui->Right_Button
    };
    for (auto *b : btns) {
        connect(b, &QPushButton::pressed,  this, &MainWindow::onManualPressed);
        connect(b, &QPushButton::released, this, &MainWindow::onManualReleased);
    }

    m_posSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_posSource) {
        connect(m_posSource, &QGeoPositionInfoSource::positionUpdated,
                this,        &MainWindow::onPositionUpdated);
        m_posSource->setUpdateInterval(10000);
        m_posSource->startUpdates();
    } else {
        statusBar()->showMessage("GPS unapproachable!");
    }

    m_horizonsMgr = new HorizonsManager(this);
    connect(m_horizonsMgr, &HorizonsManager::ephemerisReady,
            this, &MainWindow::onEphemerisReady);
    connect(m_horizonsMgr, &HorizonsManager::ephemerisError,
            this, &MainWindow::onEphemerisError);

    const auto objButtons = {
        ui->Sun_Button,
        ui->Moon_Button,
        ui->Mercury_Button,
        ui->Venus_Button,
        ui->Mars_Button,
        ui->Jupyter_Button,
        ui->Saturn_Button
    };
    for (auto *b : objButtons) {
        connect(b, &QPushButton::clicked,
                this, &MainWindow::onObjectButtonClicked, Qt::UniqueConnection);
    }

    connect(ui->Break_Button, &QPushButton::clicked, this, [=]() {
        m_bt->sendCommand(QByteArray("BREAK\n"));
        m_horizonsMgr->stopLiveTracking();
        statusBar()->showMessage("Tracking Stop", 2000);
    });
}
#ifdef Q_OS_ANDROID
void MainWindow::requestAndroidPermissions()
{
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid())
        return;

    QStringList perms = {
        "android.permission.BLUETOOTH_CONNECT",
        "android.permission.BLUETOOTH_SCAN",
        "android.permission.ACCESS_FINE_LOCATION",
        "android.permission.BLUETOOTH_ADMIN"
    };

    QJniEnvironment env;
    jclass stringClass = env.findClass("java/lang/String");
    jobjectArray jperms = env->NewObjectArray(perms.size(), stringClass, nullptr);
    for (int i = 0; i < perms.size(); ++i) {
        QJniObject str = QJniObject::fromString(perms.at(i));
        env->SetObjectArrayElement(jperms, i, str.object<jstring>());
    }

    QJniObject::callStaticMethod<void>(
        "androidx/core/app/ActivityCompat",
        "requestPermissions",
        "(Landroid/app/Activity;[Ljava/lang/String;I)V",
        activity.object<jobject>(),
        jperms,
        0
        );
}
#endif

void MainWindow::populatePairedDevices()
{
    ui->Combo_Devices->clear();
#ifdef Q_OS_ANDROID
    QJniObject adapter = QJniObject::callStaticObjectMethod(
        "android/bluetooth/BluetoothAdapter",
        "getDefaultAdapter",
        "()Landroid/bluetooth/BluetoothAdapter;");
    if (!adapter.isValid()) {
        ui->Device_List_Label->setText("No BT Device");
        return;
    }

    QJniObject bondedSet = adapter.callObjectMethod(
        "getBondedDevices",
        "()Ljava/util/Set;");
    if (!bondedSet.isValid()) {
        ui->Device_List_Label->setText("Unable to download paired devices");
        return;
    }

    QJniObject list("java/util/ArrayList",
                    "(Ljava/util/Collection;)V",
                    bondedSet.object<jobject>());
    jint count = list.callMethod<jint>("size");
    ui->Device_List_Label->setText(
        count ? "Select Device:" : "No devices paired"
        );

    for (jint i = 0; i < count; ++i) {
        QJniObject dev = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        QJniObject name = dev.callObjectMethod("getName", "()Ljava/lang/String;");
        QJniObject addr = dev.callObjectMethod("getAddress", "()Ljava/lang/String;");
        if (name.isValid() && addr.isValid()) {
            ui->Combo_Devices->addItem(
                name.toString() + " (" + addr.toString() + ")",
                addr.toString()
                );
        }
    }

    // QJniEnvironment env2;
    // if (env2.hasException()) env2.clearException();
#else
    ui->Device_List_Label->setText("Funkcja dostępna tylko na Androidzie");
#endif
}

void MainWindow::on_BT_Connect_clicked()
{
    // 1) Get selected device address
    QString addr = ui->Combo_Devices->currentData().toString();
    if (addr.isEmpty()) {
        ui->statusbar->showMessage("Please select a device first");
        return;
    }

    // 2) (Optional) Disable connect button to prevent double clicks
    // ui->BT_Connect->setEnabled(false);

    // 3) Start connection
    m_bt->connectToDevice(QBluetoothAddress(addr));
    // ui->statusbar->showMessage("Connecting to " + addr + "...", 3000);

    // 4) Show a confirmation after 3 seconds
    QTimer::singleShot(3000, this, [=]() {
        ui->statusbar->showMessage("Connected", 5000);
    });
}
void MainWindow::onManualPressed()
{
    currentDir = directionForSender(sender());
    if (currentDir == None) return;

    // Send initial short movement
    switch (currentDir) {
    case Up:    m_bt->sendCommand("UP\n");    break;
    case Down:  m_bt->sendCommand("DOWN\n");  break;
    case Left:  m_bt->sendCommand("LEFT\n");  break;
    case Right: m_bt->sendCommand("RIGHT\n"); break;
    default:    break;
    }

    // Start repeat timer (200ms) for long press
    repeatTimer->start();
}

void MainWindow::onManualReleased()
{
    repeatTimer->stop();
    m_bt->sendCommand("STOP\n");
    currentDir = None;
}

void MainWindow::onRepeatTimeout()
{
    if (currentDir == None)
        return;

    // Long press → smooth continuous movement
    switch (currentDir) {
    case Up:    m_bt->sendCommand("UP_START\n");    break;
    case Down:  m_bt->sendCommand("DOWN_START\n");  break;
    case Left:  m_bt->sendCommand("LEFT_START\n");  break;
    case Right: m_bt->sendCommand("RIGHT_START\n"); break;
    default:    break;
    }

    // No further repeats needed – ESP will continue motion
    repeatTimer->stop();
}

MainWindow::Direction MainWindow::directionForSender(QObject *s) const
{
    if (s == ui->Up_Button)    return Up;
    if (s == ui->Down_Button)  return Down;
    if (s == ui->Left_Button)  return Left;
    if (s == ui->Right_Button) return Right;
    return None;
}
void MainWindow::onPositionUpdated(const QGeoPositionInfo &info)
{
    m_currentCenter = info.coordinate();
    QString msg = QString("Location: %1°, %2°")
                      .arg(m_currentCenter.latitude(),  0, 'f', 6)
                      .arg(m_currentCenter.longitude(), 0, 'f', 6);
    statusBar()->showMessage(msg, 5000);
}
void MainWindow::onObjectButtonClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    ui->Sun_Button->setEnabled(false);
    ui->Moon_Button->setEnabled(false);
    ui->Mercury_Button->setEnabled(false);
    ui->Venus_Button->setEnabled(false);
    ui->Mars_Button->setEnabled(false);
    ui->Jupyter_Button->setEnabled(false);
    ui->Saturn_Button->setEnabled(false);

    // 1) Determine Horizons object ID
    QString objectId;
    if      (btn == ui->Sun_Button)      objectId = "10";
    else if (btn == ui->Moon_Button)     objectId = "301";
    else if (btn == ui->Mercury_Button)objectId = "199";
    else if (btn == ui->Venus_Button)    objectId = "299";
    else if (btn == ui->Mars_Button)     objectId = "499";
    else if (btn == ui->Jupyter_Button)  objectId = "599";
    else if (btn == ui->Saturn_Button)   objectId = "699";
    else return;

    // 2) Start time = now rounded to nearest minute + 2 minutes
    QDateTime now = QDateTime::currentDateTimeUtc();
    int secs = now.time().second();
    int offset = 120 - secs;
    QDateTime start = now.addSecs(offset)
                          .toLocalTime();
    start.setTime(QTime(start.time().hour(),
                        start.time().minute(), 0));
    QDateTime end = start.addSecs(3600);  // tracking for 1 hour

    // 3) Request ephemeris from JPL
    m_horizonsMgr->fetchEphemeris(
        objectId,
        m_currentCenter,
        start,
        end,
        /*step (sec):*/ 4
        );

    statusBar()->showMessage(
        QString("Fetching trajectory for %1 from %2 to %3")
            .arg(btn->text())
            .arg(start.toString(Qt::ISODate))
            .arg(end.toString(Qt::ISODate)),
        5000
        );
    m_horizonsMgr->fetchEphemeris(objectId, m_currentCenter, start, end, 4);
}
void MainWindow::onEphemerisReady(const QVector<EphemPoint> &traj) {
    qDebug() << "Ephemeris received, records:" << traj.size();

    ui->Sun_Button->setEnabled(true);
    ui->Moon_Button->setEnabled(true);
    ui->Mercury_Button->setEnabled(true);
    ui->Venus_Button->setEnabled(true);
    ui->Mars_Button->setEnabled(true);
    ui->Jupyter_Button->setEnabled(true);
    ui->Saturn_Button->setEnabled(true);
    // Send data to ESP or display graphically
    qDebug() << "Starting sendTrajectorySteps...";
    double panStepsPerDeg  = (8 * 180) / (14 * 1.8);
    double tiltStepsPerDeg = (8 * 84)  / (14 * 1.8);

    m_horizonsMgr->sendTrajectorySteps(m_bt, panStepsPerDeg, tiltStepsPerDeg);
    qDebug() << "sendTrajectorySteps completed";
}

void MainWindow::onEphemerisError(const QString &errorString) {
    ui->Sun_Button->setEnabled(true);
    ui->Moon_Button->setEnabled(true);
    ui->Mercury_Button->setEnabled(true);
    ui->Venus_Button->setEnabled(true);
    ui->Mars_Button->setEnabled(true);
    ui->Jupyter_Button->setEnabled(true);
    ui->Saturn_Button->setEnabled(true);
    QMessageBox::critical(this, "Ephemeris download error", errorString);
}

MainWindow::~MainWindow()
{
    delete ui;
}
