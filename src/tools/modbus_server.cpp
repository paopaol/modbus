#include "modbus_server_p.h"
#include <modbus/base/single_bit_access.h>
#include <modbus/base/smart_assert.h>
#include <modbus/tools/modbus_server.h>

namespace modbus {
QModbusServer::QModbusServer(AbstractServer *server, QObject *parent)
    : QObject(parent), d_ptr(new QModbusServerPrivate(this)) {
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

bool QModbusServer::listenAndServe() {
  Q_D(QModbusServer);
  return d->listenAndServe();
}

} // namespace modbus
