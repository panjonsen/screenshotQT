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
    update(); // 触发重绘，确保尺寸更新
    qDebug() << "EditWindow: Screenshot updated, size:" << newScreenshot.size() << ", pos:" << newPos;
}

void EditWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, canvas);
    painter.drawPixmap(0, 0, tempLayer);

    // 显示当前选区尺寸
    painter.setFont(QFont("Arial", 14));
    painter.setPen(Qt::red); // 使用红色以突出显示
    QString sizeText = QString("%1x%2").arg(width()).arg(height());
    painter.drawText(10, 20, sizeText); // 左上角位置 (10, 20)

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

        // 优先检查是否点击了绘制物
        selectedShape = hitTest(pos);
        if (selectedShape) {
            isDragging = true;
            if (selectedShape->type == NumberedNote && selectedShape->bubbleRect.contains(pos)) {
                dragStartPos = pos;
                selectedShape->offset = pos - selectedShape->bubbleRect.topLeft();
            } else {
                dragStartPos = pos;
                selectedShape->offset = pos - selectedShape->rect.topLeft();
            }
            qDebug() << "EditWindow: Dragging shape at:" << pos << ", mode:" << mode;
            return; // 点击绘制物后直接返回
        }

        // 小手模式下的中点调整和选区拖动逻辑
        if (mode == -1) {
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

            // 只有未点击中点且不在调整状态时，才允许选区拖动
            if (!hitHandle && isDragMode && !isAdjustingHandle) {
                dragStartPos = event->globalPosition().toPoint();
                isDraggingSelection = true;
                toolBar->hide();
                qDebug() << "EditWindow: Start dragging selection at:" << dragStartPos;
                return;
            }

            if (hitHandle) {
                return; // 点击中点后直接返回
            }
        }

        // 绘制模式的逻辑
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
            QSize originalSize = size(); // 记录拖动前的尺寸
            QPixmap newScreenshot = mainWindow->updateSelectionPosition(newPos);
            updateScreenshot(newScreenshot, newPos);
            setFixedSize(originalSize); // 强制保持原始尺寸
        }

        dragStartPos = event->globalPosition().toPoint();
        qDebug() << "EditWindow: Dragging selection to:" << newPos << ", size:" << size();
    } else if (activeHandle != None && (event->buttons() & Qt::LeftButton)) {
        emit handleDragged(activeHandle, event->globalPosition().toPoint());
    } else if (isDragging && (event->buttons() & Qt::LeftButton) && selectedShape) {
        QPoint offset = pos - dragStartPos;
        QPoint oldRectTopLeft = selectedShape->rect.topLeft();
        QPoint oldBubbleTopLeft = selectedShape->bubbleRect.topLeft();
        QPoint newRectTopLeft = oldRectTopLeft + offset;

        if (selectedShape->type == NumberedNote && !selectedShape->bubbleRect.isNull()) {
            QRect combinedRect = selectedShape->rect | selectedShape->bubbleRect;
            int combinedWidth = combinedRect.width();
            int combinedHeight = combinedRect.height();
            QPoint newCombinedTopLeft = newRectTopLeft - (selectedShape->rect.topLeft() - combinedRect.topLeft());
            newCombinedTopLeft.setX(qBound(0, newCombinedTopLeft.x(), width() - combinedWidth));
            newCombinedTopLeft.setY(qBound(0, newCombinedTopLeft.y(), height() - combinedHeight));
            QPoint rectOffset = selectedShape->rect.topLeft() - combinedRect.topLeft();
            selectedShape->rect.moveTo(newCombinedTopLeft + rectOffset);
            QPoint bubbleOffset = oldBubbleTopLeft - oldRectTopLeft;
            selectedShape->bubbleRect.moveTo(selectedShape->rect.topLeft() + bubbleOffset);
        } else {
            int rectWidth = selectedShape->rect.width();
            int rectHeight = selectedShape->rect.height();
            int borderAdjustment = (selectedShape->type == Rectangle || selectedShape->type == Ellipse) ? shapeBorderWidth : 0;
            int halfBorder = borderAdjustment / 2;
            newRectTopLeft.setX(qBound(halfBorder, newRectTopLeft.x(), width() - rectWidth - halfBorder));
            newRectTopLeft.setY(qBound(halfBorder, newRectTopLeft.y(), height() - rectHeight - halfBorder));
            selectedShape->rect.moveTo(newRectTopLeft);
        }

        dragStartPos = pos;
        updateCanvas();
        update();
        qDebug() << "EditWindow: Moving shape with offset:" << offset << ", new rect pos:" << selectedShape->rect.topLeft();
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
                QPoint delta = pos - startPoint;
                QPoint adjustedEnd;
                if (qAbs(delta.x()) > qAbs(delta.y())) {
                    adjustedEnd = QPoint(pos.x(), startPoint.y());
                } else {
                    adjustedEnd = QPoint(startPoint.x(), pos.y());
                }
                shapes.last().points.clear();
                shapes.last().points.append(startPoint);
                shapes.last().points.append(adjustedEnd);
            } else {
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
        int hitBorderWidth = 10;
        if (hoveredShape) {
            cursor = Qt::SizeAllCursor;
        } else if (mode == 0 || mode == 1 || mode == 3) {
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
            isAdjustingFromEditMode = false; // 重置中点调整标志
            isDrawing = false;
            isDragging = false;
            MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
            if (mainWindow) {
                mainWindow->resetSelectionState(); // 确保 MainWindow 状态同步
            }
            toolBar->show();
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
                    // 更新文本形状
                    shape->text = newText;
                    QFont font("Arial", shape->width);
                    QFontMetrics fm(font);
                    QRect textRect = fm.boundingRect(QRect(0, 0, width() - shape->rect.x(), height() - shape->rect.y()),
                                                     Qt::AlignLeft | Qt::TextWordWrap, newText);
                    shape->rect.setSize(textRect.size());
                } else if (shape->type == NumberedNote) {
                    // 更新序号笔记，只修改笔记内容
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
        // 绘制序号部分（固定大小）
        int fixedFontSize = 16; // 序号固定字体大小
        painter.setFont(QFont("Arial", fixedFontSize));
        int boxSize = 32; // 固定正方形大小
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
        int verticalOffset = textHeight / 4; // 调整垂直居中
        QPoint textPos = center - QPoint(numberWidth / 2, -verticalOffset);
        painter.drawText(textPos, numberText);

        // 绘制气泡框和笔记内容（动态大小）
        if (!shape.bubbleRect.isNull()) {
            painter.setBrush(shape.bubbleColor);
            painter.setPen(QPen(shape.bubbleBorderColor, 1));
            QPainterPath path;
            int radius = 5;
            path.addRoundedRect(shape.bubbleRect, radius, radius);
            painter.drawPath(path);

            // 只绘制笔记内容，使用动态 fontSize
            painter.setFont(QFont("Arial", shape.width));
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
    int hitBorderWidth = 10; // 隐式加宽触发区域，从 5 增加到 10
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
    double tolerance = borderWidth / a; // 根据宽度调整容差
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
