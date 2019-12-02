#include <QCoreApplication>
#include <QDebug>
#include <modbus/base/single_bit_access.h>
#include <modbus/tools/modbus_serial_client.h>

static QString modbusBitValueToString(modbus::BitValue value);

static modbus::DataChecker newDataChecker() {
  modbus::DataChecker dataChecker;
  dataChecker.calculateRequestSize = modbus::bytesRequired<4>;
  dataChecker.calculateResponseSize = modbus::bytesRequiredStoreInArrayIndex0;
  return dataChecker;
}

static void logMessage(modbus::LogLevel level, const std::string &msg) {
  switch (level) {
  case modbus::LogLevel::kDebug:
    qDebug() << msg.c_str();
    break;
  case modbus::LogLevel::kInfo:
    qInfo() << msg.c_str();
    break;
  case modbus::LogLevel::kWarning:
    qWarning() << msg.c_str();
    break;
  }
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  modbus::registerLogMessage(logMessage);

  QScopedPointer<modbus::QSerialClient> client(
      modbus::newQtSerialClient("/dev/ttyS0"));

  client->setOpenRetryTimes(5, 5000);

  QObject::connect(client.data(), &modbus::QSerialClient::clientClosed,
                   [&]() { qDebug() << "client is closed"; });
  QObject::connect(client.data(), &modbus::QSerialClient::clientOpened, [&]() {
    qDebug() << "client is opened";

    modbus::Request request;

    modbus::SingleBitAccess access;
    access.setStartAddress(0);
    access.setQuantity(5);

    request.setServerAddress(0);
    request.setFunctionCode(modbus::FunctionCode::kReadCoils);
    request.setUserData(access);
    request.setData(access.marshalReadRequest());
    request.setDataChecker(newDataChecker());

    client->sendRequest(request);
  });

  QObject::connect(
      client.data(), &modbus::QSerialClient::requestFinished,
      [&](const modbus::Request &req, const modbus::Response &resp) {
        if (resp.error() != modbus::Error::kNoError) {
          qDebug() << resp.errorString().c_str();
          return;
        }

        if (resp.isException()) {
          qDebug() << resp.errorString().c_str();
          return;
        }
        modbus::SingleBitAccess access =
            modbus::any::any_cast<modbus::SingleBitAccess>(req.userData());
        bool ok = access.unmarshalReadResponse(resp.data());
        if (!ok) {
          qDebug() << "data is invalid";
          return;
        }
        modbus::Address address = access.startAddress();
        for (int offset = 0; offset < access.quantity(); offset++) {
          modbus::Address currentAddress = address + offset;
          qDebug() << "address: " << currentAddress << " value: "
                   << modbusBitValueToString(access.value(currentAddress));
        }
      });

  client->open();

  return app.exec();
}

static QString modbusBitValueToString(modbus::BitValue value) {
  switch (value) {
  case modbus::BitValue::kOn:
    return "true";
  case modbus::BitValue::kOff:
    return "false";
  case modbus::BitValue::kBadValue:
    return "badValue";
  }
  return "";
}
