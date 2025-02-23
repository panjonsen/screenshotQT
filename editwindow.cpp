#include "editwindow.h"
#include "mainwindow.h"
#include "toolbarwindow.h"
#include "sizedisplaywindow.h"
#include <QPainter>
#include <QDebug>
#include <QInputDialog>
#include <QCursor>
#include <QApplication>
#include <QClipboard>
#include <cmath>
#include <QTimer>
#include <QThread>
#include <QPainterPath>


EditWindow::EditWindow(const QPixmap &screenshot, const QPoint &pos, QWidget *parent)
    : QWidget(parent), screenshot(screenshot), canvas(screenshot), drawingLayer(screenshot.size()), tempLayer(screenshot.size())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SubWindow); // 使用 SubWindow 隐藏任务栏
    setFixedSize(screenshot.size());
    move(pos);
    setMouseTracking(true);
    drawingLayer.fill(Qt::transparent);
    tempLayer.fill(Qt::transparent);
    updateCanvas();
    toolBar = new ToolBarWindow(this, this);
    toolBar->show();

    sizeDisplayWindow = new SizeDisplayWindow(this);
    QString sizeText = QString("%1x%2").arg(screenshot.width()).arg(screenshot.height());
    sizeDisplayWindow->setSizeText(sizeText);
    sizeDisplayWindow->show();
    updateSizeDisplayPosition();

    mode = -1;
    isDragMode = true;

    // 设置虚线海蓝色边框属性
    borderWidth = 1;              // 边框宽度
    borderColor = QColor(0, 105, 148); // 海蓝色
    borderStyle = Qt::DashLine;   // 虚线样式


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
    connect(toolBar, &ToolBarWindow::textFontSizeChanged, this, [this](int size) {
        fontSize = size;
    });
    connect(toolBar, &ToolBarWindow::textColorChanged, this, [this](const QColor &color) {
        textColor = color;
    });
    connect(toolBar, &ToolBarWindow::mosaicSizeChanged, this, [this](int size) {
        mosaicSize = size;
    });
    connect(toolBar, &ToolBarWindow::borderWidthChanged, this, [this](int width) {
        shapeBorderWidth = width;
    });
    connect(toolBar, &ToolBarWindow::borderColorChanged, this, [this](const QColor &color) {
        shapeBorderColor = color;
    });
    connect(toolBar, &ToolBarWindow::penWidthChanged, this, [this](int width) {
        penWidth = width;
    });
    connect(toolBar, &ToolBarWindow::penColorChanged, this, [this](const QColor &color) {
        penColor = color;
    });

    show();
}


EditWindow::~EditWindow()
{
    delete toolBar;
    delete sizeDisplayWindow;
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

    QString sizeText = QString("%1x%2").arg(newScreenshot.width()).arg(newScreenshot.height());
    sizeDisplayWindow->setSizeText(sizeText);
    updateSizeDisplayPosition();

    update();
    qDebug() << "EditWindow: Screenshot updated, size:" << newScreenshot.size() << ", pos:" << newPos;
}

void EditWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, canvas);
    painter.drawPixmap(0, 0, tempLayer);

    // 绘制虚线海蓝色边框
    QPen borderPen(QColor(0, 105, 148), borderWidth, Qt::DashLine); // 海蓝色虚线边框
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    QRect borderRect(0, 0, width() - 1, height() - 1); // 边框矩形，减去1避免超出边界
    painter.drawRect(borderRect);

    // 绘制调整手柄（如果处于拖拽模式）
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
        MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());

        // 检测并启动形状拖拽
        startShapeDragging(pos);

        // 如果没有选中形状，且处于无模式（mode == -1）
        if (!selectedShape && mode == -1) {
            startHandleAdjustment(pos, event);
            if (!isAdjustingHandle) {
                startWindowDragging(event->globalPosition().toPoint());
            }
        }

        // 如果没有调整手柄或拖拽，则开始绘制新形状
        if (mode >= 0 && !isAdjustingHandle && !isDragging && !isDraggingSelection &&
            activeHandle == None && mainWindow && !mainWindow->isAdjustingSelectionState()) {
            startDrawingShape(pos);
        }
    } else if (event->button() == Qt::RightButton) {
        MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
        if (mainWindow) {
            mainWindow->cancelEditing();
            qDebug() << "EditWindow: Right-click detected, returning to initial selection state";
        }
    }
}
void EditWindow::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();

    if ((mode == 0 || mode == 1) && isDrawing && (event->buttons() & Qt::LeftButton)) {
        pos.setX(qBound(borderWidth, pos.x(), width() - borderWidth));
        pos.setY(qBound(borderWidth, pos.y(), height() - borderWidth));
    }

    if (isDraggingSelection && (event->buttons() & Qt::LeftButton)) {
        handleWindowDragging(event);
    } else if (activeHandle != None && (event->buttons() & Qt::LeftButton)) {
        emit handleDragged(activeHandle, event->globalPosition().toPoint());
    } else if (isDragging && (event->buttons() & Qt::LeftButton) && selectedShape) {
        handleShapeDragging(pos);
    } else if (mode >= 0 && (event->buttons() & Qt::LeftButton) && isDrawing) {
        QPainter painter(&tempLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        tempLayer.fill(Qt::transparent);
        drawTemporaryPreview(pos, painter);
    } else {
        updateCursorStyle(pos);
    }
}


void EditWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();

        if ((mode == 0 || mode == 1) && isDrawing) {
            pos.setX(qBound(borderWidth, pos.x(), width() - borderWidth));
            pos.setY(qBound(borderWidth, pos.y(), height() - borderWidth));
        }

        if (isDraggingSelection) {
            stopWindowDragging();
        } else if (activeHandle != None) {
            stopHandleAdjustment();
        } else if (isDragging) {
            stopShapeDragging();
        } else if (mode == 0 || mode == 1 || mode == 3 || mode == 4 || mode == 6) {
            finishDrawingShape(pos);
        }
    }
}
void EditWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        Shape *shape = hitTest(pos);
        if (shape && (shape->type == Text || shape->type == NumberedNote)) {
            QString currentText = shape->type == Text ? shape->text : shape->text.mid(shape->text.indexOf(". ") + 2);
            QString newText = QInputDialog::getMultiLineText(this,
                                                             shape->type == Text ? "编辑文本" : "编辑序号笔记",
                                                             "请输入新文本:",
                                                             currentText);
            if (!newText.isEmpty()) {
                if (shape->type == Text) {
                    shape->text = newText;
                    QFont font("Arial", shape->width);
                    QFontMetrics fm(font);
                    QRect textRect = fm.boundingRect(QRect(0, 0, width() - shape->rect.x(), height() - shape->rect.y()),
                                                     Qt::AlignLeft | Qt::TextWordWrap, newText);
                    shape->rect.setSize(textRect.size());
                } else if (shape->type == NumberedNote) {
                    shape->text = QString("%1. %2").arg(shape->number).arg(newText);
                    QFont font("Arial", shape->width);
                    QFontMetrics fm(font);
                    QString contentText = newText;
                    int textWidth = fm.horizontalAdvance(contentText) + 20;
                    int textHeight = fm.height() * (contentText.count('\n') + 1) + 10;
                    int bubbleX = shape->rect.x() + 34 + 5;
                    int bubbleY = shape->rect.y() - (textHeight - 32) / 2;
                    shape->bubbleRect = QRect(bubbleX, bubbleY, textWidth, textHeight);
                }
                updateCanvas();
                update();
            }
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
    } else if (shape.type == NumberedNote) {
        int fixedFontSize = 16;
        painter.setFont(QFont("Arial", fixedFontSize));
        int boxSize = 32;
        QPoint topLeft = shape.rect.topLeft();
        painter.setBrush(Qt::red);
        painter.setPen(Qt::NoPen);
        QPoint center = topLeft + QPoint(boxSize / 2, boxSize / 2);
        painter.drawEllipse(center, boxSize / 2, boxSize / 2);
        QFontMetrics fmSeq(QFont("Arial", fixedFontSize));
        QString numberText = QString::number(shape.number);
        int numberWidth = fmSeq.horizontalAdvance(numberText);
        int textHeight = fmSeq.height();
        painter.setPen(Qt::white);
        int verticalOffset = textHeight / 4;
        QPoint textPos = center - QPoint(numberWidth / 2, -verticalOffset);
        painter.drawText(textPos, numberText);

        if (!shape.bubbleRect.isNull()) {
            painter.setBrush(shape.bubbleColor);
            painter.setPen(QPen(shape.bubbleBorderColor, 1));
            QPainterPath path;
            int radius = 5;
            path.addRoundedRect(shape.bubbleRect, radius, radius);
            painter.drawPath(path);

            painter.setFont(QFont("Arial", shape.width));
            painter.setPen(shape.color);
            QString contentText = shape.text.mid(shape.text.indexOf(". ") + 2);
            painter.drawText(shape.bubbleRect, Qt::AlignCenter | Qt::TextWordWrap, contentText);
        }
    } else if (shape.type == Pen || shape.type == Mask) {
        painter.setPen(QPen(shape.color, shape.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i < shape.points.size(); ++i) {
            painter.drawLine(shape.points[i - 1], shape.points[i]);
        }
    } else if (shape.type == Arrow) {
        painter.setPen(QPen(shape.color, shape.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (shape.points.size() == 2) {
            QPoint start = shape.points[0];
            QPoint end = shape.points[1];
            painter.drawLine(start, end);
            double angle = atan2(end.y() - start.y(), end.x() - start.x());
            int arrowSize = shape.width * 3;
            QPointF arrowP1 = end - QPointF(cos(angle + M_PI / 6) * arrowSize, sin(angle + M_PI / 6) * arrowSize);
            QPointF arrowP2 = end - QPointF(cos(angle - M_PI / 6) * arrowSize, sin(angle - M_PI / 6) * arrowSize);
            painter.drawLine(end, arrowP1);
            painter.drawLine(end, arrowP2);
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
    int hitBorderWidth = 10;
    for (int i = shapes.size() - 1; i >= 0; --i) {
        Shape &shape = shapes[i];
        if (shape.type == Rectangle) {
            if (isOnBorder(shape.rect, pos, hitBorderWidth)) {
                return &shape;
            }
        } else if (shape.type == Ellipse) {
            if (isOnEllipseBorder(shape.rect, pos, hitBorderWidth)) {
                return &shape;
            }
        } else if (shape.type == Text || shape.type == NumberedNote) {
            if (shape.rect.contains(pos)) {
                return &shape;
            }
            if (!shape.bubbleRect.isNull() && shape.bubbleRect.contains(pos)) {
                return &shape;
            }
        } else if (shape.type == Arrow) {
            if (shape.points.size() == 2) {
                QPoint start = shape.points[0];
                QPoint end = shape.points[1];
                QRect startRect(start - QPoint(hitBorderWidth, hitBorderWidth), QSize(hitBorderWidth * 2, hitBorderWidth * 2));
                QRect endRect(end - QPoint(hitBorderWidth, hitBorderWidth), QSize(hitBorderWidth * 2, hitBorderWidth * 2));

                if (startRect.contains(pos)) {
                    shape.offset = QPoint(0, 0);
                    return &shape;
                } else if (endRect.contains(pos)) {
                    shape.offset = QPoint(1, 1);
                    return &shape;
                }

                QLineF line(start, end);
                QLineF normal = line.normalVector();
                normal.setLength(hitBorderWidth);
                QPolygonF hitArea;
                hitArea << (start + normal.p2() - normal.p1())
                        << (start - normal.p2() + normal.p1())
                        << (end - normal.p2() + normal.p1())
                        << (end + normal.p2() - normal.p1());
                if (hitArea.containsPoint(pos, Qt::OddEvenFill)) {
                    shape.offset = pos - start;
                    return &shape;
                }
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

void EditWindow::updateSizeDisplayPosition()
{
    QPoint editPos = pos();
    int margin = 5;
    int x = editPos.x() - sizeDisplayWindow->width() - margin;
    int y = editPos.y() - sizeDisplayWindow->height() - margin;

    QRect screenRect = screen()->geometry();
    x = qMax(screenRect.left(), x);
    y = qMax(screenRect.top(), y);

    sizeDisplayWindow->move(x, y);
}

void EditWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    updateSizeDisplayPosition();
}

void EditWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    sizeDisplayWindow->show();
    updateSizeDisplayPosition();
}

void EditWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    sizeDisplayWindow->hide();
}


void EditWindow::startShapeDragging(const QPoint &pos)
{
    selectedShape = hitTest(pos);
    if (selectedShape) {
        isDragging = true;
        if (selectedShape->type == NumberedNote && selectedShape->bubbleRect.contains(pos)) {
            dragStartPos = pos;
            selectedShape->offset = pos - selectedShape->bubbleRect.topLeft();
        } else if (selectedShape->type == Arrow) {
            dragStartPos = pos;
        } else {
            dragStartPos = pos;
            selectedShape->offset = pos - selectedShape->rect.topLeft();
        }
        qDebug() << "EditWindow: Dragging shape at:" << pos << ", mode:" << mode << ", offset:" << selectedShape->offset;
    }
}
void EditWindow::startHandleAdjustment(const QPoint &pos, QMouseEvent *event)
{
    int dynamicHandleSize = calculateHandleSize();
    bool hitHandle = false;
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
            hitHandle = true;
            break;
        }
    }
}

void EditWindow::startWindowDragging(const QPoint &globalPos)
{
    if (isDragMode && !isAdjustingHandle) {
        dragStartPos = globalPos;
        isDraggingSelection = true;
        toolBar->hide();
        qDebug() << "EditWindow: Start dragging selection at:" << dragStartPos;
    }
}



void EditWindow::startDrawingShape(const QPoint &pos)
{
    startPoint = pos;
    isDrawing = true;
    qDebug() << "EditWindow: Start drawing at:" << pos << ", mode:" << mode;

    if (mode == 2) { // 文本
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
    } else if (mode == 5) { // 序号笔记
        QString text = QInputDialog::getMultiLineText(this, "输入序号笔记", "请输入笔记内容:");
        if (!text.isEmpty()) {
            Shape shape;
            shape.type = NumberedNote;
            QFont font("Arial", fontSize);
            QString numberedText = QString("%1. %2").arg(noteNumber).arg(text);
            shape.rect = QRect(pos, QSize(32, 32));
            shape.text = numberedText;
            shape.color = textColor;
            shape.width = fontSize;
            shape.number = noteNumber;
            shape.bubbleColor = QColor(200, 200, 200, 128);
            shape.bubbleBorderColor = Qt::black;

            QFontMetrics fm(font);
            QString contentText = text;
            int textWidth = fm.horizontalAdvance(contentText) + 20;
            int textHeight = fm.height() * (contentText.count('\n') + 1) + 10;
            int bubbleX = pos.x() + 34 + 5;
            int bubbleY = pos.y() - (textHeight - 32) / 2;
            shape.bubbleRect = QRect(bubbleX, bubbleY, textWidth, textHeight);

            shapes.append(shape);
            noteNumber++;
            updateCanvas();
            update();
            isDrawing = false;
        }
    } else if (mode == 3 || mode == 4) { // 画笔或遮罩
        Shape shape;
        shape.type = (mode == 3) ? Pen : Mask;
        shape.points.append(pos);
        shape.color = (mode == 3) ? penColor : Qt::gray;
        shape.width = (mode == 3) ? penWidth : mosaicSize;
        shapes.append(shape);
        updateCanvas();
        update();
    } else if (mode == 6) { // 箭头
        Shape shape;
        shape.type = Arrow;
        shape.points.append(pos);
        shape.color = shapeBorderColor;
        shape.width = shapeBorderWidth;
        shapes.append(shape);
        qDebug() << "EditWindow: Arrow shape created at:" << pos;
    }
}


void EditWindow::handleWindowDragging(QMouseEvent *event)
{
    QPoint globalOffset = event->globalPosition().toPoint() - dragStartPos;
    QPoint newPos = this->pos() + globalOffset;

    QRect screenRect = screen()->geometry();
    int maxX = screenRect.width() - width();
    int maxY = screenRect.height() - height();
    newPos.setX(qBound(0, newPos.x(), maxX));
    newPos.setY(qBound(0, newPos.y(), maxY));

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        QSize originalSize = size();
        QPixmap newScreenshot = mainWindow->updateSelectionPosition(newPos);
        updateScreenshot(newScreenshot, newPos);
        setFixedSize(originalSize);
    }

    dragStartPos = event->globalPosition().toPoint();
    qDebug() << "EditWindow: Dragging selection to:" << newPos << ", size:" << size();
}


void EditWindow::handleShapeDragging(const QPoint &pos)
{
    QPoint offset = pos - dragStartPos;
    if (selectedShape->type == Arrow && selectedShape->points.size() == 2) {
        if (selectedShape->offset == QPoint(0, 0)) {
            selectedShape->points[0] = pos;
        } else if (selectedShape->offset == QPoint(1, 1)) {
            selectedShape->points[1] = pos;
        } else {
            QPoint oldStart = selectedShape->points[0];
            QPoint oldEnd = selectedShape->points[1];
            selectedShape->points[0] = oldStart + offset;
            selectedShape->points[1] = oldEnd + offset;
            selectedShape->points[0].setX(qBound(0, selectedShape->points[0].x(), width()));
            selectedShape->points[0].setY(qBound(0, selectedShape->points[0].y(), height()));
            selectedShape->points[1].setX(qBound(0, selectedShape->points[1].x(), width()));
            selectedShape->points[1].setY(qBound(0, selectedShape->points[1].y(), height()));
        }
        drawingLayer.fill(Qt::transparent);
        updateCanvas();
        update();
        dragStartPos = pos;
        qDebug() << "EditWindow: Dragging arrow, start:" << selectedShape->points[0] << ", end:" << selectedShape->points[1];
    } else if (selectedShape->type == NumberedNote && !selectedShape->bubbleRect.isNull()) {
        QPoint bubbleOffset = selectedShape->bubbleRect.topLeft() - selectedShape->rect.topLeft();
        int rectWidth = selectedShape->rect.width();
        int rectHeight = selectedShape->rect.height();
        QRect combinedRect = selectedShape->rect | selectedShape->bubbleRect;
        int combinedWidth = combinedRect.width();
        int combinedHeight = combinedRect.height();

        QPoint newRectTopLeft = selectedShape->rect.topLeft() + offset;
        newRectTopLeft.setX(qBound(borderWidth, newRectTopLeft.x(), width() - combinedWidth - borderWidth - 1));
        newRectTopLeft.setY(qBound(borderWidth, newRectTopLeft.y(), height() - combinedHeight - borderWidth - 1));

        selectedShape->rect.moveTo(newRectTopLeft);
        selectedShape->bubbleRect.moveTo(newRectTopLeft + bubbleOffset);

        updateCanvas();
        update();
        dragStartPos = pos;
        qDebug() << "EditWindow: Dragging NumberedNote, rect:" << selectedShape->rect << ", bubbleRect:" << selectedShape->bubbleRect;
    } else if (selectedShape->type == Rectangle || selectedShape->type == Ellipse) {
        int rectWidth = selectedShape->rect.width();
        int rectHeight = selectedShape->rect.height();
        QPoint newRectTopLeft = selectedShape->rect.topLeft() + offset;
        newRectTopLeft.setX(qBound(borderWidth, newRectTopLeft.x(), width() - rectWidth - borderWidth - 1));
        newRectTopLeft.setY(qBound(borderWidth, newRectTopLeft.y(), height() - rectHeight - borderWidth - 1));
        selectedShape->rect.moveTo(newRectTopLeft);
        updateCanvas();
        update();
        dragStartPos = pos;
    } else {
        int rectWidth = selectedShape->rect.width();
        int rectHeight = selectedShape->rect.height();
        int borderAdjustment = (selectedShape->type == Rectangle || selectedShape->type == Ellipse) ? shapeBorderWidth : 0;
        int halfBorder = borderAdjustment / 2;
        QPoint newRectTopLeft = selectedShape->rect.topLeft() + offset;
        newRectTopLeft.setX(qBound(halfBorder, newRectTopLeft.x(), width() - rectWidth - halfBorder));
        newRectTopLeft.setY(qBound(halfBorder, newRectTopLeft.y(), height() - rectHeight - halfBorder));
        selectedShape->rect.moveTo(newRectTopLeft);
        updateCanvas();
        update();
        dragStartPos = pos;
    }
    qDebug() << "EditWindow: Moving shape with offset:" << offset << ", new rect pos:" << selectedShape->rect.topLeft();
}

void EditWindow::drawTemporaryPreview(const QPoint &pos, QPainter &painter)
{
    if (mode == 0 || mode == 1) {
        QPoint endPoint = pos;
        endPoint.setX(qBound(borderWidth, endPoint.x(), width() - borderWidth));
        endPoint.setY(qBound(borderWidth, endPoint.y(), height() - borderWidth));

        int left = qMin(startPoint.x(), endPoint.x());
        int top = qMin(startPoint.y(), endPoint.y());
        int right = qMax(startPoint.x(), endPoint.x());
        int bottom = qMax(startPoint.y(), endPoint.y());

        left = qMax(borderWidth, left);
        top = qMax(borderWidth, top);
        right = qMin(right, width() - borderWidth - 1);
        bottom = qMin(bottom, height() - borderWidth - 1);

        currentRect.setCoords(left, top, right, bottom);

        painter.setPen(QPen(shapeBorderColor, shapeBorderWidth));
        painter.setBrush(Qt::NoBrush);
        if (mode == 0) {
            painter.drawRect(currentRect);
        } else if (mode == 1) {
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                int size = qMin(currentRect.width(), currentRect.height());
                if (endPoint.x() < startPoint.x()) {
                    currentRect.setLeft(currentRect.right() - size);
                } else {
                    currentRect.setRight(currentRect.left() + size);
                }
                if (endPoint.y() < startPoint.y()) {
                    currentRect.setTop(currentRect.bottom() - size);
                } else {
                    currentRect.setBottom(currentRect.top() + size);
                }
                if (currentRect.right() > width() - borderWidth - 1) {
                    currentRect.setRight(width() - borderWidth - 1);
                    currentRect.setLeft(currentRect.right() - size);
                }
                if (currentRect.bottom() > height() - borderWidth - 1) {
                    currentRect.setBottom(height() - borderWidth - 1);
                    currentRect.setTop(currentRect.bottom() - size);
                }
            }
            painter.drawEllipse(currentRect);
        }
        update();
    } else if ((mode == 3 || mode == 4 || mode == 6) && isDrawing) {
        Shape *currentShape = nullptr;
        if (mode == 3 || mode == 4) {
            if (!shapes.isEmpty() && (shapes.last().type == Pen || shapes.last().type == Mask)) {
                currentShape = &shapes.last();
            }
        } else if (mode == 6) {
            if (!shapes.isEmpty() && shapes.last().type == Arrow) {
                currentShape = &shapes.last();
                if (currentShape->points.size() == 1) {
                    currentShape->points.append(pos);
                } else if (currentShape->points.size() == 2) {
                    currentShape->points[1] = pos;
                }
            }
        }

        if (currentShape) {
            if (mode == 3 || mode == 4) {
                if (QApplication::keyboardModifiers() & Qt::ShiftModifier && mode == 3) {
                    QPoint delta = pos - startPoint;
                    QPoint adjustedEnd;
                    if (qAbs(delta.x()) > qAbs(delta.y())) {
                        adjustedEnd = QPoint(pos.x(), startPoint.y());
                    } else {
                        adjustedEnd = QPoint(startPoint.x(), pos.y());
                    }
                    if (currentShape->points.size() > 1) {
                        currentShape->points[1] = adjustedEnd;
                    } else {
                        currentShape->points.append(adjustedEnd);
                    }
                } else {
                    currentShape->points.append(pos);
                }
            }

            painter.setPen(QPen(currentShape->color, currentShape->width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            if (mode == 3 || mode == 4) {
                for (int i = 1; i < currentShape->points.size(); ++i) {
                    painter.drawLine(currentShape->points[i - 1], currentShape->points[i]);
                }
            } else if (mode == 6) {
                if (currentShape->points.size() == 2) {
                    QPoint start = currentShape->points[0];
                    QPoint end = currentShape->points[1];
                    painter.drawLine(start, end);
                    double angle = atan2(end.y() - start.y(), end.x() - start.x());
                    int arrowSize = currentShape->width * 3;
                    QPointF arrowP1 = end - QPointF(cos(angle + M_PI / 6) * arrowSize, sin(angle + M_PI / 6) * arrowSize);
                    QPointF arrowP2 = end - QPointF(cos(angle - M_PI / 6) * arrowSize, sin(angle - M_PI / 6) * arrowSize);
                    painter.drawLine(end, arrowP1);
                    painter.drawLine(end, arrowP2);
                }
            }
            update();
        }
    }
}




void EditWindow::updateCursorStyle(const QPoint &pos)
{
    QCursor cursor;
    Shape *hoveredShape = hitTest(pos);
    int hitBorderWidth = 10;
    if (hoveredShape) {
        if (hoveredShape->type == Arrow && hoveredShape->points.size() == 2) {
            QRect startRect(hoveredShape->points[0] - QPoint(hitBorderWidth, hitBorderWidth), QSize(hitBorderWidth * 2, hitBorderWidth * 2));
            QRect endRect(hoveredShape->points[1] - QPoint(hitBorderWidth, hitBorderWidth), QSize(hitBorderWidth * 2, hitBorderWidth * 2));
            if (startRect.contains(pos) || endRect.contains(pos)) {
                cursor = Qt::CrossCursor;
            } else {
                cursor = Qt::SizeAllCursor;
            }
        } else {
            cursor = Qt::SizeAllCursor;
        }
    } else if (mode == 0 || mode == 1 || mode == 3 || mode == 6) {
        cursor = Qt::ArrowCursor;
    } else if (mode == -1 && isDragMode) {
        cursor = Qt::OpenHandCursor;
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
    }
    setCursor(cursor);
}


void EditWindow::stopWindowDragging()
{
    isDraggingSelection = false;
    toolBar->show();
    toolBar->adjustPosition();
    qDebug() << "EditWindow: Stop dragging selection, toolbar shown and repositioned";
}

void EditWindow::stopHandleAdjustment()
{
    emit handleReleased();
    activeHandle = None;
    isAdjustingHandle = false;
    isAdjustingFromEditMode = false;
    isDrawing = false;
    isDragging = false;
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        mainWindow->resetSelectionState();
    }
    toolBar->show();
    qDebug() << "EditWindow: Handle released, all states reset";
    update();
}

void EditWindow::stopShapeDragging()
{
    isDragging = false;
    selectedShape = nullptr;
    updateCanvas();
    update();
    qDebug() << "EditWindow: Shape dragging stopped";
}

void EditWindow::finishDrawingShape(const QPoint &pos)
{
    if ((mode == 0 || mode == 1) && isDrawing) {
        if (startPoint != pos) {
            Shape shape;
            shape.type = (mode == 0) ? Rectangle : Ellipse;

            int left = qMin(startPoint.x(), pos.x());
            int top = qMin(startPoint.y(), pos.y());
            int right = qMax(startPoint.x(), pos.x());
            int bottom = qMax(startPoint.y(), pos.y());

            left = qMax(borderWidth, left);
            top = qMax(borderWidth, top);
            right = qMin(right, width() - borderWidth - 1);
            bottom = qMin(bottom, height() - borderWidth - 1);

            shape.rect.setCoords(left, top, right, bottom);

            if (mode == 1 && (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                int size = qMin(shape.rect.width(), shape.rect.height());
                if (pos.x() < startPoint.x()) {
                    shape.rect.setLeft(shape.rect.right() - size);
                } else {
                    shape.rect.setRight(shape.rect.left() + size);
                }
                if (pos.y() < startPoint.y()) {
                    shape.rect.setTop(shape.rect.bottom() - size);
                } else {
                    shape.rect.setBottom(shape.rect.top() + size);
                }
                if (shape.rect.right() > width() - borderWidth - 1) {
                    shape.rect.setRight(width() - borderWidth - 1);
                    shape.rect.setLeft(shape.rect.right() - size);
                }
                if (shape.rect.bottom() > height() - borderWidth - 1) {
                    shape.rect.setBottom(height() - borderWidth - 1);
                    shape.rect.setTop(shape.rect.bottom() - size);
                }
            }

            shape.width = shapeBorderWidth;
            shape.color = shapeBorderColor;
            shapes.append(shape);
        }
        tempLayer.fill(Qt::transparent);
        updateCanvas();
        update();
        isDrawing = false;
    } else if (mode == 3 || mode == 4 || mode == 6) {
        tempLayer.fill(Qt::transparent);
        updateCanvas();
        update();
        isDrawing = false;
        qDebug() << "EditWindow: Mode 3/4/6 completed, mode:" << mode << ", isDrawing:" << isDrawing;
    }
}
