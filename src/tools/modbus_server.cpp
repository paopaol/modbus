#include "modbus_server_p.h"
#include "modbus_url_parser.h"
#include <algorithm>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/smart_assert.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
QModbusServer::QModbusServer(AbstractServer *server, QObject *parent)
    : QObject(parent), d_ptr(new QModbusServerPrivate(this)) {
  qRegisterMetaType<SixteenBitValue>("SixteenBitValue");
  qRegisterMetaType<Address>("Address");
  qRegisterMetaType<QVector<SixteenBitValue>>("QVector<SixteenBitValue>");
  qRegisterMetaType<ByteArray>("ByteArray");
  Q_D(QModbusServer);
  d->setServer(server);
  d->setEnv();
}

QModbusServer::~QModbusServer() = default;

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

void QModbusServer::enableDump(bool enable) {
  Q_D(QModbusServer);
  d->enableDump(enable);
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

bool QModbusServer::coilsValue(Address address) {
  Q_D(QModbusServer);
  return d->coilsValue(address);
}

bool QModbusServer::inputDiscreteValue(Address address) {
  Q_D(QModbusServer);
  return d->inputDiscreteValue(address);
}

Error QModbusServer::writeCoils(Address address, bool setValue) {
  Q_D(QModbusServer);
  return d->writeCoils(address, setValue);
}

Error QModbusServer::writeInputDiscrete(Address address, bool setValue) {
  Q_D(QModbusServer);
  return d->writeInputDiscrete(address, setValue);
}
Error QModbusServer::writeInputRegisters(
    Address address, const QVector<SixteenBitValue> &setValues) {
  Q_D(QModbusServer);
  return d->writeInputRegisters(address, setValues);
}
Error QModbusServer::writeHodingRegisters(
    Address address, const QVector<SixteenBitValue> &setValues) {
  Q_D(QModbusServer);
  return d->writeHodingRegisters(address, setValues);
}

bool QModbusServer::listenAndServe() {
  Q_D(QModbusServer);
  return d->listenAndServe();
}

QModbusServer *createServer(const QString &url, QObject *parent) {
  static const QStringList schemaSupported = {"modbus.file", "modbus.tcp"};
  internal::Config config = internal::parseConfig(url);

  bool ok =
      std::any_of(schemaSupported.begin(), schemaSupported.end(),
                  [config](const QString &el) { return config.scheme == el; });
  if (!ok) {
    log(LogLevel::kError,
        "unsupported scheme {}, see modbus.file:/// or modbus.tcp:// ",
        config.scheme.toStdString());
    return nullptr;
  }

  log(LogLevel::kInfo, "instanced modbus server on {}", url.toStdString());
  if (config.scheme == "modbus.file") {
    return createQModbusSerialServer(config.serialName, config.baudRate,
                                     config.dataBits, config.parity,
                                     config.stopBits, parent);
  } else if (config.scheme == "modbus.tcp") {
    return createQModbusTcpServer(config.port, parent);
  }
  return nullptr;
}

} // namespace modbus
