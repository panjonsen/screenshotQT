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
    QPixmap updateSelectionPosition(const QPoint &newPos);
    QRect getSelection() const;
    void resetSelectionState();
    bool isSelectingInitialState() const; // 初始选区状态
    bool isAdjustingSelection() const;    // 中点调整选区状态

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap screenshot;
    QPixmap originalScreenshot;
    QPoint startPoint, endPoint;
    bool isSelectingInitial = false;      // 初始选区状态
    bool isAdjustingSelectionState = false; // 中点调整选区状态（改为新变量名）
    bool isEditing = false;
    int borderWidth = 2;
    QColor borderColor = Qt::red;
    Qt::PenStyle borderStyle = Qt::DashLine;
    MagnifierWindow *magnifier;
    EditWindow *editWindow = nullptr;
    QPoint currentMousePos;
    Handle activeHandle = None;
    QPoint dragStartPos;
    int initialWidth = 0;
    int initialHeight = 0;

    void updateMagnifierPosition();
    void startDragging(Handle handle, const QPoint &globalPos);
};

#endif // MAINWINDOW_H
