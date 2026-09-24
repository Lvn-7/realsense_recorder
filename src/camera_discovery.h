#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct CameraDevice {
    QString model;
    QString manufacturer;
    QString serial;
    QString vendorId;
    QString productId;
    QString usbSpeed;
    QString colorNode;
    QStringList videoNodes;
    QMap<QString, QStringList> formats;

    QString displayName() const;
    QString details() const;
};

QVector<CameraDevice> discoverCameras();

