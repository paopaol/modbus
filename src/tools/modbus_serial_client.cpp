#include "modbus_serial_client_p.h"
#include <QTimer>
#include <assert.h>
#include <modbus/base/modbus_tool.h>

namespace modbus {
QSerialClient::QSerialClient(AbstractSerialPort *serialPort, QObject *parent)
    : d_ptr(new QSerialClientPrivate(serialPort, parent)), Client(parent) {
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
  d->elementQueue_.push(element);
  if (d->sessionState_.state() != SessionState::kIdle) {
    return;
  }
  /*after some delay, the request will be sent,so we change the state to sending
   * request*/
  d->sessionState_.setState(SessionState::kSendingRequest);

  runAfter(d->t3_5_, [&]() {
    Q_D(QSerialClient);

    /**
     * take out the first request,send it out,
     */
    auto &ele = d->elementQueue_.front();
    auto &request = ele.request;
    auto data = request.marshalData();
    /**
     * we append crc, then write to serialport
     */
    auto modbusSerialData = tool::appendCrc(data);
    d->serialPort_->write(
        QByteArray(reinterpret_cast<const char *>(modbusSerialData.data())),
        modbusSerialData.size());
  });
}

bool QSerialClient::isClosed() {
  Q_D(QSerialClient);

  return d->connectionState_.state() == ConnectionState::kClosed;
}

bool QSerialClient::isOpened() {
  Q_D(QSerialClient);

  return d->connectionState_.state() == ConnectionState::kOpened;
}

void QSerialClient::runAfter(int delay, const std::function<void()> &functor) {
  QTimer::singleShot(delay, functor);
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

    /*check the request is sent done*/
    auto &element = d->elementQueue_.front();
    auto &request = element.request;
    element.byteWritten_ += bytes;
    // if (element.byteWritten_ == request.marshalSize() + 2 /*crc len*/) {
    //   d->waitResponseTimer_.start();
    //   return;
    // }
  });
}

void modbus::QSerialClient::setTimeout(uint64_t timeout) {
  Q_D(QSerialClient);

  d->waitResponseTimeout_ = timeout;
}

uint64_t modbus::QSerialClient::timeout() {
  Q_D(QSerialClient);

  return d->waitResponseTimeout_;
}

void modbus::QSerialClient::setRetryTimes(uint32_t times) {
  Q_D(QSerialClient);

  d->retryTimes_ = times;
}

uint32_t modbus::QSerialClient::retryTimes() {
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
  d->retryTimes_ = 3;
}

} // namespace modbus
