#include "toolbarwindow.h"
#include "editwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QCoreApplication>
#include <QPainter>

ToolBarWindow::ToolBarWindow(EditWindow *editWindow, QWidget *parent)
    : QWidget(parent), editWindow(editWindow)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setFixedWidth(420);
    setupUI();
    adjustPosition();
    adjustHeight();
    setAttribute(Qt::WA_TranslucentBackground);

    // 默认设置为拖拽模式
    setActiveButton(dragButton);
    emit modeChanged(-1);
    emit dragModeChanged(true);
}

void ToolBarWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // 更新 buttonStyle，添加 color 属性
    QString buttonStyle = "QPushButton { "
                          "border: 1px solid #A0A0A0; "
                          "border-radius: 5px; "
                          "background-color: #FFFFFF; "
                          "font-size: 14px; "
                          "padding: 2px; "
                          "color: #000000; }"
                          "QPushButton:hover { background-color: #E0E0E0; }"
                          "QPushButton:pressed { background-color: #D0D0D0; }";

    // 定义滑块样式表
    QString sliderStyle = "QSlider::groove:horizontal { "
                          "background: #D0D0D0; "
                          "height: 6px; "
                          "border-radius: 3px; }"
                          "QSlider::handle:horizontal { "
                          "background: #FFFFFF; "
                          "border: 1px solid #A0A0A0; "
                          "width: 12px; "
                          "margin: -3px 0; "
                          "border-radius: 6px; }";

    rectButton = new QPushButton(this);
    rectButton->setFixedSize(30, 30);
    rectButton->setStyleSheet(buttonStyle + "QPushButton { border: 2px solid black; }");
    rectButton->setToolTip("绘制矩形");
    connect(rectButton, &QPushButton::clicked, [this]() {
        setActiveButton(rectButton);
        emit modeChanged(0);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->show(); penSettings->hide(); show();
        adjustHeight();
    });

    circleButton = new QPushButton(this);
    circleButton->setFixedSize(30, 30);
    circleButton->setStyleSheet(buttonStyle + "QPushButton { border: 2px solid black; border-radius: 15px; }");
    circleButton->setToolTip("绘制圆形");
    connect(circleButton, &QPushButton::clicked, [this]() {
        setActiveButton(circleButton);
        emit modeChanged(1);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->show(); penSettings->hide(); show();
        adjustHeight();
    });

    textButton = new QPushButton("A", this);
    textButton->setFixedSize(30, 30);
    textButton->setStyleSheet(buttonStyle);
    textButton->setToolTip("添加文本");
    connect(textButton, &QPushButton::clicked, [this]() {
        setActiveButton(textButton);
        emit modeChanged(2);
        hide(); textSettings->show(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    penButton = new QPushButton("✏️", this);
    penButton->setFixedSize(30, 30);
    penButton->setStyleSheet(buttonStyle +
                             "QPushButton { "
                             "font-size: 18px; "
                             "}");
    penButton->setToolTip("画笔工具");
    connect(penButton, &QPushButton::clicked, [this]() {
        setActiveButton(penButton);
        emit modeChanged(3);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->show(); show();
        adjustHeight();
    });

    mosaicButton = new QPushButton(this);
    mosaicButton->setFixedSize(30, 30);
    QPixmap mosaicIcon(20, 20);
    mosaicIcon.fill(Qt::transparent);
    QPainter mosaicPainter(&mosaicIcon);
    mosaicPainter.setPen(Qt::NoPen);
    mosaicPainter.setBrush(QColor("#696969"));
    mosaicPainter.drawRect(0, 0, 10, 10);
    mosaicPainter.setBrush(QColor("#D3D3D3"));
    mosaicPainter.drawRect(10, 0, 10, 10);
    mosaicPainter.setBrush(QColor("#D3D3D3"));
    mosaicPainter.drawRect(0, 10, 10, 10);
    mosaicPainter.setBrush(QColor("#696969"));
    mosaicPainter.drawRect(10, 10, 10, 10);
    mosaicPainter.end();
    mosaicButton->setIcon(mosaicIcon);
    mosaicButton->setIconSize(QSize(20, 20));
    mosaicButton->setStyleSheet(buttonStyle);
    mosaicButton->setToolTip("马赛克工具");
    connect(mosaicButton, &QPushButton::clicked, [this]() {
        setActiveButton(mosaicButton);
        emit modeChanged(4);
        hide(); textSettings->hide(); mosaicSettings->show(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    numberNoteButton = new QPushButton("1", this);
    numberNoteButton->setFixedSize(30, 30);
    numberNoteButton->setStyleSheet(buttonStyle +
                                    "QPushButton { "
                                    "border: 1px solid #A0A0A0; "
                                    "border-radius: 15px; "
                                    "font-size: 16px; "
                                    "}");
    numberNoteButton->setToolTip("添加序号笔记");
    connect(numberNoteButton, &QPushButton::clicked, [this]() {
        setActiveButton(numberNoteButton);
        emit modeChanged(5);
        hide(); textSettings->show(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    arrowButton = new QPushButton("➡", this);
    arrowButton->setFixedSize(30, 30);
    arrowButton->setStyleSheet(buttonStyle +
                               "QPushButton { "
                               "font-size: 18px; "
                               "}");
    arrowButton->setToolTip("绘制箭头");
    connect(arrowButton, &QPushButton::clicked, [this]() {
        setActiveButton(arrowButton);
        emit modeChanged(6);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->show(); penSettings->hide(); show();
        adjustHeight();
    });

    dragButton = new QPushButton("✋", this);
    dragButton->setFixedSize(30, 30);
    dragButton->setStyleSheet(buttonStyle +
                              "QPushButton { "
                              "font-size: 18px; "
                              "}");
    dragButton->setToolTip("拖动模式");
    connect(dragButton, &QPushButton::clicked, [this]() {
        setActiveButton(dragButton);
        emit modeChanged(-1);
        emit dragModeChanged(true);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

#ifdef Q_OS_WIN
    undoButton = new QPushButton("⮌", this); // Windows 使用 U+2B8C
#else
    undoButton = new QPushButton("⏪", this); // macOS 使用 U+23EA
#endif
    undoButton->setFixedSize(50, 30);
    undoButton->setStyleSheet(buttonStyle +
                              "QPushButton { "
                              "font-size: 18px; "
                              "}");
    undoButton->setToolTip("撤销上一步");
    connect(undoButton, &QPushButton::clicked, this, &ToolBarWindow::undoRequested);

    finishButton = new QPushButton("✅", this);
    finishButton->setFixedSize(50, 30);
    finishButton->setStyleSheet(buttonStyle +
                                "QPushButton { "
                                "font-size: 18px; "
                                "}");
    finishButton->setToolTip("完成并保存");
    connect(finishButton, &QPushButton::clicked, this, &ToolBarWindow::finishRequested);

    cancelButton = new QPushButton("❌", this);
    cancelButton->setFixedSize(50, 30);
    cancelButton->setStyleSheet(buttonStyle +
                                "QPushButton { "
                                "font-size: 18px; "
                                "}");
    cancelButton->setToolTip("取消编辑");
    connect(cancelButton, &QPushButton::clicked, this, &ToolBarWindow::cancelRequested);

    buttonLayout->addWidget(rectButton);
    buttonLayout->addWidget(circleButton);
    buttonLayout->addWidget(textButton);
    buttonLayout->addWidget(penButton);
    buttonLayout->addWidget(mosaicButton);
    buttonLayout->addWidget(numberNoteButton);
    buttonLayout->addWidget(arrowButton);
    buttonLayout->addWidget(dragButton);
    buttonLayout->addWidget(undoButton);
    buttonLayout->addWidget(finishButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    textSettings = new QWidget(this);
    QHBoxLayout *textLayout = new QHBoxLayout(textSettings);
    fontSizeSlider = new QSlider(Qt::Horizontal, this);
    fontSizeSlider->setRange(8, 72);
    fontSizeSlider->setValue(16);
    fontSizeSlider->setTickPosition(QSlider::TicksBelow);
    fontSizeSlider->setTickInterval(8);
    fontSizeSlider->setSingleStep(1);
    fontSizeSlider->setPageStep(8);
    fontSizeSlider->setStyleSheet(sliderStyle);
    colorBlock = new QPushButton(this);
    colorBlock->setFixedSize(20, 20);
    textColor = Qt::black;
    colorBlock->setStyleSheet("QPushButton { background-color: black; border: 1px solid black; border-radius: 3px; }");
    textLayout->addWidget(fontSizeSlider);
    textLayout->addWidget(colorBlock);
    textSettings->hide();
    connect(fontSizeSlider, &QSlider::valueChanged, this, &ToolBarWindow::textFontSizeChanged);
    connect(colorBlock, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(textColor, this);
        if (color.isValid()) {
            textColor = color;
            colorBlock->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid black; border-radius: 3px; }").arg(color.name()));
            emit textColorChanged(color);
        }
    });

    mosaicSettings = new QWidget(this);
    QHBoxLayout *mosaicLayout = new QHBoxLayout(mosaicSettings);
    mosaicSizeSlider = new QSlider(Qt::Horizontal, this);
    mosaicSizeSlider->setRange(5, 50);
    mosaicSizeSlider->setValue(10);
    mosaicSizeSlider->setTickPosition(QSlider::TicksBelow);
    mosaicSizeSlider->setTickInterval(5);
    mosaicSizeSlider->setSingleStep(1);
    mosaicSizeSlider->setPageStep(5);
    mosaicSizeSlider->setStyleSheet(sliderStyle);
    mosaicLayout->addWidget(mosaicSizeSlider);
    mosaicSettings->hide();
    connect(mosaicSizeSlider, &QSlider::valueChanged, this, &ToolBarWindow::mosaicSizeChanged);

    shapeSettings = new QWidget(this);
    QHBoxLayout *shapeLayout = new QHBoxLayout(shapeSettings);
    borderWidthSlider = new QSlider(Qt::Horizontal, this);
    borderWidthSlider->setRange(1, 10);
    borderWidthSlider->setValue(2);
    borderWidthSlider->setTickPosition(QSlider::TicksBelow);
    borderWidthSlider->setTickInterval(1);
    borderWidthSlider->setSingleStep(1);
    borderWidthSlider->setPageStep(1);
    borderWidthSlider->setStyleSheet(sliderStyle);
    borderColorBlock = new QPushButton(this);
    borderColorBlock->setFixedSize(20, 20);
    borderColorBlock->setStyleSheet("QPushButton { background-color: black; border: 1px solid black; border-radius: 3px; }");
    shapeLayout->addWidget(borderWidthSlider);
    shapeLayout->addWidget(borderColorBlock);
    shapeSettings->hide();
    connect(borderWidthSlider, &QSlider::valueChanged, this, &ToolBarWindow::borderWidthChanged);
    connect(borderColorBlock, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(Qt::black, this);
        if (color.isValid()) {
            borderColorBlock->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid black; border-radius: 3px; }").arg(color.name()));
            emit borderColorChanged(color);
        }
    });

    penSettings = new QWidget(this);
    QHBoxLayout *penLayout = new QHBoxLayout(penSettings);
    penWidthSlider = new QSlider(Qt::Horizontal, this);
    penWidthSlider->setRange(1, 10);
    penWidthSlider->setValue(2);
    penWidthSlider->setTickPosition(QSlider::TicksBelow);
    penWidthSlider->setTickInterval(1);
    penWidthSlider->setSingleStep(1);
    penWidthSlider->setPageStep(1);
    penWidthSlider->setStyleSheet(sliderStyle);
    penColorBlock = new QPushButton(this);
    penColorBlock->setFixedSize(20, 20);
    penColorBlock->setStyleSheet("QPushButton { background-color: black; border: 1px solid black; border-radius: 3px; }");
    penLayout->addWidget(penWidthSlider);
    penLayout->addWidget(penColorBlock);
    penSettings->hide();
    connect(penWidthSlider, &QSlider::valueChanged, this, &ToolBarWindow::penWidthChanged);
    connect(penColorBlock, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(Qt::black, this);
        if (color.isValid()) {
            penColorBlock->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid black; border-radius: 3px; }").arg(color.name()));
            emit penColorChanged(color);
        }
    });

    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(shapeSettings);
    mainLayout->addWidget(textSettings);
    mainLayout->addWidget(penSettings);
    mainLayout->addWidget(mosaicSettings);
    mainLayout->setSpacing(2);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    setLayout(mainLayout);
}

void ToolBarWindow::setActiveButton(QPushButton *button)
{
    // 重置所有按钮的样式为默认状态
    QString defaultStyle = "QPushButton { "
                           "border: 1px solid #A0A0A0; "
                           "border-radius: 5px; "
                           "background-color: #FFFFFF; "
                           "font-size: 14px; "
                           "padding: 2px; "
                           "color: #000000; }"
                           "QPushButton:hover { background-color: #E0E0E0; }"
                           "QPushButton:pressed { background-color: #D0D0D0; }";

    rectButton->setStyleSheet(defaultStyle + "QPushButton { border: 2px solid black; }");
    circleButton->setStyleSheet(defaultStyle + "QPushButton { border: 2px solid black; border-radius: 15px; }");
    textButton->setStyleSheet(defaultStyle);
    penButton->setStyleSheet(defaultStyle);
    mosaicButton->setStyleSheet(defaultStyle);
    numberNoteButton->setStyleSheet(defaultStyle +
                                    "QPushButton { "
                                    "border: 1px solid #A0A0A0; "
                                    "border-radius: 15px; "
                                    "font-size: 16px; "
                                    "}");
    arrowButton->setStyleSheet(defaultStyle + "QPushButton { font-size: 18px; }");
    dragButton->setStyleSheet(defaultStyle);
    undoButton->setStyleSheet(defaultStyle);
    finishButton->setStyleSheet(defaultStyle);
    cancelButton->setStyleSheet(defaultStyle);

    // 为选中的按钮应用高亮样式
    if (button) {
        button->setStyleSheet(button->styleSheet() + "QPushButton { background-color: #87CEEB; color: #000000; }");
    }
}

void ToolBarWindow::showSettings(QWidget *settingsWidget, QPushButton *button)
{
}

void ToolBarWindow::adjustPosition()
{
    QRect screenRect = screen()->geometry();
    QPoint editPos = editWindow->pos();
    QSize editSize = editWindow->size();

    int margin = 5;
    int x = editPos.x() + editSize.width() - width();
    int y = editPos.y() + editSize.height() + margin;
    QPoint targetPos(x, y);

    if (targetPos.y() + height() > screenRect.bottom()) {
        targetPos.setY(screenRect.bottom() - height());
    }
    if (targetPos.x() < screenRect.left()) {
        targetPos.setX(screenRect.left());
    }

    move(targetPos);
}

void ToolBarWindow::adjustHeight()
{
    int baseHeight = 40;
    int settingsHeight = 0;
    if (textSettings->isVisible()) settingsHeight = 40;
    else if (mosaicSettings->isVisible()) settingsHeight = 40;
    else if (shapeSettings->isVisible()) settingsHeight = 40;
    else if (penSettings->isVisible()) settingsHeight = 40;
    setFixedHeight(baseHeight + settingsHeight);
    adjustPosition();
}

void ToolBarWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    painter.setBrush(QColor("#F0F0F0"));
    painter.setPen(QPen(QColor("#D0D0D0"), 1));
    painter.drawRoundedRect(rect, 8, 8);
}
