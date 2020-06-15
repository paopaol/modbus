#ifndef MODBUS_URL_PARSER_H
#define MODBUS_URL_PARSER_H

#include <QMap>
#include <QSerialPort>
#include <QString>
#include <QUrl>

namespace internal {
struct Config {
  QString scheme;

  QString serialName;
  QSerialPort::BaudRate baudRate;
  QSerialPort::DataBits dataBits;
  QSerialPort::Parity parity;
  QSerialPort::StopBits stopBits;

  uint16_t port;
  QString host;
};

inline Config parseConfig(const QString &url) {
  Config config;
  QUrl qurl(url);

  config.scheme = qurl.scheme();
  config.port = qurl.port(502);
  config.host = qurl.host();
  config.serialName = qurl.path().mid(1);

  QString query = qurl.query();
  QStringList serialConfig = query.split("-");
  if (serialConfig.size() != 4) {
    serialConfig = QString("9600-8-n-1").split("-");
  }
  static QMap<QString, QSerialPort::Parity> parties = {
      {"n", QSerialPort::NoParity},   {"e", QSerialPort::EvenParity},
      {"o", QSerialPort::OddParity},  {"N", QSerialPort::NoParity},
      {"E", QSerialPort::EvenParity}, {"O", QSerialPort::OddParity}};
  config.baudRate = static_cast<QSerialPort::BaudRate>(serialConfig[0].toInt());
  config.dataBits = static_cast<QSerialPort::DataBits>(serialConfig[1].toInt());
  config.parity = parties[serialConfig[2]];
  config.stopBits = static_cast<QSerialPort::StopBits>(serialConfig[3].toInt());
  return config;
}
} // namespace internal
#endif /* MODBUS_URL_PARSER_H */
