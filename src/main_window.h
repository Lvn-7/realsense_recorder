#pragma once

#include "camera_discovery.h"

#include <QMainWindow>
#include <QVector>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshCameras();
    void cameraChanged(int index);
    void updateFrame();
    void chooseSaveDirectory();
    void startRecording();
    void stopRecording();
    void takePhoto();

private:
    void buildUi();
    void openSelectedCamera();
    void closeCamera();
    void setStatus(const QString &message, bool error = false);
    QString timestampedPath(const QString &prefix, const QString &extension) const;

    QVector<CameraDevice> cameras_;
    QComboBox *cameraCombo_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QLabel *deviceInfo_ = nullptr;
    QLabel *preview_ = nullptr;
    QLineEdit *saveDirectory_ = nullptr;
    QPushButton *browseButton_ = nullptr;
    QPushButton *recordButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QPushButton *photoButton_ = nullptr;
    QLabel *status_ = nullptr;
    QTimer *frameTimer_ = nullptr;

    cv::VideoCapture capture_;
    cv::VideoWriter writer_;
    cv::Mat latestFrame_;
    QString recordingPath_;
};

