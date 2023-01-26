#ifndef EVBOXPORT_H
#define EVBOXPORT_H

#include <QObject>
#include <QSerialPort>
#include <QQueue>
#include <QTimer>

class EVBoxPort : public QObject
{
    Q_OBJECT
public:
    enum Command {
        Command68 = 68,
        Command69 = 69
    };
    Q_ENUM(Command)

    explicit EVBoxPort(const QString &portName, QObject *parent = nullptr);

    bool open();
    bool isOpen();
    void close();

    void sendCommand(Command command, quint16 timeout, quint16 maxChargingCurrent, const QString &serial = "00000000");

signals:
    void opened();
    void closed();
    void shortResponseReceived(EVBoxPort::Command command, const QString &serial);
    void responseReceived(EVBoxPort::Command command, const QString &serial, quint16 minChargingCurrent, quint16 maxChargingCurrent, quint16 chargingCurrentL1, quint16 chargingCurrentL2, quint16 chargingCurrentL3, quint32 totalEnergyConsumed);

private slots:
    void processQueue();
    void onReadyRead();
    void processDataPacket(const QByteArray &packet);

private:
    QByteArray createChecksum(const QByteArray &data) const;

private:
    QSerialPort *m_serialPort = nullptr;

    QByteArray m_inputBuffer;

    struct CommandWrapper {
        Command command = Command68;
        QString serial;
        quint16 timeout = 0;
        quint16 maxChargingCurrent = 0;
    };
    QQueue<CommandWrapper> m_commandQueue;
    QTimer m_waitTimer;
};

#endif // EVBOXPORT_H
