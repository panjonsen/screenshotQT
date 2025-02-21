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
    void updateScreenshot(const QPixmap &newScreenshot, const QPoint &newPos); // 新增更新方法

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
    int handleSize = 10;
    Handle activeHandle = None;
    QPoint dragStartPos;

    QRect getHandleRect(Handle handle) const;
};

#endif // EDITWINDOW_H
