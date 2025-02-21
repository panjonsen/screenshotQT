#ifndef MAGNIFIERWINDOW_H
#define MAGNIFIERWINDOW_H

#include <QWidget>
#include <QPixmap>

class MagnifierWindow : public QWidget {
    Q_OBJECT

public:
    MagnifierWindow(const QPixmap &screenshot, QWidget *parent = nullptr);
    void updatePosition(const QPoint &pos);
    void setScreenshot(const QPixmap &screenshot);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap originalScreenshot; // 原始截图
    QPoint currentMousePos;     // 当前鼠标位置
    int magnifierSize = 100;    // 放大镜大小
    int zoomFactor = 3;         // 放大倍数
};

#endif // MAGNIFIERWINDOW_H
