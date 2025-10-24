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
    p.setRenderHint(QPainter::Antialiasing, false);
    const int w = width();
    const int h = height();

    // белый фон
    p.fillRect(rect(), Qt::white);

    // ==== координатная сетка ====
    QPen gridPen(Qt::lightGray, 1, Qt::DashLine);
    p.setPen(gridPen);

    // --- горизонтальные линии (по Y) ---
    for (int yVal = 0; yVal <= 65000; yVal += 10000) {
        int y = h - int(double(yVal) / 65535.0 * h);
        p.drawLine(0, y, w, y);
        p.setPen(Qt::black);
        p.drawText(2, y - 2, QString::number(yVal));   // подпись
        p.setPen(gridPen);
    }

    // --- вертикальные линии (по X) ---
    double msPerSample = 0.5 * m_skip;   // 0.5 мс на шаг × прореживание
    double totalMs = msPerSample * (points - 1);
    int divisions = 8; // количество вертикальных линий
    double dx = double(w) / divisions;

    for (int i = 0; i <= divisions; ++i) {
        int x = int(i * dx);
        p.drawLine(x, 0, x, h);
        p.setPen(Qt::black);
        QString label = QString::number(int(totalMs / divisions * i)) + " ms";
        p.drawText(x + 2, h - 4, label);
        p.setPen(gridPen);
    }

    // ==== оси ====
    p.setPen(Qt::black);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // ==== графики ====
    QColor colors[channels] = { Qt::red, Qt::green, Qt::blue, Qt::magenta };
    const double dxPoint = double(w) / (points - 1);
    const double scaleY = double(h) / 65535.0;

    for (int ch = 0; ch < channels; ++ch) {
        p.setPen(QPen(colors[ch], 2));
        for (int i = 1; i < data[ch].size(); ++i) {
            int x1 = int((i - 1) * dxPoint);
            int x2 = int(i * dxPoint);
            int y1 = h - int(data[ch][i - 1] * scaleY);
            int y2 = h - int(data[ch][i] * scaleY);

            // ступенчатое отображение
            p.drawLine(x1, y1, x2, y1);
            p.drawLine(x2, y1, x2, y2);
        }
    }
}
