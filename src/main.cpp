#include "camera_discovery.h"
#include "main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    bool listOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--list")) {
            listOnly = true;
        }
    }

    if (listOnly) {
        QCoreApplication app(argc, argv);
        QTextStream output(stdout);
        output.setCodec("UTF-8");
        const QVector<CameraDevice> cameras = discoverCameras();
        if (cameras.isEmpty()) {
            output << "没有检测到视频相机\n";
            return 1;
        }
        for (int i = 0; i < cameras.size(); ++i) {
            output << "Camera " << i + 1 << "\n" << cameras.at(i).details() << "\n\n";
        }
        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("RealSense Recorder"));
    app.setOrganizationName(QStringLiteral("Local"));
    MainWindow window;
    window.show();
    return app.exec();
}
