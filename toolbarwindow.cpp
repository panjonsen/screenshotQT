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
    setFixedWidth(350);
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

    QString buttonStyle = "QPushButton { "
                          "border: 1px solid #A0A0A0; "
                          "border-radius: 5px; "
                          "background-color: #FFFFFF; "
                          "font-size: 14px; "
                          "padding: 2px; }"
                          "QPushButton:hover { background-color: #E0E0E0; }"
                          "QPushButton:pressed { background-color: #D0D0D0; }";

    rectButton = new QPushButton(this);
    rectButton->setFixedSize(30, 30);
    rectButton->setStyleSheet(buttonStyle + "QPushButton { border: 2px solid black; }");
    connect(rectButton, &QPushButton::clicked, [this]() {
        setActiveButton(rectButton);
        emit modeChanged(0);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->show(); penSettings->hide(); show();
        adjustHeight();
    });

    circleButton = new QPushButton(this);
    circleButton->setFixedSize(30, 30);
    circleButton->setStyleSheet(buttonStyle + "QPushButton { border: 2px solid black; border-radius: 15px; }");
    connect(circleButton, &QPushButton::clicked, [this]() {
        setActiveButton(circleButton);
        emit modeChanged(1);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->show(); penSettings->hide(); show();
        adjustHeight();
    });

    textButton = new QPushButton("T", this);
    textButton->setFixedSize(30, 30);
    textButton->setStyleSheet(buttonStyle);
    connect(textButton, &QPushButton::clicked, [this]() {
        setActiveButton(textButton);
        emit modeChanged(2);
        hide(); textSettings->show(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    penButton = new QPushButton("P", this);
    penButton->setFixedSize(30, 30);
    penButton->setStyleSheet(buttonStyle);
    connect(penButton, &QPushButton::clicked, [this]() {
        setActiveButton(penButton);
        emit modeChanged(3);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->show(); show();
        adjustHeight();
    });

    mosaicButton = new QPushButton("M", this);
    mosaicButton->setFixedSize(30, 30);
    mosaicButton->setStyleSheet(buttonStyle);
    connect(mosaicButton, &QPushButton::clicked, [this]() {
        setActiveButton(mosaicButton);
        emit modeChanged(4);
        hide(); textSettings->hide(); mosaicSettings->show(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    dragButton = new QPushButton("手", this);
    dragButton->setFixedSize(30, 30);
    dragButton->setStyleSheet(buttonStyle);
    connect(dragButton, &QPushButton::clicked, [this]() {
        setActiveButton(dragButton);
        emit modeChanged(-1);
        emit dragModeChanged(true);
        hide(); textSettings->hide(); mosaicSettings->hide(); shapeSettings->hide(); penSettings->hide(); show();
        adjustHeight();
    });

    undoButton = new QPushButton("撤销", this);
    undoButton->setFixedSize(50, 30);
    undoButton->setStyleSheet(buttonStyle);
    connect(undoButton, &QPushButton::clicked, this, &ToolBarWindow::undoRequested);

    finishButton = new QPushButton("完成", this);
    finishButton->setFixedSize(50, 30);
    finishButton->setStyleSheet(buttonStyle);
    connect(finishButton, &QPushButton::clicked, this, &ToolBarWindow::finishRequested);

    cancelButton = new QPushButton("取消", this);
    cancelButton->setFixedSize(50, 30);
    cancelButton->setStyleSheet(buttonStyle);
    connect(cancelButton, &QPushButton::clicked, this, &ToolBarWindow::cancelRequested);

    buttonLayout->addWidget(rectButton);
    buttonLayout->addWidget(circleButton);
    buttonLayout->addWidget(textButton);
    buttonLayout->addWidget(penButton);
    buttonLayout->addWidget(mosaicButton);
    buttonLayout->addWidget(dragButton);
    buttonLayout->addWidget(undoButton);
    buttonLayout->addWidget(finishButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    textSettings = new QWidget(this);
    QHBoxLayout *textLayout = new QHBoxLayout(textSettings);
    fontSizeSpinBox = new QSpinBox(this);
    fontSizeSpinBox->setRange(8, 72);
    fontSizeSpinBox->setValue(16);
    colorBlock = new QPushButton(this);
    colorBlock->setFixedSize(20, 20);
    textColor = Qt::black;
    colorBlock->setStyleSheet("QPushButton { background-color: black; border: 1px solid black; border-radius: 3px; }");
    textLayout->addWidget(fontSizeSpinBox);
    textLayout->addWidget(colorBlock);
    textSettings->hide();
    connect(fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ToolBarWindow::textFontSizeChanged);
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
    rectButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                              "QPushButton:hover { background-color: #E0E0E0; }"
                              "QPushButton:pressed { background-color: #D0D0D0; }"
                              "QPushButton { border: 2px solid black; }");
    circleButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                                "QPushButton:hover { background-color: #E0E0E0; }"
                                "QPushButton:pressed { background-color: #D0D0D0; }"
                                "QPushButton { border: 2px solid black; border-radius: 15px; }");
    textButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                              "QPushButton:hover { background-color: #E0E0E0; }"
                              "QPushButton:pressed { background-color: #D0D0D0; }");
    penButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                             "QPushButton:hover { background-color: #E0E0E0; }"
                             "QPushButton:pressed { background-color: #D0D0D0; }");
    mosaicButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                                "QPushButton:hover { background-color: #E0E0E0; }"
                                "QPushButton:pressed { background-color: #D0D0D0; }");
    dragButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                              "QPushButton:hover { background-color: #E0E0E0; }"
                              "QPushButton:pressed { background-color: #D0D0D0; }");
    undoButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                              "QPushButton:hover { background-color: #E0E0E0; }"
                              "QPushButton:pressed { background-color: #D0D0D0; }");
    finishButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                                "QPushButton:hover { background-color: #E0E0E0; }"
                                "QPushButton:pressed { background-color: #D0D0D0; }");
    cancelButton->setStyleSheet("QPushButton { border: 1px solid #A0A0A0; border-radius: 5px; background-color: #FFFFFF; font-size: 14px; padding: 2px; }"
                                "QPushButton:hover { background-color: #E0E0E0; }"
                                "QPushButton:pressed { background-color: #D0D0D0; }");

    if (button) {
        button->setStyleSheet(button->styleSheet() + "QPushButton { background-color: #87CEEB; }");
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
