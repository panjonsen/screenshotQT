#ifndef SHAPE_H
#define SHAPE_H

#include <QPoint>
#include <QRect>
#include <QColor>

enum ShapeType { Rectangle, Ellipse, Text, Pen, Mask };

struct Shape {
    ShapeType type;
    QRect rect; // 用于矩形、圆形、文本和遮罩的位置和大小
    QList<QPoint> points; // 用于画笔的路径
    QString text; // 用于文本内容
    int width; // 边框或画笔粗细
    QColor color; // 颜色
    QPoint offset; // 拖动时的偏移量

    Shape() : type(Rectangle), width(2), color(Qt::black) {}
};

#endif // SHAPE_H
