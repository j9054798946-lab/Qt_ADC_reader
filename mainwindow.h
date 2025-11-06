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

    // ========== НОВОЕ: Разные слоты для двух сокетов ==========
    void onDataSocketConnected();
    void onDataSocketDisconnected();
    void onDataReceived();
    void onDataSocketError(QAbstractSocket::SocketError error);

    void onCmdSocketConnected();
    void onCmdSocketDisconnected();
    void onCmdSocketError(QAbstractSocket::SocketError error);

    void onTestSequentialClicked();

private:
    void setupUI();
    void sendCommand(quint8 cmd, quint8 arg);
    // ========== ДОБАВИТЬ ЭТИ ДВЕ СТРОКИ В mainwindow.h! ==========
    void checkBothConnected();       // Проверка что оба сокета подключены
    void updateDisconnectedState();  // Обновление UI при отключении

    // ========== ДВА СОКЕТА ==========
    QTcpSocket *m_socketData;  // Для приема данных АЦП (порт 23)
    QTcpSocket *m_socketCmd;   // Для отправки команд (порт 26)

    // UI элементы
    QLineEdit *m_ipEdit;
    QLineEdit *m_portDataEdit;  // Порт для данных
    QLineEdit *m_portCmdEdit;   // Порт для команд
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_statusLabel;
    LedWidget *m_ledWidget;
    GraphWidget *m_graph;
    QSpinBox *m_skipBox;
    QPushButton *m_testSequentialBtn;

    // Данные
    QByteArray rxBuffer;
    QString m_deviceIP;
    quint16 m_devicePortData;  // 23
    quint16 m_devicePortCmd;   // 26
    int m_skipValue;
    bool m_testSequentialActive;
};

#endif // MAINWINDOW_H
