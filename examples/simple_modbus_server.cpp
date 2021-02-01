#include <QCoreApplication>
#include <modbus/tools/modbus_server.h>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  auto modbusServer = modbus::createServer("modbus.tcp://:502");
  // auto modbusServer = modbus::createServer("modbus.file:///COM1?9600-8-n-1");

  modbusServer->setServerAddress(0x01);
  modbusServer->setTransferMode(modbus::TransferMode::kMbap);

  modbusServer->handleCoils(0x00, 100);
  modbusServer->handleDiscreteInputs(0x00, 0x10);
  modbusServer->handleHoldingRegisters(0x00, 0x20);
  modbusServer->handleInputRegisters(0x00, 0x20);

  std::vector<modbus::SixteenBitValue> data;
  modbusServer->writeHodingRegister(0, {0, 5});

  QObject::connect(
      modbusServer, &modbus::QModbusServer::holdingRegisterValueChanged,
      [&](modbus::Address _t1, const QVector<modbus::SixteenBitValue> &_t2) {});

  bool success = modbusServer->listenAndServe();
  if (!success) {
    return 1;
  }

  return app.exec();
}
