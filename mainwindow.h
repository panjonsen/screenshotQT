#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>
#include <QMouseEvent>
#include <QPainter>
#include "magnifierwindow.h"
#include "editwindow.h"
#include "common.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap screenshot;
    QPixmap originalScreenshot;
    QPoint startPoint, endPoint;
    bool isSelecting = false;
    bool isEditing = false;
    int borderWidth = 2;
    QColor borderColor = Qt::red;
    Qt::PenStyle borderStyle = Qt::DashLine;
    MagnifierWindow *magnifier;
    EditWindow *editWindow = nullptr;
    QPoint currentMousePos;
    Handle activeHandle = None;

    void updateMagnifierPosition();
    void startDragging(Handle handle, const QPoint &globalPos);
};

#endif // MAINWINDOW_H
