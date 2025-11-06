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
#include <QTimer>        // ← ВАЖНО!
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

    void onDataSocketConnected();
    void onDataSocketDisconnected();
    void onDataReceived();
    void onDataSocketError(QAbstractSocket::SocketError error);

    void onCmdSocketConnected();
    void onCmdSocketDisconnected();
    void onCmdSocketError(QAbstractSocket::SocketError error);

    void onTestSequentialClicked();  // ← Должно быть!
    void onCmdTimeout();             // ← ДОБАВИТЬ!
    void onPortDetectTimeout();  // ← ДОБАВИТЬ!

private:
    void setupUI();
    void sendCommand(quint8 cmd, quint8 arg);
    void sendCommandRaw(quint8 cmd, quint8 arg);      // ← ДОБАВИТЬ!
    void checkBothConnected();
    void updateDisconnectedState();                   // ← Должно быть!
    void updateButtonState(quint8 cmd);               // ← ДОБАВИТЬ!
    void swapPorts();  // ← ДОБАВИТЬ!

    // Сокеты
    QTcpSocket *m_socketData;
    QTcpSocket *m_socketCmd;

    // UI элементы
    QLineEdit *m_ipEdit;
    QLineEdit *m_portDataEdit;
    QLineEdit *m_portCmdEdit;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_statusLabel;
    LedWidget *m_ledWidget;
    GraphWidget *m_graph;
    QSpinBox *m_skipBox;
    QPushButton *m_testSequentialBtn;
    QLabel *m_cmdStatusLabel;      // ← ДОБАВИТЬ!

    // Данные
    QByteArray rxBuffer;
    QString m_deviceIP;
    quint16 m_devicePortData;
    quint16 m_devicePortCmd;
    int m_skipValue;
    bool m_testSequentialActive;
    bool m_portsSwapped;  // Флаг: порты перепутаны
    int m_autoDetectCounter; // Счётчик для автоопределения

    // Подтверждение команд
    QTimer *m_cmdTimer;            // ← ДОБАВИТЬ!
    quint8 m_pendingCmd;           // ← ДОБАВИТЬ!
    int m_cmdRetryCount;           // ← ДОБАВИТЬ!
    bool m_cmdConfirmed;           // ← ДОБАВИТЬ!
    // ========== ДОБАВИТЬ: Автоопределение портов ==========
    QTimer *m_portDetectTimer;     // Таймер проверки портов
    bool m_portsDetected;          // Флаг: порты проверены
    int m_batchesReceived;         // Счётчик полученных батчей
};

#endif // MAINWINDOW_H
