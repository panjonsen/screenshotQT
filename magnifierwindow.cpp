#include "magnifierwindow.h"
#include <QPainter>
#include <QImage>


MagnifierWindow::MagnifierWindow(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), originalScreenshot(screenshot)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint); // 无边框工具窗口
    setFixedSize(magnifierSize+40, magnifierSize + 80);    // 包括文本区域
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

    // 绘制放大镜图像
    QImage originalImage = originalScreenshot.toImage();
    int zoomSrcSize = magnifierSize / zoomFactor;
    QRect srcRect(currentMousePos.x() - zoomSrcSize / 2, currentMousePos.y() - zoomSrcSize / 2, zoomSrcSize, zoomSrcSize);
    srcRect = srcRect.intersected(originalScreenshot.rect());
    QImage zoomedImage = originalImage.copy(srcRect).scaled(magnifierSize, magnifierSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QRect magnifierRect(1, 1, magnifierSize, magnifierSize);
    painter.drawImage(magnifierRect, zoomedImage);

    // 绘制矩形边框
    painter.setPen(Qt::black);
    painter.drawRect(magnifierRect);

    // 绘制十字线
    painter.setPen(Qt::black);
    painter.drawLine(magnifierRect.left(), magnifierRect.center().y(),
                     magnifierRect.right(), magnifierRect.center().y());
    painter.drawLine(magnifierRect.center().x(), magnifierRect.top(),
                     magnifierRect.center().x(), magnifierRect.bottom());

    // 获取当前像素颜色
    QColor pixelColor = originalImage.pixelColor(currentMousePos);
    QString rgbText = QString("RGB: %1, %2, %3").arg(pixelColor.red()).arg(pixelColor.green()).arg(pixelColor.blue());
    QString hexText = QString("Hex: #%1").arg(pixelColor.name().mid(1).toUpper());

    // 绘制背景框
    int textStartY = magnifierSize + 15;
    int textEndY = textStartY + 60; // 假设三行文本和颜色块的高度
    QRect backgroundRect(1, textStartY - 12, width() - 20, textEndY - textStartY + 10-25);
    painter.fillRect(backgroundRect, QColor(64, 64, 64)); // 深灰色背景

    // 绘制文本（白色字体）
    painter.setFont(QFont("Arial", 10));
    painter.setPen(Qt::white);
    painter.drawText(5, textStartY, QString("X: %1, Y: %2").arg(currentMousePos.x()).arg(currentMousePos.y()));
    painter.drawText(5, textStartY + 15, rgbText);
    painter.drawText(5, textStartY + 30, hexText);

    // 绘制颜色块
    QRect colorBlock(100, textStartY+20, 12, 12);
    painter.fillRect(colorBlock, pixelColor);
    painter.setPen(Qt::white);
    painter.drawRect(colorBlock);
}
