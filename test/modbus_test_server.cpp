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
static modbus::Adu
createAdu(modbus::ServerAddress serverAddress,
          modbus::FunctionCode functionCode, const modbus::ByteArray &data,
          const modbus::DataChecker::calculateRequiredSizeFunc &func);

class TestConnection : public modbus::AbstractConnection {
  Q_OBJECT
public:
  TestConnection(QObject *parent = nullptr)
      : modbus::AbstractConnection(parent) {
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

class TestServer : public modbus::AbstractServer {
  Q_OBJECT
public:
  TestServer(QObject *parent = nullptr) : modbus::AbstractServer(parent) {}
  ~TestServer() {}
  MOCK_METHOD0(listenAndServe, bool());
};

TEST(QModbusServer, constructor) {
  TestServer testServer;
  modbus::QModbusServer server(&testServer);

  EXPECT_EQ(server.maxClients(), 1);
  EXPECT_EQ(server.serverAddress(), 0x01);
  EXPECT_EQ(server.blacklist().size(), 0);
}

TEST(QModbusServer, set_get) {
  TestServer testServer;
  modbus::QModbusServer server(&testServer);

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
  modbus::AbstractConnection *conn = &testConn;
  QSignalSpy spy(conn, &modbus::AbstractConnection::messageArrived);

  modbus::BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  testConn.messageArrived(1, requestBuffer);

  EXPECT_EQ(spy.count(), 1);

  QTimer::singleShot(0, [&]() { app.quit(); });
  app.exec();
}

TEST(QModbusServer,
     recivedRequest_requestServerAddressIsBadAddress_discardTheRequest) {
  TestServer server;
  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  modbus::BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  modbus::ByteArray raw({0x02, 0x01, 0x00, 0x00, 0x00, 0x01});
  raw = modbus::tool::appendCrc(raw);
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::shared_ptr<modbus::Frame> request;
  std::shared_ptr<modbus::Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result,
            modbus::QModbusServerPrivate::ProcessResult::kBadServerAddress);
}

TEST(QModbusServer, recivedRequest_needMoreData) {
  TestServer server;
  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  modbus::BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  modbus::ByteArray raw({0x01});
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::shared_ptr<modbus::Frame> request;
  std::shared_ptr<modbus::Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result, modbus::QModbusServerPrivate::ProcessResult::kNeedMoreData);
}

TEST(
    QModbusServer,
    recivedRequest_theRequestedFunctionCodeIsNotSupported_returnIllegalFunctionCodeError) {
  TestServer server;
  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  modbus::BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  modbus::ByteArray raw({0x01, 0x01, 0x00, 0x00, 0x00, 0x01});
  raw = modbus::tool::appendCrc(raw);
  requestBuffer->Write((char *)raw.data(), raw.size());

  std::shared_ptr<modbus::Frame> request;
  std::shared_ptr<modbus::Frame> response;
  auto result = d.processModbusRequest(requestBuffer, request, response);
  EXPECT_EQ(result,
            modbus::QModbusServerPrivate::ProcessResult::kBadFunctionCode);
}

TEST(QModbusServer, processReadCoils_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x01);
  accessServer->setQuantity(10);
  accessServer->setValue(0x01, modbus::BitValue::kOn);
  accessServer->setValue(0x03, modbus::BitValue::kOn);
  d.handleFunc(modbus::FunctionCode::kReadCoils, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x3);

  modbus::Request request(createAdu(0x01, modbus::FunctionCode::kReadCoils,
                                    access.marshalReadRequest(),
                                    modbus::bytesRequiredStoreInArrayIndex<0>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.serverAddress(), 0x01);
  EXPECT_EQ(response.functionCode(), modbus::FunctionCode::kReadCoils);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x01, 0x05}));
}

TEST(QModbusServer, processReadCoils_badDataAddress_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x01);
  accessServer->setQuantity(10);
  accessServer->setValue(0x01, modbus::BitValue::kOn);
  accessServer->setValue(0x03, modbus::BitValue::kOn);
  d.handleFunc(modbus::FunctionCode::kReadCoils, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x06);
  access.setQuantity(0x10);

  modbus::Request request(createAdu(0x01, modbus::FunctionCode::kReadCoils,
                                    access.marshalReadRequest(),
                                    modbus::bytesRequiredStoreInArrayIndex<0>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
}

TEST(QModbusServer, processWriteSingleCoils_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x01);
  accessServer->setQuantity(10);
  d.handleFunc(modbus::FunctionCode::kWriteSingleCoil, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x01);
  access.setValue(modbus::BitValue::kOn);

  modbus::Request request(
      createAdu(0x01, modbus::FunctionCode::kWriteSingleCoil,
                access.marshalSingleWriteRequest(), modbus::bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.serverAddress(), 0x01);
  EXPECT_EQ(response.functionCode(), modbus::FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x00, 0x01, 0xff, 0x00}));
}

TEST(QModbusServer, processWriteSingleCoils_badAddress_Failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x99);
  accessServer->setQuantity(10);
  d.handleFunc(modbus::FunctionCode::kWriteSingleCoil, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x01);
  access.setValue(modbus::BitValue::kOn);

  modbus::Request request(
      createAdu(0x01, modbus::FunctionCode::kWriteSingleCoil,
                access.marshalSingleWriteRequest(), modbus::bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), modbus::FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x02}));
}

TEST(QModbusServer, processWriteSingleCoils_badValue_Failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x01);
  accessServer->setQuantity(10);
  d.handleFunc(modbus::FunctionCode::kWriteSingleCoil, accessServer);

  modbus::Request request(createAdu(
      0x01, modbus::FunctionCode::kWriteSingleCoil,
      modbus::ByteArray({0x00, 0x01, 0xff, 0xff}), modbus::bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataValue);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), modbus::FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x03}));
}

TEST(QModbusServer, processWriteSingleCoils_badValue_checkWriteFailed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  d.setCanWriteSingleBitValueFunc(
      [&](modbus::FunctionCode functionCode, modbus::Address address,
          modbus::BitValue value) { return modbus::Error::kSlaveDeviceBusy; });
  std::shared_ptr<modbus::SingleBitAccess> accessServer(
      new modbus::SingleBitAccess);

  accessServer->setStartAddress(0x01);
  accessServer->setQuantity(10);
  d.handleFunc(modbus::FunctionCode::kWriteSingleCoil, accessServer);

  modbus::Request request(createAdu(
      0x01, modbus::FunctionCode::kWriteSingleCoil,
      modbus::ByteArray({0x00, 0x01, 0x00, 0x00}), modbus::bytesRequired<4>));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kSlaveDeviceBusy);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.functionCode(), modbus::FunctionCode::kWriteSingleCoil);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x06}));
}

TEST(QModbusServer, processWriteMultipleCoils_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SingleBitAccess> access(new modbus::SingleBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  d.handleFunc(modbus::FunctionCode::kWriteMultipleCoils, access);

  modbus::Request request(
      createAdu(0x01, modbus::FunctionCode::kWriteMultipleCoils,
                modbus::ByteArray({0x00, 0x00, 0x00, 0x09, 0x02, 0xff, 0x01}),
                modbus::bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x00, 0x00, 0x00, 0x09}));
}

TEST(QModbusServer, processWriteMultipleCoils_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SingleBitAccess> access(new modbus::SingleBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  d.handleFunc(modbus::FunctionCode::kWriteMultipleCoils, access);

  modbus::Request request(
      createAdu(0x01, modbus::FunctionCode::kWriteMultipleCoils,
                modbus::ByteArray({0x00, 0x00, 0x00, 0x19, 0x02, 0xff, 0x01}),
                modbus::bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(),
            modbus::ByteArray({uint8_t(modbus::Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processReadMultipleRegisters_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kReadInputRegister, access);

  modbus::Request request(createAdu(0x01,
                                    modbus::FunctionCode::kReadInputRegister,
                                    modbus::ByteArray({0x00, 0x00, 0x00, 0x03}),
                                    modbus::bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(),
            modbus::ByteArray({0x06, 0x12, 0x34, 0x56, 0x78, 0x98, 0x76}));
}

TEST(QModbusServer, processReadMultipleRegisters_badAddress_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kReadInputRegister, access);

  modbus::Request request(createAdu(0x01,
                                    modbus::FunctionCode::kReadInputRegister,
                                    modbus::ByteArray({0x00, 0x99, 0x00, 0x03}),
                                    modbus::bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(),
            modbus::ByteArray({uint8_t(modbus::Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processReadMultipleRegisters_badQuantity_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kReadInputRegister, access);

  modbus::Request request(createAdu(0x01,
                                    modbus::FunctionCode::kReadInputRegister,
                                    modbus::ByteArray({0x00, 0x08, 0x00, 0x09}),
                                    modbus::bytesRequiredStoreInArrayIndex<0>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(),
            modbus::ByteArray({uint8_t(modbus::Error::kIllegalDataAddress)}));
}

TEST(QModbusServer, processWriteSingleRegister_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kWriteSingleRegister, access);

  modbus::Request request(createAdu(
      0x01, modbus::FunctionCode::kWriteSingleRegister,
      modbus::ByteArray({0x00, 0x08, 0x00, 0x09}), modbus::bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x00, 0x08, 0x00, 0x09}));
}

TEST(QModbusServer, processWriteSingleRegister_badAddress_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kWriteSingleRegister, access);

  modbus::Request request(createAdu(
      0x01, modbus::FunctionCode::kWriteSingleRegister,
      modbus::ByteArray({0x00, 0x88, 0x00, 0x09}), modbus::bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x02}));
}

TEST(QModbusServer, processWriteSingleRegister_badValue_failed) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  d.setCanWriteSixteenBitValueFunc([&](modbus::FunctionCode functionCode,
                                       modbus::Address address,
                                       const modbus::SixteenBitValue &value) {
    return modbus::Error::kIllegalDataValue;
  });

  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kWriteSingleRegister, access);

  modbus::Request request(createAdu(
      0x01, modbus::FunctionCode::kWriteSingleRegister,
      modbus::ByteArray({0x00, 0x08, 0x00, 0x09}), modbus::bytesRequired<4>));
  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataValue);
  EXPECT_EQ(response.isException(), true);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x03}));
}

TEST(QModbusServer, processWriteMultipleRegisters_success) {
  modbus::QModbusServerPrivate d;

  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  std::shared_ptr<modbus::SixteenBitAccess> access(
      new modbus::SixteenBitAccess);
  access->setStartAddress(0x00);
  access->setQuantity(0x10);
  access->setValue(0x00, 0x1234);
  access->setValue(0x01, 0x5678);
  access->setValue(0x02, 0x9876);
  d.handleFunc(modbus::FunctionCode::kWriteMultipleRegisters, access);

  modbus::Request request(
      createAdu(0x01, modbus::FunctionCode::kWriteMultipleRegisters,
                modbus::ByteArray({0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01}),
                modbus::bytesRequired<4>));
  auto response = d.processRequest(request);

  EXPECT_EQ(response.error(), modbus::Error::kNoError);
  EXPECT_EQ(response.isException(), false);
  EXPECT_EQ(response.data(), modbus::ByteArray({0x00, 0x00, 0x00, 0x01}));
}

static modbus::Adu
createAdu(modbus::ServerAddress serverAddress,
          modbus::FunctionCode functionCode, const modbus::ByteArray &data,
          const modbus::DataChecker::calculateRequiredSizeFunc &func) {
  modbus::Adu adu;
  adu.setServerAddress(serverAddress);
  adu.setFunctionCode(functionCode);
  adu.setData(data);
  adu.setDataChecker(modbus::DataChecker({func}));
  return adu;
}

#include "modbus_test_server.moc"
