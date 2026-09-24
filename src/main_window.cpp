#include "main_window.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

VideoPreview::VideoPreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAutoFillBackground(false);
}

void VideoPreview::setFrame(const QImage &frame)
{
    frame_ = frame;
    if (!frame_.isNull()) {
        aspectRatio_ = static_cast<double>(frame_.width()) / frame_.height();
    }
    updateGeometry();
    update();
}

void VideoPreview::clearFrame()
{
    frame_ = QImage();
    update();
}

void VideoPreview::setExpectedAspect(const QSize &size)
{
    if (size.width() > 0 && size.height() > 0) {
        aspectRatio_ = static_cast<double>(size.width()) / size.height();
        updateGeometry();
        update();
    }
}

QSize VideoPreview::sizeHint() const
{
    return QSize(960, qRound(960.0 / aspectRatio_));
}

bool VideoPreview::hasHeightForWidth() const
{
    return true;
}

int VideoPreview::heightForWidth(int width) const
{
    return qRound(width / aspectRatio_);
}

void VideoPreview::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (frame_.isNull()) {
        painter.fillRect(rect(), QColor(QStringLiteral("#111111")));
        painter.setPen(QColor(QStringLiteral("#bbbbbb")));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待相机画面"));
        return;
    }

    const double sourceRatio = static_cast<double>(frame_.width()) / frame_.height();
    const double targetRatio = static_cast<double>(width()) / qMax(1, height());
    QRectF source(0, 0, frame_.width(), frame_.height());
    if (sourceRatio > targetRatio) {
        const double cropWidth = frame_.height() * targetRatio;
        source.setX((frame_.width() - cropWidth) / 2.0);
        source.setWidth(cropWidth);
    } else if (sourceRatio < targetRatio) {
        const double cropHeight = frame_.width() / targetRatio;
        source.setY((frame_.height() - cropHeight) / 2.0);
        source.setHeight(cropHeight);
    }
    painter.drawImage(rect(), frame_, source);
}

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
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f5f7fb; }"
        "QLabel { color: #243247; }"
        "QComboBox, QLineEdit, QSpinBox { background: white; border: 1px solid #d6dce6; border-radius: 7px; padding: 7px 9px; min-height: 18px; }"
        "QPushButton { background: white; border: 1px solid #cbd5e1; border-radius: 7px; padding: 8px 13px; color: #243247; }"
        "QPushButton:hover { background: #eef4ff; border-color: #8db4f5; }"
        "QPushButton:disabled { color: #9aa4b2; background: #edf0f4; }"));

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *cameraRow = new QHBoxLayout;
    auto *cameraLabel = new QLabel(QStringLiteral("当前相机："), central);
    cameraCombo_ = new QComboBox(central);
    cameraCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *resolutionLabel = new QLabel(QStringLiteral("分辨率："), central);
    resolutionCombo_ = new QComboBox(central);
    resolutionCombo_->setMinimumWidth(130);
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), central);
    cameraRow->addWidget(cameraLabel);
    cameraRow->addWidget(cameraCombo_, 1);
    cameraRow->addWidget(resolutionLabel);
    cameraRow->addWidget(resolutionCombo_);
    cameraRow->addWidget(refreshButton_);
    root->addLayout(cameraRow);

    deviceInfo_ = new QLabel(central);
    deviceInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deviceInfo_->setStyleSheet(QStringLiteral("QLabel { background: #f2f4f7; border-radius: 6px; padding: 8px; }"));
    root->addWidget(deviceInfo_);

    preview_ = new VideoPreview(central);
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
    openFolderButton_ = new QPushButton(QStringLiteral("打开此文件夹"), central);
    pathRow->addWidget(saveDirectory_, 1);
    pathRow->addWidget(browseButton_);
    pathRow->addWidget(openFolderButton_);
    root->addLayout(pathRow);

    auto *buttonRow = new QHBoxLayout;
    recordButton_ = new QPushButton(QStringLiteral("● 开始录制"), central);
    timedRecordButton_ = new QPushButton(QStringLiteral("定时录制"), central);
    stopButton_ = new QPushButton(QStringLiteral("■ 结束录制"), central);
    photoButton_ = new QPushButton(QStringLiteral("拍照"), central);
    durationSpinBox_ = new QSpinBox(central);
    durationSpinBox_->setRange(1, 3600);
    durationSpinBox_->setValue(10);
    durationSpinBox_->setSuffix(QStringLiteral(" 秒"));
    durationSpinBox_->setToolTip(QStringLiteral("定时录制时长"));
    recordButton_->setMinimumHeight(42);
    stopButton_->setMinimumHeight(42);
    photoButton_->setMinimumHeight(42);
    recordButton_->setStyleSheet(QStringLiteral("QPushButton { background: #2e9d57; color: white; border: 1px solid #258348; font-weight: 700; } QPushButton:hover { background: #258348; }"));
    timedRecordButton_->setStyleSheet(QStringLiteral("QPushButton { color: #1769aa; font-weight: 700; }"));
    stopButton_->setStyleSheet(QStringLiteral("QPushButton:disabled { background: #d9dee6; color: #8993a2; border-color: #cbd2dc; } QPushButton:enabled { background: #d9363e; color: white; border-color: #bd2730; font-weight: 700; } QPushButton:enabled:hover { background: #bd2730; }"));
    stopButton_->setEnabled(false);
    auto *timedRow = new QHBoxLayout;
    timedRow->setSpacing(6);
    timedRow->addWidget(durationSpinBox_);
    timedRow->addWidget(timedRecordButton_);

    auto *mainRecordRow = new QHBoxLayout;
    mainRecordRow->setSpacing(6);
    mainRecordRow->addWidget(recordButton_);
    mainRecordRow->addWidget(stopButton_);

    auto *photoRow = new QHBoxLayout;
    photoRow->addWidget(photoButton_);

    buttonRow->addLayout(timedRow, 1);
    buttonRow->addStretch(1);
    buttonRow->addLayout(mainRecordRow, 0);
    buttonRow->addStretch(1);
    buttonRow->addLayout(photoRow, 1);
    root->addLayout(buttonRow);

    status_ = new QLabel(QStringLiteral("就绪"), central);
    status_->setAlignment(Qt::AlignCenter);
    root->addWidget(status_);

    setCentralWidget(central);

    frameTimer_ = new QTimer(this);
    frameTimer_->setTimerType(Qt::PreciseTimer);
    frameTimer_->setInterval(33);
    timedStopTimer_ = new QTimer(this);
    timedStopTimer_->setSingleShot(true);

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshCameras);
    connect(cameraCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::cameraChanged);
    connect(resolutionCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::resolutionChanged);
    connect(frameTimer_, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(browseButton_, &QPushButton::clicked, this, &MainWindow::chooseSaveDirectory);
    connect(openFolderButton_, &QPushButton::clicked, this, &MainWindow::openSaveDirectory);
    connect(recordButton_, &QPushButton::clicked, this, &MainWindow::startRecording);
    connect(timedRecordButton_, &QPushButton::clicked, this, &MainWindow::startTimedRecording);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopRecording);
    connect(timedStopTimer_, &QTimer::timeout, this, &MainWindow::stopRecording);
    connect(photoButton_, &QPushButton::clicked, this, &MainWindow::takePhoto);
}

void MainWindow::refreshCameras()
{
    stopRecording();
    closeCamera();

    cameraCombo_->blockSignals(true);
    cameraCombo_->clear();
    resolutionCombo_->blockSignals(true);
    resolutionCombo_->clear();
    resolutionCombo_->blockSignals(false);
    cameras_ = discoverCameras();
    for (const CameraDevice &camera : cameras_) {
        cameraCombo_->addItem(camera.displayName());
    }
    cameraCombo_->blockSignals(false);

    if (cameras_.isEmpty()) {
        deviceInfo_->setText(QStringLiteral("没有检测到视频相机"));
        preview_->clearFrame();
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
    const CameraDevice &camera = cameras_.at(index);
    deviceInfo_->setText(camera.simpleDetails());

    resolutionCombo_->blockSignals(true);
    resolutionCombo_->clear();
    int preferredIndex = 0;
    const QVector<QSize> resolutions = camera.colorResolutions();
    for (int i = 0; i < resolutions.size(); ++i) {
        const QSize size = resolutions.at(i);
        resolutionCombo_->addItem(
            QStringLiteral("%1 × %2").arg(size.width()).arg(size.height()), size);
        if (size == QSize(1280, 720)) {
            preferredIndex = i;
        }
    }
    resolutionCombo_->setCurrentIndex(preferredIndex);
    resolutionCombo_->setEnabled(!resolutions.isEmpty());
    resolutionCombo_->blockSignals(false);
    openSelectedCamera();
}

void MainWindow::resolutionChanged(int index)
{
    if (index < 0 || writer_.isOpened()) {
        return;
    }
    closeCamera();
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

    const QSize requestedSize = resolutionCombo_->currentData().toSize();
    preview_->setExpectedAspect(requestedSize);

    capture_.open(node.toStdString(), cv::CAP_V4L2);
    if (!capture_.isOpened()) {
        setStatus(QStringLiteral("无法打开彩色节点 %1，请检查权限或是否被其他程序占用").arg(node), true);
        return;
    }

    const QStringList formats = cameras_.at(index).formats.value(node);
    if (formats.contains(QStringLiteral("MJPG"))) {
        capture_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    } else if (formats.contains(QStringLiteral("YUYV"))) {
        capture_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
    }
    if (requestedSize.isValid()) {
        capture_.set(cv::CAP_PROP_FRAME_WIDTH, requestedSize.width());
        capture_.set(cv::CAP_PROP_FRAME_HEIGHT, requestedSize.height());
    }
    capture_.set(cv::CAP_PROP_FPS, 30);
    capture_.set(cv::CAP_PROP_BUFFERSIZE, 2);
    frameTimer_->start();
    recordButton_->setEnabled(true);
    timedRecordButton_->setEnabled(true);
    durationSpinBox_->setEnabled(true);
    photoButton_->setEnabled(true);
    setStatus(QStringLiteral("实时预览：%1 × %2")
                  .arg(requestedSize.width())
                  .arg(requestedSize.height()));
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
        preview_->clearFrame();
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
    preview_->setFrame(image.copy());
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

void MainWindow::openSaveDirectory()
{
    QDir().mkpath(saveDirectory_->text());
    QDesktopServices::openUrl(QUrl::fromLocalFile(saveDirectory_->text()));
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
    timedRecordButton_->setEnabled(false);
    durationSpinBox_->setEnabled(false);
    stopButton_->setEnabled(true);
    browseButton_->setEnabled(false);
    cameraCombo_->setEnabled(false);
    resolutionCombo_->setEnabled(false);
    refreshButton_->setEnabled(false);
    setStatus(QStringLiteral("正在录制 MP4：%1").arg(recordingPath_));
}

void MainWindow::startTimedRecording()
{
    startRecording();
    if (writer_.isOpened()) {
        timedStopTimer_->start(durationSpinBox_->value() * 1000);
        setStatus(QStringLiteral("定时录制中，将在 %1 秒后自动保存").arg(durationSpinBox_->value()));
    }
}

void MainWindow::stopRecording()
{
    if (!writer_.isOpened()) {
        return;
    }
    writer_.release();
    timedStopTimer_->stop();
    const QString completedPath = recordingPath_;
    recordingPath_.clear();
    recordButton_->setEnabled(capture_.isOpened());
    timedRecordButton_->setEnabled(capture_.isOpened());
    durationSpinBox_->setEnabled(capture_.isOpened());
    stopButton_->setEnabled(false);
    browseButton_->setEnabled(true);
    cameraCombo_->setEnabled(true);
    resolutionCombo_->setEnabled(true);
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
