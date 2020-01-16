#include <QCoreApplication>
#include <modbus/tools/modbus_server.h>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  auto modbusServer = modbus::createQModbusTcpServer(33333);
  modbusServer->setServerAddress(0x01);

  modbus::SingleBitAccess access;
  access.setStartAddress(0x00);
  access.setQuantity(0x10);
  access.setValue(0x00, modbus::BitValue::kOn);
  access.setValue(0x01, modbus::BitValue::kOff);
  access.setValue(0x02, modbus::BitValue::kOn);
  access.setValue(0x03, modbus::BitValue::kOff);
  access.setValue(0x04, modbus::BitValue::kOn);

  modbusServer->handleFunc(modbus::FunctionCode::kReadCoils, access);

  bool success = modbusServer->listenAndServe();
  if (!success) {
    return 1;
  }

  return app.exec();
}
