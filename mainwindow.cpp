#include "mainwindow.h"
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("截图工具");
    setWindowFlags(Qt::FramelessWindowHint);
    setMouseTracking(true);
    qDebug() << "Mouse tracking enabled:" << hasMouseTracking();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        originalScreenshot = screen->grabWindow(0);
        screenshot = originalScreenshot;
        setGeometry(screen->geometry());
        setFixedSize(screen->geometry().size());
    }

    magnifier = new MagnifierWindow(originalScreenshot, this);
    magnifier->show();
    currentMousePos = mapFromGlobal(QCursor::pos());
    updateMagnifierPosition();
}

MainWindow::~MainWindow()
{
    delete magnifier;
    delete editWindow;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isEditing) {
        startPoint = event->pos();
        endPoint = startPoint;
        isSelecting = true;
        magnifier->hide();
        update();
    }
    event->ignore();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    currentMousePos = event->pos();
    qDebug() << "MainWindow mouse moved to:" << currentMousePos << "isSelecting:" << isSelecting << "activeHandle:" << activeHandle;
    if (isEditing && editWindow && editWindow->isVisible()) {
        qDebug() << "Ignoring move event due to EditWindow being active";
        return; // 编辑窗口显示时，不处理鼠标移动
    }
    if (isSelecting) {
        if (activeHandle == None) {
            endPoint = event->pos(); // 普通选区
        } else {
            // 根据中点调整选区，固定对边或对角
            switch (activeHandle) {
            case TopLeft:
                startPoint = event->pos(); // 固定 BottomRight
                break;
            case Top:
                startPoint.setY(event->pos().y()); // 固定 Bottom
                break;
            case TopRight:
                startPoint.setY(event->pos().y()); // 固定 BottomLeft
                endPoint.setX(event->pos().x());
                break;
            case Right:
                endPoint.setX(event->pos().x()); // 固定 Left
                break;
            case BottomRight:
                endPoint = event->pos(); // 固定 TopLeft
                break;
            case Bottom:
                endPoint.setY(event->pos().y()); // 固定 Top
                break;
            case BottomLeft:
                startPoint.setX(event->pos().x()); // 固定 TopRight
                endPoint.setY(event->pos().y());
                break;
            case Left:
                startPoint.setX(event->pos().x()); // 固定 Right
                break;
            default:
                break;
            }
        }
        update();
    } else if (!isEditing) {
        updateMagnifierPosition();
        magnifier->show();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isSelecting) {
        if (activeHandle == None) {
            endPoint = event->pos(); // 普通选区
        } else {
            QPoint pos = event->pos();
            switch (activeHandle) {
            case TopLeft:
                startPoint = pos; // 固定 BottomRight
                break;
            case Top:
                startPoint.setY(pos.y()); // 固定 Bottom
                break;
            case TopRight:
                startPoint.setY(pos.y()); // 固定 BottomLeft
                endPoint.setX(pos.x());
                break;
            case Right:
                endPoint.setX(pos.x()); // 固定 Left
                break;
            case BottomRight:
                endPoint = pos; // 固定 TopLeft
                break;
            case Bottom:
                endPoint.setY(pos.y()); // 固定 Top
                break;
            case BottomLeft:
                startPoint.setX(pos.x()); // 固定 TopRight
                endPoint.setY(pos.y());
                break;
            case Left:
                startPoint.setX(pos.x()); // 固定 Right
                break;
            default:
                break;
            }
        }
        isSelecting = false;
        isEditing = true;
        activeHandle = None;
        releaseMouse(); // 确保释放鼠标捕获
        qDebug() << "Entered editing state";

        QRect selection(startPoint, endPoint);
        selection = selection.normalized();
        QPixmap selectedPixmap = originalScreenshot.copy(selection);
        if (editWindow) {
            editWindow->updateScreenshot(selectedPixmap, selection.topLeft());
            editWindow->show();
            editWindow->activateWindow(); // 设置为活动窗口
            editWindow->setFocus();      // 赋予焦点
        } else {
            editWindow = new EditWindow(selectedPixmap, selection.topLeft(), this);
            editWindow->show();
            editWindow->activateWindow(); // 设置为活动窗口
            editWindow->setFocus();      // 赋予焦点
        }
        connect(editWindow, &EditWindow::handleDragged, this, &MainWindow::startDragging);
        connect(editWindow, &EditWindow::handleReleased, this, [this]() {
            if (isSelecting) {
                releaseMouse(); // 释放鼠标捕获
                isSelecting = false;
                isEditing = true;
                activeHandle = None;
                QRect selection(startPoint, endPoint);
                selection = selection.normalized();
                QPixmap selectedPixmap = originalScreenshot.copy(selection);
                if (editWindow) {
                    editWindow->updateScreenshot(selectedPixmap, selection.topLeft());
                    editWindow->show();
                    editWindow->activateWindow(); // 设置为活动窗口
                    editWindow->setFocus();      // 赋予焦点
                }
                update();
            }
        });
        update();
    }
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, screenshot);

    if (isSelecting) {
        QRect selection(startPoint, endPoint);
        QRegion maskRegion(rect());
        maskRegion = maskRegion.subtracted(selection.normalized());

        painter.setClipRegion(maskRegion);
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
        painter.setClipping(false);

        QPen pen(borderColor, borderWidth, borderStyle);
        painter.setPen(pen);
        painter.drawRect(selection);

        int width = qAbs(endPoint.x() - startPoint.x());
        int height = qAbs(endPoint.y() - startPoint.y());
        QString sizeText = QString("%1x%2").arg(width).arg(height);

        painter.setPen(Qt::red);
        painter.setFont(QFont("Arial", 14));
        // 使用 normalized 后的左上角动态调整文本位置
        QPoint topLeft = selection.normalized().topLeft();
        QPoint textPos = topLeft;
        int margin = 5;
        // 如果左上角靠近窗口边界，调整文本位置
        if (topLeft.x() < 20 || topLeft.y() < 20) {
            textPos = QPoint(topLeft.x() + margin, topLeft.y() + 20 + margin);
        } else {
            textPos = QPoint(topLeft.x() - margin, topLeft.y() - 20 - margin);
        }
        painter.drawText(textPos, sizeText);
    } else {
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }
}

void MainWindow::updateMagnifierPosition()
{
    QPoint globalPos = mapToGlobal(currentMousePos);
    magnifier->move(globalPos.x() + 20, globalPos.y() + 20);
    if (magnifier->x() + magnifier->width() > screen()->geometry().right()) {
        magnifier->move(globalPos.x() - magnifier->width() - 20, magnifier->y());
    }
    if (magnifier->y() + magnifier->height() > screen()->geometry().bottom()) {
        magnifier->move(magnifier->x(), globalPos.y() - magnifier->height() - 20);
    }
    magnifier->updatePosition(currentMousePos);
    qDebug() << "Magnifier updated to:" << magnifier->pos();
}

void MainWindow::startDragging(Handle handle, const QPoint &globalPos)
{
    if (!isSelecting) {
        if (editWindow) {
            editWindow->hide(); // 隐藏编辑窗口
        }
        isEditing = false;
        isSelecting = true;
        activeHandle = handle;
        grabMouse(); // 抢占鼠标，确保 mouseMoveEvent 触发
    }
    QPoint localPos = mapFromGlobal(globalPos);
    if (activeHandle == Bottom) {
        endPoint.setY(localPos.y()); // 只改变高度
    } else {
        switch (activeHandle) {
        case TopLeft:
            startPoint = localPos; // 固定 BottomRight
            break;
        case Top:
            startPoint.setY(localPos.y()); // 固定 Bottom
            break;
        case TopRight:
            startPoint.setY(localPos.y()); // 固定 BottomLeft
            endPoint.setX(localPos.x());
            break;
        case Right:
            endPoint.setX(localPos.x()); // 固定 Left
            break;
        case BottomRight:
            endPoint = localPos; // 固定 TopLeft
            break;
        case BottomLeft:
            startPoint.setX(localPos.x()); // 固定 TopRight
            endPoint.setY(localPos.y());
            break;
        case Left:
            startPoint.setX(localPos.x()); // 固定 Right
            break;
        default:
            break;
        }
    }
    update();
}
