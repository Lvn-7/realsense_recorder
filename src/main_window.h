#pragma once

#include "camera_discovery.h"

#include <QImage>
#include <QMainWindow>
#include <QVector>
#include <QWidget>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPaintEvent;
class QPushButton;
class QSpinBox;
class QTimer;

class VideoPreview final : public QWidget {
public:
    explicit VideoPreview(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void clearFrame();
    void setExpectedAspect(const QSize &size);
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage frame_;
    double aspectRatio_ = 16.0 / 9.0;
};

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
    void resolutionChanged(int index);
    void updateFrame();
    void chooseSaveDirectory();
    void openSaveDirectory();
    void startRecording();
    void startTimedRecording();
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
    QComboBox *resolutionCombo_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QLabel *deviceInfo_ = nullptr;
    VideoPreview *preview_ = nullptr;
    QLineEdit *saveDirectory_ = nullptr;
    QPushButton *browseButton_ = nullptr;
    QPushButton *openFolderButton_ = nullptr;
    QPushButton *recordButton_ = nullptr;
    QPushButton *timedRecordButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QPushButton *photoButton_ = nullptr;
    QSpinBox *durationSpinBox_ = nullptr;
    QLabel *status_ = nullptr;
    QTimer *frameTimer_ = nullptr;
    QTimer *timedStopTimer_ = nullptr;

    cv::VideoCapture capture_;
    cv::VideoWriter writer_;
    cv::Mat latestFrame_;
    QString recordingPath_;
};
