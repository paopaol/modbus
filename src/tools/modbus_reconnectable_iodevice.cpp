#include "modbus_client_p.h"
#include "modbus_client_types.h"
#include <assert.h>
#include <base/modbus_logger.h>
#include <modbus/tools/modbus_client.h>

namespace modbus {

ReconnectableIoDevice::ReconnectableIoDevice(modbus::AbstractIoDevice *iodevice,
                                             QObject *parent)
    : d_ptr(new ReconnectableIoDevicePrivate(iodevice, this)), QObject(parent) {
  setupEnvironment();
}

ReconnectableIoDevice::ReconnectableIoDevice(QObject *parent)
    : d_ptr(nullptr), QObject(parent) {
  setupEnvironment();
}

ReconnectableIoDevice::~ReconnectableIoDevice() {
  Q_D(ReconnectableIoDevice);
  if (isOpened()) {
    close();
  }
  if (d->ioDevice_) {
    d->ioDevice_->deleteLater();
  }
}

void ReconnectableIoDevice::setOpenRetryTimes(int retryTimes, int delay) {
  Q_D(ReconnectableIoDevice);

  if (retryTimes < 0) {
    retryTimes = kBrokenLineReconnection;
  }
  d->openRetryTimes_ = retryTimes;
  d->openRetryTimesBack_ = retryTimes;

  if (delay < 0) {
    delay = 0;
  }
  d->reopenDelay_ = delay;
}

int ReconnectableIoDevice::openRetryTimes() {
  Q_D(ReconnectableIoDevice);
  return d->openRetryTimes_;
}

int ReconnectableIoDevice::openRetryDelay() {
  Q_D(ReconnectableIoDevice);
  return d->reopenDelay_;
}

void ReconnectableIoDevice::open() {
  Q_D(ReconnectableIoDevice);
  d->ioDevice_->open();
}

void ReconnectableIoDevice::close() {
  Q_D(ReconnectableIoDevice);
  d->forceClose_ = true;
  closeButNotSetForceCloseFlag();
}

void ReconnectableIoDevice::write(const char *data, size_t size) {
  Q_D(ReconnectableIoDevice);
  d->ioDevice_->write(data, size);
}

QByteArray ReconnectableIoDevice::readAll() {
  Q_D(ReconnectableIoDevice);
  return d->ioDevice_->readAll();
}

void ReconnectableIoDevice::clear() {
  Q_D(ReconnectableIoDevice);
  return d->ioDevice_->clear();
}

std::string ReconnectableIoDevice::name() {
  Q_D(ReconnectableIoDevice);
  return d->ioDevice_->name();
}

void ReconnectableIoDevice::setupEnvironment() {
  Q_D(ReconnectableIoDevice);
  assert(d->ioDevice_ && "the io device backend is invalid");
  connect(d->ioDevice_, &AbstractIoDevice::opened, this,
          &ReconnectableIoDevice::onIoDeviceOpened);
  connect(d->ioDevice_, &AbstractIoDevice::closed, this,
          &ReconnectableIoDevice::onIoDeviceClosed);
  connect(d->ioDevice_, &AbstractIoDevice::error, this,
          &ReconnectableIoDevice::onIoDeviceError);
  connect(d->ioDevice_, &AbstractIoDevice::bytesWritten, this,
          &ReconnectableIoDevice::bytesWritten);
  connect(d->ioDevice_, &AbstractIoDevice::readyRead, this,
          &ReconnectableIoDevice::readyRead);

  d->connectionState_.setState(ConnectionState::kClosed);
}

void ReconnectableIoDevice::onIoDeviceOpened() {
  Q_D(ReconnectableIoDevice);
  d->connectionState_.setState(ConnectionState::kOpened);
  d->openRetryTimes_ = d->openRetryTimesBack_;
  emit opened();
}

void ReconnectableIoDevice::onIoDeviceClosed() {
  Q_D(ReconnectableIoDevice);
  d->connectionState_.setState(ConnectionState::kClosed);
  /**
   * closed final,clear all pending request
   */
  // clearPendingRequest();

  /// force close, do not check reconnect
  if (d->forceClose_) {
    d->forceClose_ = false;
    emit error(d->errorString_);
    emit closed();
    return;
  }

  // check reconnect
  if (d->openRetryTimes_ == 0) {
    emit error(d->errorString_);
    emit closed();
    return;
  }
  emit connectionIsLostWillReconnect();

  /// do reconnect
  log(LogLevel::kWarning, d->ioDevice_->name() +
                              " closed, try reconnect after " +
                              std::to_string(d->reopenDelay_) + "ms");
  d->openRetryTimes_ > 0 ? --d->openRetryTimes_ : (int)0;
  QTimer::singleShot(d->reopenDelay_, this, &ReconnectableIoDevice::open);
}

bool ReconnectableIoDevice::isOpened() {
  Q_D(ReconnectableIoDevice);

  return d->connectionState_.state() == ConnectionState::kOpened;
}

bool ReconnectableIoDevice::isClosed() {
  Q_D(ReconnectableIoDevice);

  return d->connectionState_.state() == ConnectionState::kClosed;
}

void ReconnectableIoDevice::onIoDeviceError(const QString &errorString) {
  Q_D(ReconnectableIoDevice);

  d->errorString_ = errorString;

  if (d->errorString_.isEmpty()) {
    /**
     *  empty is no error
     */
    return;
  }

  log(LogLevel::kWarning,
      d->ioDevice_->name() + " " + errorString.toStdString());
  if (isOpened()) {
    closeButNotSetForceCloseFlag();
  } else {
    onIoDeviceClosed();
  }
}

void ReconnectableIoDevice::closeButNotSetForceCloseFlag() {
  Q_D(ReconnectableIoDevice);

  if (!isOpened()) {
    log(LogLevel::kInfo,
        d->ioDevice_->name() + ": is already closed or closing or opening");
    return;
  }

  d->connectionState_.setState(ConnectionState::kClosing);
  d->ioDevice_->close();
}

} // namespace modbus
