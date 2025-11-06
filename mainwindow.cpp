// версия 4014
// Добавлена отправка команд на устройство
#include "mainwindow.h"
#include <QDebug>  // ← ДОБАВИТЬ!
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextCursor>
#include <QDateTime>
#include <QElapsedTimer>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_socketData(nullptr)
    , m_socketCmd(nullptr)
    , m_deviceIP("192.168.0.7")
    , m_devicePortData(23)
    , m_devicePortCmd(26)
    , m_skipValue(10)
    , m_testSequentialActive(false)
{
    setupUI();

    // Создаем сокет для приема данных АЦП (порт 23)
    m_socketData = new QTcpSocket(this);
    connect(m_socketData, &QTcpSocket::connected, this, &MainWindow::onDataSocketConnected);
    connect(m_socketData, &QTcpSocket::disconnected, this, &MainWindow::onDataSocketDisconnected);
    connect(m_socketData, &QTcpSocket::readyRead, this, &MainWindow::onDataReceived);
    connect(m_socketData, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onDataSocketError);

    // Создаем сокет для отправки команд (порт 26)
    m_socketCmd = new QTcpSocket(this);
    connect(m_socketCmd, &QTcpSocket::connected, this, &MainWindow::onCmdSocketConnected);
    connect(m_socketCmd, &QTcpSocket::disconnected, this, &MainWindow::onCmdSocketDisconnected);
    connect(m_socketCmd, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onCmdSocketError);
}

MainWindow::~MainWindow()
{
    if (m_socketData && m_socketData->state() == QTcpSocket::ConnectedState) {
        m_socketData->disconnectFromHost();
        m_socketData->waitForDisconnected(1000);
    }

    if (m_socketCmd && m_socketCmd->state() == QTcpSocket::ConnectedState) {
        m_socketCmd->disconnectFromHost();
        m_socketCmd->waitForDisconnected(1000);
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("ADC Monitor - Dual Channel");
    resize(1000, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    QVBoxLayout *leftLayout = new QVBoxLayout();

    // ===== ПОДКЛЮЧЕНИЕ =====
    QGroupBox *connectionGroup = new QGroupBox("Подключение", this);
    QGridLayout *connectionLayout = new QGridLayout(connectionGroup);

    connectionLayout->addWidget(new QLabel("IP адрес:"), 0, 0);
    m_ipEdit = new QLineEdit("192.168.0.7", this);
    connectionLayout->addWidget(m_ipEdit, 0, 1);

    // ========== НОВОЕ: Два порта ==========
    connectionLayout->addWidget(new QLabel("Порт данных:"), 1, 0);
    m_portDataEdit = new QLineEdit("23", this);
    connectionLayout->addWidget(m_portDataEdit, 1, 1);

    connectionLayout->addWidget(new QLabel("Порт команд:"), 2, 0);
    m_portCmdEdit = new QLineEdit("26", this);
    connectionLayout->addWidget(m_portCmdEdit, 2, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton("Подключиться", this);
    m_disconnectBtn = new QPushButton("Отключиться", this);
    m_disconnectBtn->setEnabled(false);
    buttonLayout->addWidget(m_connectBtn);
    buttonLayout->addWidget(m_disconnectBtn);
    connectionLayout->addLayout(buttonLayout, 3, 0, 1, 2);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Отключено", this);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);

    m_ledWidget = new LedWidget(this);
    m_ledWidget->setFixedSize(40, 40);
    statusLayout->addStretch();
    statusLayout->addWidget(m_ledWidget);
    connectionLayout->addLayout(statusLayout, 4, 0, 1, 2);

    leftLayout->addWidget(connectionGroup);

    // ===== УПРАВЛЕНИЕ =====
    QGroupBox *ctrlGroup = new QGroupBox("Управление", this);
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);

    QHBoxLayout *skipRow = new QHBoxLayout();
    QLabel *lbl = new QLabel("Прореживание (1–500):", this);
    m_skipBox = new QSpinBox(this);
    m_skipBox->setRange(1, 500);
    m_skipBox->setValue(10);
    skipRow->addWidget(lbl);
    skipRow->addWidget(m_skipBox);
    skipRow->addStretch();
    ctrlLayout->addLayout(skipRow);

    connect(m_skipBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val){
                m_skipValue = val;
                m_graph->setSkipValue(val);
            });

    ctrlLayout->addSpacing(10);
    QLabel *dacLabel = new QLabel("<b>Управление DAC:</b>", this);
    ctrlLayout->addWidget(dacLabel);

    m_testSequentialBtn = new QPushButton("▶ Последовательный тест", this);
    m_testSequentialBtn->setCheckable(true);
    m_testSequentialBtn->setStyleSheet(
        "QPushButton { padding: 8px; font-size: 12px; }"
        "QPushButton:checked { background-color: #90EE90; }"
    );
    ctrlLayout->addWidget(m_testSequentialBtn);
    connect(m_testSequentialBtn, &QPushButton::clicked,
            this, &MainWindow::onTestSequentialClicked);

    ctrlLayout->addStretch();
    leftLayout->addWidget(ctrlGroup);
    leftLayout->addStretch();

    m_graph = new GraphWidget(this);
    m_graph->setMinimumWidth(650);

    mainLayout->addLayout(leftLayout, 0);
    mainLayout->addWidget(m_graph, 1);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::connectToDevice);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::disconnectFromDevice);
}

void MainWindow::connectToDevice()
{
    m_deviceIP = m_ipEdit->text();
    m_devicePortData = m_portDataEdit->text().toUShort();
    m_devicePortCmd = m_portCmdEdit->text().toUShort();

    if (m_deviceIP.isEmpty() || m_devicePortData == 0 || m_devicePortCmd == 0) {
        QMessageBox::warning(this, "Ошибка", "Введите корректные параметры");
        return;
    }

    qDebug() << "Подключение к" << m_deviceIP;
    qDebug() << "  Порт данных:" << m_devicePortData;
    qDebug() << "  Порт команд:" << m_devicePortCmd;

    // Подключаем оба сокета
    m_socketData->connectToHost(m_deviceIP, m_devicePortData);
    m_socketCmd->connectToHost(m_deviceIP, m_devicePortCmd);

    m_connectBtn->setEnabled(false);
}

void MainWindow::disconnectFromDevice()
{
    if (m_socketData->state() == QTcpSocket::ConnectedState) {
        m_socketData->disconnectFromHost();
    }
    if (m_socketCmd->state() == QTcpSocket::ConnectedState) {
        m_socketCmd->disconnectFromHost();
    }
}


void MainWindow::onDataSocketConnected()
{
    qDebug() << "✅ Сокет данных подключен (порт" << m_devicePortData << ")";
    checkBothConnected();
}

void MainWindow::onCmdSocketConnected()
{
    qDebug() << "✅ Сокет команд подключен (порт" << m_devicePortCmd << ")";
    checkBothConnected();
}

void MainWindow::checkBothConnected()
{
    if (m_socketData->state() == QTcpSocket::ConnectedState &&
        m_socketCmd->state() == QTcpSocket::ConnectedState) {

        m_statusLabel->setText("Подключено (2 канала)");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");

        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(true);
        m_ipEdit->setEnabled(false);
        m_portDataEdit->setEnabled(false);
        m_portCmdEdit->setEnabled(false);
    }
}

void MainWindow::onDataSocketDisconnected()
{
    qDebug() << "❌ Сокет данных отключен";
    updateDisconnectedState();
}

void MainWindow::onCmdSocketDisconnected()
{
    qDebug() << "❌ Сокет команд отключен";
    updateDisconnectedState();
}

void MainWindow::updateDisconnectedState()
{
    m_statusLabel->setText("Отключено");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portDataEdit->setEnabled(true);
    m_portCmdEdit->setEnabled(true);

    m_ledWidget->setState(false);
    m_testSequentialActive = false;
    m_testSequentialBtn->setChecked(false);
}

void MainWindow::sendCommand(quint8 cmd, quint8 arg)
{
    if (!m_socketCmd || m_socketCmd->state() != QTcpSocket::ConnectedState) {
        QMessageBox::warning(this, "Ошибка", "Канал команд не подключен");
        return;
    }

    QByteArray packet;
    packet.append(static_cast<char>(0xCC));  // Маркер
    packet.append(static_cast<char>(cmd));   // Команда
    packet.append(static_cast<char>(arg));   // Аргумент

    // ========== Отправляем через КОМАНДНЫЙ сокет ==========
    m_socketCmd->write(packet);
    m_socketCmd->flush();

    qDebug() << "📤 Команда:" << QString("0x%1").arg(cmd, 2, 16, QChar('0'))
             << "через порт" << m_devicePortCmd;
}

void MainWindow::onTestSequentialClicked()
{
    m_testSequentialActive = !m_testSequentialActive;

    if (m_testSequentialActive) {
        sendCommand(0x01, 0x00);  // Включить тест
        m_testSequentialBtn->setText("⏸ Остановить тест");
        m_testSequentialBtn->setChecked(true);
    } else {
        sendCommand(0x02, 0x00);  // Выключить тест
        m_testSequentialBtn->setText("▶ Последовательный тест");
        m_testSequentialBtn->setChecked(false);
    }
}

void MainWindow::onDataReceived()
{
    // ========== Читаем из СОКЕТА ДАННЫХ ==========
    QByteArray newData = m_socketData->readAll();
    rxBuffer.append(newData);

    static int skipCounter = 0;
    bool foundSomething = true;

    // ========== ТОЛЬКО БАТЧИ АЦП, БЕЗ Echo! ==========
    while (foundSomething && rxBuffer.size() > 0) {
        foundSomething = false;

        // Поиск батча [0xBB][N][...][0xCC]
        for (int i = 0; i <= rxBuffer.size() - 2; i++) {
            if (static_cast<quint8>(rxBuffer.at(i)) == 0xBB) {

                if (rxBuffer.size() < i + 2) {
                    break;
                }

                quint8 batch_count = static_cast<quint8>(rxBuffer.at(i + 1));

                if (batch_count == 0 || batch_count > 60) {  // С запасом для BATCH_SIZE=50
                    rxBuffer.remove(0, 1);
                    foundSomething = true;
                    break;
                }

                // Рассчитать размер батча
                int expected_size = 2 + batch_count * 9 + 1;

                if (rxBuffer.size() < i + expected_size) {
                    break;  // Ждём больше данных
                }

                // Проверка маркера конца
                if (static_cast<quint8>(rxBuffer.at(i + expected_size - 1)) != 0xCC) {
                    rxBuffer.remove(0, i + 1);
                    foundSomething = true;
                    break;
                }

                // ========== РАСПАКОВКА БАТЧА ==========
                int pos = i + 2;

                for (quint8 m = 0; m < batch_count; m++) {
                    char ledChar = rxBuffer.at(pos++);

                    quint16 adc[4];
                    for (int ch = 0; ch < 4; ch++) {
                        quint8 hi = static_cast<quint8>(rxBuffer.at(pos++));
                        quint8 lo = static_cast<quint8>(rxBuffer.at(pos++));
                        adc[ch] = (hi << 8) | lo;
                    }

                    // Обновление LED
                    if (m == batch_count - 1) {
                        m_ledWidget->setState(ledChar == '1');
                    }

                    // Добавление в график
                    skipCounter++;
                    if (skipCounter >= m_skipValue) {
                        skipCounter = 0;

                        QVector<quint16> v(4);
                        for (int ch = 0; ch < 4; ch++) {
                            v[ch] = adc[ch];
                        }
                        m_graph->addValues(v);
                    }
                }

                // Удалить обработанный батч
                rxBuffer.remove(0, i + expected_size);
                foundSomething = true;
                break;
            }
        }

        // Защита от переполнения буфера
        if (rxBuffer.size() > 5000) {
            qDebug() << "⚠️ Переполнение буфера, очистка";
            rxBuffer.remove(0, 1000);
            foundSomething = true;
        }
    }

    // Ограничение частоты перерисовки
    static QElapsedTimer frameTimer;
    static bool timerStarted = false;
    if (!timerStarted) {
        frameTimer.start();
        timerStarted = true;
    }

    if (frameTimer.elapsed() >= 50) {
        frameTimer.restart();
        m_graph->update();
    }
}

void MainWindow::onDataSocketError(QAbstractSocket::SocketError error)
{
    QString errorString;
    switch (error) {
        case QAbstractSocket::HostNotFoundError:
            errorString = "Хост не найден";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "Порт данных: соединение отклонено (порт " +
                         QString::number(m_devicePortData) + ")";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorString = "Порт данных: удалённый хост закрыл соединение";
            break;
        default:
            errorString = "Порт данных: " + m_socketData->errorString();
    }

    qDebug() << "❌" << errorString;

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portDataEdit->setEnabled(true);
    m_portCmdEdit->setEnabled(true);
}

void MainWindow::onCmdSocketError(QAbstractSocket::SocketError error)
{
    QString errorString;
    switch (error) {
        case QAbstractSocket::HostNotFoundError:
            errorString = "Хост не найден";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "Порт команд: соединение отклонено (порт " +
                         QString::number(m_devicePortCmd) + ")";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorString = "Порт команд: удалённый хост закрыл соединение";
            break;
        default:
            errorString = "Порт команд: " + m_socketCmd->errorString();
    }

    qDebug() << "❌" << errorString;

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portDataEdit->setEnabled(true);
    m_portCmdEdit->setEnabled(true);
}

