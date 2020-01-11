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

void QModbusServer::processRequest(quintptr fd, const Request &request) {
  Q_D(QModbusServer);
  d->processRequest(fd, request);
}

void QModbusServer::processBrocastRequest(const Request &request) {
  Q_D(QModbusServer);
  d->processBrocastRequest(request);
}

void QModbusServer::handleFunc(FunctionCode functionCode,
                               const SingleBitAccess &access,
                               DataChecker *requestDataChecker) {
  Q_D(QModbusServer);
  d->handleFunc(functionCode, access, requestDataChecker);
}

} // namespace modbus
