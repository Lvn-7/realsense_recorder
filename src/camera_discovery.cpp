#include "camera_discovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString findUsbRoot(QString path)
{
    QFileInfo info(path);
    if (info.isSymLink()) {
        path = info.canonicalFilePath();
    }

    QDir current(QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath());
    while (current.exists()) {
        if (QFileInfo::exists(current.filePath("idVendor")) &&
            QFileInfo::exists(current.filePath("idProduct"))) {
            return current.absolutePath();
        }
        if (!current.cdUp()) {
            break;
        }
    }
    return {};
}

QString fourccToString(quint32 value)
{
    char text[5] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
        '\0'
    };
    for (int i = 0; i < 4; ++i) {
        if (text[i] < 32 || text[i] > 126) {
            text[i] = '?';
        }
    }
    return QString::fromLatin1(text);
}

struct NodeInfo {
    QString card;
    QStringList formats;
    QVector<QSize> resolutions;
    int colorScore = -1;
};

void addResolution(QVector<QSize> &resolutions, int width, int height)
{
    const QSize size(width, height);
    if (width > 0 && height > 0 && !resolutions.contains(size)) {
        resolutions.append(size);
    }
}

void enumerateType(int fd, v4l2_buf_type type, NodeInfo &result)
{
    v4l2_fmtdesc format{};
    format.type = type;
    for (format.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &format) == 0; ++format.index) {
        const QString fourcc = fourccToString(format.pixelformat);
        if (!result.formats.contains(fourcc)) {
            result.formats.append(fourcc);
        }

        v4l2_frmsizeenum frameSize{};
        frameSize.pixel_format = format.pixelformat;
        for (frameSize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frameSize) == 0; ++frameSize.index) {
            if (frameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                addResolution(result.resolutions, frameSize.discrete.width, frameSize.discrete.height);
            } else if (frameSize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                       frameSize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                addResolution(result.resolutions, frameSize.stepwise.min_width, frameSize.stepwise.min_height);
                addResolution(result.resolutions, frameSize.stepwise.max_width, frameSize.stepwise.max_height);
            }
        }

        int score = 1;
        if (fourcc == "MJPG") {
            score = 240;
        } else if (fourcc == "YUYV") {
            score = 230;
        } else if (fourcc == "RGB3" || fourcc == "BGR3") {
            score = 210;
        } else if (fourcc == "UYVY") {
            score = 120;
        } else if (fourcc == "GREY") {
            score = 20;
        } else if (fourcc == "Z16 " || fourcc == "INZI") {
            score = 0;
        }
        result.colorScore = std::max(result.colorScore, score);
    }
}

NodeInfo inspectVideoNode(const QString &node)
{
    NodeInfo result;
    const QByteArray encoded = QFile::encodeName(node);
    const int fd = open(encoded.constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return result;
    }

    v4l2_capability capability{};
    if (ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0) {
        result.card = QString::fromUtf8(reinterpret_cast<const char *>(capability.card));
        const quint32 caps = capability.capabilities & V4L2_CAP_DEVICE_CAPS
            ? capability.device_caps
            : capability.capabilities;
        if (caps & V4L2_CAP_VIDEO_CAPTURE) {
            enumerateType(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, result);
        }
        if (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
            enumerateType(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, result);
        }
    }
    close(fd);
    return result;
}

int videoNodeNumber(const QString &node)
{
    const QRegularExpressionMatch match = QRegularExpression("(\\d+)$").match(node);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

} // namespace

QString CameraDevice::displayName() const
{
    QString name = model.isEmpty() ? QStringLiteral("Video camera") : model;
    if (!serial.isEmpty()) {
        name += QStringLiteral("  ·  ") + serial;
    }
    return name;
}

QString CameraDevice::simpleDetails() const
{
    return QStringLiteral("型号：%1    序列号：%2")
        .arg(model.isEmpty() ? QStringLiteral("未知") : model,
             serial.isEmpty() ? QStringLiteral("未知") : serial);
}

QVector<QSize> CameraDevice::colorResolutions() const
{
    QVector<QSize> result = resolutions.value(colorNode);
    std::sort(result.begin(), result.end(), [](const QSize &a, const QSize &b) {
        if (a.width() * a.height() != b.width() * b.height()) {
            return a.width() * a.height() > b.width() * b.height();
        }
        return a.width() > b.width();
    });
    return result;
}

QVector<CameraDevice> discoverCameras()
{
    const QDir videoClass(QStringLiteral("/sys/class/video4linux"));
    QStringList entries = videoClass.entryList({QStringLiteral("video*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(entries.begin(), entries.end(), [](const QString &a, const QString &b) {
        return videoNodeNumber(a) < videoNodeNumber(b);
    });

    QMap<QString, CameraDevice> grouped;
    QMap<QString, int> bestScores;

    for (const QString &entry : entries) {
        const QString node = QStringLiteral("/dev/") + entry;
        const QString sysPath = videoClass.absoluteFilePath(entry);
        const QString usbRoot = findUsbRoot(sysPath);
        const QString groupKey = usbRoot.isEmpty() ? QFileInfo(sysPath).canonicalPath() : usbRoot;
        CameraDevice &camera = grouped[groupKey];

        if (camera.model.isEmpty()) {
            camera.model = readTextFile(usbRoot + QStringLiteral("/product"));
            camera.manufacturer = readTextFile(usbRoot + QStringLiteral("/manufacturer"));
            camera.serial = readTextFile(usbRoot + QStringLiteral("/serial"));
            camera.vendorId = readTextFile(usbRoot + QStringLiteral("/idVendor"));
            camera.productId = readTextFile(usbRoot + QStringLiteral("/idProduct"));
            camera.usbSpeed = readTextFile(usbRoot + QStringLiteral("/speed"));
        }

        camera.videoNodes.append(node);
        const NodeInfo nodeInfo = inspectVideoNode(node);
        camera.formats.insert(node, nodeInfo.formats);
        camera.resolutions.insert(node, nodeInfo.resolutions);
        if (camera.model.isEmpty()) {
            camera.model = nodeInfo.card;
        }
        if (nodeInfo.colorScore > bestScores.value(groupKey, -1)) {
            bestScores[groupKey] = nodeInfo.colorScore;
            camera.colorNode = node;
        }
    }

    QVector<CameraDevice> cameras;
    cameras.reserve(grouped.size());
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        CameraDevice camera = it.value();
        if (bestScores.value(it.key(), -1) <= 0) {
            camera.colorNode.clear();
        }
        cameras.append(camera);
    }
    return cameras;
}
