#include "modbus_client_p.h"
#include "modbus_url_parser.h"
#include <QTimer>
#include <algorithm>
#include <assert.h>
#include <base/modbus_logger.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/smart_assert.h>
#include <modbus_frame.h>

namespace modbus {

static QVector<SixteenBitValue>
toSixteenBitValueList(const SixteenBitAccess &access);
static ByteArray toBitValueList(const SingleBitAccess &access);

struct ReadWriteRegistersAccess {
  SixteenBitAccess readAccess;
  SixteenBitAccess writeAccess;
};

QModbusClient::QModbusClient(AbstractIoDevice *iodevice, QObject *parent)
    : QObject(parent), d_ptr(new QModbusClientPrivate(iodevice, this)) {
  setupEnvironment();
}

QModbusClient::QModbusClient(QObject *parent)
    : QObject(parent), d_ptr(new QModbusClientPrivate(nullptr, parent)) {
  setupEnvironment();
}

QModbusClient::~QModbusClient() = default;

void QModbusClient::open() {
  Q_D(QModbusClient);

  d->device_->open();
}

/**
 * Allows shutdown and transmits clientClosed signal regardless of whether the
 * device is already turned on
 */
void QModbusClient::close() {
  Q_D(QModbusClient);
  d->device_->close();
}

void QModbusClient::sendRequest(std::unique_ptr<Request> &request) {
  Q_D(QModbusClient);

  if (!isOpened()) {
    log(d->log_prefix_, LogLevel::kWarning, "{} closed, discard reuqest",
        d->device_->name());
    return;
  }

  /*just queue the request, when the session state is in idle, it will be sent
   * out*/
  auto *element = d->enqueueAndPeekLastElement();
  createElement(request, element);

  element->retryTimes = d->retryTimes_;
  d->scheduleNextRequest(d->t3_5_);
}

void QModbusClient::readSingleBits(ServerAddress serverAddress,
                                   FunctionCode functionCode,
                                   Address startAddress, Quantity quantity) {
  Q_D(QModbusClient);

  if (functionCode != FunctionCode::kReadCoils &&
      functionCode != FunctionCode::kReadInputDiscrete) {
    log(d->log_prefix_, LogLevel::kError,
        "single bit access:[read] invalid function code(" +
            std::to_string(functionCode) + ")");
  }

  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(quantity);

  std::unique_ptr<Request> request(new Request(
      serverAddress, functionCode, access, access.marshalReadRequest()));
  sendRequest(request);
}

void QModbusClient::writeSingleCoil(ServerAddress serverAddress,
                                    Address startAddress, bool value) {
  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(1);
  access.setValue(value);
  std::unique_ptr<Request> request(
      new Request(serverAddress, FunctionCode::kWriteSingleCoil, access,
                  access.marshalSingleWriteRequest()));
  sendRequest(request);
}

void QModbusClient::writeMultipleCoils(ServerAddress serverAddress,
                                       Address startAddress,
                                       const QVector<uint8_t> &valueList) {
  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(valueList.size());
  for (int offset = 0; offset < valueList.size(); offset++) {
    Address address = startAddress + offset;
    access.setValue(address, valueList[offset]);
  }

  std::unique_ptr<Request> request(
      new Request(serverAddress, FunctionCode::kWriteMultipleCoils, access,
                  access.marshalMultipleWriteRequest()));
  sendRequest(request);
}

void QModbusClient::readRegisters(ServerAddress serverAddress,
                                  FunctionCode functionCode,
                                  Address startAddress, Quantity quantity) {
  Q_D(QModbusClient);

  if (functionCode != FunctionCode::kReadHoldingRegisters &&
      functionCode != FunctionCode::kReadInputRegister) {
    log(d->log_prefix_, LogLevel::kError,
        "invalid function code for read registers" +
            std::to_string(functionCode));
  }

  SixteenBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(quantity);

  std::unique_ptr<Request> request(
      new Request(serverAddress, functionCode, access,
                  access.marshalMultipleReadRequest()));
  sendRequest(request);
}

void QModbusClient::writeSingleRegister(ServerAddress serverAddress,
                                        Address address,
                                        const SixteenBitValue &value) {
  SixteenBitAccess access;

  access.setStartAddress(address);
  access.setValue(value.toUint16());

  std::unique_ptr<Request> request(
      new Request(serverAddress, FunctionCode::kWriteSingleRegister, access,
                  access.marshalSingleWriteRequest()));
  sendRequest(request);
}

void QModbusClient::writeMultipleRegisters(
    ServerAddress serverAddress, Address startAddress,
    const QVector<SixteenBitValue> &valueList) {
  SixteenBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(valueList.size());

  int offset = 0;
  for (const auto &sixValue : valueList) {
    auto address = access.startAddress() + offset;
    access.setValue(address, sixValue.toUint16());
    offset++;
  }
  std::unique_ptr<Request> request(
      new Request(serverAddress, FunctionCode::kWriteMultipleRegisters, access,
                  access.marshalMultipleWriteRequest()));
  sendRequest(request);
}

void QModbusClient::readWriteMultipleRegisters(
    ServerAddress serverAddress, Address readStartAddress,
    Quantity readQuantity, Address writeStartAddress,
    const QVector<SixteenBitValue> &valueList) {
  ReadWriteRegistersAccess access;

  access.readAccess.setStartAddress(readStartAddress);
  access.readAccess.setQuantity(readQuantity);

  access.writeAccess.setStartAddress(writeStartAddress);
  access.writeAccess.setQuantity(valueList.size());

  int offset = 0;
  for (const auto &value : valueList) {
    auto address = writeStartAddress + offset++;
    access.writeAccess.setValue(address, value.toUint16());
  }

  ByteArray data = access.readAccess.marshalMultipleReadRequest();
  ByteArray writeData = access.writeAccess.marshalMultipleWriteRequest();

  data.insert(data.end(), writeData.begin(), writeData.end());
  std::unique_ptr<Request> request(new Request(
      serverAddress, FunctionCode::kReadWriteMultipleRegisters, access, data));
  sendRequest(request);
}

bool QModbusClient::isIdle() {
  Q_D(QModbusClient);
  return d->sessionState_.state() == SessionState::kIdle;
}

bool QModbusClient::isClosed() {
  Q_D(QModbusClient);
  return d->device_->isClosed();
}

bool QModbusClient::isOpened() {
  Q_D(QModbusClient);
  return d->device_->isOpened();
}

void QModbusClient::setupEnvironment() {
  qRegisterMetaType<Request>("Request");
  qRegisterMetaType<Response>("Response");
  qRegisterMetaType<SixteenBitAccess>("SixteenBitAccess");
  qRegisterMetaType<ByteArray>("ByteArray");
  qRegisterMetaType<modbus::ByteArray>("modbus::ByteArray");
  qRegisterMetaType<ServerAddress>("ServerAddress");
  qRegisterMetaType<Address>("Address");
  qRegisterMetaType<Error>("Error");
  qRegisterMetaType<QVector<SixteenBitValue>>("QVector<SixteenBitValue>");
  qRegisterMetaType<QVector<uint8_t>>("QVector<uint8_t>");
  qRegisterMetaType<FunctionCode>("FunctionCode");
  qRegisterMetaType<Quantity>("Quantity");

  Q_D(QModbusClient);

  connect(d->device_, &ReconnectableIoDevice::opened, this,
          &QModbusClient::clientOpened, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::closed, this,
          &QModbusClient::clientClosed, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::error, this,
          &QModbusClient::clearPendingRequest, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::connectionIsLostWillReconnect,
          this, &QModbusClient::clearPendingRequest, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::connectionIsLostWillReconnect,
          this, &QModbusClient::connectionIsLostWillReconnect,
          Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::error, this,
          &QModbusClient::onIoDeviceError, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::bytesWritten, this,
          &QModbusClient::onIoDeviceBytesWritten, Qt::QueuedConnection);
  connect(d->device_, &ReconnectableIoDevice::readyRead, this,
          &QModbusClient::onIoDeviceReadyRead, Qt::QueuedConnection);
  connect(d->waitResponseTimer_, &QTimer::timeout, this,
          &QModbusClient::onIoDeviceResponseTimeout, Qt::QueuedConnection);
  connect(this, &QModbusClient::requestFinished, this,
          &QModbusClient::processResponseAnyFunctionCode, Qt::QueuedConnection);
}

void QModbusClient::setTimeout(uint64_t timeout) {
  Q_D(QModbusClient);

  d->waitResponseTimeout_ = timeout;
}

uint64_t QModbusClient::timeout() {
  Q_D(QModbusClient);

  return d->waitResponseTimeout_;
}

void QModbusClient::setTransferMode(TransferMode transferMode) {
  Q_D(QModbusClient);

  d->transferMode_ = transferMode;
  d->decoder_ = createModbusFrameDecoder(transferMode, d->checkSizeFuncTable_);
  d->encoder_ = createModbusFrameEncoder(transferMode);
}

TransferMode QModbusClient::transferMode() const {
  const Q_D(QModbusClient);

  return d->transferMode_;
}

void QModbusClient::setRetryTimes(int times) {
  Q_D(QModbusClient);

  d->retryTimes_ = std::max(0, times);
}

int QModbusClient::retryTimes() {
  Q_D(QModbusClient);
  return d->retryTimes_;
}

void QModbusClient::setOpenRetryTimes(int retryTimes, int delay) {
  Q_D(QModbusClient);
  d->device_->setOpenRetryTimes(retryTimes, delay);
}

int QModbusClient::openRetryTimes() {
  Q_D(QModbusClient);
  return d->device_->openRetryTimes();
}

int QModbusClient::openRetryDelay() {
  Q_D(QModbusClient);
  return d->device_->openRetryDelay();
}

void QModbusClient::setFrameInterval(int frameInterval) {
  Q_D(QModbusClient);
  if (frameInterval < 0) {
    frameInterval = 0;
  }
  d->t3_5_ = frameInterval;
}

void QModbusClient::clearPendingRequest() {
  Q_D(QModbusClient);
  while (!d->elementQueue_.empty()) {
    auto e = d->elementQueue_.front();
    d->elementQueue_.pop_front();
    delete e;
  }
  d->waitTimerAlive_ = false;
  d->waitResponseTimer_->stop();
  d->sessionState_.setState(SessionState::kIdle);
}

size_t QModbusClient::pendingRequestSize() {
  Q_D(QModbusClient);
  return d->elementQueue_.size();
}

QString QModbusClient::errorString() {
  Q_D(QModbusClient);
  return d->errorString_;
}

void QModbusClient::setPrefix(const QString &prefix) {
  Q_D(QModbusClient);
  d->log_prefix_ = prefix.toStdString();
  d->device_->setPrefix(prefix);
}

void QModbusClient::enableDiagnosis(bool enable) {
  Q_D(QModbusClient);
  if (d->enableDiagnosis_ == enable) {
    return;
  }
  d->enableDiagnosis_ = enable;
}

void QModbusClient::enableDump(bool enable) {
  Q_D(QModbusClient);
  if (d->enableDump_ == enable) {
    return;
  }
  d->enableDump_ = enable;
}

RuntimeDiagnosis QModbusClient::runtimeDiagnosis() const {
  const Q_D(QModbusClient);
  return d->runtimeDiagnosis_;
}

void QModbusClient::onIoDeviceResponseTimeout() {
  Q_D(QModbusClient);

  /**
   * @brief when the request is sent, the client will start a timer, until
   * response is got. but,sometimes we got a response successfully,and the timer
   * is timeout too, although we explicit stoped the timer and set state into
   * `idle`, but the `timeout` event has been enqueued into eventloop,call
   * `timer.stop()` operation does not work. in this case, we must use a flag
   * indicates the timer had stopped
   * */
  if (!d->waitTimerAlive_) {
    return;
  }

  smart_assert(d->sessionState_.state() ==
               SessionState::kWaitingResponse)(d->sessionState_.state());

  auto &element = d->elementQueue_.front();
  element->bytesWritten = 0;
  element->dumpReadArray.clear();
  d->decoder_->Clear();

  /**
   *  An error occurs when the response times out but no response is
   * received. Then the master node enters the "idle" state and issues a
   * retry request. The maximum number of retries depends on the settings
   * of the primary node
   *
   */
  d->sessionState_.setState(SessionState::kIdle);

  const auto &request = *element->request;
  auto &response = element->response;

  response.setServerAddress(request.serverAddress());
  response.setFunctionCode(request.functionCode());
  response.setTransactionId(request.transactionId());
  response.setError(Error::kTimeout);
  if (element->retryTimes-- > 0) {
    log(d->log_prefix_, LogLevel::kWarning,
        "{} waiting response timeout, retry it, retrytimes ",
        d->device_->name(), element->retryTimes);

    processDiagnosis(request, response);
  } else {
    log(d->log_prefix_, LogLevel::kWarning, "{}: waiting response timeout",
        d->device_->name());

    /**
     * if have no retry times, remove this request
     */
    auto e = d->elementQueue_.front();
    d->elementQueue_.pop_front();
    emit requestFinished(*e->request, e->response);
    delete e;
  }
  d->scheduleNextRequest(d->t3_5_);
}

void QModbusClient::onIoDeviceReadyRead() {
  Q_D(QModbusClient);

  /**
   * When the last byte of the request is sent, it will enter the wait-response
   * state. Therefore, if data is received but not in the wait-response state,
   * then this data is not what we want,discard them
   */
  auto qdata = d->device_->readAll();
  d->readBuffer_.Write(qdata.data(), qdata.size());
  if (d->sessionState_.state() != SessionState::kWaitingResponse) {
    d->readBuffer_.Reset();

    std::stringstream stream;
    stream << d->sessionState_.state();
    log(d->log_prefix_, LogLevel::kWarning,
        "{} now state is in {}.got unexpected data, discard them.[{}]",
        d->device_->name(), stream.str(), dump(d->transferMode_, qdata));

    d->device_->clear();
    return;
  }

  auto &element = d->elementQueue_.front();
  auto &request = element->request;

  if (d->enableDump_) {
    element->dumpReadArray.append(qdata);
  }

  d->decoder_->Decode(d->readBuffer_, &element->response);
  if (!d->decoder_->IsDone()) {
    log(d->log_prefix_, LogLevel::kWarning,
        d->device_->name() + ":need more data." + "[" +
            dump(d->transferMode_, element->dumpReadArray) + "]");
    return;
  }

  const auto lastError = d->decoder_->LasError();
  d->decoder_->Clear();

  // replace with swap/move
  Response response = element->response;
  if (lastError != Error::kNoError) {
    response.setError(lastError);
  }

  /**
   * When receiving a response from an undesired child node,
   * Should continue to time out
   * discard all recived dat
   */
  if (response.serverAddress() != request->serverAddress()) {
    log(d->log_prefix_, LogLevel::kWarning,
        d->device_->name() +
            ":got response, unexpected serveraddress, discard it.[" +
            dump(d->transferMode_, qdata) + "]");

    d->readBuffer_.Reset();

    return;
  }

  if (response.functionCode() != request->functionCode()) {
    log(d->log_prefix_, LogLevel::kWarning,
        d->device_->name() +
            ":got response, unexpected functioncode, discard it.[" +
            dump(d->transferMode_, qdata) + "]");

    d->readBuffer_.Reset();

    return;
  }

  if (response.transactionId() != request->transactionId()) {
    log(d->log_prefix_, LogLevel::kWarning,
        d->device_->name() +
            ":got response, unexpected transaction Id, discard it.[" +
            dump(d->transferMode_, qdata) + "]");

    d->readBuffer_.Reset();

    return;
  }

  d->waitTimerAlive_ = false;
  d->waitResponseTimer_->stop();
  d->sessionState_.setState(SessionState::kIdle);

  if (d->enableDump_) {
    log(d->log_prefix_, LogLevel::kDebug,
        d->device_->name() + " recived " +
            dump(d->transferMode_, element->dumpReadArray));
  }

  if (response.isException()) {
    log(d->log_prefix_, LogLevel::kError, response.errorString());
  }

  /**
   * Pop at the end
   */
  auto e = d->elementQueue_.front();
  d->elementQueue_.pop_front();
  emit requestFinished(*e->request, response);
  delete e;
  d->scheduleNextRequest(d->t3_5_);
}

void QModbusClient::onIoDeviceBytesWritten(qint16 bytes) {
  Q_D(QModbusClient);

  assert(d->sessionState_.state() == SessionState::kSendingRequest &&
         "when write operation is not done, the session state must be in "
         "kSendingRequest");

  /*check the request is sent done*/
  auto &element = d->elementQueue_.front();
  auto &request = element->request;
  element->bytesWritten += bytes;
  if (element->bytesWritten != element->totalBytes) {
    return;
  }

  if (request->isBrocast()) {
    auto e = d->elementQueue_.front();
    d->elementQueue_.pop_front();
    delete e;
    d->sessionState_.setState(SessionState::kIdle);
    d->decoder_->Clear();
    d->scheduleNextRequest(d->waitConversionDelay_);

    log(d->log_prefix_, LogLevel::kWarning,
        d->device_->name() + " brocast request, turn into idle status");
    return;
  }

  /**
   * According to the modebus rtu master station state diagram, when the
   * request is sent to the child node, the response timeout timer is
   * started. If the response times out, the next step is to retry. After
   * the number of retries is exceeded, error processing is performed
   * (return to the user).
   */
  d->sessionState_.setState(SessionState::kWaitingResponse);
  d->waitResponseTimer_->setSingleShot(true);
  d->waitResponseTimer_->setInterval(d->waitResponseTimeout_);
  d->waitTimerAlive_ = true;
  d->waitResponseTimer_->start();
}

void QModbusClient::onIoDeviceError(const QString &errorString) {
  Q_D(QModbusClient);

  d->errorString_ = errorString;

  switch (d->sessionState_.state()) {
  case SessionState::kWaitingResponse:
    d->waitResponseTimer_->stop();
  default:
    break;
  }

  d->waitTimerAlive_ = false;
  d->sessionState_.setState(SessionState::kIdle);
  d->decoder_->Clear();
  emit errorOccur(errorString);
}

void QModbusClient::processResponseAnyFunctionCode(const Request &request,
                                                   const Response &response) {
  processDiagnosis(request, response);
  try {
    /// any_cast maybe thown exception
    processFunctionCode(request, response);
  } catch (...) {
  }
}

void QModbusClient::processDiagnosis(const Request &request,
                                     const Response &response) {
  Q_D(QModbusClient);
  if (!d->enableDiagnosis_) {
    return;
  }
  auto &runtimeDiagnosis = d->runtimeDiagnosis_;
  if (response.error() == Error::kNoError) {
    runtimeDiagnosis.incrementtotalFrameNumbers();
    return;
  }
  runtimeDiagnosis.insertErrorRecord(request.serverAddress(),
                                     request.functionCode(), response.error(),
                                     request.data());
}

void QModbusClient::processFunctionCode(const Request &request,
                                        const Response &response) {
  Q_D(QModbusClient);

  const any &data = request.userData();
  if (data.empty()) {
    return;
  }
  switch (request.functionCode()) {
  case FunctionCode::kReadCoils:
  case FunctionCode::kReadInputDiscrete: {
    auto access = modbus::any::any_cast<SingleBitAccess>(data);
    bool ok = false;
    if (!response.isException()) {
      ok = processReadSingleBit(request, response, &access, d->log_prefix_);
    }
    if (ok) {
      emit readSingleBitsFinished(request.serverAddress(),
                                  request.functionCode(), access.startAddress(),
                                  access.quantity(), toBitValueList(access),
                                  response.error());
    }
    return;
  }
  case FunctionCode::kWriteSingleCoil: {
    auto access = modbus::any::any_cast<SingleBitAccess>(data);
    emit writeSingleCoilFinished(request.serverAddress(), access.startAddress(),
                                 response.error());
    return;
  }
  case FunctionCode::kWriteMultipleCoils: {
    auto access = modbus::any::any_cast<SingleBitAccess>(data);
    emit writeMultipleCoilsFinished(request.serverAddress(),
                                    access.startAddress(), response.error());
    return;
  }
  case FunctionCode::kReadHoldingRegisters:
  case FunctionCode::kReadInputRegister: {
    auto access = modbus::any::any_cast<SixteenBitAccess>(data);
    bool ok = false;
    if (!response.isException()) {
      ok = processReadRegisters(request, response, &access, d->log_prefix_);
    }
    if (ok) {
      emit readRegistersFinished(request.serverAddress(),
                                 request.functionCode(), access.startAddress(),
                                 access.quantity(), access.value(),
                                 response.error());
    }
    return;
  }
  case FunctionCode::kWriteSingleRegister: {
    auto access = modbus::any::any_cast<SixteenBitAccess>(data);
    emit writeSingleRegisterFinished(request.serverAddress(),
                                     access.startAddress(), response.error());
    return;
  }
  case FunctionCode::kWriteMultipleRegisters: {
    auto access = modbus::any::any_cast<SixteenBitAccess>(data);
    emit writeMultipleRegistersFinished(
        request.serverAddress(), access.startAddress(), response.error());
    return;
  }
  case FunctionCode::kReadWriteMultipleRegisters: {
    auto access = modbus::any::any_cast<ReadWriteRegistersAccess>(data);
    auto readAccess = access.readAccess;
    if (!response.isException()) {
      processReadRegisters(request, response, &readAccess, d->log_prefix_);
    }
    emit readWriteMultipleRegistersFinished(
        request.serverAddress(), readAccess.startAddress(),
        toSixteenBitValueList(readAccess), response.error());
    return;
  }
  default:
    return;
  }
}

static QVector<SixteenBitValue>
toSixteenBitValueList(const SixteenBitAccess &access) {
  QVector<SixteenBitValue> valueList;

  for (size_t i = 0; i < access.quantity(); i++) {
    Address address = access.startAddress() + i;

    bool found = true;
    SixteenBitValue value = access.value(address, &found);
    if (!found) {
      continue;
    }
    valueList.push_back(value);
  }
  return valueList;
}

static ByteArray toBitValueList(const SingleBitAccess &access) {
  ByteArray valueList;

  valueList.reserve(access.quantity());

  for (size_t i = 0; i < access.quantity(); i++) {
    Address address = access.startAddress() + i;
    valueList.emplace_back(access.value(address));
  }
  return valueList;
}

Request createRequest(ServerAddress serverAddress, FunctionCode functionCode,
                      const any &userData, const ByteArray &data) {
  return Request(serverAddress, functionCode, userData, data);
}

QModbusClient *createClient(const QString &url, QObject *parent) {
  static const QStringList schemaSupported = {"modbus.file", "modbus.tcp",
                                              "modbus.udp"};
  internal::Config config = internal::parseConfig(url);

  bool ok =
      std::any_of(schemaSupported.begin(), schemaSupported.end(),
                  [config](const QString &el) { return config.scheme == el; });
  if (!ok) {
    log("", LogLevel::kError,
        "unsupported scheme {}, see modbus.file:/// or modbus.tcp:// or "
        "modbus.udp://",
        config.scheme.toStdString());
    return nullptr;
  }

  log("", LogLevel::kInfo, "instanced modbus client on {}", url.toStdString());
  if (config.scheme == "modbus.file") {
    return newQtSerialClient(config.serialName, config.baudRate,
                             config.dataBits, config.parity, config.stopBits,
                             parent);
  } else if (config.scheme == "modbus.tcp") {
    return newSocketClient(QAbstractSocket::TcpSocket, config.host, config.port,
                           parent);
  } else if (config.scheme == "modbus.udp") {
    return newSocketClient(QAbstractSocket::UdpSocket, config.host, config.port,
                           parent);
  }
  return nullptr;
}

} // namespace modbus
