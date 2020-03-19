#include <QCoreApplication>
#include <modbus/tools/modbus_client.h>

static void processFunctionCode3(
    modbus::ServerAddress serverAddress, modbus::FunctionCode functionCode,
    modbus::Address address, modbus::Quantity quantity,
    const QVector<modbus::SixteenBitValue> &valueList, modbus::Error error) {
  if (error != modbus::Error::kNoError) {
    return;
  }

  for (const auto &value : valueList) {
    qDebug() << "value is:" << value.toUint16();
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
  client->readRegisters(modbus::ServerAddress(0x01), modbus::FunctionCode(0x03),
                        modbus::Address(0x00), modbus::Quantity(0x02));

  /**
   * function code 0x06
   */
  client->writeSingleRegister(modbus::ServerAddress(0x01),
                              modbus::Address(0x01),
                              modbus::SixteenBitValue(0x17));

  return app.exec();
}
