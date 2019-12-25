#include <QCoreApplication>
#include <modbus/tools/modbus_client.h>

static void process(const modbus::Request &request,
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

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  modbus::QModbusClient *client = modbus::newQtSerialClient("COM1");

  QObject::connect(client, &modbus::QModbusClient::readRegistersFinished, &app,
                   process);
  client->open();

  modbus::SixteenBitAccess access;

  access.setStartAddress(0x00);
  access.setQuantity(0x02);

  client->readRegisters(0x01, modbus::FunctionCode(0x03), access);

  return app.exec();
}