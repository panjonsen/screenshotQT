#include "editwindow.h"
#include <QPainter>
#include <QDebug>

EditWindow::EditWindow(const QPixmap &screenshot, const QPoint &pos, QWidget *parent)
    : QWidget(parent), screenshot(screenshot)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFixedSize(screenshot.size());
    move(pos);
    setMouseTracking(true);
    show();
}

EditWindow::~EditWindow()
{
}

void EditWindow::updateScreenshot(const QPixmap &newScreenshot, const QPoint &newPos)
{
    screenshot = newScreenshot;
    setFixedSize(newScreenshot.size());
    move(newPos);
    update(); // 触发重绘
}

void EditWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, screenshot);

    QPen pen(borderColor, borderWidth, borderStyle);
    painter.setPen(pen);
    painter.drawRect(rect().adjusted(1, 1, -1, -1));

    painter.setBrush(Qt::white);
    painter.setPen(Qt::black);
    for (int i = 0; i < 8; ++i) {
        Handle handle = static_cast<Handle>(i + 1);
        QRect handleRect = getHandleRect(handle);
        painter.drawRect(handleRect);
    }
}

void EditWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        for (int i = 0; i < 8; ++i) {
            Handle handle = static_cast<Handle>(i + 1);
            if (getHandleRect(handle).contains(pos)) {
                activeHandle = handle;
                dragStartPos = event->globalPos();
                qDebug() << "Handle pressed:" << activeHandle << "at:" << pos;
                emit handleDragged(activeHandle, event->globalPos());
                return;
            }
        }
        activeHandle = None;
    }
}

void EditWindow::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    qDebug() << "EditWindow mouse moved to:" << pos;
    if (activeHandle != None && (event->buttons() & Qt::LeftButton)) {
        emit handleDragged(activeHandle, event->globalPos());
    } else {
        QCursor cursor;
        if (getHandleRect(TopLeft).contains(pos) || getHandleRect(BottomRight).contains(pos)) {
            cursor = Qt::SizeFDiagCursor;
        } else if (getHandleRect(TopRight).contains(pos) || getHandleRect(BottomLeft).contains(pos)) {
            cursor = Qt::SizeBDiagCursor;
        } else if (getHandleRect(Top).contains(pos) || getHandleRect(Bottom).contains(pos)) {
            cursor = Qt::SizeVerCursor;
        } else if (getHandleRect(Left).contains(pos) || getHandleRect(Right).contains(pos)) {
            cursor = Qt::SizeHorCursor;
        } else {
            cursor = Qt::ArrowCursor;
        }
        setCursor(cursor);
    }
}

void EditWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && activeHandle != None) {
        activeHandle = None;
        emit handleReleased();
    }
}

QRect EditWindow::getHandleRect(Handle handle) const
{
    int halfHandle = handleSize / 2;
    switch (handle) {
    case TopLeft: return QRect(0 - halfHandle, 0 - halfHandle, handleSize, handleSize);
    case Top: return QRect(width() / 2 - halfHandle, 0 - halfHandle, handleSize, handleSize);
    case TopRight: return QRect(width() - halfHandle, 0 - halfHandle, handleSize, handleSize);
    case Right: return QRect(width() - halfHandle, height() / 2 - halfHandle, handleSize, handleSize);
    case BottomRight: return QRect(width() - halfHandle, height() - halfHandle, handleSize, handleSize);
    case Bottom: return QRect(width() / 2 - halfHandle, height() - halfHandle, handleSize, handleSize);
    case BottomLeft: return QRect(0 - halfHandle, height() - halfHandle, handleSize, handleSize);
    case Left: return QRect(0 - halfHandle, height() / 2 - halfHandle, handleSize, handleSize);
    default: return QRect();
    }
}
