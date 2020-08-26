#include <QCoreApplication>
#include <modbus/tools/modbus_server.h>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

   auto modbusServer = modbus::createServer("modbus.tcp://:502");
  //auto modbusServer = modbus::createServer("modbus.file:///COM1?9600-8-p-1");

  modbusServer->setServerAddress(0x01);
  modbusServer->setTransferMode(modbus::TransferMode::kMbap);

  modbusServer->handleCoils(0x00, 100);
  modbusServer->handleDiscreteInputs(0x00, 0x10);
  modbusServer->handleHoldingRegisters(0x00, 0x20);
  modbusServer->handleInputRegisters(0x00, 0x20);


  std::vector<modbus::SixteenBitValue> data;
  //decoder
  //v1 5.00 --> 0x00 0x05 0x00 0x04
  //v2 5.00 --> 0x00 0x05 0x00 0x04
  struct F {
	  F(const std::string &t, const  std::shared_ptr<std::vector<uint8_t>> &d, int i) {

	  }
	  std::string type = "FLOAT32";
	  int index;
	  std::shared_ptr<std::vector<uint8_t>> data;
  };

  std::map<modbus::Address, F> fmaps;
  int i = 0;
  for (const auto &v:fields) {
	  std::vector<uint8_t> f = fromFloat(v);
	  std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>(f);
	 
	  for (int j = 0; j < data->size(); j += 2) {
		  fmaps[i++] = F("FLOAT32", data, j);;
		  fmaps[i++] = F("FLOAT32", data, j + 2);
	  }
  }

 // vector<sss> f = fromFloat(v1);
  modbusServer->writeHodingRegister(0, {0, 5});

  
  QObject::connect(modbusServer, &modbus::QModbusServer::holdingRegisterValueChanged, 
	  [&](modbus::Address _t1, const QVector<modbus::SixteenBitValue> & _t2) {
	  int i = 0;
  });

  bool success = modbusServer->listenAndServe();
  if (!success) {
    return 1;
  }

  return app.exec();
}
