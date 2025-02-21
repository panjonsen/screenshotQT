#include "editwindow.h"
#include "mainwindow.h"
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
    update();
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

        if (isDragMode) {
            dragStartPos = event->globalPos();
            isDraggingSelection = true;
            qDebug() << "Start dragging selection at:" << dragStartPos;
        } else {
            activeHandle = None;
            isDragging = true;
            dragStartPos = event->globalPos();
            qDebug() << "Drag started at:" << dragStartPos;
        }
    }
}

void EditWindow::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    qDebug() << "EditWindow mouse moved to:" << pos;

    if (isDraggingSelection && (event->buttons() & Qt::LeftButton)) {
        QPoint globalOffset = event->globalPos() - dragStartPos;
        QPoint newPos = this->pos() + globalOffset;

        // 限制拖动范围在屏幕内
        QRect screenRect = screen()->geometry();
        int maxX = screenRect.width() - width();
        int maxY = screenRect.height() - height();
        newPos.setX(qBound(0, newPos.x(), maxX));
        newPos.setY(qBound(0, newPos.y(), maxY));

        MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
        if (mainWindow) {
            QPixmap newScreenshot = mainWindow->updateSelectionPosition(newPos);
            updateScreenshot(newScreenshot, newPos);
        }

        dragStartPos = event->globalPos();
        qDebug() << "Dragging selection to:" << newPos;
    } else if (activeHandle != None && (event->buttons() & Qt::LeftButton)) {
        emit handleDragged(activeHandle, event->globalPos());
    } else if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint globalOffset = event->globalPos() - dragStartPos;
        QPoint newPos = this->pos() + globalOffset;

        // 限制拖动范围在屏幕内
        QRect screenRect = screen()->geometry();
        int maxX = screenRect.width() - width();
        int maxY = screenRect.height() - height();
        newPos.setX(qBound(0, newPos.x(), maxX));
        newPos.setY(qBound(0, newPos.y(), maxY));

        move(newPos);
        dragStartPos = event->globalPos();
        qDebug() << "Dragging EditWindow to:" << newPos;
        update();
    } else {
        QCursor cursor = Qt::OpenHandCursor;
        for (int i = 0; i < 8; ++i) {
            Handle handle = static_cast<Handle>(i + 1);
            if (getHandleRect(handle).contains(pos)) {
                switch (handle) {
                case TopLeft:
                case BottomRight:
                    cursor = Qt::SizeFDiagCursor;
                    break;
                case TopRight:
                case BottomLeft:
                    cursor = Qt::SizeBDiagCursor;
                    break;
                case Top:
                case Bottom:
                    cursor = Qt::SizeVerCursor;
                    break;
                case Left:
                case Right:
                    cursor = Qt::SizeHorCursor;
                    break;
                default:
                    break;
                }
                break;
            }
        }
        setCursor(cursor);
    }
}

void EditWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isDraggingSelection) {
            isDraggingSelection = false;
            qDebug() << "Stop dragging selection";
        } else if (activeHandle != None) {
            emit handleReleased();
            activeHandle = None;
        } else if (isDragging) {
            isDragging = false;
        }
    }
}

QRect EditWindow::getHandleRect(Handle handle) const
{
    int halfHandle = handleSize / 2;
    switch (handle) {
    case TopLeft: return QRect(0, 0, handleSize, handleSize);
    case Top: return QRect(width() / 2 - halfHandle, 0, handleSize, handleSize);
    case TopRight: return QRect(width() - handleSize, 0, handleSize, handleSize);
    case Right: return QRect(width() - handleSize, height() / 2 - halfHandle, handleSize, handleSize);
    case BottomRight: return QRect(width() - handleSize, height() - handleSize, handleSize, handleSize);
    case Bottom: return QRect(width() / 2 - halfHandle, height() - handleSize, handleSize, handleSize);
    case BottomLeft: return QRect(0, height() - handleSize, handleSize, handleSize);
    case Left: return QRect(0, height() / 2 - halfHandle, handleSize, handleSize);
    default: return QRect();
    }
}
