// версия 4013
//12 байт посылка, "прореживание" принятых данных для вывода на график, СВД статус.
#include "mainwindow.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextCursor>
#include <QDateTime>
#include <QElapsedTimer>

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
    setWindowTitle("ADC Monitor");
    resize(1000, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    //------------------------------------------------------------------
    // главный горизонтальный layout
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    //------------------------------------------------------------------
    // левая колонка: подключение + управление
    QVBoxLayout *leftLayout = new QVBoxLayout();

    // ===== ПОДКЛЮЧЕНИЕ =====
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

    // ---- строка статуса и индикатора ----
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Отключено", this);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);

    // наш светодиод - сбоку справа
    m_ledWidget = new LedWidget(this);
    m_ledWidget->setFixedSize(40, 40);  // "роскошный" вид
    statusLayout->addStretch();
    statusLayout->addWidget(m_ledWidget);
    connectionLayout->addLayout(statusLayout, 3, 0, 1, 2);

    leftLayout->addWidget(connectionGroup);

    // ===== УПРАВЛЕНИЕ =====
    QGroupBox *ctrlGroup = new QGroupBox("Управление", this);
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);

    QHBoxLayout *skipRow = new QHBoxLayout();
    QLabel *lbl = new QLabel("Прореживание (1–500):", this);
    m_skipBox = new QSpinBox(this);
    m_skipBox->setRange(1, 500);
    m_skipBox->setValue(10);
    m_skipValue = 10;
    skipRow->addWidget(lbl);
    skipRow->addWidget(m_skipBox);
    skipRow->addStretch();
    ctrlLayout->addLayout(skipRow);

    connect(m_skipBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val){ m_skipValue = val; });
    leftLayout->addWidget(ctrlGroup);
    leftLayout->addStretch();

    //------------------------------------------------------------------
    // справа — график
    m_graph = new GraphWidget(this);
    m_graph->setMinimumWidth(650);

    mainLayout->addLayout(leftLayout, 0);
    mainLayout->addWidget(m_graph, 1);

    //------------------------------------------------------------------
    // кнопки подключения
    connect(m_connectBtn, &QPushButton::clicked,
            this, &MainWindow::connectToDevice);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &MainWindow::disconnectFromDevice);
}

void MainWindow::connectToDevice()
{
    m_deviceIP = m_ipEdit->text();
    m_devicePort = m_portEdit->text().toUShort();

    if (m_deviceIP.isEmpty() || m_devicePort == 0) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный IP адрес и порт");
        return;
    }

    //m_logEdit->append(QString("Подключение к %1:%2...").arg(m_deviceIP).arg(m_devicePort));
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
    //m_logEdit->append("Успешно подключено!");

    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_ipEdit->setEnabled(false);
    m_portEdit->setEnabled(false);
}

void MainWindow::onDisconnected()
{
    m_statusLabel->setText("Отключено");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    //m_logEdit->append("Соединение разорвано");

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
    static int skipCounter = 0;
    rxBuffer.append(m_socket->readAll());

    // условия пакета: 1 + 8 (4 ADC) + 2 служебных = 11‑12 байт, берем минимум 11
    const int PACKET_LEN = 12;

    static QElapsedTimer frameTimer;
    static bool timerStarted = false;
    if (!timerStarted) {
        frameTimer.start();
        timerStarted = true;
    }

    while (rxBuffer.size() >= PACKET_LEN)
    {
        char ledChar = rxBuffer.at(0);
        if (ledChar != '0' && ledChar != '1') {
            rxBuffer.remove(0, 1);
            continue;
        }

        if (rxBuffer.size() < PACKET_LEN)
            break;

        quint16 adc[4];
        for (int i = 0; i < 4; ++i) {
            quint8 hi = static_cast<quint8>(rxBuffer.at(1 + 2*i));
            quint8 lo = static_cast<quint8>(rxBuffer.at(2 + 2*i));
            adc[i] = (hi << 8) | lo;
        }

        m_ledWidget->setState(ledChar == '1');

        // копим значения, но график обновляем реже
        QVector<quint16> v(4);
        for (int i = 0; i < 4; ++i)
            v[i] = adc[i];

        skipCounter++;
        if (skipCounter < m_skipValue) {
            rxBuffer.remove(0, PACKET_LEN);
            continue;  // пропускаем до нужного шага
        }
        skipCounter = 0;

        m_graph->addValues(v);
        rxBuffer.remove(0, PACKET_LEN);
    }

    // ограничим скорость перерисовки графика
    if (frameTimer.elapsed() < 50)  // 20 Гц обновления
        return;
    frameTimer.restart();

    m_graph->update();    // перерисовка не чаще 20 раз/с
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

    //m_logEdit->append(QString("Ошибка: %1").arg(errorString));

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portEdit->setEnabled(true);
}

void MainWindow::appendLog(const QString &text)
{
    //m_logEdit->append(text);
    //QTextCursor c = m_logEdit->textCursor();
    //c.movePosition(QTextCursor::End);
    //m_logEdit->setTextCursor(c);
}
