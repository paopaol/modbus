#include "../src/tools/modbus_server_p.h"
#include <QObject>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTimer>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus/tools/modbus_server.h>

using namespace testing;
using namespace modbus;

static Adu createAdu(ServerAddress serverAddress, FunctionCode functionCode,
                     const ByteArray &data,
                     const DataChecker::calculateRequiredSizeFunc &func);

class TestConnection : public AbstractConnection {
  Q_OBJECT
public:
  TestConnection(QObject *parent = nullptr) : AbstractConnection(parent) {
    EXPECT_CALL(*this, fullName()).WillRepeatedly(Invoke([]() {
      return "COM1";
    }));
  }
  ~TestConnection() {}

  MOCK_CONST_METHOD0(fd, quintptr());
  MOCK_METHOD2(write, void(const char *data, size_t size));
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(fullName, std::string());
};

class TestServer : public AbstractServer {
  Q_OBJECT
public:
  TestServer(QObject *parent = nullptr) : AbstractServer(parent) {}
  ~TestServer() {}
  MOCK_METHOD0(listenAndServe, bool());
};

TEST(QModbusServer, constructor) {
  TestServer testServer;
  QModbusServer server(&testServer);

  EXPECT_EQ(server.maxClients(), 1);
  EXPECT_EQ(server.serverAddress(), 0x01);
  EXPECT_EQ(server.blacklist().size(), 0);
}

TEST(QModbusServer, set_get) {
  TestServer testServer;
  QModbusServer server(&testServer);

  server.setMaxClients(3);
  EXPECT_EQ(server.maxClients(), 3);
  server.setMaxClients(-1);
  EXPECT_EQ(server.maxClients(), 1);
  server.addBlacklist("192.168.1.111");
  server.addBlacklist("192.168.1.123");
  EXPECT_THAT(server.blacklist(),
              ElementsAre("192.168.1.111", "192.168.1.123"));

  server.setServerAddress(0x01);
  EXPECT_EQ(server.serverAddress(), 0x01);
}

TEST(QModbusServer, testSignles) {
  int argc = 1;
  char *argv[] = {(char *)"test"};
  QCoreApplication app(argc, argv);

  TestConnection testConn;
  AbstractConnection *conn = &testConn;
  qRegisterMetaType<BytesBufferPtr>("BytesBufferPtr");
  QSignalSpy spy(conn, &AbstractConnection::messageArrived);

  BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  testConn.messageArrived(1, requestBuffer);

  EXPECT_EQ(spy.count(), 1);

  QTimer::singleShot(0, [&]() { app.quit(); });
  app.exec();
}

TEST(QModbusServer,
     recivedRequest_requestServerAddressIsBadAddress_discardTheRequest) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  ByteArray raw({0x02, 0x01, 0x00, 0x00, 0x00, 0x01});
  raw = tool::appendCrc(raw);
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::unique_ptr<Frame> request;
  std::unique_ptr<Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result, QModbusServerPrivate::ProcessResult::kBadServerAddress);
}

TEST(QModbusServer, recivedRequest_needMoreData) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  ByteArray raw({0x01});
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::unique_ptr<Frame> request;
  std::unique_ptr<Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result, QModbusServerPrivate::ProcessResult::kNeedMoreData);
}

TEST(
    QModbusServer,
    recivedRequest_theRequestedFunctionCodeIsNotSupported_returnIllegalFunctionCodeError) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  ByteArray raw({0x01, 0x01, 0x00, 0x00, 0x00, 0x01});
  raw = tool::appendCrc(raw);
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::unique_ptr<Frame> request;
  std::unique_ptr<Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result, QModbusServerPrivate::ProcessResult::kBadFunctionCode);
}

TEST(QModbusServer, processReadCoils_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x01, 10);
  d.writeCoils(0x01, BitValue::kOn);
  d.writeCoils(0x03, BitValue::kOn);

  SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x3);

  Request request;

  request.setServerAddress(0x01);
  request.setFunctionCode(FunctionCode::kReadCoils);
  request.setDataChecker({bytesRequiredStoreInArrayIndex<0>});
  request.setData({0x00, 0x01, 0x00, 0x03});

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.serverAddress(), 0x01);
  EXPECT_EQ(response.functionCode(), FunctionCode::kReadCoils);
  EXPECT_EQ(response.data(), ByteArray({0x01, 0x05}));
}

TEST(QModbusServer, processReadCoils_badDataAddress_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x01, 10);
  d.writeCoils(0x01, BitValue::kOn);
  d.writeCoils(0x03, BitValue::kOn);

  SingleBitAccess access;

  access.setStartAddress(0x06);
  access.setQuantity(0x10);

  Request request(createAdu(0x01, FunctionCode::kReadCoils,
                            access.marshalReadRequest(),
                            bytesRequiredStoreInArrayIndex<0>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
}

TEST(QModbusServer, processWriteSingleCoils_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x01, 10);

  SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x01);
  access.setValue(BitValue::kOn);

  Request request(createAdu(0x01, FunctionCode::kWriteSingleCoil,
                            access.marshalSingleWriteRequest(),
                            bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.serverAddress(), 0x01);
  EXPECT_EQ(response.functionCode(), FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), ByteArray({0x00, 0x01, 0xff, 0x00}));
}

TEST(QModbusServer, processWriteSingleCoils_badAddress_Failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x99, 10);

  SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x01);
  access.setValue(BitValue::kOn);

  Request request(createAdu(0x01, FunctionCode::kWriteSingleCoil,
                            access.marshalSingleWriteRequest(),
                            bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), ByteArray({0x02}));
}

TEST(QModbusServer, processWriteSingleCoils_badValue_Failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x01, 10);

  Request request(createAdu(0x01, FunctionCode::kWriteSingleCoil,
                            ByteArray({0x00, 0x01, 0xff, 0xff}),
                            bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataValue);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), ByteArray({0x03}));
}

TEST(QModbusServer, processWriteSingleCoils_badValue_checkWriteFailed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);
  d.setCanWriteSingleBitValueFunc(
      [&](Address address, BitValue value) { return Error::kSlaveDeviceBusy; });

  d.handleCoils(0x01, 10);

  Request request(createAdu(0x01, FunctionCode::kWriteSingleCoil,
                            ByteArray({0x00, 0x01, 0x00, 0x00}),
                            bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kSlaveDeviceBusy);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), ByteArray({0x06}));
}

TEST(QModbusServer, processWriteMultipleCoils_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x00, 0x10);

  Request request(createAdu(
      0x01, FunctionCode::kWriteMultipleCoils,
      ByteArray({0x00, 0x00, 0x00, 0x09, 0x02, 0xff, 0x01}), bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), ByteArray({0x00, 0x00, 0x00, 0x09}));
}

TEST(QModbusServer, processWriteMultipleCoils_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleCoils(0x00, 0x10);

  Request request(createAdu(
      0x01, FunctionCode::kWriteMultipleCoils,
      ByteArray({0x00, 0x00, 0x00, 0x19, 0x02, 0xff, 0x01}), bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), ByteArray({uint8_t(Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processReadMultipleRegisters_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleInputRegisters(0x00, 0x10);
  d.writeInputRegister(0x00, SixteenBitValue(0x1234));
  d.writeInputRegister(0x01, SixteenBitValue(0x5678));
  d.writeInputRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kReadInputRegister,
                            ByteArray({0x00, 0x00, 0x00, 0x03}),
                            bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(),
            ByteArray({0x06, 0x12, 0x34, 0x56, 0x78, 0x98, 0x76}));
}

TEST(QModbusServer, processReadMultipleRegisters_badAddress_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleInputRegisters(0x00, 0x10);
  d.writeInputRegister(0x00, SixteenBitValue(0x1234));
  d.writeInputRegister(0x01, SixteenBitValue(0x5678));
  d.writeInputRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kReadInputRegister,
                            ByteArray({0x00, 0x99, 0x00, 0x03}),
                            bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), ByteArray({uint8_t(Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processReadMultipleRegisters_badQuantity_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleInputRegisters(0x00, 0x10);
  d.writeInputRegister(0x00, SixteenBitValue(0x1234));
  d.writeInputRegister(0x01, SixteenBitValue(0x5678));
  d.writeInputRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kReadInputRegister,
                            ByteArray({0x00, 0x08, 0x00, 0x09}),
                            bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), ByteArray({uint8_t(Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processWriteSingleRegister_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleHoldingRegisters(0x00, 0x10);
  d.writeHodingRegister(0x00, SixteenBitValue(0x1234));
  d.writeHodingRegister(0x01, SixteenBitValue(0x5678));
  d.writeHodingRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kWriteSingleRegister,
                            ByteArray({0x00, 0x08, 0x00, 0x09}),
                            bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), ByteArray({0x00, 0x08, 0x00, 0x09}));
}

TEST(QModbusServer, processWriteSingleRegister_badAddress_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleHoldingRegisters(0x00, 0x10);
  d.writeHodingRegister(0x00, SixteenBitValue(0x1234));
  d.writeHodingRegister(0x01, SixteenBitValue(0x5678));
  d.writeHodingRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kWriteSingleRegister,
                            ByteArray({0x00, 0x88, 0x00, 0x09}),
                            bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), ByteArray({0x02}));
}

TEST(QModbusServer, processWriteSingleRegister_badValue_failed) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);
  d.setCanWriteSixteenBitValueFunc(
      [&](Address address, const SixteenBitValue &value) {
        return Error::kIllegalDataValue;
      });

  d.handleHoldingRegisters(0x00, 0x10);
  d.writeHodingRegister(0x00, SixteenBitValue(0x1234));
  d.writeHodingRegister(0x01, SixteenBitValue(0x5678));
  d.writeHodingRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(0x01, FunctionCode::kWriteSingleRegister,
                            ByteArray({0x00, 0x08, 0x00, 0x09}),
                            bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), Error::kIllegalDataValue);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), ByteArray({0x03}));
}

TEST(QModbusServer, processWriteMultipleRegisters_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  QSignalSpy spy(&modbusServer, &QModbusServer::holdingRegisterValueChanged);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleHoldingRegisters(0x00, 0x10);
  d.writeHodingRegister(0x00, SixteenBitValue(0x1234));
  d.writeHodingRegister(0x01, SixteenBitValue(0x5678));
  d.writeHodingRegister(0x02, SixteenBitValue(0x9876));

  Request request(createAdu(
      0x01, FunctionCode::kWriteMultipleRegisters,
      ByteArray({0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01}), bytesRequired<4>));
  auto response = d.processRequest(request);

  EXPECT_EQ(response.error(), Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), ByteArray({0x00, 0x00, 0x00, 0x01}));
  EXPECT_EQ(spy.count(), 4);
}

TEST(QModbusServer, writeValueSixteenValue_success) {
  TestServer server;
  QModbusServer modbusServer(&server);
  QModbusServerPrivate d(&modbusServer);

  QSignalSpy spy(&modbusServer, &QModbusServer::holdingRegisterValueChanged);

  d.setServerAddress(1);
  d.setTransferMode(TransferMode::kRtu);

  d.handleHoldingRegisters(0x00, 0x10);
  d.writeHodingRegister(0x00, SixteenBitValue(0x1234));
  d.writeHodingRegister(0x01, SixteenBitValue(0x5678));
  d.writeHodingRegister(0x02, SixteenBitValue(0x9876));

  EXPECT_EQ(spy.count(), 3);

  SixteenBitValue value;
  bool ok = d.holdingRegisterValue(0x02, &value);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(value.toUint16(), SixteenBitValue(0x9876).toUint16());
}

static Adu createAdu(ServerAddress serverAddress, FunctionCode functionCode,
                     const ByteArray &data,
                     const DataChecker::calculateRequiredSizeFunc &func) {
  Adu adu;
  adu.setServerAddress(serverAddress);
  adu.setFunctionCode(functionCode);
  adu.setData(data);
  adu.setDataChecker(DataChecker({func}));
  return adu;
}

#include "modbus_test_server.moc"
