#ifndef EDITWINDOW_H
#define EDITWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QMouseEvent>
#include <QList>
#include "common.h"
#include "shape.h"

class ToolBarWindow;

class EditWindow : public QWidget {
    Q_OBJECT

public:
    EditWindow(const QPixmap &screenshot, const QPoint &pos, QWidget *parent = nullptr);
    ~EditWindow();
    void updateScreenshot(const QPixmap &newScreenshot, const QPoint &newPos);
    QPixmap getCanvas() const { return canvas; }
    void setMode(int newMode); // 允许 MainWindow 重置 mode

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void handleDragged(Handle handle, const QPoint &globalPos);
    void handleReleased();

private:
    QPixmap screenshot;
    QPixmap canvas;
    QPixmap drawingLayer;
    QPixmap tempLayer;
    int borderWidth = 3;
    QColor borderColor = Qt::blue;
    Qt::PenStyle borderStyle = Qt::DashLine;
    int handleSize = 16;
    Handle activeHandle = None;
    QPoint dragStartPos;
    bool isDragging = false;
    bool isDragMode = true;
    bool isDraggingSelection = false;
    bool isAdjustingHandle = false; // 中点调整状态
    bool isDrawing = false;         // 绘制状态
    int mode = -1; // -1:无, 0:矩形, 1:圆形, 2:文本, 3:画笔, 4:遮罩
    QList<Shape> shapes;
    Shape *selectedShape = nullptr;
    QPoint startPoint;
    int fontSize = 16;
    QColor textColor = Qt::black;
    int mosaicSize = 10;
    int shapeBorderWidth = 2;
    QColor shapeBorderColor = Qt::black;
    int penWidth = 2;
    QColor penColor = Qt::black;
    ToolBarWindow *toolBar;
    QRect currentRect;

    QRect getHandleRect(Handle handle) const;
    void drawShape(QPainter &painter, const Shape &shape);
    void updateCanvas();
    Shape* hitTest(const QPoint &pos);
    bool isOnBorder(const QRect &rect, const QPoint &pos, int borderWidth = 5);
    bool isOnEllipseBorder(const QRect &rect, const QPoint &pos, int borderWidth);
    int calculateHandleSize() const;
};

#endif // EDITWINDOW_H
