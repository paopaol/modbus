# modbus
a modbus library for c++11,using qt

## required
   QT version >= 5.6

   gcc version >= 4.9

   vs version >= vs 2015

## features
   [x] modbus master serial client

   [x] modbus master tcp client

   [x] modbus master udp client

   [x] disconnection and reconnection

   [x] request failed and retry

   [x] single bit access

   [x] sixteen bit access

   [x] custom functions
   
## function support

   [x] 0x01 Read coils
   
   [x] 0x02 Read input discrete
   
   [x] 0x03 Read multiple registers
   
   [x] 0x04 Read input register
   
   [x] 0x05 Write single coil
   
   [x] 0x06 Write single register
   
   [x] 0x0f Write multiple coils
   
   [x] 0x10 Write multiple registers
   
   [x] 0x17 Read/Write multiple registers


## build from source

  * windows

  ```cmd
  git clone --recursive https://github.com/paopaol/modbus.git
  cd modbus
  cmake -Bbuild -H. -G"Visual Studio 14 2015" -DCMAKE_PREFIX_PATH=%QTDIR%
  cmake --build build --config rlease
  ```

  * linux

  ```cmd
  git clone --recursive https://github.com/paopaol/modbus.git
  cd modbus
  cmake -Bbuild -H. -DCMAKE_PREFIX_PATH=$QTDIR
  cmake --build build --config rlease
  ```


## examples

* single bit access

```cpp
#include <QCoreApplication>
#include <modbus/tools/modbus_client.h>

static void processFunctionCode3(
    modbus::ServerAddress serverAddress, modbus::Address address,
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
```
