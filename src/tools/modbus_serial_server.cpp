#include <QSerialPort>
#include <base/modbus_frame.h>
#include <base/modbus_logger.h>
#include <bytes/buffer.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
class SerialConnection : public AbstractConnection {
  Q_OBJECT
public:
  SerialConnection(QSerialPort *serialPort, QObject *parent = nullptr)
      : AbstractConnection(parent), serialPort_(serialPort),
        readBuffer_(new pp::bytes::Buffer()) {
    connect(serialPort_, &QSerialPort::aboutToClose, this,
            [&]() { emit disconnected(fd()); });
    connect(serialPort_, &QSerialPort::readyRead, this,
            &SerialConnection::onClientReadyRead);
  }
  virtual ~SerialConnection() {}

  bool open() {
    bool success = serialPort_->open(QIODevice::ReadWrite);
    if (!success) {
      log(LogLevel::kError, "open {} {}", serialPort_->portName().toStdString(),
          serialPort_->errorString().toStdString());
      return false;
    }
    return true;
  }
  quintptr fd() const override { return quintptr(serialPort_->handle()); }

  void write(const char *data, size_t size) override {
    serialPort_->write(data, size);
  }

  std::string name() const override {
    return serialPort_->portName().toStdString();
  }

  std::string fullName() const override {
    return serialPort_->portName().toStdString();
  }

private:
  void onClientReadyRead() {
    while (serialPort_->bytesAvailable() > 0) {
      char buf[1024] = {0};
      int size = serialPort_->read(buf, sizeof(buf));
      readBuffer_->Write(buf, size);
    }
    emit messageArrived(fd(), readBuffer_);
  }

  QSerialPort *serialPort_;
  BytesBufferPtr readBuffer_;
};

class SerialServer : public AbstractServer {
  Q_OBJECT
public:
  SerialServer(QSerialPort *serialPort, QObject *parent = nullptr)
      : serialConnection_(new SerialConnection(serialPort, this)),
        AbstractServer(parent) {}

  bool listenAndServe() override {
    bool success = serialConnection_->open();
    if (!success) {
      return false;
    }
    handleNewConnFunc_(serialConnection_);
    return true;
  }

private:
  SerialConnection *serialConnection_;
};

QModbusServer *createQModbusSerialServer(const QString &serialName,
                                         QSerialPort::BaudRate baudRate,
                                         QSerialPort::DataBits dataBits,
                                         QSerialPort::Parity parity,
                                         QSerialPort::StopBits stopBits,
                                         QObject *parent) {
  QSerialPort *serialPort = new QSerialPort(parent);
  serialPort->setBaudRate(baudRate);
  serialPort->setDataBits(dataBits);
  serialPort->setParity(parity);
  serialPort->setStopBits(stopBits);
  serialPort->setPortName(serialName);

  auto serialServer = new SerialServer(serialPort, parent);
  auto modbusServer = new QModbusServer(serialServer, parent);
  modbusServer->setTransferMode(TransferMode::kRtu);
  return modbusServer;
}

} // namespace modbus

#include "modbus_serial_server.moc"
