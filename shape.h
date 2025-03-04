#ifndef SHAPE_H
#define SHAPE_H

#include <QPoint>
#include <QRect>
#include <QColor>

enum ShapeType { Rectangle, Ellipse, Text, Pen, Mask, NumberedNote, Arrow }; // 添加 Arrow 类型

struct Shape {
    ShapeType type;
    QRect rect; // 用于矩形、圆形、文本、序号笔记、箭头的位置和大小
    QList<QPoint> points; // 用于画笔的路径（箭头也可以用）
    QString text; // 用于文本和序号笔记的内容
    int width; // 边框或字体大小
    QColor color; // 颜色
    QPoint offset; // 拖动时的偏移量
    int number; // 序号（仅用于 NumberedNote）
    QRect bubbleRect; // 气泡框的矩形区域（仅用于 NumberedNote）
    QColor bubbleColor; // 气泡框背景颜色（默认白色）
    QColor bubbleBorderColor; // 气泡框边框颜色（默认黑色）

    Shape() : type(Rectangle), width(2), color(Qt::black), number(0), bubbleColor(Qt::white), bubbleBorderColor(Qt::black) {}
};

#endif // SHAPE_H
