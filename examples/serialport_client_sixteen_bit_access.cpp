#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <memory>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus/tools/modbus_client.h>

static void usage();

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  if (argc != 2) {
    qDebug() << "No serial port specified!";
    usage();
    return 1;
  }
  QString serialportName = argv[1];
  QScopedPointer<modbus::QModbusClient> client(
      modbus::newQtSerialClient(serialportName));

  client->setOpenRetryTimes(5, 5000);
  client->setRetryTimes(3);

  auto sendAfter = [&](int delay) {
    QTimer::singleShot(delay, [&]() {
      {
        modbus::SixteenBitAccess access;
        access.setStartAddress(0);
        access.setQuantity(10);
        access.setDeviceName("device-1");
        access.setDescription(modbus::Address(0x00), "humidity");
        access.setDescription(modbus::Address(0x01), "temperature");
        access.setDescription(modbus::Address(0x05), "CO2 concentration");

        client->readRegisters(modbus::ServerAddress(0x01),
                              modbus::FunctionCode::kReadHoldingRegisters,
                              access);
      }
      {
        modbus::SixteenBitAccess access;
        access.setStartAddress(0);
        access.setQuantity(10);
        access.setDeviceName("Smoke detector");
        access.setDescription(modbus::Address(0x03), "Alarm status");

        client->readRegisters(modbus::ServerAddress(0x02),
                              modbus::FunctionCode::kReadHoldingRegisters,
                              access);
      }
    });
  };

  QObject::connect(client.data(), &modbus::QModbusClient::clientClosed, [&]() {
    qDebug() << "client is closed" << client->errorString();
    app.quit();
  });
  QObject::connect(client.data(), &modbus::QModbusClient::clientOpened, [&]() {
    qDebug() << "client is opened";
    sendAfter(0);
  });

  QObject::connect(
      client.data(), &modbus::QModbusClient::readRegistersFinished,
      [&](const modbus::Request &req, const modbus::Response &resp,
          const modbus::SixteenBitAccess &access) {
        std::shared_ptr<void> _(nullptr, std::bind([&]() {
                                  printf("pending Request size:%ld\n",
                                         client->pendingRequestSize());
                                  if (client->pendingRequestSize() == 0) {
                                    sendAfter(3000);
                                  }
                                }));
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
      });

  client->open();

  return app.exec();
}

static void usage() {
  printf("usage: serialport_client_sixteen_bit_access  serialport\n");
  printf("example: serialport_client_sixteen_bit_access COM4\n");
}
