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

static void appendQByteArray(ByteArray &array, const QByteArray &qarray);
static QVector<SixteenBitValue>
toSixteenBitValueList(const SixteenBitAccess &access);
static QVector<BitValue> toBitValueList(const SingleBitAccess &access);

struct ReadWriteRegistersAccess {
  SixteenBitAccess readAccess;
  SixteenBitAccess writeAccess;
};

QModbusClient::QModbusClient(AbstractIoDevice *iodevice, QObject *parent)
    : d_ptr(new QModbusClientPrivate(iodevice, this)), QObject(parent) {
  initMemberValues();
  setupEnvironment();
}

QModbusClient::QModbusClient(QObject *parent)
    : d_ptr(new QModbusClientPrivate(nullptr, parent)), QObject(parent) {
  initMemberValues();
  setupEnvironment();
}

QModbusClient::~QModbusClient() {}

void QModbusClient::open() {
  Q_D(QModbusClient);

  d->device_.open();
  return;
}

/**
 * Allows shutdown and transmits clientClosed signal regardless of whether the
 * device is already turned on
 */
void QModbusClient::close() {
  Q_D(QModbusClient);
  d->device_.close();
}

void QModbusClient::sendRequest(const Request &request) {
  Q_D(QModbusClient);

  if (!isOpened()) {
    log(LogLevel::kWarning, "{} closed, discard reuqest", d->device_.name());
    return;
  }

  /*just queue the request, when the session state is in idle, it will be sent
   * out*/
  auto element = createElement(request);

  element.requestFrame = createModebusFrame(d->transferMode_);
  element.requestFrame->setAdu(element.request);

  element.responseFrame = createModebusFrame(d->transferMode_);
  element.responseFrame->setAdu(element.response);

  element.retryTimes = d->retryTimes_;
  d->enqueueElement(element);
}

void QModbusClient::readSingleBits(ServerAddress serverAddress,
                                   FunctionCode functionCode,
                                   Address startAddress, Quantity quantity) {
  if (functionCode != FunctionCode::kReadCoils &&
      functionCode != FunctionCode::kReadInputDiscrete) {
    log(LogLevel::kError, "single bit access:[read] invalid function code(" +
                              std::to_string(functionCode) + ")");
  }

  static const DataChecker dataChecker = {bytesRequiredStoreInArrayIndex<0>};

  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(quantity);

  auto request = createRequest(serverAddress, functionCode, dataChecker, access,
                               access.marshalReadRequest());
  sendRequest(request);
}

void QModbusClient::writeSingleCoil(ServerAddress serverAddress,
                                    Address startAddress, BitValue value) {
  static const DataChecker dataChecker = {bytesRequired<4>};

  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(1);
  access.setValue(value);
  auto request =
      createRequest(serverAddress, FunctionCode::kWriteSingleCoil, dataChecker,
                    access, access.marshalSingleWriteRequest());
  sendRequest(request);
}

void QModbusClient::writeMultipleCoils(ServerAddress serverAddress,
                                       Address startAddress,
                                       const QVector<BitValue> &valueList) {
  static const DataChecker dataChecker = {bytesRequired<4>};
  SingleBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(valueList.size());
  for (size_t offset = 0; offset < valueList.size(); offset++) {
    Address address = startAddress + offset;
    access.setValue(address, valueList[offset]);
  }
  auto request =
      createRequest(serverAddress, FunctionCode::kWriteMultipleCoils,
                    dataChecker, access, access.marshalMultipleWriteRequest());
  sendRequest(request);
}

void QModbusClient::readRegisters(ServerAddress serverAddress,
                                  FunctionCode functionCode,
                                  Address startAddress, Quantity quantity) {
  if (functionCode != FunctionCode::kReadHoldingRegisters &&
      functionCode != FunctionCode::kReadInputRegister) {
    log(LogLevel::kError, "invalid function code for read registers" +
                              std::to_string(functionCode));
  }

  static const DataChecker dataChecker = {bytesRequiredStoreInArrayIndex<0>};

  SixteenBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(quantity);

  auto request = createRequest(serverAddress, functionCode, dataChecker, access,
                               access.marshalMultipleReadRequest());

  sendRequest(request);
}

void QModbusClient::writeSingleRegister(ServerAddress serverAddress,
                                        Address address,
                                        const SixteenBitValue &value) {
  static const DataChecker dataChecker = {bytesRequired<4>};
  SixteenBitAccess access;

  access.setStartAddress(address);
  access.setValue(value.toUint16());

  auto request =
      createRequest(serverAddress, FunctionCode::kWriteSingleRegister,
                    dataChecker, access, access.marshalSingleWriteRequest());
  sendRequest(request);
}

void QModbusClient::writeMultipleRegisters(
    ServerAddress serverAddress, Address startAddress,
    const QVector<SixteenBitValue> &valueList) {
  static const DataChecker dataChecker = {bytesRequired<4>};
  SixteenBitAccess access;

  access.setStartAddress(startAddress);
  access.setQuantity(valueList.size());

  int offset = 0;
  for (const auto &sixValue : valueList) {
    auto address = access.startAddress() + offset;
    access.setValue(address, sixValue.toUint16());
    offset++;
  }
  auto request =
      createRequest(serverAddress, FunctionCode::kWriteMultipleRegisters,
                    dataChecker, access, access.marshalMultipleWriteRequest());
  sendRequest(request);
}

void QModbusClient::readWriteMultipleRegisters(
    ServerAddress serverAddress, Address readStartAddress,
    Quantity readQuantity, Address writeStartAddress,
    const QVector<SixteenBitValue> &valueList) {
  static const DataChecker dataChecker = {bytesRequiredStoreInArrayIndex<0>};

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
  auto request =
      createRequest(serverAddress, FunctionCode::kReadWriteMultipleRegisters,
                    dataChecker, access, data);
  sendRequest(request);
}

bool QModbusClient::isIdle() {
  Q_D(QModbusClient);
  return d->sessionState_.state() == SessionState::kIdle;
}

bool QModbusClient::isClosed() {
  Q_D(QModbusClient);
  return d->device_.isClosed();
}

bool QModbusClient::isOpened() {
  Q_D(QModbusClient);
  return d->device_.isOpened();
}

void QModbusClient::setupEnvironment() {
  qRegisterMetaType<Request>("Request");
  qRegisterMetaType<Response>("Response");
  qRegisterMetaType<SixteenBitAccess>("SixteenBitAccess");
  qRegisterMetaType<ServerAddress>("ServerAddress");
  qRegisterMetaType<Address>("Address");
  qRegisterMetaType<Error>("Error");
  qRegisterMetaType<QVector<SixteenBitValue>>("QVector<SixteenBitValue>");
  qRegisterMetaType<QVector<BitValue>>("QVector<BitValue>");
  qRegisterMetaType<FunctionCode>("FunctionCode");
  qRegisterMetaType<Quantity>("Quantity");

  Q_D(QModbusClient);

  connect(&d->device_, &ReconnectableIoDevice::opened, this,
          &QModbusClient::clientOpened);
  connect(&d->device_, &ReconnectableIoDevice::closed, this,
          &QModbusClient::clientClosed);
  connect(&d->device_, &ReconnectableIoDevice::error, this,
          &QModbusClient::clearPendingRequest);
  connect(&d->device_, &ReconnectableIoDevice::connectionIsLostWillReconnect,
          this, &QModbusClient::clearPendingRequest);
  connect(&d->device_, &ReconnectableIoDevice::connectionIsLostWillReconnect,
          this, &QModbusClient::connectionIsLostWillReconnect);
  connect(&d->device_, &ReconnectableIoDevice::error, this,
          &QModbusClient::onIoDeviceError);
  connect(&d->device_, &ReconnectableIoDevice::bytesWritten, this,
          &QModbusClient::onIoDeviceBytesWritten);
  connect(&d->device_, &ReconnectableIoDevice::readyRead, this,
          &QModbusClient::onIoDeviceReadyRead);
  connect(&d->waitResponseTimer_, &QTimer::timeout, this,
          &QModbusClient::onIoDeviceResponseTimeout);
  connect(this, &QModbusClient::requestFinished, this,
          &QModbusClient::processResponseAnyFunctionCode);
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
  d->device_.setOpenRetryTimes(retryTimes, delay);
}

int QModbusClient::openRetryTimes() {
  Q_D(QModbusClient);
  return d->device_.openRetryTimes();
}

int QModbusClient::openRetryDelay() {
  Q_D(QModbusClient);
  return d->device_.openRetryDelay();
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
    d->elementQueue_.pop();
  }
  d->waitResponseTimer_.stop();
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

void QModbusClient::enableDiagnosis(bool enable) {
  Q_D(QModbusClient);
  if (d->enableDiagnosis_ == enable) {
    return;
  }
  d->enableDiagnosis_ = enable;
}

RuntimeDiagnosis QModbusClient::runtimeDiagnosis() const {
  const Q_D(QModbusClient);
  return d->runtimeDiagnosis_;
}

void QModbusClient::initMemberValues() {
  Q_D(QModbusClient);

  d->sessionState_.setState(SessionState::kIdle);
  d->waitConversionDelay_ = 200;
  d->t3_5_ = 60;
  d->waitResponseTimeout_ = 1000;
  d->retryTimes_ = 0; /// default no retry
  d->transferMode_ = TransferMode::kRtu;
}

void QModbusClient::onIoDeviceResponseTimeout() {
  Q_D(QModbusClient);
  assert(d->sessionState_.state() == SessionState::kWaitingResponse);

  auto &element = d->elementQueue_.front();
  element.bytesWritten = 0;
  element.dataRecived.clear();

  /**
   *  An error occurs when the response times out but no response is
   * received. Then the master node enters the "idle" state and issues a
   * retry request. The maximum number of retries depends on the settings
   * of the primary node
   *
   */
  d->sessionState_.setState(SessionState::kIdle);

  auto request = element.request;
  auto response = element.response;

  if (element.retryTimes-- > 0) {
    log(LogLevel::kWarning,
        "{} waiting response timeout, retry it, retrytimes ", d->device_.name(),
        element.retryTimes);

    response.setFunctionCode(request.functionCode());
    response.setServerAddress(request.serverAddress());
    response.setError(Error::kTimeout);
    processDiagnosis(request, response);
  } else {
    log(LogLevel::kWarning, "{}: waiting response timeout", d->device_.name());

    /**
     * if have no retry times, remove this request
     */
    d->elementQueue_.pop();
    response.setError(Error::kTimeout);
    emit requestFinished(request, response);
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
  auto qdata = d->device_.readAll();
  if (d->sessionState_.state() != SessionState::kWaitingResponse) {
    ByteArray data;
    appendQByteArray(data, qdata);
    std::stringstream stream;
    stream << d->sessionState_.state();
    log(LogLevel::kWarning,
        "{} now state is in {}.got unexpected data, discard them.[{}]",
        d->device_.name(), stream.str(), d->dump(data));

    d->device_.clear();
    return;
  }

  auto &element = d->elementQueue_.front();
  auto &dataRecived = element.dataRecived;
  auto request = element.request;
  auto response = element.response;

  auto sessionState = d->sessionState_.state();

  appendQByteArray(dataRecived, qdata);

  Error error = Error::kNoError;
  auto result = element.responseFrame->unmarshal(dataRecived, &error);
  if (result != DataChecker::Result::kSizeOk) {
    log(LogLevel::kWarning, d->device_.name() + ":need more data." + "[" +
                                d->dump(dataRecived) + "]");
    return;
  }

  response = Response(element.responseFrame->adu());
  response.setError(error);

  /**
   * When receiving a response from an undesired child node,
   * Should continue to time out
   * discard all recived dat
   */
  if (response.serverAddress() != request.serverAddress()) {
    log(LogLevel::kWarning,
        d->device_.name() +
            ":got response, unexpected serveraddress, discard it.[" +
            d->dump(dataRecived) + "]");

    dataRecived.clear();
    return;
  }

  d->waitResponseTimer_.stop();
  d->sessionState_.setState(SessionState::kIdle);

  log(LogLevel::kDebug, d->device_.name() + " recived " + d->dump(dataRecived));

  /**
   * Pop at the end
   */
  d->elementQueue_.pop();
  emit requestFinished(request, response);
  d->scheduleNextRequest(d->t3_5_);
}

void QModbusClient::onIoDeviceBytesWritten(qint16 bytes) {
  Q_D(QModbusClient);

  assert(d->sessionState_.state() == SessionState::kSendingRequest &&
         "when write operation is not done, the session state must be in "
         "kSendingRequest");

  /*check the request is sent done*/
  auto &element = d->elementQueue_.front();
  auto &request = element.request;
  element.bytesWritten += bytes;
  if (element.bytesWritten != element.requestFrame->marshalSize()) {
    return;
  }

  if (request.isBrocast()) {
    d->elementQueue_.pop();
    d->sessionState_.setState(SessionState::kIdle);
    d->scheduleNextRequest(d->waitConversionDelay_);

    log(LogLevel::kWarning,
        d->device_.name() + " brocast request, turn into idle status");
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
  d->waitResponseTimer_.setSingleShot(true);
  d->waitResponseTimer_.setInterval(d->waitResponseTimeout_);
  d->waitResponseTimer_.start();
}

void QModbusClient::onIoDeviceError(const QString &errorString) {
  Q_D(QModbusClient);

  d->errorString_ = errorString;

  switch (d->sessionState_.state()) {
  case SessionState::kWaitingResponse:
    d->waitResponseTimer_.stop();
  default:
    break;
  }

  d->sessionState_.setState(SessionState::kIdle);
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
  const auto &data = request.userData();
  if (data.empty()) {
    return;
  }
  switch (request.functionCode()) {
  case FunctionCode::kReadCoils:
  case FunctionCode::kReadInputDiscrete: {
    auto access = modbus::any::any_cast<SingleBitAccess>(data);
    if (!response.isException()) {
      processReadSingleBit(request, response, &access);
    }
    emit readSingleBitsFinished(request.serverAddress(), request.functionCode(),
                                access.startAddress(), access.quantity(),
                                toBitValueList(access), response.error());
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
    if (!response.isException()) {
      processReadRegisters(request, response, &access);
    }
    emit readRegistersFinished(request.serverAddress(), request.functionCode(),
                               access.startAddress(), access.quantity(),
                               toSixteenBitValueList(access), response.error());
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
      processReadRegisters(request, response, &readAccess);
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

static void appendQByteArray(ByteArray &array, const QByteArray &qarray) {
  uint8_t *data = (uint8_t *)qarray.constData();
  size_t size = qarray.size();
  array.insert(array.end(), data, data + size);
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

static QVector<BitValue> toBitValueList(const SingleBitAccess &access) {
  QVector<BitValue> valueList;

  for (size_t i = 0; i < access.quantity(); i++) {
    Address address = access.startAddress() + i;

    BitValue value = access.value(address);
    if (value == BitValue::kBadValue) {
      continue;
    }
    valueList.push_back(value);
  }
  return valueList;
}

Request createRequest(ServerAddress serverAddress, FunctionCode functionCode,
                      const DataChecker &dataChecker, const any &userData,
                      const ByteArray &data) {
  Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(functionCode);
  request.setUserData(userData);
  request.setDataChecker(dataChecker);
  request.setData(data);

  return request;
}

QModbusClient *createClient(const QString &url, QObject *parent) {
  static const QStringList schemaSupported = {"modbus.file", "modbus.tcp",
                                              "modbus.udp"};
  internal::Config config = internal::parseConfig(url);

  bool ok =
      std::any_of(schemaSupported.begin(), schemaSupported.end(),
                  [config](const QString &el) { return config.scheme == el; });
  if (!ok) {
    log(LogLevel::kError,
        "unsupported scheme {}, see modbus.file:/// or modbus.tcp:// or "
        "modbus.udp://",
        config.scheme.toStdString());
    return nullptr;
  }

  log(LogLevel::kInfo, "instanced modbus client on {}", url.toStdString());
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
