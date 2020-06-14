#include "modbus_server_p.h"
#include <QUrl>
#include <algorithm>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/smart_assert.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
QModbusServer::QModbusServer(AbstractServer *server, QObject *parent)
    : QObject(parent), d_ptr(new QModbusServerPrivate(this)) {
  qRegisterMetaType<SixteenBitValue>("SixteenBitValue");
  qRegisterMetaType<BitValue>("BitValue");
  Q_D(QModbusServer);
  d->setServer(server);
  d->setEnv();
}

QModbusServer::~QModbusServer() {}

void QModbusServer::setMaxClients(int maxClients) {
  Q_D(QModbusServer);
  d->setMaxClients(maxClients);
}

int QModbusServer::maxClients() const {
  const Q_D(QModbusServer);
  return d->maxClients();
}

void QModbusServer::setTransferMode(TransferMode transferMode) {
  Q_D(QModbusServer);
  d->setTransferMode(transferMode);
}

TransferMode QModbusServer::transferMode() const {
  const Q_D(QModbusServer);
  return d->transferMode();
}

void QModbusServer::setServerAddress(ServerAddress serverAddress) {
  Q_D(QModbusServer);
  d->setServerAddress(serverAddress);
}

ServerAddress QModbusServer::serverAddress() const {
  const Q_D(QModbusServer);
  return d->serverAddress();
}

void QModbusServer::addBlacklist(const QString &clientIp) {
  Q_D(QModbusServer);
  d->addBlacklist(clientIp);
}

QList<QString> QModbusServer::blacklist() const {
  const Q_D(QModbusServer);
  return d->blacklist();
}

void QModbusServer::setCanWriteSingleBitValueFunc(
    const canWriteSingleBitValueFunc &func) {
  Q_D(QModbusServer);
  d->setCanWriteSingleBitValueFunc(func);
}

void QModbusServer::setCanWriteSixteenBitValueFunc(
    const canWriteSixteenBitValueFunc &func) {
  Q_D(QModbusServer);
  d->setCanWriteSixteenBitValueFunc(func);
}

Response QModbusServer::processRequest(const Request &request) {
  Q_D(QModbusServer);
  return d->processRequest(request);
}

void QModbusServer::processBrocastRequest(const Request &request) {
  Q_D(QModbusServer);
  d->processBrocastRequest(request);
}

// read write
void QModbusServer::handleHoldingRegisters(Address startAddress,
                                           Quantity quantity) {
  Q_D(QModbusServer);
  d->handleHoldingRegisters(startAddress, quantity);
}
// read only
void QModbusServer::handleInputRegisters(Address startAddress,
                                         Quantity quantity) {
  Q_D(QModbusServer);
  d->handleInputRegisters(startAddress, quantity);
}
// read only
void QModbusServer::handleDiscreteInputs(Address startAddress,
                                         Quantity quantity) {
  Q_D(QModbusServer);
  d->handleDiscreteInputs(startAddress, quantity);
}
// read write
void QModbusServer::handleCoils(Address startAddress, Quantity quantity) {

  Q_D(QModbusServer);
  d->handleCoils(startAddress, quantity);
}

bool QModbusServer::holdingRegisterValue(Address address,
                                         SixteenBitValue *value) {
  Q_D(QModbusServer);
  return d->holdingRegisterValue(address, value);
}

bool QModbusServer::inputRegisterValue(Address address,
                                       SixteenBitValue *value) {
  Q_D(QModbusServer);
  return d->inputRegisterValue(address, value);
}

bool QModbusServer::coilsValue(Address address, BitValue *value) {
  Q_D(QModbusServer);
  return d->coilsValue(address, value);
}

bool QModbusServer::inputDiscreteValue(Address address, BitValue *value) {
  Q_D(QModbusServer);
  return d->inputDiscreteValue(address, value);
}

Error QModbusServer::writeCoils(Address address, BitValue setValue) {
  Q_D(QModbusServer);
  return d->writeCoils(address, setValue);
}

Error QModbusServer::writeInputDiscrete(Address address, BitValue setValue) {
  Q_D(QModbusServer);
  return d->writeInputDiscrete(address, setValue);
}
Error QModbusServer::writeInputRegister(Address address,
                                        const SixteenBitValue &setValue) {
  Q_D(QModbusServer);
  return d->writeInputRegister(address, setValue);
}
Error QModbusServer::writeHodingRegister(Address address,
                                         const SixteenBitValue &setValue) {
  Q_D(QModbusServer);
  return d->writeHodingRegister(address, setValue);
}

bool QModbusServer::listenAndServe() {
  Q_D(QModbusServer);
  return d->listenAndServe();
}

namespace internal {
struct Config {
  QString scheme;

  QString serialName;
  QSerialPort::BaudRate baudRate;
  QSerialPort::DataBits dataBits;
  QSerialPort::Parity parity;
  QSerialPort::StopBits stopBits;

  uint16_t port;
};

Config parseConfig(const QString &url) {
  Config config;
  QUrl qurl(url);

  config.scheme = qurl.scheme();
  config.port = qurl.port(502);
  QString host = qurl.host();
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

QModbusServer *createServer(const QString &url, QObject *parent) {
  static const QStringList schemaSupported = {"modbus.file", "modbus.tcp"};
  internal::Config config = internal::parseConfig(url);

  bool ok =
      std::any_of(schemaSupported.begin(), schemaSupported.end(),
                  [config](const QString &el) { return config.scheme == el; });
  if (!ok) {
    log(LogLevel::kError, "unsupported scheme {}, see file:/// or tcp:// ",
        config.scheme.toStdString());
    return nullptr;
  }

  log(LogLevel::kInfo, "instanced modbus server on {}", url.toStdString());
  if (config.scheme == "file") {
    return createQModbusSerialServer(config.serialName, config.baudRate,
                                     config.dataBits, config.parity,
                                     config.stopBits, parent);
  } else if (config.scheme == "tcp") {
    return createQModbusTcpServer(config.port, parent);
  }
  return nullptr;
}

} // namespace modbus
