#include "mainwindow.h"
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("截图工具");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool); // 添加 Qt::Tool
    setMouseTracking(true);
    qDebug() << "MainWindow: Mouse tracking enabled:" << hasMouseTracking();

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

    isSelectingInitial = true;
    qDebug() << "MainWindow: Initialized with isSelectingInitial:" << isSelectingInitial;
}


MainWindow::~MainWindow()
{
    delete magnifier;
    delete editWindow;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isEditing) {
        if (isSelectingInitial) {
            startPoint = event->pos();
            endPoint = startPoint;
            magnifier->hide();
            update();
            qDebug() << "MainWindow: Mouse pressed at:" << startPoint << ", isSelectingInitial:" << isSelectingInitial;
        }
    } else if (event->button() == Qt::RightButton) {
        if (editWindow && editWindow->isVisible()) {
            cancelEditing();
        } else {
            qDebug() << "MainWindow: Right-click detected, no edit window present, exiting program";
            QCoreApplication::quit();
        }
    }
    event->ignore();
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    currentMousePos = event->pos();

    if (isEditing && editWindow && editWindow->isVisible()) {
        return;
    }

    if ((isSelectingInitial || isAdjustingSelection) && (event->buttons() & Qt::LeftButton)) {
        if (activeHandle == None) {
            endPoint = event->pos();
        } else {
            switch (activeHandle) {
            case TopLeft:
                startPoint = event->pos();
                break;
            case Top:
                startPoint.setY(event->pos().y());
                break;
            case TopRight:
                startPoint.setY(event->pos().y());
                endPoint.setX(event->pos().x());
                break;
            case Right:
                endPoint.setX(event->pos().x());
                break;
            case BottomRight:
                endPoint = event->pos();
                break;
            case Bottom:
                endPoint.setY(event->pos().y());
                break;
            case BottomLeft:
                startPoint.setX(event->pos().x());
                endPoint.setY(event->pos().y());
                break;
            case Left:
                startPoint.setX(event->pos().x());
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
    if (event->button() == Qt::LeftButton && (isSelectingInitial || isAdjustingSelection)) {
        if (activeHandle == None) {
            endPoint = event->pos();
        } else {
            QPoint pos = event->pos();
            switch (activeHandle) {
            case TopLeft:
                startPoint = pos;
                break;
            case Top:
                startPoint.setY(pos.y());
                break;
            case TopRight:
                startPoint.setY(pos.y());
                endPoint.setX(pos.x());
                break;
            case Right:
                endPoint.setX(pos.x());
                break;
            case BottomRight:
                endPoint = pos;
                break;
            case Bottom:
                endPoint.setY(pos.y());
                break;
            case BottomLeft:
                startPoint.setX(pos.x());
                endPoint.setY(pos.y());
                break;
            case Left:
                startPoint.setX(pos.x());
                break;
            default:
                break;
            }
        }
        isSelectingInitial = false;
        isAdjustingSelection = false;
        isEditing = true;
        activeHandle = None;
        releaseMouse();
        qDebug() << "MainWindow: Entered editing state after adjustment";

        QRect selection(startPoint, endPoint);
        selection = selection.normalized();
        initialWidth = selection.width();
        initialHeight = selection.height();
        QPixmap selectedPixmap = originalScreenshot.copy(selection);
        if (editWindow) {
            editWindow->updateScreenshot(selectedPixmap, selection.topLeft());
            editWindow->show();
            editWindow->activateWindow();
            editWindow->setFocus();
            if (editWindow->getIsAdjustingFromEditMode()) {
                editWindow->showToolBar();
                editWindow->setMode(-1);
                qDebug() << "MainWindow: Adjusting from edit mode completed, toolbar shown";
            }
        } else {
            editWindow = new EditWindow(selectedPixmap, selection.topLeft(), this);
            editWindow->show();
            editWindow->activateWindow();
            editWindow->setFocus();
            connect(editWindow, &EditWindow::finished, this, [this](const QPixmap &) {
                for (int i = 0; i < 5; ++i) {
                    QApplication::processEvents();
                    QThread::msleep(50);
                }
                if (editWindow) {
                    editWindow->hide();
                    editWindow->hideToolBar();
                }
                hide();
                QCoreApplication::quit();
            });
        }
        connect(editWindow, &EditWindow::handleDragged, this, &MainWindow::startDragging);
        connect(editWindow, &EditWindow::handleReleased, this, &MainWindow::resetSelectionState);
        update();
    }
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, screenshot);

    if (isSelectingInitial || isAdjustingSelection) {
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
        QPoint topLeft = selection.normalized().topLeft();
        QPoint textPos = topLeft;
        int margin = 5;
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
}

void MainWindow::startDragging(Handle handle, const QPoint &globalPos)
{
    if (!isSelectingInitial && !isAdjustingSelection) {
        if (editWindow) {
            editWindow->hide();
        }
        isEditing = false;
        isAdjustingSelection = true;
        activeHandle = handle;
        grabMouse();
    }
    QPoint localPos = mapFromGlobal(globalPos);
    QRect screenRect = screen()->geometry();

    switch (activeHandle) {
    case TopLeft:
        startPoint = localPos;
        if (startPoint.x() < 0) startPoint.setX(0);
        if (startPoint.y() < 0) startPoint.setY(0);
        if (startPoint.x() > endPoint.x()) startPoint.setX(endPoint.x());
        if (startPoint.y() > endPoint.y()) startPoint.setY(endPoint.y());
        break;
    case Top:
        startPoint.setY(localPos.y());
        if (startPoint.y() < 0) startPoint.setY(0);
        if (startPoint.y() > endPoint.y()) startPoint.setY(endPoint.y());
        break;
    case TopRight:
        startPoint.setY(localPos.y());
        endPoint.setX(localPos.x());
        if (startPoint.y() < 0) startPoint.setY(0);
        if (endPoint.x() > screenRect.width()) endPoint.setX(screenRect.width());
        if (startPoint.y() > endPoint.y()) startPoint.setY(endPoint.y());
        if (endPoint.x() < startPoint.x()) endPoint.setX(startPoint.x());
        break;
    case Right:
        endPoint.setX(localPos.x());
        if (endPoint.x() > screenRect.width()) endPoint.setX(screenRect.width());
        if (endPoint.x() < startPoint.x()) endPoint.setX(startPoint.x());
        break;
    case BottomRight:
        endPoint = localPos;
        if (endPoint.x() > screenRect.width()) endPoint.setX(screenRect.width());
        if (endPoint.y() > screenRect.height()) endPoint.setY(screenRect.height());
        if (endPoint.x() < startPoint.x()) endPoint.setX(startPoint.x());
        if (endPoint.y() < startPoint.y()) endPoint.setY(startPoint.y());
        break;
    case Bottom:
        endPoint.setY(localPos.y());
        if (endPoint.y() > screenRect.height()) endPoint.setY(screenRect.height());
        if (endPoint.y() < startPoint.y()) endPoint.setY(startPoint.y());
        break;
    case BottomLeft:
        startPoint.setX(localPos.x());
        endPoint.setY(localPos.y());
        if (startPoint.x() < 0) startPoint.setX(0);
        if (endPoint.y() > screenRect.height()) endPoint.setY(screenRect.height());
        if (startPoint.x() > endPoint.x()) startPoint.setX(endPoint.x());
        if (endPoint.y() < startPoint.y()) endPoint.setY(startPoint.y());
        break;
    case Left:
        startPoint.setX(localPos.x());
        if (startPoint.x() < 0) startPoint.setX(0);
        if (startPoint.x() > endPoint.x()) startPoint.setX(endPoint.x());
        break;
    default:
        break;
    }
    update();
    qDebug() << "MainWindow: Dragging handle:" << activeHandle << ", startPoint:" << startPoint << ", endPoint:" << endPoint;
}

QPixmap MainWindow::updateSelectionPosition(const QPoint &newPos)
{
    startPoint = newPos;
    endPoint = startPoint + QPoint(initialWidth, initialHeight);

    QRect screenRect = screen()->geometry();
    if (startPoint.x() < 0) {
        startPoint.setX(0);
        endPoint.setX(initialWidth);
    }
    if (startPoint.y() < 0) {
        startPoint.setY(0);
        endPoint.setY(initialHeight);
    }
    if (endPoint.x() > screenRect.width()) {
        endPoint.setX(screenRect.width());
        startPoint.setX(screenRect.width() - initialWidth);
    }
    if (endPoint.y() > screenRect.height()) {
        endPoint.setY(screenRect.height());
        startPoint.setY(screenRect.height() - initialHeight);
    }

    QRect newSelection(startPoint, endPoint);
    QPixmap newScreenshot = originalScreenshot.copy(newSelection);
    qDebug() << "MainWindow: New selection size:" << newSelection.width() << "x" << newSelection.height();
    return newScreenshot;
}

QRect MainWindow::getSelection() const
{
    return QRect(startPoint, endPoint).normalized();
}

void MainWindow::resetSelectionState()
{
    isSelectingInitial = false;
    isAdjustingSelection = false;
    activeHandle = None;
    isEditing = true;
    releaseMouse();
    if (editWindow) {
        editWindow->setMode(-1);
        QRect selection(startPoint, endPoint);
        selection = selection.normalized();
        QPixmap newScreenshot = originalScreenshot.copy(selection);
        editWindow->updateScreenshot(newScreenshot, selection.topLeft());
        editWindow->show();
        editWindow->activateWindow();
        editWindow->setFocus();
    }
    qDebug() << "MainWindow: Selection state reset, isSelectingInitial:" << isSelectingInitial
             << ", isAdjustingSelection:" << isAdjustingSelection
             << ", isEditing:" << isEditing
             << ", activeHandle:" << activeHandle;
    update();
}

bool MainWindow::isSelectingInitialState() const
{
    return isSelectingInitial;
}

bool MainWindow::isAdjustingSelectionState() const
{
    return isAdjustingSelection;
}



void MainWindow::cancelEditing()
{
    if (editWindow) {
        editWindow->hide();
        editWindow->hideToolBar();
        delete editWindow; // 清理指针，避免内存泄漏
        editWindow = nullptr;
    }
    isEditing = false;
    isAdjustingSelection = false;
    isSelectingInitial = true;
    activeHandle = None;
    startPoint = QPoint();
    endPoint = QPoint();
    screenshot = originalScreenshot; // 重置截图
    magnifier->show();
    update();
    qDebug() << "MainWindow: Editing canceled, returned to initial selection state";
}
