#ifndef __MODBUS_SERIAL_CLIENT_H_
#define __MODBUS_SERIAL_CLIENT_H_

#include <QSerialPort>
#include <modbus.h>
#include <modbus_qt.h>

namespace modbus {
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
  Response *sendRequest(const modbus::Adu &adu, Mode mode = Mode::kRtu) {
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

  // void send(const std::vector<char> &byteArray) { serial_.write(byteArray); }

private:
  // FIXME:need dip
  QSerialPort serialPort_;
};
} // namespace modbus
#endif // __MODBUS_SERIAL_CLIENT_H_
