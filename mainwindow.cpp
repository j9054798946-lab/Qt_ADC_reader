#include "mainwindow.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextCursor>

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
    QByteArray data = m_socket->readAll();
    static QByteArray buffer;
    buffer.append(data);

    while (buffer.size() >= 1)
    {
        // ищем маркер состояния
        int idx0 = buffer.indexOf('0');
        int idx1 = buffer.indexOf('1');
        int idx;

        if (idx0 == -1 && idx1 == -1)
            return; // пока нет ни одного корректного байта

        idx = (idx0 == -1) ? idx1 :
              (idx1 == -1) ? idx0 :
              qMin(idx0, idx1);

        if (idx > 0)
            buffer.remove(0, idx); // отброс лишнего перед сообщением

        if (buffer.isEmpty())
            return;

        char ledChar = buffer.at(0);

        // пытаемся понять, есть ли данные АЦП
        quint16 adc = 0xFFFF; // по умолчанию "нет данных"
        bool hasAdc = (buffer.size() >= 3);

        if (hasAdc) {
            quint8 hi = static_cast<quint8>(buffer.at(1));
            quint8 lo = static_cast<quint8>(buffer.at(2));
            adc = (hi << 8) | lo;
        }

        // обновляем GUI
        bool ledOn = (ledChar == '1');
        m_ledWidget->setState(ledOn);

        if (hasAdc)
            appendLog(QString("Получено: '%1' - LED %2, ADC = %3")
                          .arg(ledChar)
                          .arg(ledOn ? "ВКЛ" : "ВЫКЛ")
                          .arg(adc));
        else
            appendLog(QString("Получено: '%1' - LED %2 (старый формат прошивки)")
                          .arg(ledChar)
                          .arg(ledOn ? "ВКЛ" : "ВЫКЛ"));

        // удаляем обработанные байты (1 или 3)
        buffer.remove(0, hasAdc ? 3 : 1);
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
