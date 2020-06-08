#include <QCoreApplication>
#include <modbus/tools/modbus_server.h>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  // auto modbusServer = modbus::createServer("tcp://:502");
  auto modbusServer = modbus::createServer("file:///COM1?9600-8-p-1");

  modbusServer->setServerAddress(0x01);
  modbusServer->setTransferMode(modbus::TransferMode::kAscii);

  modbusServer->handleCoils(0x00, 100);
  modbusServer->handleDiscreteInputs(0x00, 0x10);
  modbusServer->handleHoldingRegisters(0x00, 0x20);
  modbusServer->handleInputRegisters(0x00, 0x20);

  bool success = modbusServer->listenAndServe();
  if (!success) {
    return 1;
  }

  return app.exec();
}
