#include "mainwindow.h"
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslConfiguration>

#include <QApplication>

int main(int argc, char *argv[])
{
    qDebug() << "SSL support:" << QSslSocket::supportsSsl();
    // → true
    qDebug() << "SSL build version:" << QSslSocket::sslLibraryBuildVersionString();
    // → np. “OpenSSL 1.1.1k …”

    QApplication a(argc, argv);

    auto caCerts = QSslCertificate::fromPath(":/certs/cacert.pem", QSsl::Pem);
    qDebug() << "Loaded CA certificates:" << caCerts.count();
    if (caCerts.isEmpty()) {
        qWarning() << "Failed to load CACERT.PEM!";
    } else {
        // Add certificates to the default SSL configuration
        QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
        cfg.setCaCertificates(caCerts);
        QSslConfiguration::setDefaultConfiguration(cfg);
        qDebug() << "CACERT.PEM successfully loaded from resources.";
    }
    MainWindow w;
    w.show();
    return a.exec();
}
