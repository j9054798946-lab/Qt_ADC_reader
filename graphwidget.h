#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QWidget>
#include <QVector>

class GraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GraphWidget(QWidget *parent = nullptr);
    void addValues(const QVector<quint16> &vals);   // новые 4 значения

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static const int channels = 4;
    static const int points   = 16;
    QVector<QVector<quint16>> data;     // [канал][точка]
};

#endif
