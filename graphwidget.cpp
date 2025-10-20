#include "graphwidget.h"
#include <QPainter>
#include <QPen>

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent),
      data(channels)
{
    for (int ch = 0; ch < channels; ++ch)
        data[ch].fill(0, points);

    setMinimumSize(300, 200);
}

void GraphWidget::addValues(const QVector<quint16> &vals)
{
    for (int ch = 0; ch < channels && ch < vals.size(); ++ch) {
        data[ch].pop_front();          // убрать старое
        data[ch].append(vals[ch]);     // добавить новое
    }
    update();                          // перерисовать
}

void GraphWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    // оси
    p.setPen(Qt::black);
    p.drawRect(rect().adjusted(0,0,-1,-1));

    const int w = width();
    const int h = height();
    const double dx = double(w) / (points - 1);
    const double scaleY = double(h) / 65535.0;

    QColor colors[channels] = { Qt::red, Qt::green, Qt::blue, Qt::magenta };

    for (int ch = 0; ch < channels; ++ch) {
        p.setPen(QPen(colors[ch], 2));
        for (int i = 1; i < data[ch].size(); ++i) {
            int x1 = int((i - 1) * dx);
            int x2 = int(i * dx);
            int y1 = h - int(data[ch][i - 1] * scaleY);
            int y2 = h - int(data[ch][i] * scaleY);
            p.drawLine(x1, y1, x2, y2);
        }
    }
}
