#include "modbus_serial_client_p.h"
#include <QTimer>
#include <algorithm>
#include <assert.h>
#include <modbus/base/modbus_tool.h>

namespace modbus {
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

void QSerialClient::close() {
  if (!isOpened()) {
    return;
  }
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

void QSerialClient::setupEnvironment() {
  qRegisterMetaType<Request>("Request");
  qRegisterMetaType<Response>("Response");
  Q_D(QSerialClient);

  assert(d->serialPort_ && "the serialport backend is invalid");
  connect(d->serialPort_, &AbstractSerialPort::opened, [&]() {
    Q_D(QSerialClient);
    d->connectionState_.setState(ConnectionState::kOpened);
    emit clientOpened();
  });
  connect(d->serialPort_, &AbstractSerialPort::closed, [&]() {
    Q_D(QSerialClient);
    d->connectionState_.setState(ConnectionState::kClosed);
    emit clientClosed();
  });
  connect(d->serialPort_, &AbstractSerialPort::error,
          [&](const QString &errorString) {
            emit errorOccur(errorString);
            close();
          });
  connect(d->serialPort_, &AbstractSerialPort::bytesWritten, [&](qint16 bytes) {
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

    /**
     * According to the modebus rtu master station state diagram, when the
     * request is sent to the child node, the response timeout timer is
     * started. If the response times out, the next step is to retry. After
     * the number of retries is exceeded, error processing is performed
     * (return to the user).
     */
    d->sessionState_.setState(SessionState::kWaitingResponse);
    d->waitResponseTimer_.setInterval(d->waitResponseTimeout_);
    d->waitResponseTimer_.start();
  });

  connect(&d->waitResponseTimer_, &QTimer::timeout, this, [&]() {
    Q_D(QSerialClient);
    assert(d->sessionState_.state() == SessionState::kWaitingResponse);

    /// FIXME:add debug log

    auto &element = d->elementQueue_.front();
    element.byteWritten = 0;

    /**
     *  An error occurs when the response times out but no response is received.
     *  Then the master node enters the "idle" state and issues a retry request.
     *  The maximum number of retries depends on the settings of the primary
     * node
     *
     */
    d->sessionState_.setState(SessionState::kIdle);

    if (d->retryTimes_-- > 0) {
      d->scheduleNextRequest();
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
  });
} // namespace modbus

void modbus::QSerialClient::setTimeout(uint64_t timeout) {
  Q_D(QSerialClient);

  d->waitResponseTimeout_ = timeout;
}

uint64_t modbus::QSerialClient::timeout() {
  Q_D(QSerialClient);

  return d->waitResponseTimeout_;
}

void modbus::QSerialClient::setRetryTimes(int times) {
  Q_D(QSerialClient);

  d->retryTimes_ = std::max(0, times);
}

int modbus::QSerialClient::retryTimes() {
  Q_D(QSerialClient);
  return d->retryTimes_;
}

void QSerialClient::initMemberValues() {
  Q_D(QSerialClient);

  d->connectionState_.setState(ConnectionState::kClosed);
  d->sessionState_.setState(SessionState::kIdle);
  d->waitConversionDelay_ = 200;
  d->t3_5_ = 100;
  d->waitResponseTimeout_ = 1000;
  d->retryTimes_ = 0; /// default no retry
}

} // namespace modbus
