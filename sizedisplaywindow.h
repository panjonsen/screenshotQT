#ifndef SIZEDISPLAYWINDOW_H
#define SIZEDISPLAYWINDOW_H

#include <QWidget>
#include <QLabel>

class SizeDisplayWindow : public QWidget {
    Q_OBJECT

public:
    explicit SizeDisplayWindow(QWidget *parent = nullptr);
    void setSizeText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *sizeLabel;
};

#endif // SIZEDISPLAYWINDOW_H
