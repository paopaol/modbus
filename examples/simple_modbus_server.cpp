#include <QCoreApplication>
#include <modbus/tools/modbus_server.h>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  // auto modbusServer = modbus::createQModbusTcpServer(33333);
  auto modbusServer = modbus::createQModbusSerialServer("COM1");

  modbusServer->setServerAddress(0x01);
  modbusServer->setTransferMode(modbus::TransferMode::kAscii);

  std::shared_ptr<modbus::SingleBitAccess> access(new modbus::SingleBitAccess);

  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, modbus::BitValue::kOn);
  access->setValue(0x01, modbus::BitValue::kOff);
  access->setValue(0x02, modbus::BitValue::kOn);
  access->setValue(0x03, modbus::BitValue::kOff);
  access->setValue(0x04, modbus::BitValue::kOn);

  std::shared_ptr<modbus::SixteenBitAccess> sixteenAccess(
      new modbus::SixteenBitAccess);
  sixteenAccess->setStartAddress(0x00);
  sixteenAccess->setQuantity(0x20);
  sixteenAccess->setValue(0x00, modbus::SixteenBitValue(0x00, 0x11).toUint16());
  sixteenAccess->setValue(0x03, modbus::SixteenBitValue(0x22, 0x33).toUint16());

  modbusServer->handleFunc(modbus::FunctionCode::kReadCoils, access);
  modbusServer->handleFunc(modbus::FunctionCode::kReadInputDiscrete, access);
  modbusServer->handleFunc(modbus::FunctionCode::kWriteSingleCoil, access);
  modbusServer->handleFunc(modbus::FunctionCode::kWriteMultipleCoils, access);

  modbusServer->handleFunc(modbus::FunctionCode::kReadInputRegister,
                           sixteenAccess);
  modbusServer->handleFunc(modbus::FunctionCode::kReadHoldingRegisters,
                           sixteenAccess);
  modbusServer->handleFunc(modbus::FunctionCode::kWriteSingleRegister,
                           sixteenAccess);
  modbusServer->handleFunc(modbus::FunctionCode::kWriteMultipleRegisters,
                           sixteenAccess);

  bool success = modbusServer->listenAndServe();
  if (!success) {
    return 1;
  }

  return app.exec();
}
