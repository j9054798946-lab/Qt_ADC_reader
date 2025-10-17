#include "ledwidget.h"
#include <QPainter>

LedWidget::LedWidget(QWidget *parent)
    : QWidget(parent), m_isOn(false)
{
    setFixedSize(35, 35);
}

void LedWidget::setState(bool isOn)
{
    if (m_isOn != isOn) {
        m_isOn = isOn;
        update(); // Перерисовать виджет
    }
}

void LedWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Определяем цвет в зависимости от состояния
    QColor ledColor = m_isOn ? Qt::green : Qt::white;

    // Рисуем круг
    painter.setBrush(QBrush(ledColor));
    painter.setPen(QPen(Qt::black, 2));

    int margin = 10;
    QRect ledRect(margin, margin, width() - 2*margin, height() - 2*margin);
    painter.drawEllipse(ledRect);

    // Добавляем эффект свечения для включенного состояния
    if (m_isOn) {
        painter.setBrush(QBrush(QColor(100, 255, 100, 100)));
        painter.setPen(Qt::NoPen);
        QRect glowRect = ledRect.adjusted(-4, -4, 4, 4);
        painter.drawEllipse(glowRect);
    }
}
