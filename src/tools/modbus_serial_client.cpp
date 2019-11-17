#include "modbus_serial_client_p.h"
#include <QTimer>
#include <assert.h>

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

  auto element = createElement(request);
  d->enqueueElement(element);
  runAfter(d->t3_5_, [&]() {
    Q_D(QSerialClient);

    auto ele = d->elementQueue_.front();
    auto request = ele.request;
    auto array = request.marshalData();
    d->serialPort_->write(
        QByteArray(reinterpret_cast<const char *>(array.data())), array.size());
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

    auto &element = d->elementQueue_.front();
    auto &request = element.request;
  });
}

void QSerialClient::initMemberValues() {
  Q_D(QSerialClient);

  d->connectionState_.setState(ConnectionState::kClosed);
  d->sessionState_.setState(SessionState::kIdle);
  d->waitConversionDelay_ = 200;
  d->t3_5_ = 100;
}

} // namespace modbus
