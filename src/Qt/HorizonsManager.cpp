#include "HorizonsManager.h"
#include "bluetoothmanager.h"
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QVector3D>
#include <QtMath>
#include <QLocale>
#include <QList>
#include <QThread>
#include <QTimer>


HorizonsManager::HorizonsManager(QObject *parent)
    : QObject(parent),
    m_liveTimer(nullptr),
    m_stepIdx(nullptr)
{
    connect(&m_manager, &QNetworkAccessManager::finished,
            this, &HorizonsManager::onNetworkFinished);
}

void HorizonsManager::fetchEphemeris(const QString &objectId,
                                     const QGeoCoordinate &m_currentCenter,
                                     const QDateTime &start,
                                     const QDateTime &end,
                                     int stepSec)
{
    m_stepSec = stepSec;
    m_center = m_currentCenter;
    double alt_km = m_center.altitude() / 1000.0;
    QUrl url("https://ssd.jpl.nasa.gov/api/horizons.api");
    QUrlQuery q;
    q.addQueryItem("format",     "text");
    q.addQueryItem("COMMAND",    QString("'%1'").arg(objectId));
    q.addQueryItem("CENTER",     "'coord@399'");
    q.addQueryItem("COORD_TYPE", "'GEODETIC'");
    q.addQueryItem("SITE_COORD", QString("'%1,%2,%3'")
                                     .arg(m_center.longitude(), 0, 'f', 6)
                                     .arg(m_center.latitude(),  0, 'f', 6)
                                     .arg(alt_km,              0, 'f', 6));
    q.addQueryItem("START_TIME",
                   QString("'%1'").arg(start.toUTC().toString("yyyy-MMM-dd HH:mm")));
    q.addQueryItem("STOP_TIME",
                   QString("'%1'").arg(end.toUTC().toString("yyyy-MMM-dd HH:mm")));
    q.addQueryItem("STEP_SIZE",  "'1 m'");
    q.addQueryItem("MAKE_EPHEM", "YES");
    q.addQueryItem("TABLE_TYPE","'OBSERVER'");
    q.addQueryItem("ANG_FORMAT","DEG");
    q.addQueryItem("CSV_FORMAT","YES");
    q.addQueryItem("QUANTITIES","'2'");
    url.setQuery(q);
    qDebug() << "Horizons query URL:" << url.toString();
    m_manager.get(QNetworkRequest(url));
}

void HorizonsManager::onNetworkFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit ephemerisError(reply->errorString());
        reply->deleteLater();
        return;
    }
    QString raw = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    dumpRawCsv(raw);

    // 1) parse RA/DEC
    QVector<EphemRD> rawPts = parseHorizonsText(raw);
    dumpParsedCsv(rawPts);

    // 2) RA/DEC -> topocentric Az/El
    QVector<EphemPoint> topo = radecToAltAz(rawPts,
                                            m_center.latitude(),
                                            m_center.longitude());
    dumpTopoCsv(topo);

    // 3) Interpolation m_stepSec
    m_fullTraj = interpolateTrajectory(topo);
    dumpDebugCsv(m_fullTraj);

    emit ephemerisReady(m_fullTraj);
}

QVector<EphemRD> HorizonsManager::parseHorizonsText(const QString &txt) const {
    QStringList all = txt.split('\n', Qt::SkipEmptyParts);
    int start=0, end=all.size();
    for(int i=0; i<all.size(); ++i) {
        QString t = all[i].trimmed();
        if(t.startsWith("$$SOE")) start = i+1;
        else if(t.startsWith("$$EOE")) { end = i; break; }
    }
    QVector<EphemRD> out;
    QLocale eng(QLocale::English);
    for(int i=start; i<end; ++i) {
        QString line = all[i].trimmed();
        if(line.startsWith("$$") || line.isEmpty()) continue;
        QStringList cols = line.split(',', Qt::KeepEmptyParts);
        if(cols.size()<5) continue;
        QDateTime dt = eng.toDateTime(cols[0].trimmed(), "yyyy-MMM-dd HH:mm");
        if(!dt.isValid()) continue;
        bool ok1, ok2;
        double ra  = cols[3].toDouble(&ok1);
        double dec = cols[4].toDouble(&ok2);
        if(!ok1||!ok2) continue;
        out.append({dt.toUTC(), ra, dec});
    }
    return out;
}

double HorizonsManager::julianDay(const QDateTime &dtUtc) const {
    int Y=dtUtc.date().year(), M=dtUtc.date().month();
    if(M<=2){ Y--; M+=12; }
    int D=dtUtc.date().day();
    double frac = (dtUtc.time().hour()
                   +dtUtc.time().minute()/60.0
                   +dtUtc.time().second()/3600.0)/24.0;
    int A=Y/100, B=2-A+A/4;
    return qFloor(365.25*(Y+4716)) + qFloor(30.6001*(M+1))
           + D + B - 1524.5 + frac;
}

double HorizonsManager::gmstDeg(const QDateTime &dtUtc) const {
    double JD=julianDay(dtUtc), T=(JD-2451545.0)/36525.0;
    double gmst=280.46061837
                  +360.98564736629*(JD-2451545.0)
                  +0.000387933*T*T
                  -T*T*T/38710000.0;
    gmst=fmod(gmst,360.0); if(gmst<0) gmst+=360.0;
    return gmst;
}

QVector<EphemPoint> HorizonsManager::radecToAltAz(
    const QVector<EphemRD> &in,
    double latDeg,
    double lonDeg) const
{
    QVector<EphemPoint> out; out.reserve(in.size());
    double latRad=qDegreesToRadians(latDeg);
    for(const auto &p: in){
        double GMST=gmstDeg(p.utc);
        double LST=fmod(GMST+lonDeg,360.0);
        if(LST<0) LST+=360.0;
        double haDeg=fmod(LST-p.raDeg+180.0,360.0)-180.0;
        double ha=qDegreesToRadians(haDeg);
        double dec=qDegreesToRadians(p.decDeg);
        double sinEl=qSin(dec)*qSin(latRad)
                       +qCos(dec)*qCos(latRad)*qCos(ha);
        sinEl=qBound(-1.0,sinEl,1.0);
        double el=qAsin(sinEl);
        double cosAz=(qSin(dec)-qSin(el)*qSin(latRad))
                       /(qCos(el)*qCos(latRad));
        cosAz=qBound(-1.0,cosAz,1.0);
        double az=qAcos(cosAz);
        if(qSin(ha)>0) az=2*M_PI-az;
        out.append({p.utc, qRadiansToDegrees(az), qRadiansToDegrees(el)});
    }
    return out;
}

QVector<EphemPoint> HorizonsManager::interpolateTrajectory(
    const QVector<EphemPoint> &in) const
{
    QVector<EphemPoint> out;
    if(in.size()<2) return out;
    auto toVec=[&](double az,double el){
        double a=qDegreesToRadians(az), e=qDegreesToRadians(el);
        return QVector3D(qCos(e)*qCos(a), qCos(e)*qSin(a), qSin(e));
    };
    auto toAzEl=[&](const QVector3D &v){
        double r=v.length();
        double el=qRadiansToDegrees(qAsin(v.z()/r));
        double az=qRadiansToDegrees(qAtan2(v.y(),v.x()));
        return qMakePair(az,el);
    };
    for(int i=0;i+1<in.size();++i){
        const auto &p0=in[i], &p1=in[i+1];
        int delta=p0.utc.secsTo(p1.utc);
        int steps=delta/m_stepSec;
        QVector3D v0=toVec(p0.az,p0.el), v1=toVec(p1.az,p1.el);
        double dot=QVector3D::dotProduct(v0,v1);
        dot=qBound(-1.0,dot,1.0);
        double omega=qAcos(dot);
        for(int s=0;s<steps;++s){
            double t=(double)s/steps;
            QVector3D vs = qFuzzyIsNull(omega)? v0
                                               : (v0*qSin((1-t)*omega) + v1*qSin(t*omega)) / qSin(omega);
            auto pr=toAzEl(vs);
            out.append({p0.utc.addSecs(s*m_stepSec), pr.first, pr.second});
        }
    }
    out.append(in.last());
    return out;
}

// Debug dumps
void HorizonsManager::dumpRawCsv(const QString &raw) const{
    QString loc=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if(loc.isEmpty()) loc=QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path=loc+QDir::separator()+"horizons_raw.txt";
    QFile f(path);
    if(f.open(QIODevice::WriteOnly|QIODevice::Truncate)){
        f.write(raw.toUtf8()); f.close();
        qDebug()<<"Raw saved to"<<path;
    }
}
void HorizonsManager::dumpParsedCsv(const QVector<EphemRD> &parsed) const{
    QString loc=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if(loc.isEmpty()) loc=QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path=loc+QDir::separator()+"horizons_parsed.csv";
    QFile f(path);
    if(f.open(QIODevice::WriteOnly|QIODevice::Truncate)){
        QTextStream out(&f);
        out<<"UTC,RA_deg,DEC_deg\n";
        for(auto &p: parsed){
            out<<p.utc.toString(Qt::ISODate)<<","<<p.raDeg<<","<<p.decDeg<<"\n";
        }
        f.close(); qDebug()<<"Parsed CSV saved to"<<path;
    }
}
void HorizonsManager::dumpTopoCsv(const QVector<EphemPoint> &topo) const{
    QString loc=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if(loc.isEmpty()) loc=QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path=loc+QDir::separator()+"horizons_topo.csv";
    QFile f(path);
    if(f.open(QIODevice::WriteOnly|QIODevice::Truncate)){
        QTextStream out(&f);
        out<<"UTC,Az_deg,El_deg\n";
        for(auto &p: topo){
            out<<p.utc.toString(Qt::ISODate)<<","<<p.az<<","<<p.el<<"\n";
        }
        f.close(); qDebug()<<"Topo CSV saved to"<<path;
    }
}
void HorizonsManager::dumpDebugCsv(const QList<EphemPoint> &traj) const
{
    QString loc = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (loc.isEmpty()) loc = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = loc + QDir::separator() + "horizons_debug.csv";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "UTC,Az_deg,El_deg\n";
        for (const auto &p : traj) {
            out << p.utc.toString(Qt::ISODate) << ","
                << QString::number(p.az, 'f', 4) << ","
                << QString::number(p.el, 'f', 4) << "\n";
        }
        f.close();
        qDebug() << "Debug saved to" << path;
    }
}
void HorizonsManager::dumpStepsCsv(const QVector<StepRecord> &steps) const {
    QString loc=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(loc.isEmpty()) loc=QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path=loc+QDir::separator()+"horizons_steps.csv";
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) return;
    QTextStream out(&f);
    out<<"offset_ms,deltaPan,deltaTilt\n";
    for(const auto &r: steps) {
        out<<r.offset_ms<<","<<r.deltaPan<<","<<r.deltaTilt<<"\n";
    }
    f.close();
    qDebug()<<"Steps CSV saved to"<<path;
}

void HorizonsManager::sendTrajectorySteps(BluetoothManager *bt,
                                          double degPerStepPan,
                                          double degPerStepTilt)
{
    if (!bt || m_fullTraj.isEmpty()) return;

    QVector<StepRecord> buf;
    buf.reserve(m_fullTraj.size());

    const double panStartAz  = 180.0;
    const double tiltStartEl =  45.0;
    int executedPan  = 0;
    int executedTilt = 0;

    for (int i = 0; i < m_fullTraj.size(); ++i) {
        const auto &p = m_fullTraj[i];
        uint32_t offset_ms = uint32_t(i * m_stepSec * 1000);

        double rawDeltaPan = p.az - panStartAz;
        double deltaPan    = fmod(rawDeltaPan + 540.0, 360.0) - 180.0;
        int desiredPan     = qRound(deltaPan * degPerStepPan);

        double deltaTilt   = p.el - tiltStartEl;
        int desiredTilt    = qRound(deltaTilt * degPerStepTilt);

        int16_t stepPan  = int16_t(desiredPan  - executedPan);
        int16_t stepTilt = int16_t(desiredTilt - executedTilt);
        executedPan  += stepPan;
        executedTilt += stepTilt;

        buf.append({ offset_ms, stepPan, stepTilt });
    }

    if (buf.isEmpty()) return;

    // T0 = now rounded to full minute + 2 minutes (seconds = 0)
    QDateTime tNow = QDateTime::currentDateTime();
    int secs = tNow.time().second();
    QDateTime t0 = tNow.addSecs(120 - secs);
    t0.setTime(QTime(t0.time().hour(), t0.time().minute(), 0));
    QDateTime baseTime = t0.addSecs(-30);  // wykonanie PREP 30s wcześniej

    const StepRecord &prep = buf.first();

    // 1. Send PREP command
    bt->sendCommand("PREP\n");
    QThread::msleep(200);

    // 2. Send initial STEP deltaPan deltaTilt
    QString stepCmd = QString("STEP %1 %2\n")
                          .arg(prep.deltaPan)
                          .arg(prep.deltaTilt);
    bt->sendCommand(stepCmd.toUtf8());

    // 3. Set up a timer for real-time tracking
    m_liveTimer = new QTimer(bt);
    m_stepIdx = new int(1);  // start from 1, because 0 was already sent as STEP

    m_liveTimer->setInterval(100);
    m_liveTimer->start();

    QObject::connect(m_liveTimer, &QTimer::timeout, bt, [=]() mutable {
        if (*m_stepIdx >= buf.size()) {
            m_liveTimer->stop();
            m_liveTimer->deleteLater();
            delete m_stepIdx;
            qDebug() << "Trajectory completed.";
            return;
        }

        const StepRecord &r = buf[*m_stepIdx];
        QDateTime execTime = baseTime.addMSecs(r.offset_ms);
        QDateTime now = QDateTime::currentDateTime();

        if (now >= execTime) {
            if (r.deltaPan != 0) {
                if (r.deltaPan > 0) bt->sendCommand(QByteArray("FRIGHT\n"));
                else                bt->sendCommand(QByteArray("FLEFT\n"));

                QTimer::singleShot(100, bt, [=]() {
                    if (r.deltaTilt > 0) bt->sendCommand(QByteArray("FUP\n"));
                    else if (r.deltaTilt < 0) bt->sendCommand(QByteArray("FDOWN\n"));
                });
            } else {
                if (r.deltaTilt > 0) bt->sendCommand(QByteArray("FUP\n"));
                else if (r.deltaTilt < 0) bt->sendCommand(QByteArray("FDOWN\n"));
            }

            (*m_stepIdx)++;
        }
    });
}
void HorizonsManager::stopLiveTracking() {
    if (m_liveTimer) {
        m_liveTimer->stop();
        m_liveTimer->deleteLater();
        m_liveTimer = nullptr;
    }

    if (m_stepIdx) {
        delete m_stepIdx;
        m_stepIdx = nullptr;
    }

    qDebug() << "Tracking stopped.";
}
