#ifndef TOOLBARWINDOW_H
#define TOOLBARWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QColorDialog>

class EditWindow;

class ToolBarWindow : public QWidget {
    Q_OBJECT

public:
    explicit ToolBarWindow(EditWindow *editWindow, QWidget *parent = nullptr);
    void adjustPosition();
    void setActiveButton(QPushButton *button);

signals:
    void modeChanged(int mode);
    void dragModeChanged(bool enabled);
    void undoRequested();
    void finishRequested();
    void cancelRequested();
    void textFontSizeChanged(int size);
    void textColorChanged(const QColor &color);
    void mosaicSizeChanged(int size);
    void borderWidthChanged(int width);
    void borderColorChanged(const QColor &color);
    void penWidthChanged(int width);
    void penColorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    EditWindow *editWindow;
    QPushButton *rectButton, *circleButton, *textButton, *penButton, *mosaicButton, *dragButton;
    QPushButton *undoButton, *finishButton, *cancelButton;
    QWidget *textSettings, *mosaicSettings, *shapeSettings, *penSettings;
    QSpinBox *fontSizeSpinBox;
    QPushButton *colorBlock;
    QSlider *mosaicSizeSlider;
    QSlider *borderWidthSlider;
    QPushButton *borderColorBlock;
    QSlider *penWidthSlider;
    QPushButton *penColorBlock;
    QColor textColor;

    void setupUI();
    void showSettings(QWidget *settingsWidget, QPushButton *button);
    void adjustHeight();
};

#endif // TOOLBARWINDOW_H
