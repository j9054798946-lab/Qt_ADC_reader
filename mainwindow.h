#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork/QTcpSocket>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include "ledwidget.h"
#include "graphwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void connectToDevice();
    void disconnectFromDevice();
    void onConnected();
    void onDisconnected();
    void onDataReceived();
    void onError(QAbstractSocket::SocketError error);

private:
    void setupUI();
    //void processReceivedData(const QByteArray &data);
    void appendLog(const QString &text);   // ← вот эта строка нужна!
    GraphWidget *m_graph;
private:
    QByteArray rxBuffer;
    QTcpSocket *m_socket;
    QLineEdit *m_ipEdit;
    QLineEdit *m_portEdit;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_statusLabel;
    QTextEdit *m_logEdit;
    LedWidget *m_ledWidget;

    QString m_deviceIP;
    quint16 m_devicePort;
};

#endif // MAINWINDOW_H
