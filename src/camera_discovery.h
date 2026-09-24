#pragma once

#include <QMap>
#include <QSize>
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
    QMap<QString, QVector<QSize>> resolutions;

    QString displayName() const;
    QString simpleDetails() const;
    QVector<QSize> colorResolutions() const;
};

QVector<CameraDevice> discoverCameras();
