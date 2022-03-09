#include "usbrly82.h"
#include "extern-plugininfo.h"


UsbRly82Reply::Error UsbRly82Reply::error() const
{
    return m_error;
}

QByteArray UsbRly82Reply::requestData() const
{
    return m_requestData;
}

QByteArray UsbRly82Reply::responseData() const
{
    return m_responseData;
}

UsbRly82Reply::UsbRly82Reply(QObject *parent)  : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this](){
        m_error = ErrorTimeout;
        emit finished();
    });
}



UsbRly82::UsbRly82(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<QSerialPort::SerialPortError>();
    m_refreshTimer.setInterval(100);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &UsbRly82::poll);
}

bool UsbRly82::available() const
{
    return m_available;
}

bool UsbRly82::powerRelay1() const
{
    return m_powerRelay1;
}

UsbRly82Reply *UsbRly82::setRelay1Power(bool power)
{
    UsbRly82Reply *reply;
    if (power) {
        reply = createReply(QByteArray::fromHex("65"), false);
    } else {
        reply = createReply(QByteArray::fromHex("6F"), false);
    }
    sendNextRequest();
    return reply;
}


bool UsbRly82::powerRelay2() const
{
    return m_powerRelay2;
}

UsbRly82Reply *UsbRly82::setRelay2Power(bool power)
{
    UsbRly82Reply *reply;
    if (power) {
        reply = createReply(QByteArray::fromHex("66"), false);
    } else {
        reply = createReply(QByteArray::fromHex("70"), false);
    }
    reply->m_expectsResponse = false;
    sendNextRequest();
    return reply;
}

quint8 UsbRly82::digitalInputs() const
{
    return m_digitalInputs;
}

bool UsbRly82::connectRelay(const QString &serialPort)
{
    qCDebug(dcUsbRly82()) << "Connecting to" << serialPort;
    if (m_serialPort) {
        m_serialPort->close();
        delete m_serialPort;
        m_serialPort = nullptr;
    }

    m_available = false;

    m_serialPort = new QSerialPort(serialPort, this);
    m_serialPort->setBaudRate(19200);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setParity(QSerialPort::NoParity);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        qCWarning(dcUsbRly82()) << "Could not open serial port" << serialPort << m_serialPort->errorString();
        m_serialPort->deleteLater();
        m_serialPort = nullptr;
        emit availableChanged(m_available);
        return false;
    }

    connect(m_serialPort, &QSerialPort::readyRead, this, &UsbRly82::onReadyRead);
    connect(m_serialPort, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onError(QSerialPort::SerialPortError)), Qt::QueuedConnection);

    // Get serial number
    UsbRly82Reply *reply = getSerialNumber();
    connect(reply, &UsbRly82Reply::finished, this, [=](){
        if (reply->error() != UsbRly82Reply::ErrorNoError) {
            qCWarning(dcUsbRly82()) << "Reading serial number finished with error" << reply->error();
            return;
        }

        m_serialNumber = QString::fromUtf8(reply->responseData());
        qCDebug(dcUsbRly82()) << "Get serial number finished successfully." << m_serialNumber;

        // Get software version
        UsbRly82Reply *reply = getSoftwareVersion();
        connect(reply, &UsbRly82Reply::finished, this, [=](){
            if (reply->error() != UsbRly82Reply::ErrorNoError) {
                qCWarning(dcUsbRly82()) << "Reading software version finished with error" << reply->error();
                return;
            }

            m_softwareVersion = QString::fromUtf8(reply->responseData().toHex());
            qCDebug(dcUsbRly82()) << "Get software version finished successfully." << m_softwareVersion;

            UsbRly82Reply *reply = getRelayStates();
            connect(reply, &UsbRly82Reply::finished, this, [=](){
                if (reply->error() != UsbRly82Reply::ErrorNoError) {
                    qCWarning(dcUsbRly82()) << "Reading relay states finished with error" << reply->error();
                    return;
                }

                qCDebug(dcUsbRly82()) << "Reading relay states finished successfully." << reply->responseData().toHex();
                bool power = checkBit(reply->responseData().at(0), 0);
                if (m_powerRelay1 != power) {
                    m_powerRelay1 = power;
                    emit powerRelay1Changed(m_powerRelay1);
                }

                power = checkBit(reply->responseData().at(0), 1);
                if (m_powerRelay2 != power) {
                    m_powerRelay2 = power;
                    emit powerRelay2Changed(m_powerRelay2);
                }

                qCDebug(dcUsbRly82()) << "Relay 1:" << m_powerRelay1;
                qCDebug(dcUsbRly82()) << "Relay 2:" << m_powerRelay2;

                m_available = true;
                emit availableChanged(m_available);

                m_refreshTimer.start();
            });
        });
    });

    return true;
}

void UsbRly82::disconnectRelay()
{
    if (m_serialPort) {
        qCDebug(dcUsbRly82()) << "Disconnecting from" << m_serialPort->portName();
        m_serialPort->close();
        delete m_serialPort;
        m_serialPort = nullptr;
    }

    m_refreshTimer.stop();

    m_available = false;
    emit availableChanged(m_available);
}


UsbRly82Reply *UsbRly82::getSerialNumber()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("38"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::getSoftwareVersion()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("5A"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::getRelayStates()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("5B"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::getDigitalInputs()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("5E"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::getAdcValues()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("80"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::getAdcReference()
{
    UsbRly82Reply *reply = createReply(QByteArray::fromHex("82"));
    sendNextRequest();
    return reply;
}

UsbRly82Reply *UsbRly82::createReply(const QByteArray &requestData, bool expectsResponse)
{
    UsbRly82Reply *reply = new UsbRly82Reply(this);
    reply->m_expectsResponse = expectsResponse;
    reply->m_requestData = requestData;
    connect(reply, &UsbRly82Reply::finished, this, [=](){
        if (m_currentReply == reply) {
            m_currentReply = nullptr;
            sendNextRequest();
        }

        reply->deleteLater();
    });

    if (!expectsResponse) {
        m_replyQueue.enqueue(reply);
    } else {
        // Priorize requests without response (like switching the relay)
        m_replyQueue.prepend(reply);
    }
    return reply;
}

void UsbRly82::sendNextRequest()
{
    if (m_currentReply)
        return;

    if (m_replyQueue.isEmpty())
        return;

    m_currentReply = m_replyQueue.dequeue();
    qCDebug(dcUsbRly82()) << "-->" << m_currentReply->requestData().toHex();
    m_serialPort->write(m_currentReply->requestData());
    if (m_currentReply->m_expectsResponse) {
        m_currentReply->m_timer.start(1000);
    } else {
        // Finish the reply on next event loop
        QTimer::singleShot(0, m_currentReply, &UsbRly82Reply::finished);
    }
}

bool UsbRly82::checkBit(quint8 byte, uint bitNumber)
{
    return ((byte >> bitNumber) & 0x01) == 1;
}

void UsbRly82::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    qCDebug(dcUsbRly82()) << "<--" << data.toHex();

    if (m_currentReply) {
        m_currentReply->m_responseData = data;
        m_currentReply->m_timer.stop();
        emit m_currentReply->finished();
    } else {
        qCWarning(dcUsbRly82()) << "Unexpected data received" << data.toHex();
    }
}

void UsbRly82::onError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError && error != QSerialPort::OpenError && m_serialPort && m_serialPort->isOpen()) {
        qCWarning(dcUsbRly82()) << "Serial port error occurred:" << error << m_serialPort->errorString() << "(Is open:" << m_serialPort->isOpen() << ")";

        m_available = false;
        emit availableChanged(available());

        disconnectRelay();
    }
}

void UsbRly82::poll()
{
    if (m_replyQueue.count() > 10)
        return;

    UsbRly82Reply *reply = getDigitalInputs();
    connect(reply, &UsbRly82Reply::finished, this, [=](){
        if (reply->error() != UsbRly82Reply::ErrorNoError) {
            qCWarning(dcUsbRly82()) << "Reading digital inputs finished with error" << reply->error();
            return;
        }

        if (reply->responseData().isEmpty())
            return;

        quint8 digitalInputs = reply->responseData().at(0);
        if (m_digitalInputs != digitalInputs) {
            qCDebug(dcUsbRly82()) << "Digital inputs changed";
            m_digitalInputs = digitalInputs;
            emit digitalInputsChanged();
        }
    });


    reply = getAdcValues();
    connect(reply, &UsbRly82Reply::finished, this, [=](){
        if (reply->error() != UsbRly82Reply::ErrorNoError) {
            qCWarning(dcUsbRly82()) << "Reading analog inputs finished with error" << reply->error();
            return;
        }

        qCDebug(dcUsbRly82()) << "Analog inputs";
    });
}
