#include "../src/tools/modbus_server_p.h"
#include <QObject>
#include <QScopedPointer>
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

TEST(QModbusServer,
     recivedRequest_requestServerAddressIsBadAddress_discardTheRequest) {
  quintptr fd = 1;
  modbus::ServerAddress goodServerAddress = 0x01;
  modbus::ServerAddress badServerAddress = 0x02;
  std::shared_ptr<pp::bytes::Buffer> requestBuffer(new pp::bytes::Buffer);

  TestConnection conn;
  TestServer server;

  EXPECT_CALL(conn, fd()).WillRepeatedly(Invoke([&]() { return fd; }));

  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  d.incomingConnection(&conn);

  auto &session = d.clientList_.find(fd).value();

  modbus::SingleBitAccess access;

  access.setStartAddress(goodServerAddress);
  access.setQuantity(0x10);

  modbus::RtuFrame frame;
  auto adu = createSingleBitAccessAdu(badServerAddress,
                                      modbus::FunctionCode::kReadCoils, access);
  frame.setAdu(adu);
  auto data = frame.marshal();
  requestBuffer->Write((const char *)data.data(), data.size());

  auto result = d.processModbusFrame(session, requestBuffer);
  EXPECT_EQ(result,
            modbus::QModbusServerPrivate::ProcessResult::kBadServerAddress);
}

TEST(QModbusServer, recivedRequest_needMoreData) {
  quintptr fd = 1;
  std::shared_ptr<pp::bytes::Buffer> requestBuffer(new pp::bytes::Buffer);

  TestConnection conn;
  TestServer server;

  EXPECT_CALL(conn, fd()).WillRepeatedly(Invoke([&]() { return fd; }));

  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);
  d.incomingConnection(&conn);
  auto &session = d.clientList_.find(fd).value();

  requestBuffer->Write(std::vector<char>({0x00}));
  auto result = d.processModbusFrame(session, requestBuffer);
  EXPECT_EQ(result, modbus::QModbusServerPrivate::ProcessResult::kNeedMoreData);
}

TEST(
    QModbusServer,
    recivedRequest_theRequestedFunctionCodeIsNotSupported_returnIllegalFunctionCodeError) {
  quintptr fd = 1;
  std::shared_ptr<pp::bytes::Buffer> requestBuffer(new pp::bytes::Buffer);

  TestConnection conn;
  TestServer server;

  EXPECT_CALL(conn, fd()).WillRepeatedly(Invoke([&]() { return fd; }));
  EXPECT_CALL(conn, write(_, _)).Times(1);

  modbus::QModbusServerPrivate d;

  d.setServer(&server);
  d.setServerAddress(1);
  d.setTransferMode(modbus::TransferMode::kRtu);

  d.incomingConnection(&conn);

  auto &session = d.clientList_.find(fd).value();

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x10);

  modbus::RtuFrame frame;
  auto adu =
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access);
  frame.setAdu(adu);
  auto data = frame.marshal();
  requestBuffer->Write((const char *)data.data(), data.size());

  auto result = d.processModbusFrame(session, requestBuffer);
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
