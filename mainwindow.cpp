#include "mainwindow.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextCursor>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_socket(nullptr)
    , m_deviceIP("192.168.0.7") // IP по умолчанию
    , m_devicePort(23)
{
    setupUI();

    // Создаем TCP сокет
    m_socket = new QTcpSocket(this);

    // Подключаем сигналы
    connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onDataReceived);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onError);
}

MainWindow::~MainWindow()
{
    if (m_socket && m_socket->state() == QTcpSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("LED State Monitor");
    setMinimumSize(400, 500);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Группа подключения
    QGroupBox *connectionGroup = new QGroupBox("Подключение", this);
    QGridLayout *connectionLayout = new QGridLayout(connectionGroup);

    connectionLayout->addWidget(new QLabel("IP адрес:"), 0, 0);
    m_ipEdit = new QLineEdit("192.168.0.7", this);
    connectionLayout->addWidget(m_ipEdit, 0, 1);

    connectionLayout->addWidget(new QLabel("Порт:"), 1, 0);
    m_portEdit = new QLineEdit("23", this);
    connectionLayout->addWidget(m_portEdit, 1, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton("Подключиться", this);
    m_disconnectBtn = new QPushButton("Отключиться", this);
    m_disconnectBtn->setEnabled(false);

    buttonLayout->addWidget(m_connectBtn);
    buttonLayout->addWidget(m_disconnectBtn);
    connectionLayout->addLayout(buttonLayout, 2, 0, 1, 2);

    // Статус подключения
    m_statusLabel = new QLabel("Отключено", this);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    connectionLayout->addWidget(m_statusLabel, 3, 0, 1, 2);

    mainLayout->addWidget(connectionGroup);

    // Группа отображения светодиода
    QGroupBox *ledGroup = new QGroupBox("Состояние светодиода", this);
    QVBoxLayout *ledLayout = new QVBoxLayout(ledGroup);

    m_ledWidget = new LedWidget(this);
    ledLayout->addWidget(m_ledWidget, 0, Qt::AlignCenter);

    QLabel *ledStateLabel = new QLabel("Красный = on, Белый = off", this);
    ledStateLabel->setAlignment(Qt::AlignCenter);
    ledLayout->addWidget(ledStateLabel);

    mainLayout->addWidget(ledGroup);

    // Лог сообщений
    QGroupBox *logGroup = new QGroupBox("Лог сообщений", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setMaximumHeight(150);
    m_logEdit->setReadOnly(true);
    logLayout->addWidget(m_logEdit);

    mainLayout->addWidget(logGroup);

    // Подключаем кнопки
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::connectToDevice);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::disconnectFromDevice);
}

void MainWindow::connectToDevice()
{
    m_deviceIP = m_ipEdit->text();
    m_devicePort = m_portEdit->text().toUShort();

    if (m_deviceIP.isEmpty() || m_devicePort == 0) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный IP адрес и порт");
        return;
    }

    m_logEdit->append(QString("Подключение к %1:%2...").arg(m_deviceIP).arg(m_devicePort));
    m_socket->connectToHost(m_deviceIP, m_devicePort);

    m_connectBtn->setEnabled(false);
}

void MainWindow::disconnectFromDevice()
{
    if (m_socket->state() == QTcpSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void MainWindow::onConnected()
{
    m_statusLabel->setText("Подключено");
    m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    m_logEdit->append("Успешно подключено!");

    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_ipEdit->setEnabled(false);
    m_portEdit->setEnabled(false);
}

void MainWindow::onDisconnected()
{
    m_statusLabel->setText("Отключено");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_logEdit->append("Соединение разорвано");

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portEdit->setEnabled(true);

    // Сбрасываем состояние светодиода
    m_ledWidget->setState(false);
}

//---------------- Прием данных-----------------------------------

void MainWindow::onDataReceived()
{
    rxBuffer.append(m_socket->readAll());

    while (rxBuffer.size() >= 9)  // 1 + 8
    {
        char ledChar = rxBuffer.at(0);
        if (ledChar != '0' && ledChar != '1') {
            rxBuffer.remove(0, 1);
            continue;
        }

        // есть ли полный пакет?
        if (rxBuffer.size() < 9)
            break;

        quint16 adc[4];
        for (int i = 0; i < 4; i++) {
            quint8 hi = static_cast<quint8>(rxBuffer.at(1 + 2*i));
            quint8 lo = static_cast<quint8>(rxBuffer.at(2 + 2*i));
            adc[i] = (hi << 8) | lo;
        }

        QString ledState = (ledChar == '1') ? "LED ВКЛ" : "LED ВЫКЛ";
        QString msg = QString("'%1' - %2 | ADC1=%3  ADC2=%4  ADC3=%5  ADC4=%6")
                        .arg(ledChar)
                        .arg(ledState)
                        .arg(adc[0])
                        .arg(adc[1])
                        .arg(adc[2])
                        .arg(adc[3]);

        appendLog(QDateTime::currentDateTime().toString("hh:mm:ss  ") + msg);
        m_ledWidget->setState(ledChar == '1');

        rxBuffer.remove(0, 9);
    }
}

void MainWindow::onError(QAbstractSocket::SocketError error)
{
    QString errorString;
    switch (error) {
        case QAbstractSocket::HostNotFoundError:
            errorString = "Хост не найден";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "Соединение отклонено";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorString = "Удаленный хост закрыл соединение";
            break;
        default:
            errorString = m_socket->errorString();
    }

    m_logEdit->append(QString("Ошибка: %1").arg(errorString));

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portEdit->setEnabled(true);
}

void MainWindow::appendLog(const QString &text)
{
    m_logEdit->append(text);
    QTextCursor c = m_logEdit->textCursor();
    c.movePosition(QTextCursor::End);
    m_logEdit->setTextCursor(c);
}
