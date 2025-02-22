#include "editwindow.h"
#include "mainwindow.h"
#include "toolbarwindow.h"
#include <QPainter>
#include <QDebug>
#include <QInputDialog>
#include <QCursor>
#include <QApplication>
#include <QClipboard>
#include <cmath>

EditWindow::EditWindow(const QPixmap &screenshot, const QPoint &pos, QWidget *parent)
    : QWidget(parent), screenshot(screenshot), canvas(screenshot), drawingLayer(screenshot.size()), tempLayer(screenshot.size())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFixedSize(screenshot.size());
    move(pos);
    setMouseTracking(true);
    drawingLayer.fill(Qt::transparent);
    tempLayer.fill(Qt::transparent);
    updateCanvas();
    toolBar = new ToolBarWindow(this, this);
    toolBar->show();

    // 默认模式已在 ToolBarWindow 构造函数中设置，无需在此处设置 dragButton
    mode = -1; // 确保初始模式为小手模式
    isDragMode = true;

    connect(toolBar, &ToolBarWindow::modeChanged, this, [this](int m) {
        setMode(m);
    });
    connect(toolBar, &ToolBarWindow::dragModeChanged, this, [this](bool enabled) {
        isDragMode = enabled;
        qDebug() << "EditWindow: Drag mode:" << isDragMode;
    });
    connect(toolBar, &ToolBarWindow::undoRequested, this, [this]() {
        if (!shapes.isEmpty()) {
            shapes.removeLast();
            updateCanvas();
            update();
        }
    });
    connect(toolBar, &ToolBarWindow::finishRequested, this, [this]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(canvas);
        hide();
        toolBar->hide();
        QCoreApplication::quit();
    });
    connect(toolBar, &ToolBarWindow::cancelRequested, this, []() {
        QCoreApplication::quit();
    });
    connect(toolBar, &ToolBarWindow::textFontSizeChanged, this, [this](int size) { fontSize = size; });
    connect(toolBar, &ToolBarWindow::textColorChanged, this, [this](const QColor &color) { textColor = color; });
    connect(toolBar, &ToolBarWindow::mosaicSizeChanged, this, [this](int size) { mosaicSize = size; });
    connect(toolBar, &ToolBarWindow::borderWidthChanged, this, [this](int width) { shapeBorderWidth = width; });
    connect(toolBar, &ToolBarWindow::borderColorChanged, this, [this](const QColor &color) { shapeBorderColor = color; });
    connect(toolBar, &ToolBarWindow::penWidthChanged, this, [this](int width) { penWidth = width; });
    connect(toolBar, &ToolBarWindow::penColorChanged, this, [this](const QColor &color) { penColor = color; });
    show();
}

// 其余部分保持不变，以下仅列出构造函数修改后的内容，完整代码请参考前次提交
EditWindow::~EditWindow()
{
    delete toolBar;
}

void EditWindow::updateScreenshot(const QPixmap &newScreenshot, const QPoint &newPos)
{
    screenshot = newScreenshot;

    QPixmap newDrawingLayer(newScreenshot.size());
    newDrawingLayer.fill(Qt::transparent);
    QPixmap newTempLayer(newScreenshot.size());
    newTempLayer.fill(Qt::transparent);

    QPainter painter(&newDrawingLayer);
    painter.drawPixmap(0, 0, drawingLayer);

    drawingLayer = newDrawingLayer;
    tempLayer = newTempLayer;

    setFixedSize(newScreenshot.size());
    move(newPos);
    updateCanvas();
    toolBar->adjustPosition();
    update();
    qDebug() << "EditWindow: Screenshot updated, size:" << newScreenshot.size() << ", pos:" << newPos;
}

void EditWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, canvas);
    painter.drawPixmap(0, 0, tempLayer);

    // 只有在小手模式（mode == -1）下显示中点
    if (mode == -1) {
        int dynamicHandleSize = calculateHandleSize();
        for (int i = 0; i < 8; ++i) {
            Handle handle = static_cast<Handle>(i + 1);
            QRect handleRect = getHandleRect(handle);
            handleRect.setSize(QSize(dynamicHandleSize, dynamicHandleSize));
            handleRect.moveCenter(getHandleRect(handle).center());
            painter.fillRect(handleRect, Qt::blue);
        }
    }
}

void EditWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        int dynamicHandleSize = calculateHandleSize();
        MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());

        // 检查是否在拖拽模式下移动整个选区或调整中点
        if (mode == -1) {
            // 检查中点点击
            for (int i = 0; i < 8; ++i) {
                Handle handle = static_cast<Handle>(i + 1);
                QRect handleRect = getHandleRect(handle);
                handleRect.setSize(QSize(dynamicHandleSize, dynamicHandleSize));
                handleRect.moveCenter(getHandleRect(handle).center());
                if (handleRect.contains(pos)) {
                    activeHandle = handle;
                    isAdjustingHandle = true;
                    dragStartPos = event->globalPosition().toPoint();
                    qDebug() << "EditWindow: Handle pressed:" << activeHandle << "at:" << pos;
                    emit handleDragged(activeHandle, dragStartPos);
                    return;
                }
            }

            // 检查拖动整个选区
            if (isDragMode) {
                dragStartPos = event->globalPosition().toPoint();
                isDraggingSelection = true;
                qDebug() << "EditWindow: Start dragging selection at:" << dragStartPos;
                return;
            }

            // 检查拖动已有形状
            selectedShape = hitTest(pos);
            if (selectedShape) {
                isDragging = true;
                dragStartPos = pos;
                selectedShape->offset = pos - selectedShape->rect.topLeft();
                qDebug() << "EditWindow: Dragging shape at:" << pos;
                return;
            }
        }

        // 绘制新形状，仅在绘制模式下触发
        if (mode >= 0 && !isAdjustingHandle && !isDragging && !isDraggingSelection && activeHandle == None &&
            mainWindow && !mainWindow->isAdjustingSelection()) {
            if (!hitTest(pos)) { // 确保不在现有形状上
                startPoint = pos;
                isDrawing = true;
                qDebug() << "EditWindow: Start drawing at:" << pos << ", mode:" << mode;
                if (mode == 2) {
                    QString text = QInputDialog::getMultiLineText(this, "输入文本", "请输入文本:");
                    if (!text.isEmpty()) {
                        Shape shape;
                        shape.type = Text;
                        QFont font("Arial", fontSize);
                        QRect textRect = QFontMetrics(font).boundingRect(QRect(0, 0, width() - pos.x(), height() - pos.y()), Qt::AlignLeft | Qt::TextWordWrap, text);
                        shape.rect = QRect(pos, textRect.size());
                        shape.text = text;
                        shape.color = textColor;
                        shape.width = fontSize;
                        shapes.append(shape);
                        updateCanvas();
                        update();
                        isDrawing = false;
                    }
                } else if (mode == 3 || mode == 4) { // 画笔或马赛克
                    Shape shape;
                    shape.type = (mode == 3) ? Pen : Mask;
                    shape.points.append(pos);
                    shape.color = (mode == 3) ? penColor : Qt::gray;
                    shape.width = (mode == 3) ? penWidth : mosaicSize;
                    shapes.append(shape);
                    updateCanvas();
                    update();
                }
            }
        }
    }
}


void EditWindow::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    qDebug() << "EditWindow: Mouse moved to:" << pos << ", isDrawing:" << isDrawing << ", isAdjustingHandle:" << isAdjustingHandle;

    if (isDraggingSelection && (event->buttons() & Qt::LeftButton)) {
        QPoint globalOffset = event->globalPosition().toPoint() - dragStartPos;
        QPoint newPos = this->pos() + globalOffset;

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

        dragStartPos = event->globalPosition().toPoint();
        qDebug() << "EditWindow: Dragging selection to:" << newPos;
    } else if (activeHandle != None && (event->buttons() & Qt::LeftButton)) {
        emit handleDragged(activeHandle, event->globalPosition().toPoint());
    } else if (isDragging && (event->buttons() & Qt::LeftButton) && selectedShape) {
        QPoint newTopLeft = pos - selectedShape->offset;
        selectedShape->rect.moveTo(newTopLeft);
        updateCanvas();
        update();
    } else if (mode >= 0 && (event->buttons() & Qt::LeftButton) && isDrawing) {
        QPainter painter(&tempLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        tempLayer.fill(Qt::transparent);
        if (mode == 0) {
            currentRect = QRect(startPoint, pos).normalized();
            painter.setPen(QPen(shapeBorderColor, shapeBorderWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(currentRect);
            update();
        } else if (mode == 1) {
            painter.setPen(QPen(shapeBorderColor, shapeBorderWidth));
            painter.setBrush(Qt::NoBrush);
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                int size = qMin(qAbs(pos.x() - startPoint.x()), qAbs(pos.y() - startPoint.y()));
                QPoint adjustedEnd = startPoint + QPoint(pos.x() > startPoint.x() ? size : -size,
                                                         pos.y() > startPoint.y() ? size : -size);
                currentRect = QRect(startPoint, adjustedEnd).normalized();
            } else {
                currentRect = QRect(startPoint, pos).normalized();
            }
            painter.drawEllipse(currentRect);
            update();
        } else if ((mode == 3 || mode == 4) && !shapes.isEmpty() && (shapes.last().type == Pen || shapes.last().type == Mask)) {
            shapes.last().points.append(pos);
            painter.setPen(QPen(shapes.last().color, shapes.last().width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            if (mode == 3) { // 画笔：绘制连续线段
                for (int i = 1; i < shapes.last().points.size(); ++i) {
                    painter.drawLine(shapes.last().points[i - 1], shapes.last().points[i]);
                }
            } else if (mode == 4) { // 马赛克：绘制粗线模拟连续马赛克效果
                painter.setPen(QPen(shapes.last().color, shapes.last().width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                for (int i = 1; i < shapes.last().points.size(); ++i) {
                    painter.drawLine(shapes.last().points[i - 1], shapes.last().points[i]);
                }
            }
            update();
        }
    } else {
        // 光标逻辑保持不变
        QCursor cursor;
        Shape *hoveredShape = hitTest(pos);
        if (hoveredShape) {
            cursor = Qt::SizeAllCursor;
        } else if (mode == 0 || mode == 1 || mode == 3) {
            cursor = Qt::ArrowCursor;
        } else if (mode == -1 && isDragMode) {
            cursor = Qt::OpenHandCursor;
        } else if (mode == 4) {
            QPixmap cursorPixmap(mosaicSize, mosaicSize);
            cursorPixmap.fill(Qt::transparent);
            QPainter cursorPainter(&cursorPixmap);
            cursorPainter.setRenderHint(QPainter::Antialiasing);
            cursorPainter.setBrush(Qt::gray);
            cursorPainter.setPen(Qt::NoPen);
            cursorPainter.drawEllipse(0, 0, mosaicSize, mosaicSize);
            cursor = QCursor(cursorPixmap);
        } else {
            cursor = Qt::OpenHandCursor;
            if (mode == -1) {
                int dynamicHandleSize = calculateHandleSize();
                for (int i = 0; i < 8; ++i) {
                    Handle handle = static_cast<Handle>(i + 1);
                    QRect handleRect = getHandleRect(handle);
                    handleRect.setSize(QSize(dynamicHandleSize, dynamicHandleSize));
                    handleRect.moveCenter(getHandleRect(handle).center());
                    if (handleRect.contains(pos)) {
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
            qDebug() << "EditWindow: Stop dragging selection";
        } else if (activeHandle != None) {
            emit handleReleased();
            activeHandle = None;
            isAdjustingHandle = false;
            isDrawing = false;
            isDragging = false;
            MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
            if (mainWindow) {
                mainWindow->resetSelectionState();
            }
            qDebug() << "EditWindow: Handle released, all states reset";
            update();
        } else if (isDragging) {
            isDragging = false;
            updateCanvas();
            update();
        } else if ((mode == 0 || mode == 1) && isDrawing) {
            if (startPoint != event->pos()) {
                Shape shape;
                shape.type = (mode == 0) ? Rectangle : Ellipse;
                shape.rect = QRect(startPoint, event->pos()).normalized();
                shape.width = shapeBorderWidth;
                shape.color = shapeBorderColor;
                shapes.append(shape);
            }
            tempLayer.fill(Qt::transparent);
            updateCanvas();
            update();
            isDrawing = false;
        } else if (mode == 3 || mode == 4) {
            tempLayer.fill(Qt::transparent);
            updateCanvas();
            update();
            if (mode == 3 || mode == 4) {
                isDrawing = false;
            }
            qDebug() << "EditWindow: Mode 3/4 completed, mode:" << mode << ", isDrawing:" << isDrawing;
        }
    }
}

QRect EditWindow::getHandleRect(Handle handle) const
{
    int dynamicHandleSize = calculateHandleSize();
    int halfHandle = dynamicHandleSize / 2;
    switch (handle) {
    case TopLeft: return QRect(0, 0, dynamicHandleSize, dynamicHandleSize);
    case Top: return QRect(width() / 2 - halfHandle, 0, dynamicHandleSize, dynamicHandleSize);
    case TopRight: return QRect(width() - dynamicHandleSize, 0, dynamicHandleSize, dynamicHandleSize);
    case Right: return QRect(width() - dynamicHandleSize, height() / 2 - halfHandle, dynamicHandleSize, dynamicHandleSize);
    case BottomRight: return QRect(width() - dynamicHandleSize, height() - dynamicHandleSize, dynamicHandleSize, dynamicHandleSize);
    case Bottom: return QRect(width() / 2 - halfHandle, height() - dynamicHandleSize, dynamicHandleSize, dynamicHandleSize);
    case BottomLeft: return QRect(0, height() - dynamicHandleSize, dynamicHandleSize, dynamicHandleSize);
    case Left: return QRect(0, height() / 2 - halfHandle, dynamicHandleSize, dynamicHandleSize);
    default: return QRect();
    }
}
void EditWindow::drawShape(QPainter &painter, const Shape &shape)
{
    painter.setPen(QPen(shape.color, shape.width));
    painter.setBrush(Qt::NoBrush);
    if (shape.type == Rectangle) {
        painter.drawRect(shape.rect);
    } else if (shape.type == Ellipse) {
        painter.drawEllipse(shape.rect);
    } else if (shape.type == Text) {
        painter.setFont(QFont("Arial", shape.width));
        painter.setPen(shape.color);
        painter.drawText(shape.rect, Qt::AlignLeft | Qt::TextWordWrap, shape.text);
    } else if (shape.type == Pen || shape.type == Mask) {
        painter.setPen(QPen(shape.color, shape.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i < shape.points.size(); ++i) {
            painter.drawLine(shape.points[i - 1], shape.points[i]);
        }
    }
}

void EditWindow::updateCanvas()
{
    drawingLayer.fill(Qt::transparent);
    QPainter painter(&drawingLayer);
    painter.setRenderHint(QPainter::Antialiasing);
    for (const Shape &shape : shapes) {
        drawShape(painter, shape);
    }
    canvas = screenshot.copy();
    QPainter canvasPainter(&canvas);
    canvasPainter.setRenderHint(QPainter::Antialiasing);
    canvasPainter.drawPixmap(0, 0, drawingLayer);
}

Shape* EditWindow::hitTest(const QPoint &pos)
{
    int dynamicHandleSize = calculateHandleSize();
    for (int i = shapes.size() - 1; i >= 0; --i) {
        Shape &shape = shapes[i];
        if (shape.type == Rectangle) {
            if (isOnBorder(shape.rect, pos, dynamicHandleSize / 2)) {
                return &shape;
            }
        } else if (shape.type == Ellipse) {
            if (isOnEllipseBorder(shape.rect, pos, shape.width)) {
                return &shape;
            }
        } else if (shape.type == Text) {
            if (shape.rect.contains(pos)) {
                return &shape;
            }
        }
    }
    return nullptr;
}

bool EditWindow::isOnBorder(const QRect &rect, const QPoint &pos, int borderWidth)
{
    QRect outerRect = rect.adjusted(-borderWidth, -borderWidth, borderWidth, borderWidth);
    QRect innerRect = rect.adjusted(borderWidth, borderWidth, -borderWidth, -borderWidth);
    return outerRect.contains(pos) && !innerRect.contains(pos);
}

bool EditWindow::isOnEllipseBorder(const QRect &rect, const QPoint &pos, int borderWidth)
{
    double centerX = rect.center().x();
    double centerY = rect.center().y();
    double a = rect.width() / 2.0;
    double b = rect.height() / 2.0;
    double x = pos.x() - centerX;
    double y = pos.y() - centerY;

    double value = (x * x) / (a * a) + (y * y) / (b * b);
    double tolerance = borderWidth / a;
    return std::abs(value - 1.0) <= tolerance;
}

int EditWindow::calculateHandleSize() const
{
    int minDimension = qMin(width(), height());
    int size = minDimension / 20;
    return qBound(4, size, 16);
}

void EditWindow::setMode(int newMode)
{
    mode = newMode;
    isDrawing = false;
    isAdjustingHandle = false;
    isDragging = false;
    isDraggingSelection = false;
    activeHandle = None;
    selectedShape = nullptr;
    qDebug() << "EditWindow: Mode set to:" << mode << "by external caller, all states reset";
}
