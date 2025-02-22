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
#include <QTimer>
#include <QThread>
#include <QPainterPath> // 添加此行
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

    mode = -1;
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
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            qDebug() << "不在主线程中！";
            return;
        }
        QClipboard *clipboard = QApplication::clipboard();
        bool settingPixmap = true;

        connect(clipboard, &QClipboard::dataChanged, this, [this, clipboard, settingPixmap]() {
            if (settingPixmap) {
                QPixmap pixmap = clipboard->pixmap();
                if (!pixmap.isNull() && pixmap.size() == canvas.size()) {
                    qDebug() << "11";
                    QTimer::singleShot(100, qApp, &QApplication::quit);
                } else {
                    qDebug() << "22";
                }
            } else {
                if (clipboard->text() == "Hello, world!") {
                    qDebug() << "01";
                } else {
                    qDebug() << "02";
                }
            }
            disconnect(clipboard, &QClipboard::dataChanged, this, nullptr);
        });

        if (settingPixmap) {
            QImage image = canvas.toImage();
            clipboard->setImage(image);
        } else {
            clipboard->setText("Hello, world!");
        }
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
    // 移除 toolBar->show()，避免拖动过程中显示工具栏
    update();
    qDebug() << "EditWindow: Screenshot updated, size:" << newScreenshot.size() << ", pos:" << newPos;
}

void EditWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, canvas);
    painter.drawPixmap(0, 0, tempLayer);

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

        selectedShape = hitTest(pos);
        if (selectedShape) {
            isDragging = true;
            dragStartPos = pos;
            selectedShape->offset = pos - selectedShape->rect.topLeft();
            qDebug() << "EditWindow: Dragging shape at:" << pos << ", mode:" << mode;
            return;
        }

        if (mode == -1) {
            for (int i = 0; i < 8; ++i) {
                Handle handle = static_cast<Handle>(i + 1);
                QRect handleRect = getHandleRect(handle);
                handleRect.setSize(QSize(dynamicHandleSize, dynamicHandleSize));
                handleRect.moveCenter(getHandleRect(handle).center());
                if (handleRect.contains(pos)) {
                    activeHandle = handle;
                    isAdjustingHandle = true;
                    isAdjustingFromEditMode = true;
                    dragStartPos = event->globalPosition().toPoint();
                    toolBar->hide();
                    qDebug() << "EditWindow: Handle pressed:" << activeHandle << "at:" << pos;
                    emit handleDragged(activeHandle, dragStartPos);
                    return;
                }
            }

            if (isDragMode) {
                dragStartPos = event->globalPosition().toPoint();
                isDraggingSelection = true;
                toolBar->hide();
                qDebug() << "EditWindow: Start dragging selection at:" << dragStartPos;
                return;
            }
        }

        if (mode >= 0 && !isAdjustingHandle && !isDragging && !isDraggingSelection && activeHandle == None &&
            mainWindow && !mainWindow->isAdjustingSelectionState()) {
            startPoint = pos;
            isDrawing = true;
            qDebug() << "EditWindow: Start drawing at:" << pos << ", mode:" << mode;
            if (mode == 2) { // 文本模式
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
            } else if (mode == 5) { // 序号笔记模式
                QString text = QInputDialog::getMultiLineText(this, "输入序号笔记", "请输入笔记内容:");
                if (!text.isEmpty()) {
                    Shape shape;
                    shape.type = NumberedNote;
                    QFont font("Arial", fontSize);
                    QString numberedText = QString("%1. %2").arg(noteNumber).arg(text); // 格式化序号和文本
                    QRect textRect = QFontMetrics(font).boundingRect(QRect(0, 0, width() - pos.x(), height() - pos.y()), Qt::AlignLeft | Qt::TextWordWrap, numberedText);
                    shape.rect = QRect(pos, QSize(32, 32)); // 固定正方形大小为 32x32
                    shape.text = numberedText;
                    shape.color = textColor;
                    shape.width = fontSize;
                    shape.number = noteNumber; // 记录序号
                    shape.bubbleColor = QColor(200, 200, 200, 128); // 半透明灰色背景（RGBA: 200,200,200,128 约 50% 透明）
                    shape.bubbleBorderColor = Qt::black; // 气泡边框颜色（黑色）

                    // 计算气泡框的尺寸和位置，增加与正方形的间距
                    QFontMetrics fm(font);
                    QString contentText = text; // 仅使用实际输入的文本内容
                    int textWidth = fm.horizontalAdvance(contentText) + 20; // 增加边距
                    int textHeight = fm.height() * (contentText.count('\n') + 1) + 10; // 考虑多行
                    int bubbleX = pos.x() + 34 + 10; // 增加 10 像素的间距，紧邻正方形右侧
                    int bubbleY = pos.y() - (textHeight - 32) / 2; // 垂直居中于正方形
                    shape.bubbleRect = QRect(bubbleX, bubbleY, textWidth, textHeight);

                    shapes.append(shape);
                    noteNumber++; // 递增序号
                    updateCanvas();
                    update();
                    isDrawing = false;
                }
            } else if (mode == 3 || mode == 4) {
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


void EditWindow::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();

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
        qDebug() << "EditWindow: Moving shape to:" << newTopLeft;
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
            if (mode == 3 && (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                // 画笔模式按住 Shift 键绘制直线
                QPoint delta = pos - startPoint;
                QPoint adjustedEnd;
                if (qAbs(delta.x()) > qAbs(delta.y())) {
                    // 水平直线
                    adjustedEnd = QPoint(pos.x(), startPoint.y());
                } else {
                    // 垂直直线
                    adjustedEnd = QPoint(startPoint.x(), pos.y());
                }
                shapes.last().points.clear(); // 清空当前路径，只保留直线
                shapes.last().points.append(startPoint);
                shapes.last().points.append(adjustedEnd);
            } else {
                // 正常画笔或马赛克
                shapes.last().points.append(pos);
            }
            painter.setPen(QPen(shapes.last().color, shapes.last().width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            if (mode == 3) {
                for (int i = 1; i < shapes.last().points.size(); ++i) {
                    painter.drawLine(shapes.last().points[i - 1], shapes.last().points[i]);
                }
            } else if (mode == 4) {
                painter.setPen(QPen(shapes.last().color, shapes.last().width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                for (int i = 1; i < shapes.last().points.size(); ++i) {
                    painter.drawLine(shapes.last().points[i - 1], shapes.last().points[i]);
                }
            }
            update();
        }
    } else {
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
            cursor = Qt::ArrowCursor;
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
            toolBar->show();
            qDebug() << "EditWindow: Stop dragging selection, toolbar shown";
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
            selectedShape = nullptr;
            updateCanvas();
            update();
            qDebug() << "EditWindow: Shape dragging stopped";
        } else if ((mode == 0 || mode == 1) && isDrawing) {
            if (startPoint != event->pos()) {
                Shape shape;
                shape.type = (mode == 0) ? Rectangle : Ellipse;
                if (mode == 1 && (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                    // 按住 Shift 键时保持正圆
                    int size = qMin(qAbs(event->pos().x() - startPoint.x()), qAbs(event->pos().y() - startPoint.y()));
                    QPoint adjustedEnd = startPoint + QPoint(event->pos().x() > startPoint.x() ? size : -size,
                                                             event->pos().y() > startPoint.y() ? size : -size);
                    shape.rect = QRect(startPoint, adjustedEnd).normalized();
                } else {
                    shape.rect = QRect(startPoint, event->pos()).normalized();
                }
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
    } else if (shape.type == NumberedNote) { // 绘制序号笔记
        painter.setFont(QFont("Arial", shape.width));

        // 绘制 32x32 像素的正方形部分（序号）
        int boxSize = 32; // 固定大小为 32x32 像素
        QPoint topLeft = shape.rect.topLeft(); // 基于 shape.rect 的左上角位置

        // 绘制 32x32 像素的矩形边框（黑色）
        painter.setPen(Qt::black);
        painter.setBrush(Qt::NoBrush); // 空填充，绘制边框
        painter.drawRect(topLeft.x(), topLeft.y(), boxSize, boxSize);

        // 绘制 32x32 像素的红色实心圆，中心点为矩形中心（16,16）
        painter.setBrush(Qt::red);
        painter.setPen(Qt::NoPen); // 无边框
        QPoint center = topLeft + QPoint(boxSize / 2, boxSize / 2); // 矩形中心（16,16）
        painter.drawEllipse(center, boxSize / 2, boxSize / 2); // 直径为 32 像素的圆，完全填充矩形

        // 绘制白色数字，在矩形中心点（16,16）水平垂直居中（数字中点与中心对齐）
        int number = shape.number;
        QString numberText = QString::number(number);
        QFontMetrics fm(QFont("Arial", shape.width));
        int numberWidth = fm.horizontalAdvance(numberText);
        int textHeight = fm.height();
        painter.setPen(Qt::white);
        int verticalOffset = textHeight / 2; // 数字的垂直中点偏移
        int back = verticalOffset;
        verticalOffset -= back;
        verticalOffset -= back;
        verticalOffset = verticalOffset / 2; // 最终 verticalOffset = -textHeight / 4
        QPoint textPos = center - QPoint(numberWidth / 2, verticalOffset); // 水平垂直居中（以中点为基准）
        painter.drawText(textPos, numberText);

        // 绘制气泡框和笔记内容
        if (!shape.bubbleRect.isNull()) {
            // 绘制圆角矩形气泡框
            painter.setBrush(shape.bubbleColor); // 半透明灰色背景（RGBA: 200,200,200,128）
            painter.setPen(QPen(shape.bubbleBorderColor, 1)); // 气泡边框颜色（黑色），宽度 1
            QPainterPath path;
            int radius = 5; // 圆角半径
            path.addRoundedRect(shape.bubbleRect, radius, radius);
            painter.drawPath(path);

            // 绘制笔记内容（文本），在气泡框内居中
            painter.setPen(shape.color);
            QString contentText = shape.text.mid(shape.text.indexOf(". ") + 2); // 移除 "n. " 部分
            painter.drawText(shape.bubbleRect, Qt::AlignCenter | Qt::TextWordWrap, contentText);
        }

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
        } else if (shape.type == Text || shape.type == NumberedNote) {
            if (shape.rect.contains(pos) || (shape.type == NumberedNote && shape.bubbleRect.contains(pos))) {
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
    isAdjustingFromEditMode = false;
    toolBar->show();
    qDebug() << "EditWindow: Mode set to:" << mode << "by external caller, all states reset, toolbar shown";
}

void EditWindow::hideToolBar()
{
    if (toolBar) {
        toolBar->hide();
    }
}

void EditWindow::showToolBar()
{
    if (toolBar) {
        toolBar->show();
    }
}
