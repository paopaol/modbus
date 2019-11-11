#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include <QSerialPort>
#include <modbus.h>
#include <modbus_qt.h>

namespace modbus {
class QRequest : public QObject {
public:
  QRequest(ServerAddress serverAddress, FunctionCode functionCode, Data &data,
           QObject *parent = nullptr)
      : QObject(parent), adu_(serverAddress, functionCode, data) {}
  FunctionCode functionCode() const { return adu_.functionCode(); }
  Adu adu() const { return adu_; }
  bool isException() { return adu_.isException(); }
  template <typename T> T data() const { return T; }

private:
  Adu adu_;
};
class QSerialClient : public QObject {
  Q_OBJECT
public:
  enum class Mode { kAscii, kRtu };

  bool
  setBaudRate(qint32 baudRate,
              QSerialPort::Directions directions = QSerialPort::AllDirections) {
    return serialPort_.setBaudRate(baudRate, directions);
  }
  bool setDataBits(QSerialPort::DataBits dataBits) {
    return serialPort_.setDataBits(dataBits);
  }
  bool setParity(QSerialPort::Parity parity) {
    return serialPort_.setParity(parity);
  }
  void setPort(const QSerialPortInfo &serialPortInfo) {
    serialPort_.setPort(serialPortInfo);
  }
  bool setStopBits(QSerialPort::StopBits stopBits) {
    serialPort_.setStopBits(stopBits);
  }

  template <typename RequestType>
  QResponse *sendRequest(const modbus::QRequest &request,
                         Mode mode = Mode::kRtu) {
    Request request;

    request.setAdu(adu);

    auto reqData = request.dataGenerator<RequestType>();
    auto array = reqData.toByteArray();
    switch (mode) {
    case kRtu: {
    }
    case kAscii: {
    }
    }
    return nullptr;
  }

  bool open() { return false; }
  bool close() { return false; }

  // void send(const std::vector<char> &byteArray) {
  // serial_.write(byteArray); }

private:
  // FIXME:need dip
  QSerialPort serialPort_;
  Client client_;
};

class Client : public QObject {
public:
  enum class Type { kTcp, kSerial };
  Client(const Stream *stream) {}

  void connect() {}
  void close() {}
  void setTimeout() {}

  template <typename T0, typename T1>
  QResponse *sendRequest(const QRequest &request) {
    auto element = createRequestElement(request);
    stream->enqueueRequestElement();
    return nullptr;
  }
  static Client *NewSerial(const SerialPortInfo &info) {}
  static Client *NewTcp(const QString &ip, int port) {}
  static Client *NewUdp(const QString &ip) {}
public
signals:
  void requestFinished(QRequest *request, QResponse *response);

protected:
  void processPrivateResponse(QRequest *request, QResponse *response) {}

private:
  Stream *stream;
};
} // namespace modbus
#endif // __MODBUS_SERIAL_CLIENT_H_
