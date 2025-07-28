TEMPLATE = app
TARGET = gigaprojekt

QT += core gui network positioning bluetooth widgets

RESOURCES += resources.qrc

CONFIG += c++17 android

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    bluetoothmanager.cpp \
    HorizonsManager.cpp

HEADERS += \
    mainwindow.h \
    bluetoothmanager.h \
    HorizonsManager.h

FORMS += mainwindow.ui


android {
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    QT += network
        ANDROID_EXTRA_LIBS += \
            $$PWD/android/libs/arm64-v8a/libcrypto.so \
            $$PWD/android/libs/arm64-v8a/libssl.so \
}
