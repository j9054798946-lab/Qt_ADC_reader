// версия 4014
// Добавлена отправка команд на устройство
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
    , m_deviceIP("192.168.0.7")
    , m_devicePort(23)
    , m_testSequentialActive(false)  // ← ДОБАВЛЕНО
{
    setupUI();

    m_socket = new QTcpSocket(this);

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

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Левая колонка
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

    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Отключено", this);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);

    m_ledWidget = new LedWidget(this);
    m_ledWidget->setFixedSize(40, 40);
    statusLayout->addStretch();
    statusLayout->addWidget(m_ledWidget);
    connectionLayout->addLayout(statusLayout, 3, 0, 1, 2);

    leftLayout->addWidget(connectionGroup);

    // ===== УПРАВЛЕНИЕ ===== (РАСШИРЕНО)
    QGroupBox *ctrlGroup = new QGroupBox("Управление", this);
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);

    // Прореживание
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
            this, [this](int val){ m_skipValue = val; m_graph->setSkipValue(val); });

    // ========== НОВОЕ: Кнопки управления DAC ==========
    ctrlLayout->addSpacing(10);
    QLabel *dacLabel = new QLabel("<b>Управление DAC:</b>", this);
    ctrlLayout->addWidget(dacLabel);

    // Кнопка "Последовательный тест"
    m_testSequentialBtn = new QPushButton("▶ Последовательный тест", this);
    m_testSequentialBtn->setCheckable(true);
    m_testSequentialBtn->setStyleSheet(
        "QPushButton { padding: 8px; font-size: 12px; }"
        "QPushButton:checked { background-color: #90EE90; }"
    );
    ctrlLayout->addWidget(m_testSequentialBtn);
    connect(m_testSequentialBtn, &QPushButton::clicked,
            this, &MainWindow::onTestSequentialClicked);

    // В setupUI() добавьте после кнопки последовательного теста:

    QPushButton *testRawBtn = new QPushButton("🔧 Тест связи", this);
    ctrlLayout->addWidget(testRawBtn);

    connect(testRawBtn, &QPushButton::clicked, this, [this]() {
        if (!m_socket || m_socket->state() != QTcpSocket::ConnectedState) {
            QMessageBox::warning(this, "Ошибка", "Нет соединения");
            return;
        }

        // Отправить простую команду 0xFF (тестовая)
        QByteArray test;
        test.append(static_cast<char>(0xAA));
        test.append(static_cast<char>(0x55));
        test.append(static_cast<char>(0xFF));  // Тестовая команда
        test.append(static_cast<char>(0x12));
        test.append(static_cast<char>(0x34));
        test.append(static_cast<char>(0xFF ^ 0x12 ^ 0x34));

        m_socket->write(test);
        m_socket->flush();

        qDebug() << "🔧 Отправлен тест:" << test.toHex(' ');
    });


    // Здесь позже добавим кнопки "Выровнять" и "Меандр"
    // ...

    ctrlLayout->addStretch();
    leftLayout->addWidget(ctrlGroup);
    leftLayout->addStretch();

    // График
    m_graph = new GraphWidget(this);
    m_graph->setMinimumWidth(650);

    mainLayout->addLayout(leftLayout, 0);
    mainLayout->addWidget(m_graph, 1);

    // Подключение кнопок
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

    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_ipEdit->setEnabled(false);
    m_portEdit->setEnabled(false);
}

void MainWindow::onDisconnected()
{
    m_statusLabel->setText("Отключено");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portEdit->setEnabled(true);

    m_ledWidget->setState(false);

    // Сброс состояния кнопок
    m_testSequentialActive = false;
    m_testSequentialBtn->setChecked(false);
    m_testSequentialBtn->setText("▶ Последовательный тест");
}

void MainWindow::onDataReceived()
{
    QByteArray newData = m_socket->readAll();
    rxBuffer.append(newData);

    static int skipCounter = 0;
    bool foundSomething = true;

    // ========== ЦИКЛ РАЗБОРА БУФЕРА ==========
    while (foundSomething && rxBuffer.size() > 0) {
        foundSomething = false;

        // ========== ПРИОРИТЕТ 1: Поиск Echo команды ==========
        // Формат: [0xAA][0xEE][CMD][DATA1][DATA2]
        for (int i = 0; i <= rxBuffer.size() - 5; i++) {
            if (static_cast<quint8>(rxBuffer.at(i)) == 0xAA &&
                static_cast<quint8>(rxBuffer.at(i + 1)) == 0xEE) {

                quint8 echo_cmd = static_cast<quint8>(rxBuffer.at(i + 2));
                quint8 echo_data1 = static_cast<quint8>(rxBuffer.at(i + 3));
                quint8 echo_data2 = static_cast<quint8>(rxBuffer.at(i + 4));

                qDebug() << "✅ Echo команды получен:";
                qDebug() << "   CMD:" << QString("0x%1").arg(echo_cmd, 2, 16, QChar('0'));
                qDebug() << "   DATA1:" << QString("0x%1").arg(echo_data1, 2, 16, QChar('0'));
                qDebug() << "   DATA2:" << QString("0x%1").arg(echo_data2, 2, 16, QChar('0'));

                // Удалить Echo (и весь мусор до него)
                rxBuffer.remove(0, i + 5);
                foundSomething = true;
                break;  // Выход из цикла поиска Echo
            }
        }

        if (foundSomething) continue;  // Нашли Echo, начать сначала

        // ========== ПРИОРИТЕТ 2: Поиск батча АЦП ==========
        // Формат: [0xBB][N][...данные...][0xCC]
        for (int i = 0; i <= rxBuffer.size() - 2; i++) {
            if (static_cast<quint8>(rxBuffer.at(i)) == 0xBB) {
                // Нашли начало батча

                if (rxBuffer.size() < i + 2) {
                    // Недостаточно данных для чтения размера
                    break;
                }

                quint8 batch_count = static_cast<quint8>(rxBuffer.at(i + 1));

                // Проверка разумности размера батча
                if (batch_count == 0 || batch_count > 30) {
                    // Невалидный размер - это не батч, а случайный 0xBB
                    rxBuffer.remove(0, 1);
                    foundSomething = true;
                    break;
                }

                // Рассчитать ожидаемый размер батча
                // Заголовок (2) + Измерения (N × 9) + Конец (1)
                int expected_size = 2 + batch_count * 9 + 1;

                if (rxBuffer.size() < i + expected_size) {
                    // Недостаточно данных - ждём
                    break;
                }

                // Проверка маркера конца батча
                if (static_cast<quint8>(rxBuffer.at(i + expected_size - 1)) != 0xCC) {
                    // Нет маркера конца - это не батч
                    rxBuffer.remove(0, i + 1);
                    foundSomething = true;
                    break;
                }

                // ========== ВАЛИДНЫЙ БАТЧ! Распаковка ==========
                int pos = i + 2;  // Начало данных (после 0xBB и размера)

                for (quint8 m = 0; m < batch_count; m++) {
                    // Чтение одного измерения (9 байт)
                    char ledChar = rxBuffer.at(pos);
                    pos++;

                    quint16 adc[4];
                    for (int ch = 0; ch < 4; ch++) {
                        quint8 hi = static_cast<quint8>(rxBuffer.at(pos++));
                        quint8 lo = static_cast<quint8>(rxBuffer.at(pos++));
                        adc[ch] = (hi << 8) | lo;
                    }

                    // Обновление LED (только последнее значение в батче)
                    if (m == batch_count - 1) {
                        m_ledWidget->setState(ledChar == '1');
                    }

                    // Добавление в график с прореживанием
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

                // Удалить обработанный батч (включая мусор до него)
                rxBuffer.remove(0, i + expected_size);
                foundSomething = true;
                break;  // Выход из цикла поиска батчей
            }
        }

        if (foundSomething) continue;  // Нашли батч, начать сначала

        // ========== Если ничего не нашли и буфер большой - удалить мусор ==========
        if (rxBuffer.size() > 1000) {
            // Удалить первые 100 байт (явный мусор)
            rxBuffer.remove(0, 100);
            foundSomething = true;
        }
    }

    // Ограничение частоты перерисовки графика
    static QElapsedTimer frameTimer;
    static bool timerStarted = false;
    if (!timerStarted) {
        frameTimer.start();
        timerStarted = true;
    }

    if (frameTimer.elapsed() >= 50) {  // 20 FPS
        frameTimer.restart();
        m_graph->update();
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

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_ipEdit->setEnabled(true);
    m_portEdit->setEnabled(true);
}

void MainWindow::appendLog(const QString &text)
{
    // Если нужно, можно вернуть логирование
}
// ========== ИСПРАВЛЕННАЯ версия sendCommand() ==========
void MainWindow::sendCommand(quint8 cmd, quint8 data1, quint8 data2)
{
    if (!m_socket || m_socket->state() != QTcpSocket::ConnectedState) {
        QMessageBox::warning(this, "Ошибка", "Нет соединения с устройством");
        qDebug() << "❌ Попытка отправки команды без соединения";
        return;
    }

    QByteArray packet;
    packet.append(static_cast<char>(0xAA));  // Заголовок
    packet.append(static_cast<char>(0x55));  // Заголовок
    packet.append(static_cast<char>(cmd));   // Команда
    packet.append(static_cast<char>(data1)); // Данные 1
    packet.append(static_cast<char>(data2)); // Данные 2

    // Контрольная сумма
    quint8 checksum = cmd ^ data1 ^ data2;
    packet.append(static_cast<char>(checksum));

    // Отправка
    qint64 bytesWritten = m_socket->write(packet);  // ← ИСПРАВЛЕНО: переименовано
    m_socket->flush();

    // Отладочный вывод
    qDebug() << "📤 Отправлено байт:" << bytesWritten;
    qDebug() << "   HEX:" << packet.toHex(' ');
    qDebug() << "   CMD:" << QString("0x%1").arg(cmd, 2, 16, QChar('0'));
    qDebug() << "   DATA1:" << QString("0x%1").arg(data1, 2, 16, QChar('0'));
    qDebug() << "   DATA2:" << QString("0x%1").arg(data2, 2, 16, QChar('0'));
    qDebug() << "   CHECKSUM:" << QString("0x%1").arg(checksum, 2, 16, QChar('0'));
}

// ========== НОВОЕ: Обработчик кнопки "Последовательный тест" ==========
void MainWindow::onTestSequentialClicked()
{
    m_testSequentialActive = !m_testSequentialActive;

    if (m_testSequentialActive) {
        // Включить тест
        sendCommand(0x01, 0x01, 0x00);  // CMD=0x01, DATA1=0x01 (старт)
        m_testSequentialBtn->setText("⏸ Остановить тест");
        m_testSequentialBtn->setChecked(true);
    } else {
        // Выключить тест
        sendCommand(0x01, 0x00, 0x00);  // CMD=0x01, DATA1=0x00 (стоп)
        m_testSequentialBtn->setText("▶ Последовательный тест");
        m_testSequentialBtn->setChecked(false);
    }
}
