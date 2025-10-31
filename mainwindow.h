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
#include <QSpinBox>
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
    void onTestSequentialClicked();  // Обработчик кнопки теста

private:
    void setupUI();
    void appendLog(const QString &text);

    // ========== ВАЖНО: Добавлено! ==========
    void sendCommand(quint8 cmd, quint8 data1, quint8 data2);  // ← ДОБАВЛЕНО!

    // UI элементы
    QTcpSocket *m_socket;
    QLineEdit *m_ipEdit;
    QLineEdit *m_portEdit;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_statusLabel;
    QTextEdit *m_logEdit;
    LedWidget *m_ledWidget;
    GraphWidget *m_graph;
    QSpinBox *m_skipBox;

    // ========== ВАЖНО: Добавлено! ==========
    QPushButton *m_testSequentialBtn;  // ← ДОБАВЛЕНО!

    // Данные
    QByteArray rxBuffer;
    QString m_deviceIP;
    quint16 m_devicePort;
    int m_skipValue;

    // ========== ВАЖНО: Добавлено! ==========
    bool m_testSequentialActive;  // ← ДОБАВЛЕНО!
};

#endif // MAINWINDOW_H
