#include "main_window.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    refreshCameras();
}

MainWindow::~MainWindow()
{
    closeCamera();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("RealSense 录像机"));
    resize(1100, 820);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *cameraRow = new QHBoxLayout;
    auto *cameraLabel = new QLabel(QStringLiteral("当前相机："), central);
    cameraCombo_ = new QComboBox(central);
    cameraCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), central);
    cameraRow->addWidget(cameraLabel);
    cameraRow->addWidget(cameraCombo_, 1);
    cameraRow->addWidget(refreshButton_);
    root->addLayout(cameraRow);

    deviceInfo_ = new QLabel(central);
    deviceInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deviceInfo_->setStyleSheet(QStringLiteral("QLabel { background: #f2f4f7; border-radius: 6px; padding: 8px; }"));
    root->addWidget(deviceInfo_);

    preview_ = new QLabel(QStringLiteral("等待相机画面"), central);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(640, 420);
    preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    preview_->setStyleSheet(QStringLiteral("QLabel { background: #111; color: #bbb; border-radius: 6px; }"));
    QFont previewFont = preview_->font();
    previewFont.setPointSize(14);
    preview_->setFont(previewFont);
    root->addWidget(preview_, 1);

    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(new QLabel(QStringLiteral("保存位置："), central));
    saveDirectory_ = new QLineEdit(central);
    saveDirectory_->setReadOnly(true);
    QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (movies.isEmpty()) {
        movies = QDir::homePath() + QStringLiteral("/Videos");
    }
    saveDirectory_->setText(movies + QStringLiteral("/RealSense"));
    QDir().mkpath(saveDirectory_->text());
    browseButton_ = new QPushButton(QStringLiteral("选择位置…"), central);
    pathRow->addWidget(saveDirectory_, 1);
    pathRow->addWidget(browseButton_);
    root->addLayout(pathRow);

    auto *buttonRow = new QHBoxLayout;
    recordButton_ = new QPushButton(QStringLiteral("● 开始录制"), central);
    stopButton_ = new QPushButton(QStringLiteral("■ 结束录制"), central);
    photoButton_ = new QPushButton(QStringLiteral("拍照"), central);
    recordButton_->setMinimumHeight(42);
    stopButton_->setMinimumHeight(42);
    photoButton_->setMinimumHeight(42);
    recordButton_->setStyleSheet(QStringLiteral("QPushButton { color: #b00020; font-weight: 600; }"));
    stopButton_->setEnabled(false);
    buttonRow->addStretch();
    buttonRow->addWidget(recordButton_);
    buttonRow->addWidget(stopButton_);
    buttonRow->addWidget(photoButton_);
    buttonRow->addStretch();
    root->addLayout(buttonRow);

    status_ = new QLabel(QStringLiteral("就绪"), central);
    status_->setAlignment(Qt::AlignCenter);
    root->addWidget(status_);

    setCentralWidget(central);

    frameTimer_ = new QTimer(this);
    frameTimer_->setTimerType(Qt::PreciseTimer);
    frameTimer_->setInterval(33);

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshCameras);
    connect(cameraCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::cameraChanged);
    connect(frameTimer_, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(browseButton_, &QPushButton::clicked, this, &MainWindow::chooseSaveDirectory);
    connect(recordButton_, &QPushButton::clicked, this, &MainWindow::startRecording);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopRecording);
    connect(photoButton_, &QPushButton::clicked, this, &MainWindow::takePhoto);
}

void MainWindow::refreshCameras()
{
    stopRecording();
    closeCamera();

    cameraCombo_->blockSignals(true);
    cameraCombo_->clear();
    cameras_ = discoverCameras();
    for (const CameraDevice &camera : cameras_) {
        cameraCombo_->addItem(camera.displayName());
    }
    cameraCombo_->blockSignals(false);

    if (cameras_.isEmpty()) {
        deviceInfo_->setText(QStringLiteral("没有检测到视频相机"));
        preview_->setText(QStringLiteral("请连接相机后点击“刷新”"));
        recordButton_->setEnabled(false);
        photoButton_->setEnabled(false);
        setStatus(QStringLiteral("没有检测到相机"), true);
        return;
    }
    cameraCombo_->setCurrentIndex(0);
    cameraChanged(0);
}

void MainWindow::cameraChanged(int index)
{
    stopRecording();
    closeCamera();
    if (index < 0 || index >= cameras_.size()) {
        return;
    }
    deviceInfo_->setText(cameras_.at(index).details());
    openSelectedCamera();
}

void MainWindow::openSelectedCamera()
{
    const int index = cameraCombo_->currentIndex();
    if (index < 0 || index >= cameras_.size()) {
        return;
    }
    const QString node = cameras_.at(index).colorNode;
    if (node.isEmpty()) {
        setStatus(QStringLiteral("该设备没有可用的彩色视频节点"), true);
        return;
    }

    capture_.open(node.toStdString(), cv::CAP_V4L2);
    if (!capture_.isOpened()) {
        preview_->setText(QStringLiteral("无法打开 %1").arg(node));
        setStatus(QStringLiteral("无法打开彩色节点 %1，请检查权限或是否被其他程序占用").arg(node), true);
        return;
    }

    capture_.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    capture_.set(cv::CAP_PROP_FPS, 30);
    capture_.set(cv::CAP_PROP_BUFFERSIZE, 2);
    frameTimer_->start();
    recordButton_->setEnabled(true);
    photoButton_->setEnabled(true);
    setStatus(QStringLiteral("实时预览：%1").arg(node));
}

void MainWindow::closeCamera()
{
    if (frameTimer_) {
        frameTimer_->stop();
    }
    if (capture_.isOpened()) {
        capture_.release();
    }
    latestFrame_.release();
    if (preview_) {
        preview_->clear();
        preview_->setText(QStringLiteral("等待相机画面"));
    }
    if (recordButton_) {
        recordButton_->setEnabled(false);
    }
    if (photoButton_) {
        photoButton_->setEnabled(false);
    }
}

void MainWindow::updateFrame()
{
    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) {
        setStatus(QStringLiteral("读取画面失败，正在等待相机…"), true);
        return;
    }
    latestFrame_ = frame.clone();

    if (writer_.isOpened()) {
        writer_.write(latestFrame_);
    }

    cv::Mat rgb;
    cv::cvtColor(latestFrame_, rgb, cv::COLOR_BGR2RGB);
    const QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    preview_->setPixmap(QPixmap::fromImage(image.copy()).scaled(
        preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::chooseSaveDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择保存位置"), saveDirectory_->text());
    if (!selected.isEmpty()) {
        saveDirectory_->setText(selected);
        setStatus(QStringLiteral("保存位置：%1").arg(selected));
    }
}

QString MainWindow::timestampedPath(const QString &prefix, const QString &extension) const
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return QDir(saveDirectory_->text()).filePath(prefix + timestamp + extension);
}

void MainWindow::startRecording()
{
    if (!capture_.isOpened() || latestFrame_.empty() || writer_.isOpened()) {
        return;
    }
    if (!QDir().mkpath(saveDirectory_->text())) {
        setStatus(QStringLiteral("无法创建保存目录"), true);
        return;
    }

    recordingPath_ = timestampedPath(QStringLiteral("recording_"), QStringLiteral(".mp4"));
    double fps = capture_.get(cv::CAP_PROP_FPS);
    if (fps < 1.0 || fps > 120.0) {
        fps = 30.0;
    }
    const cv::Size size(latestFrame_.cols, latestFrame_.rows);
    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (!writer_.open(recordingPath_.toStdString(), codec, fps, size, true)) {
        setStatus(QStringLiteral("无法创建 MP4 文件；请检查 OpenCV/FFmpeg 编码支持"), true);
        recordingPath_.clear();
        return;
    }

    recordButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    browseButton_->setEnabled(false);
    cameraCombo_->setEnabled(false);
    refreshButton_->setEnabled(false);
    setStatus(QStringLiteral("正在录制 MP4：%1").arg(recordingPath_));
}

void MainWindow::stopRecording()
{
    if (!writer_.isOpened()) {
        return;
    }
    writer_.release();
    const QString completedPath = recordingPath_;
    recordingPath_.clear();
    recordButton_->setEnabled(capture_.isOpened());
    stopButton_->setEnabled(false);
    browseButton_->setEnabled(true);
    cameraCombo_->setEnabled(true);
    refreshButton_->setEnabled(true);
    setStatus(QStringLiteral("录像已保存：%1").arg(completedPath));
}

void MainWindow::takePhoto()
{
    if (latestFrame_.empty()) {
        return;
    }
    if (!QDir().mkpath(saveDirectory_->text())) {
        setStatus(QStringLiteral("无法创建保存目录"), true);
        return;
    }
    const QString path = timestampedPath(QStringLiteral("photo_"), QStringLiteral(".png"));
    if (!cv::imwrite(path.toStdString(), latestFrame_)) {
        setStatus(QStringLiteral("照片保存失败"), true);
        return;
    }
    setStatus(QStringLiteral("照片已保存：%1").arg(path));
}

void MainWindow::setStatus(const QString &message, bool error)
{
    status_->setText(message);
    status_->setStyleSheet(error
        ? QStringLiteral("QLabel { color: #b00020; }")
        : QStringLiteral("QLabel { color: #1b5e20; }"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopRecording();
    closeCamera();
    event->accept();
}

