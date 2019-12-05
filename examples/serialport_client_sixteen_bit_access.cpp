#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus/tools/modbus_serial_client.h>
#include <sstream>

static QString modbusBitValueToString(modbus::BitValue value);
static modbus::Request
marshalMultipleReadRegisterRequest(modbus::ServerAddress serverAddress,
                                   const modbus::SixteenBitAccess &access);
static bool validateSixteenBitAccessResponse(const modbus::Response &resp);
static bool unmarshalMultipleReadRegister(const modbus::Request &req,
                                          const modbus::Response &resp,
                                          modbus::SixteenBitAccess *access);

namespace std {
template <typename T> std::string to_string(const T &t) {
  std::stringsream s;

  s << t;
  return s.str();
}
} // namespace std

static modbus::DataChecker newDataChecker() {
  modbus::DataChecker dataChecker;
  dataChecker.calculateRequestSize = modbus::bytesRequired<4>;
  dataChecker.calculateResponseSize = modbus::bytesRequiredStoreInArrayIndex0;
  return dataChecker;
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  QScopedPointer<modbus::QSerialClient> client(
      modbus::newQtSerialClient("COM4"));

  client->setOpenRetryTimes(5, 5000);
  client->setRetryTimes(3);

  auto sendAfter = [&](int delay = 3000) {
    QTimer::singleShot(delay, [&]() {
      {
        modbus::SixteenBitAccess access;
        access.setStartAddress(0);
        access.setQuantity(10);
        access.setDeviceName("device-1");
        access.setDescription(modbus::Address(0x00), "humidity");
        access.setDescription(modbus::Address(0x01), "temperature");
        access.setDescription(modbus::Address(0x05), "CO2 concentration");

        auto request = marshalMultipleReadRegisterRequest(
            modbus::ServerAddress(0x01), access);
        client->sendRequest(request);
      }
      {
        modbus::SixteenBitAccess access;
        access.setStartAddress(0);
        access.setQuantity(10);
        access.setDeviceName("Smoke detector");
        access.setDescription(modbus::Address(0x03), "Alarm status");

        auto request = marshalMultipleReadRegisterRequest(
            modbus::ServerAddress(0x02), access);
        client->sendRequest(request);
      }
    });
  };

  QObject::connect(client.data(), &modbus::QSerialClient::clientClosed, [&]() {
    qDebug() << "client is closed" << client->errorString();
  });
  QObject::connect(client.data(), &modbus::QSerialClient::clientOpened, [&]() {
    qDebug() << "client is opened";
    sendAfter(0);
  });

  QObject::connect(
      client.data(), &modbus::QSerialClient::requestFinished,
      [&](const modbus::Request &req, const modbus::Response &resp) {
        modbus::SixteenBitAccess access;

        bool success = unmarshalMultipleReadRegister(req, resp, &access);
        if (!success) {
          return;
        }

        printf("device name:[%d] %s\n", resp.serverAddress(),
               access.deviceName().c_str());
        for (int offset = 0; offset < access.quantity(); offset++) {
          modbus::Address currentAddress = access.startAddress() + offset;
          auto valueEx = access.valueEx(currentAddress);
          if (valueEx.description.empty()) {
            continue;
          }

          printf("\taddress: %d value:%d [%s] \t%s\n", currentAddress,
                 valueEx.value.toUint16(), valueEx.value.toHexString().c_str(),
                 valueEx.description.c_str());
        }
        std::cout << std::endl;

        printf("pending Request size:%d\n", client->pendingRequestSize());
        if (client->pendingRequestSize() == 0) {
          sendAfter();
        }
      });

  client->open();

  return app.exec();
}

static modbus::Request
marshalMultipleReadRegisterRequest(modbus::ServerAddress serverAddress,
                                   const modbus::SixteenBitAccess &access) {
  modbus::Request request;

  request.setServerAddress(serverAddress);
  request.setFunctionCode(modbus::FunctionCode(3));
  request.setUserData(access);
  request.setData(access.marshalMultipleReadRequest());
  request.setDataChecker(newDataChecker());

  return request;
}

static bool validateSixteenBitAccessResponse(const modbus::Response &resp) {
  if (resp.error() != modbus::Error::kNoError) {
    qDebug() << resp.errorString().c_str();
    return false;
  }

  if (resp.isException()) {
    qDebug() << resp.errorString().c_str();
    return false;
  }
  return true;
}

static bool unmarshalMultipleReadRegister(const modbus::Request &req,
                                          const modbus::Response &resp,
                                          modbus::SixteenBitAccess *access) {
  if (!access) {
    return false;
  }

  bool success = validateSixteenBitAccessResponse(resp);
  if (!success) {
    return false;
  }
  *access = modbus::any::any_cast<modbus::SixteenBitAccess>(req.userData());
  success = access->unmarshalReadResponse(resp.data());
  if (!success) {
    qDebug() << "data is invalid";
    return false;
  }
  return true;
}
