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
      client->readRegisters(modbus::ServerAddress(0x01),
                            modbus::FunctionCode::kReadHoldingRegisters, 0, 10);
      client->readRegisters(modbus::ServerAddress(0x02),
                            modbus::FunctionCode::kReadHoldingRegisters, 0, 10);
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
      [&](modbus::ServerAddress serverAddress,
          modbus::FunctionCode functionCode, modbus::Address startAddress,
          modbus::Quantity quantity,
          const QVector<modbus::SixteenBitValue> &valueList,
          modbus::Error error) {
        std::shared_ptr<void> _(nullptr, std::bind([&]() {
                                  printf("pending Request size:%zu\n",
                                         client->pendingRequestSize());
                                  if (client->pendingRequestSize() == 0) {
                                    sendAfter(3000);
                                  }
                                }));
        int offset = 0;
        for (const auto &value : valueList) {
          modbus::Address address = startAddress + offset;
          printf("\taddress: %d value:%d\n", address, value.toUint16());
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
