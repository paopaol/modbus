#include "../src/tools/modbus_server_p.h"
#include <QObject>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/tools/modbus_server.h>

using namespace testing;
static modbus::Adu
createSingleBitAccessAdu(modbus::ServerAddress serverAddress,
                         modbus::FunctionCode functionCode,
                         const modbus::SingleBitAccess &access);

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
  MOCK_METHOD0(listenAndServe, void());
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
  QCoreApplication name(argc, argv);

  TestConnection testConn;
  modbus::AbstractConnection *conn = &testConn;
  QSignalSpy spy(conn, &modbus::AbstractConnection::messageArrived);

  modbus::BytesBufferPtr requestBuffer(new pp::bytes::Buffer);
  testConn.messageArrived(1, requestBuffer);

  EXPECT_EQ(spy.count(), 1);

  name.exec();
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
  modbus::SingleBitAccess accessServer;

  accessServer.setStartAddress(0x01);
  accessServer.setQuantity(10);
  accessServer.setValue(0x01, modbus::BitValue::kOn);
  accessServer.setValue(0x03, modbus::BitValue::kOn);
  d.handleFunc(modbus::FunctionCode::kReadCoils, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x3);

  modbus::Request request(
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access));

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
  modbus::SingleBitAccess accessServer;

  accessServer.setStartAddress(0x01);
  accessServer.setQuantity(10);
  accessServer.setValue(0x01, modbus::BitValue::kOn);
  accessServer.setValue(0x03, modbus::BitValue::kOn);
  d.handleFunc(modbus::FunctionCode::kReadCoils, accessServer);

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x10);

  modbus::Request request(
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access));

  auto response = d.processRequest(request);
  EXPECT_EQ(response.error(), modbus::Error::kIllegalDataAddress);
}

static modbus::Adu
createSingleBitAccessAdu(modbus::ServerAddress serverAddress,
                         modbus::FunctionCode functionCode,
                         const modbus::SingleBitAccess &access) {
  modbus::Adu adu;
  adu.setServerAddress(serverAddress);
  adu.setFunctionCode(functionCode);
  adu.setData(access.marshalReadRequest());
  adu.setDataChecker(modbus::DataChecker({modbus::bytesRequired<4>}));
  return adu;
}

#include "modbus_test_server.moc"
