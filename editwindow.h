#ifndef EDITWINDOW_H
#define EDITWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QMouseEvent>
#include "common.h"

class EditWindow : public QWidget {
    Q_OBJECT

public:
    EditWindow(const QPixmap &screenshot, const QPoint &pos, QWidget *parent = nullptr);
    ~EditWindow();
    void updateScreenshot(const QPixmap &newScreenshot, const QPoint &newPos);

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
    int borderWidth = 3;
    QColor borderColor = Qt::blue;
    Qt::PenStyle borderStyle = Qt::DashLine;
    int handleSize = 16;            // 增大中点尺寸，从 10 改为 16
    Handle activeHandle = None;
    QPoint dragStartPos;
    bool isDragging = false;        // 标记是否拖动整个选区（调整手柄）
    bool isDragMode = true;         // 默认处于拖动模式
    bool isDraggingSelection = false; // 标记是否正在拖动选区位置

    QRect getHandleRect(Handle handle) const;
};

#endif // EDITWINDOW_H
