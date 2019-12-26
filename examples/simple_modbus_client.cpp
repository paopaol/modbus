#include <QCoreApplication>
#include <modbus/tools/modbus_client.h>

static void processFunctionCode3(const modbus::Request &request,
                                 const modbus::Response &response,
                                 const modbus::SixteenBitAccess &access) {
  if (response.isException()) {
    return;
  }

  for (int i = 0; i < access.quantity(); i++) {
    qDebug() << "value is:"
             << access.value(access.startAddress() + i).toUint16();
  }
}

static void processFunctionCode6(modbus::ServerAddress serverAddress,
                                 modbus::Address address, modbus::Error error) {
  qDebug() << "write signle register:" << (error == modbus::Error::kNoError);
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  modbus::QModbusClient *client = modbus::newQtSerialClient("COM1");

  QObject::connect(client, &modbus::QModbusClient::readRegistersFinished, &app,
                   processFunctionCode3);
  QObject::connect(client, &modbus::QModbusClient::writeSingleRegisterFinished,
                   &app, processFunctionCode6);
  client->open();

  /**
   * function code 0x03
   */
  modbus::SixteenBitAccess access;
  access.setStartAddress(0x00);
  access.setQuantity(0x02);

  client->readRegisters(0x01, modbus::FunctionCode(0x03), access);

  /**
   * function code 0x06
   */
  client->writeSingleRegister(modbus::ServerAddress(0x01),
                              modbus::Address(0x01),
                              modbus::SixteenBitValue(0x17));

  return app.exec();
}
