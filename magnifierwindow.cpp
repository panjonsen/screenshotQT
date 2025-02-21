#include "magnifierwindow.h"
#include <QPainter>
#include <QImage>


MagnifierWindow::MagnifierWindow(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), originalScreenshot(screenshot)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint); // 无边框工具窗口
    setFixedSize(magnifierSize, magnifierSize + 60);    // 包括文本区域
    setAttribute(Qt::WA_TranslucentBackground);         // 透明背景

}

void MagnifierWindow::updatePosition(const QPoint &pos)
{
    currentMousePos = pos;
    update(); // 触发重绘
}

void MagnifierWindow::setScreenshot(const QPixmap &screenshot)
{
    originalScreenshot = screenshot;
    update();
}

void MagnifierWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制放大镜矩形
    QImage originalImage = originalScreenshot.toImage();
    int zoomSrcSize = magnifierSize / zoomFactor;
    QRect srcRect(currentMousePos.x() - zoomSrcSize / 2, currentMousePos.y() - zoomSrcSize / 2, zoomSrcSize, zoomSrcSize);
    srcRect = srcRect.intersected(originalScreenshot.rect());
    QImage zoomedImage = originalImage.copy(srcRect).scaled(magnifierSize, magnifierSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QRect magnifierRect(0, 0, magnifierSize, magnifierSize);
    painter.drawImage(magnifierRect, zoomedImage);
    painter.setPen(Qt::black);
    painter.drawRect(magnifierRect);

    // 获取当前像素颜色
    QColor pixelColor = originalImage.pixelColor(currentMousePos);
    QString rgbText = QString("RGB: %1, %2, %3").arg(pixelColor.red()).arg(pixelColor.green()).arg(pixelColor.blue());
    QString hexText = QString("Hex: #%1").arg(pixelColor.name().mid(1).toUpper());

    // 绘制坐标和颜色信息
    painter.setFont(QFont("Arial", 10));
    painter.drawText(0, magnifierSize + 15, QString("X: %1, Y: %2").arg(currentMousePos.x()).arg(currentMousePos.y()));
    painter.drawText(0, magnifierSize + 30, rgbText);
    painter.drawText(0, magnifierSize + 45, hexText);

    // 绘制颜色块
    QRect colorBlock(70, magnifierSize + 40, 20, 20);
    painter.fillRect(colorBlock, pixelColor);
    painter.drawRect(colorBlock);
}
