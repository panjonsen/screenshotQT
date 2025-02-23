#include "sizedisplaywindow.h"
#include <QPainter>
#include <QVBoxLayout>

SizeDisplayWindow::SizeDisplayWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    sizeLabel = new QLabel(this);
    sizeLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(sizeLabel);
    layout->setContentsMargins(5, 5, 5, 5);
    setLayout(layout);
}

void SizeDisplayWindow::setSizeText(const QString &text)
{
    sizeLabel->setText(text);
}

void SizeDisplayWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 5, 5);
}
