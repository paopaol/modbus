#include "modbus_serial_client_p.h"
#include <QTimer>
#include <algorithm>
#include <assert.h>
#include <base/modbus_logger.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>
#include <modbus_frame.h>

namespace modbus {

static void appendQByteArray(ByteArray &array, const QByteArray &qarray);
std::shared_ptr<Frame> createModebusFrame(TransferMode mode);

QModbusClient::QModbusClient(AbstractSerialPort *serialPort, QObject *parent)
    : d_ptr(new QModbusClientPrivate(serialPort, this)), QObject(parent) {
  initMemberValues();
  setupEnvironment();
}

QModbusClient::QModbusClient(QObject *parent)
    : d_ptr(new QModbusClientPrivate(nullptr, parent)), QObject(parent) {
  initMemberValues();
  setupEnvironment();
}

QModbusClient::~QModbusClient() {
  Q_D(QModbusClient);

  if (isOpened()) {
    close();
  }
  if (d->serialPort_) {
    d->serialPort_->deleteLater();
  }
}

void QModbusClient::open() {
  Q_D(QModbusClient);

  if (!isClosed()) {
    log(LogLevel::kInfo,
        d->serialPort_->name() + ": is already opened or opening or closing");
    return;
  }

  d->connectionState_.setState(ConnectionState::kOpening);
  d->serialPort_->open();
  return;
}

/**
 * Allows shutdown and transmits clientClosed signal regardless of whether the
 * device is already turned on
 */
void QModbusClient::close() {
  Q_D(QModbusClient);
  d->forceClose_ = true;
  closeNotClearOpenRetrys();
}

void QModbusClient::closeNotClearOpenRetrys() {
  Q_D(QModbusClient);

  if (!isOpened()) {
    log(LogLevel::kInfo,
        d->serialPort_->name() + ": is already closed or closing or opening");
    return;
  }

  d->connectionState_.setState(ConnectionState::kClosing);
  d->serialPort_->close();
}

void QModbusClient::sendRequest(const Request &request) {
  Q_D(QModbusClient);

  if (!isOpened()) {
    log(LogLevel::kWarning,
        d->serialPort_->name() + " closed, discard reuqest");
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

bool QModbusClient::isClosed() {
  Q_D(QModbusClient);

  return d->connectionState_.state() == ConnectionState::kClosed;
}

bool QModbusClient::isOpened() {
  Q_D(QModbusClient);

  return d->connectionState_.state() == ConnectionState::kOpened;
}

bool QModbusClient::isIdle() {
  Q_D(QModbusClient);
  return d->sessionState_.state() == SessionState::kIdle;
}

void QModbusClient::setupEnvironment() {
  qRegisterMetaType<Request>("Request");
  qRegisterMetaType<Response>("Response");
  Q_D(QModbusClient);

  assert(d->serialPort_ && "the serialport backend is invalid");
  connect(d->serialPort_, &AbstractSerialPort::opened, this,
          &QModbusClient::onSerialPortOpened);
  connect(d->serialPort_, &AbstractSerialPort::closed, this,
          &QModbusClient::onSerialPortClosed);
  connect(d->serialPort_, &AbstractSerialPort::error, this,
          &QModbusClient::onSerialPortError);
  connect(d->serialPort_, &AbstractSerialPort::bytesWritten, this,
          &QModbusClient::onSerialPortBytesWritten);
  connect(d->serialPort_, &AbstractSerialPort::readyRead, this,
          &QModbusClient::onSerialPortReadyRead);
  connect(&d->waitResponseTimer_, &QTimer::timeout, this,
          &QModbusClient::onSerialPortResponseTimeout);
}

void QModbusClient::setTimeout(uint64_t timeout) {
  Q_D(QModbusClient);

  d->waitResponseTimeout_ = timeout;
}

uint64_t QModbusClient::timeout() {
  Q_D(QModbusClient);

  return d->waitResponseTimeout_;
}

void QModbusClient::setTransferMode(modbus::TransferMode transferMode) {
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
  if (retryTimes < 0) {
    retryTimes = kBrokenLineReconnection;
  }
  d->openRetryTimes_ = retryTimes;

  if (delay < 0) {
    delay = 0;
  }
  d->reopenDelay_ = delay;
}

int QModbusClient::openRetryTimes() {
  Q_D(QModbusClient);
  return d->openRetryTimes_;
}

int QModbusClient::openRetryDelay() {
  Q_D(QModbusClient);
  return d->reopenDelay_;
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
}

size_t QModbusClient::pendingRequestSize() {
  Q_D(QModbusClient);
  return d->elementQueue_.size();
}

QString QModbusClient::errorString() {
  Q_D(QModbusClient);
  return d->errorString_;
}

void QModbusClient::initMemberValues() {
  Q_D(QModbusClient);

  d->connectionState_.setState(ConnectionState::kClosed);
  d->sessionState_.setState(SessionState::kIdle);
  d->waitConversionDelay_ = 200;
  d->t3_5_ = 60;
  d->waitResponseTimeout_ = 1000;
  d->retryTimes_ = 0; /// default no retry
  d->openRetryTimes_ = 0;
  d->reopenDelay_ = 1000;
  d->transferMode_ = TransferMode::kRtu;
}

void QModbusClient::onSerialPortResponseTimeout() {
  Q_D(QModbusClient);
  assert(d->sessionState_.state() == SessionState::kWaitingResponse);

  auto &element = d->elementQueue_.front();
  element.bytesWritten = 0;

  /**
   *  An error occurs when the response times out but no response is
   * received. Then the master node enters the "idle" state and issues a
   * retry request. The maximum number of retries depends on the settings
   * of the primary node
   *
   */
  d->sessionState_.setState(SessionState::kIdle);

  if (element.retryTimes-- > 0) {
    log(LogLevel::kWarning,
        d->serialPort_->name() +
            ": waiting response timeout, retry it, retrytimes " +
            std::to_string(element.retryTimes));

    d->scheduleNextRequest(d->t3_5_);
  } else {
    log(LogLevel::kWarning,
        d->serialPort_->name() + ": waiting response timeout");

    auto request = element.request;
    auto response = element.response;
    /**
     * if have no retry times, remove this request
     */
    d->elementQueue_.pop();
    response.setError(modbus::Error::kTimeout);
    emit requestFinished(request, response);
    d->scheduleNextRequest(d->t3_5_);
  }
}

void QModbusClient::onSerialPortReadyRead() {
  Q_D(QModbusClient);

  /**
   * When the last byte of the request is sent, it will enter the wait-response
   * state. Therefore, if data is received but not in the wait-response state,
   * then this data is not what we want,discard them
   */
  auto qdata = d->serialPort_->readAll();
  if (d->sessionState_.state() != SessionState::kWaitingResponse) {
    ByteArray data;
    appendQByteArray(data, qdata);
    std::stringstream stream;
    stream << d->sessionState_.state();
    log(LogLevel::kWarning, d->serialPort_->name() + " now state is in " +
                                stream.str() +
                                ".got unexpected data, discard them." + "[" +
                                tool::dumpHex(data) + "]");

    d->serialPort_->clear();
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
    log(LogLevel::kWarning, d->serialPort_->name() + ":need more data." + "[" +
                                tool::dumpHex(dataRecived) + "]");
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
        d->serialPort_->name() +
            ":got response, unexpected serveraddress, discard it.[" +
            tool::dumpHex(dataRecived) + "]");

    dataRecived.clear();
    return;
  }

  d->waitResponseTimer_.stop();
  d->sessionState_.setState(SessionState::kIdle);

  log(LogLevel::kDebug,
      d->serialPort_->name() + " recived " + tool::dumpHex(dataRecived));

  /**
   * Pop at the end
   */
  d->elementQueue_.pop();
  emit requestFinished(request, response);
  d->scheduleNextRequest(d->t3_5_);
}

void QModbusClient::onSerialPortBytesWritten(qint16 bytes) {
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
        d->serialPort_->name() + " brocast request, turn into idle status");
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

void QModbusClient::onSerialPortError(const QString &errorString) {
  Q_D(QModbusClient);

  d->errorString_ = errorString;
  /**
   * no error
   */
  if (d->errorString_.isEmpty()) {
    return;
  }

  switch (d->sessionState_.state()) {
  case SessionState::kWaitingResponse:
    d->waitResponseTimer_.stop();
  default:
    break;
  }

  d->sessionState_.setState(SessionState::kIdle);
  if (d->openRetryTimes_ == 0) {
    emit errorOccur(errorString);
  }
  log(LogLevel::kWarning,
      d->serialPort_->name() + " " + errorString.toStdString());
  if (isOpened()) {
    closeNotClearOpenRetrys();
  } else {
    onSerialPortClosed();
  }
} // namespace modbus

void QModbusClient::onSerialPortClosed() {
  Q_D(QModbusClient);
  d->connectionState_.setState(ConnectionState::kClosed);
  /**
   * closed final,clear all pending request
   */
  clearPendingRequest();

  /// force close, do not check reconnect
  if (d->forceClose_) {
    d->forceClose_ = false;
    emit clientClosed();
    return;
  }

  // check reconnect
  if (d->openRetryTimes_ == 0) {
    emit clientClosed();
    return;
  }

  /// do reconnect
  log(LogLevel::kWarning, d->serialPort_->name() +
                              " closed, try reconnect after " +
                              std::to_string(d->reopenDelay_) + "ms");
  d->openRetryTimes_ > 0 ? --d->openRetryTimes_ : (int)0;
  QTimer::singleShot(d->reopenDelay_, this, &QModbusClient::open);
}

void QModbusClient::onSerialPortOpened() {
  Q_D(QModbusClient);
  d->connectionState_.setState(ConnectionState::kOpened);
  emit clientOpened();
}

static void appendQByteArray(ByteArray &array, const QByteArray &qarray) {
  uint8_t *data = (uint8_t *)qarray.constData();
  size_t size = qarray.size();
  array.insert(array.end(), data, data + size);
}

std::shared_ptr<Frame> createModebusFrame(TransferMode mode) {
  switch (mode) {
  case TransferMode::kRtu:
    return std::make_shared<RtuFrame>();
  default:
    return nullptr;
  }
}

} // namespace modbus
