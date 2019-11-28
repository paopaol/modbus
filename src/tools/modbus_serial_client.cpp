#include "modbus_serial_client_p.h"
#include <QTimer>
#include <algorithm>
#include <assert.h>
#include <modbus/base/modbus_exception_datachecket.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>

namespace modbus {
static void appendQByteArray(ByteArray &array, const QByteArray &qarray);

QSerialClient::QSerialClient(AbstractSerialPort *serialPort, QObject *parent)
    : d_ptr(new QSerialClientPrivate(serialPort, this)), Client(parent) {
  initMemberValues();
  setupEnvironment();
}

QSerialClient::QSerialClient(QObject *parent)
    : d_ptr(new QSerialClientPrivate(nullptr, parent)), Client(parent) {
  initMemberValues();
  setupEnvironment();
}

QSerialClient::~QSerialClient() {
  Q_D(QSerialClient);

  if (isOpened()) {
    close();
  }
  if (d->serialPort_) {
    d->serialPort_->deleteLater();
  }
}

void QSerialClient::open() {
  if (!isClosed()) {
    // FIXME:add log
    return;
  }
  Q_D(QSerialClient);

  d->connectionState_.setState(ConnectionState::kOpening);
  d->serialPort_->open();
  return;
}

/**
 * Allows shutdown and transmits clientClosed signal regardless of whether the
 * device is already turned on
 */
void QSerialClient::close() {
  Q_D(QSerialClient);
  d->forceClose_ = true;
  closeNotClearOpenRetrys();
}

void QSerialClient::closeNotClearOpenRetrys() {
  Q_D(QSerialClient);

  d->connectionState_.setState(ConnectionState::kClosing);
  d->serialPort_->close();
}

void QSerialClient::sendRequest(const Request &request) {
  Q_D(QSerialClient);

  /*just queue the request, when the session state is in idle, it will be sent
   * out*/
  auto element = createElement(request);
  d->enqueueElement(element);
}

bool QSerialClient::isClosed() {
  Q_D(QSerialClient);

  return d->connectionState_.state() == ConnectionState::kClosed;
}

bool QSerialClient::isOpened() {
  Q_D(QSerialClient);

  return d->connectionState_.state() == ConnectionState::kOpened;
}

bool QSerialClient::isIdle() {
  Q_D(QSerialClient);
  return d->sessionState_.state() == SessionState::kIdle;
}

void QSerialClient::setupEnvironment() {
  qRegisterMetaType<Request>("Request");
  qRegisterMetaType<Response>("Response");
  Q_D(QSerialClient);

  assert(d->serialPort_ && "the serialport backend is invalid");
  connect(d->serialPort_, &AbstractSerialPort::opened, this,
          &QSerialClient::onSerialPortOpened);
  connect(d->serialPort_, &AbstractSerialPort::closed, this,
          &QSerialClient::onSerialPortClosed);
  connect(d->serialPort_, &AbstractSerialPort::error, this,
          &QSerialClient::onSerialPortError);
  connect(d->serialPort_, &AbstractSerialPort::bytesWritten, this,
          &QSerialClient::onSerialPortBytesWritten);
  connect(d->serialPort_, &AbstractSerialPort::readyRead, this,
          &QSerialClient::onSerialPortReadyRead);
  connect(&d->waitResponseTimer_, &QTimer::timeout, this,
          &QSerialClient::onSerialPortResponseTimeout);
}

void QSerialClient::setTimeout(uint64_t timeout) {
  Q_D(QSerialClient);

  d->waitResponseTimeout_ = timeout;
}

uint64_t QSerialClient::timeout() {
  Q_D(QSerialClient);

  return d->waitResponseTimeout_;
}

void QSerialClient::setRetryTimes(int times) {
  Q_D(QSerialClient);

  d->retryTimes_ = std::max(0, times);
}

int QSerialClient::retryTimes() {
  Q_D(QSerialClient);
  return d->retryTimes_;
}

void QSerialClient::setOpenRetryTimes(int retryTimes, int delay) {
  Q_D(QSerialClient);
  if (retryTimes < 0) {
    retryTimes = Client::kBrokenLineReconnection;
  }
  d->openRetryTimes_ = retryTimes;

  if (delay < 0) {
    delay = 0;
  }
  d->reopenDelay_ = delay;
}

int QSerialClient::openRetryTimes() {
  Q_D(QSerialClient);
  return d->openRetryTimes_;
}

int QSerialClient::openRetryDelay() {
  Q_D(QSerialClient);
  return d->reopenDelay_;
}

void QSerialClient::initMemberValues() {
  Q_D(QSerialClient);

  d->connectionState_.setState(ConnectionState::kClosed);
  d->sessionState_.setState(SessionState::kIdle);
  d->waitConversionDelay_ = 200;
  d->t3_5_ = 100;
  d->waitResponseTimeout_ = 1000;
  d->retryTimes_ = 0; /// default no retry
  d->openRetryTimes_ = 0;
  d->reopenDelay_ = 1000;
}

void QSerialClient::onSerialPortResponseTimeout() {
  Q_D(QSerialClient);
  assert(d->sessionState_.state() == SessionState::kWaitingResponse);

  /// FIXME:add debug log

  auto &element = d->elementQueue_.front();
  element.byteWritten = 0;

  /**
   *  An error occurs when the response times out but no response is
   * received. Then the master node enters the "idle" state and issues a
   * retry request. The maximum number of retries depends on the settings
   * of the primary node
   *
   */
  d->sessionState_.setState(SessionState::kIdle);

  if (d->retryTimes_-- > 0) {
    d->scheduleNextRequest(d->t3_5_);
  } else {
    auto request = element.request;
    auto response = element.response;
    /**
     * if have no retry times, remove this request
     */
    d->elementQueue_.pop();
    response.setError(modbus::Error::kTimeout, "timeout");
    emit requestFinished(request, response);
  }
}

void QSerialClient::onSerialPortReadyRead() {
  Q_D(QSerialClient);

  auto &element = d->elementQueue_.front();
  auto &dataRecived = element.dataRecived;
  auto request = element.request;
  auto response = element.response;

  appendQByteArray(dataRecived, d->serialPort_->readAll());
  /// make sure got serveraddress + function code
  if (dataRecived.size() < 2) {
    /// FIXME:log
    return;
  }

  response.setServerAddress(dataRecived[0]);
  response.setFunctionCode(static_cast<FunctionCode>(dataRecived[1]));

  /**
   * When receiving a response from an undesired child node,
   * Should continue to time out
   * discard all recived data
   */
  if (response.serverAddress() != request.serverAddress()) {
    // FIXME:log
    d->serialPort_->clear();
    dataRecived.clear();
    return;
  }

  size_t expectSize = 0;
  DataChecker dataChecker;
  if (response.isException()) {
    dataChecker = expectionResponseDataChecker;
  } else {
    dataChecker = request.dataChecker();
  }

  DataChecker::Result result = dataChecker.calculateResponseSize(
      expectSize, tool::subArray(dataRecived, 2));
  if (result == DataChecker::Result::kNeedMoreData) {
    /// FIXME:log
    return;
  }
  response.setData(tool::subArray(dataRecived, 2, expectSize));
  /// server address(1) + function code(1) + data(expectSize) + crc(2)
  size_t totalSize = 2 + expectSize + 2;
  if (dataRecived.size() != totalSize) {
    /// need more data
    /// FIXME:log
    return;
  }
  d->waitResponseTimer_.stop();
  d->sessionState_.setState(SessionState::kIdle);

  auto dataWithCrc =
      tool::appendCrc(tool::subArray(dataRecived, 0, 2 + expectSize));

  /**
   * Received frame error
   */
  if (dataWithCrc != dataRecived) {
    response.setError(Error::kStorageParityError, "modbus frame parity error");
  }

  if (response.isException()) {
    response.setError(Error(response.data()[0]), "error return");
  }

  /**
   * Pop at the end
   */
  d->elementQueue_.pop();
  emit requestFinished(request, response);
}

void QSerialClient::onSerialPortBytesWritten(qint16 bytes) {
  Q_D(QSerialClient);

  assert(d->sessionState_.state() == SessionState::kSendingRequest &&
         "when write operation is not done, the session state must be in "
         "kSendingRequest");

  /*check the request is sent done*/
  auto &element = d->elementQueue_.front();
  auto &request = element.request;
  element.byteWritten += bytes;
  if (element.byteWritten != request.marshalSize() + 2 /*crc len*/) {
    return;
  }

  if (request.isBrocast()) {
    d->elementQueue_.pop();
    d->sessionState_.setState(SessionState::kIdle);
    d->scheduleNextRequest(d->waitConversionDelay_);
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

void QSerialClient::onSerialPortError(const QString &errorString) {
  Q_D(QSerialClient);

  switch (d->sessionState_.state()) {
  case SessionState::kWaitingResponse:
    d->waitResponseTimer_.stop();
    // fallthough
  case SessionState::kSendingRequest:
    d->elementQueue_.pop();
  default:
    break;
  }
  d->sessionState_.setState(SessionState::kIdle);
  if (d->openRetryTimes_ == 0) {
    emit errorOccur(errorString);
  }
  // FIXME:log
  closeNotClearOpenRetrys();
} // namespace modbus

void QSerialClient::onSerialPortClosed() {
  Q_D(QSerialClient);
  d->connectionState_.setState(ConnectionState::kClosed);

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
  d->openRetryTimes_ > 0 ? --d->openRetryTimes_ : (int)0;
  QTimer::singleShot(d->reopenDelay_, this, &QSerialClient::open);
}

void QSerialClient::onSerialPortOpened() {
  Q_D(QSerialClient);
  d->connectionState_.setState(ConnectionState::kOpened);
  emit clientOpened();
}

static void appendQByteArray(ByteArray &array, const QByteArray &qarray) {
  uint8_t *data = (uint8_t *)qarray.constData();
  size_t size = qarray.size();
  array.insert(array.end(), data, data + size);
}

} // namespace modbus
