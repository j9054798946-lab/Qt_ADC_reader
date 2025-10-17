#ifndef LEDWIDGET_H
#define LEDWIDGET_H

#include <QWidget>

class LedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LedWidget(QWidget *parent = nullptr);
    void setState(bool isOn);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_isOn;
};

#endif // LEDWIDGET_H
