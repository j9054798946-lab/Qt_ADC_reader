// версия 4014
// Добавлена отправка команд на плату в подтверждением в служебном байте
#include "mainwindow.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextCursor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>        // ← ДОБАВИТЬ если нет!
#include <QDebug>        // ← ДОБАВИТЬ если нет!


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_socketData(nullptr)
    , m_socketCmd(nullptr)
    , m_deviceIP("192.168.0.7")
    , m_devicePortData(23)
    , m_devicePortCmd(26)
    , m_skipValue(10)
    , m_testSequentialActive(false)
    , m_pendingCmd(0)        // ← ДОБАВИТЬ
    , m_cmdRetryCount(0)     // ← ДОБАВИТЬ
    , m_cmdConfirmed(false)  // ← ДОБАВИТЬ
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

    // ========== НОВОЕ: Таймер для подтверждения команд ==========
    m_cmdTimer = new QTimer(this);
    m_cmdTimer->setInterval(100);  // 100 мс таймаут
    connect(m_cmdTimer, &QTimer::timeout, this, &MainWindow::onCmdTimeout);
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

    // ========== НОВОЕ: Кнопка с индикатором статуса ==========
    QHBoxLayout *testBtnLayout = new QHBoxLayout();

    m_testSequentialBtn = new QPushButton("▶ Последовательный тест", this);
    m_testSequentialBtn->setCheckable(true);
    m_testSequentialBtn->setStyleSheet(
        "QPushButton { padding: 8px; font-size: 12px; }"
        "QPushButton:checked { background-color: #90EE90; }"
    );
    testBtnLayout->addWidget(m_testSequentialBtn);

    // Индикатор статуса команды (зелёный/серый/красный кружок)
    m_cmdStatusLabel = new QLabel("●", this);
    m_cmdStatusLabel->setFixedSize(20, 20);
    m_cmdStatusLabel->setStyleSheet("QLabel { color: gray; font-size: 20px; }");
    m_cmdStatusLabel->setToolTip("Статус команды");
    testBtnLayout->addWidget(m_cmdStatusLabel);

    testBtnLayout->addStretch();
    ctrlLayout->addLayout(testBtnLayout);

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
// это старая функция:
/*void MainWindow::updateDisconnectedState()
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
}*/

// ========== Отправка команды БЕЗ повторов (низкий уровень) ==========
void MainWindow::sendCommandRaw(quint8 cmd, quint8 arg)
{
    if (!m_socketCmd || m_socketCmd->state() != QTcpSocket::ConnectedState) {
        return;
    }

    QByteArray packet;
    packet.append(static_cast<char>(0xCC));
    packet.append(static_cast<char>(cmd));
    packet.append(static_cast<char>(arg));

    m_socketCmd->write(packet);
    m_socketCmd->flush();
}

// ========== Отправка команды С повторами и подтверждением ==========
void MainWindow::sendCommand(quint8 cmd, quint8 arg)
{
    if (!m_socketCmd || m_socketCmd->state() != QTcpSocket::ConnectedState) {
        QMessageBox::warning(this, "Ошибка", "Канал команд не подключен");
        return;
    }

    // Установить индикатор в "ожидание" (серый)
    m_cmdStatusLabel->setStyleSheet("QLabel { color: gray; font-size: 20px; }");

    // Сохранить ожидаемую команду
    m_pendingCmd = cmd;
    m_cmdRetryCount = 0;
    m_cmdConfirmed = false;

    // Отправить команду
    sendCommandRaw(cmd, arg);

    // Запустить таймер ожидания подтверждения
    m_cmdTimer->start();

    qDebug() << "📤 Команда отправлена:" << QString("0x%1").arg(cmd, 2, 16, QChar('0'));
}

// ========== Таймаут ожидания подтверждения ==========
void MainWindow::onCmdTimeout()
{
    if (m_cmdConfirmed) {
        // Команда подтверждена - остановить таймер
        m_cmdTimer->stop();
        m_cmdStatusLabel->setStyleSheet("QLabel { color: green; font-size: 20px; }");
        qDebug() << "✅ Команда подтверждена:" << QString("0x%1").arg(m_pendingCmd, 2, 16, QChar('0'));
        return;
    }

    // Команда не подтверждена - повторить
    m_cmdRetryCount++;

    if (m_cmdRetryCount >= 10) {
        // Превышен лимит повторов
        m_cmdTimer->stop();
        m_cmdStatusLabel->setStyleSheet("QLabel { color: red; font-size: 20px; }");
        qDebug() << "❌ Команда не подтверждена после 10 попыток:"
                 << QString("0x%1").arg(m_pendingCmd, 2, 16, QChar('0'));

        QMessageBox::warning(this, "Ошибка команды",
                           "Команда не подтверждена устройством.\n"
                           "Проверьте соединение.");
        return;
    }

    // Повторить отправку
    qDebug() << "⏳ Повтор команды" << m_cmdRetryCount << "/10";
    sendCommandRaw(m_pendingCmd, 0x00);
}
// это старая функция
/*void MainWindow::onTestSequentialClicked()
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
}*/

void MainWindow::onDataReceived()
{
    QByteArray newData = m_socketData->readAll();
    rxBuffer.append(newData);

    static int skipCounter = 0;
    bool foundSomething = true;

    while (foundSomething && rxBuffer.size() > 0) {
        foundSomething = false;

        // ========== Поиск батча [0xBB][N][...][0xCC] ==========
        for (int i = 0; i <= rxBuffer.size() - 2; i++) {
            if (static_cast<quint8>(rxBuffer.at(i)) == 0xBB) {

                if (rxBuffer.size() < i + 2) {
                    break;
                }

                quint8 batch_count = static_cast<quint8>(rxBuffer.at(i + 1));

                if (batch_count == 0 || batch_count > 60) {
                    rxBuffer.remove(0, 1);
                    foundSomething = true;
                    break;
                }

                int expected_size = 2 + batch_count * 9 + 1;

                if (rxBuffer.size() < i + expected_size) {
                    break;
                }

                if (static_cast<quint8>(rxBuffer.at(i + expected_size - 1)) != 0xCC) {
                    rxBuffer.remove(0, i + 1);
                    foundSomething = true;
                    break;
                }

                // ========== РАСПАКОВКА БАТЧА ==========
                int pos = i + 2;

                for (quint8 m = 0; m < batch_count; m++) {
                    // Читаем байт статуса
                    quint8 status_byte = static_cast<quint8>(rxBuffer.at(pos++));

                    // Биты 0: LED состояние
                    bool ledState = (status_byte & 0x01) != 0;

                    // Биты 7-4: Подтверждение команды
                    quint8 cmd_ack = (status_byte >> 4) & 0x0F;

                    // ========== ПРОВЕРКА ПОДТВЕРЖДЕНИЯ ==========
                    if (cmd_ack != 0 && cmd_ack == m_pendingCmd && !m_cmdConfirmed) {
                        m_cmdConfirmed = true;
                        qDebug() << "✅ Получено подтверждение команды:"
                                 << QString("0x%1").arg(cmd_ack, 2, 16, QChar('0'));

                        // Обновить состояние кнопки согласно подтверждённой команде
                        updateButtonState(cmd_ack);
                    }

                    // Чтение 4 каналов АЦП
                    quint16 adc[4];
                    for (int ch = 0; ch < 4; ch++) {
                        quint8 hi = static_cast<quint8>(rxBuffer.at(pos++));
                        quint8 lo = static_cast<quint8>(rxBuffer.at(pos++));
                        adc[ch] = (hi << 8) | lo;
                    }

                    // Обновление LED
                    if (m == batch_count - 1) {
                        m_ledWidget->setState(ledState);
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

                rxBuffer.remove(0, i + expected_size);
                foundSomething = true;
                break;
            }
        }

        if (rxBuffer.size() > 5000) {
            qDebug() << "⚠️ Переполнение буфера, очистка";
            rxBuffer.remove(0, 1000);
            foundSomething = true;
        }
    }

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
// ========== Обновление состояния кнопки по подтверждённой команде ==========
void MainWindow::updateButtonState(quint8 cmd)
{
    switch (cmd) {
        case 0x01:  // Последовательный тест ВКЛ
            m_testSequentialActive = true;
            m_testSequentialBtn->setChecked(true);
            m_testSequentialBtn->setText("⏸ Остановить тест");
            break;

        case 0x02:  // Последовательный тест ВЫКЛ
            m_testSequentialActive = false;
            m_testSequentialBtn->setChecked(false);
            m_testSequentialBtn->setText("▶ Последовательный тест");
            break;

        default:
            break;
    }
}
void MainWindow::onTestSequentialClicked()
{
    // Переключить ЖЕЛАЕМОЕ состояние
    m_testSequentialActive = !m_testSequentialActive;

    if (m_testSequentialActive) {
        // Хотим включить
        sendCommand(0x01, 0x00);
        // НЕ меняем состояние кнопки сразу!
        // Оно изменится в updateButtonState() после подтверждения
    } else {
        // Хотим выключить
        sendCommand(0x02, 0x00);
        // НЕ меняем состояние кнопки сразу!
    }
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
    m_testSequentialBtn->setText("▶ Последовательный тест");

    // ========== ДОБАВИТЬ: Остановить таймер команд ==========
    if (m_cmdTimer) {
        m_cmdTimer->stop();
    }
    m_cmdStatusLabel->setStyleSheet("QLabel { color: gray; font-size: 20px; }");
}


