
#include "modbus_types.h"
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace modbus {

namespace pub {
class ReadCoilsRequest final : public modbus::DataGenerator {
public:
  ReadCoilsRequest(modbus::CoilAddress coilAddress, modbus::Quantity quantity)
      : coilAddress_(coilAddress), quantity_(quantity) {}
  ~ReadCoilsRequest() {}

  modbus::CoilAddress coilAddress() const { return coilAddress_; }

  modbus::Quantity quantity() const { return quantity_; }

  std::vector<char> toByteArray() {
    std::vector<char> byteArray;
    std::string s = "coiladdress:" + std::to_string(coilAddress_) +
                    ",quantity:" + std::to_string(quantity_);
    byteArray.resize(s.size());
    byteArray.assign(s.begin(), s.end());
    return byteArray;
  }
  bool isException() {}

  std::error_code fromByteArray(const std::vector<char> &byteArray) {
    std::string str;
    str.resize(byteArray.size());
    str.assign(byteArray.begin(), byteArray.end());

    return std::error_code();
  }

private:
  modbus::CoilAddress coilAddress_;
  modbus::Quantity quantity_;
};

class ReadCoilsResponse final : public modbus::DataGenerator {
public:
  ReadCoilsResponse(modbus::CoilAddress coilAddress, modbus::Quantity quantity)
      : coilAddress_(coilAddress), quantity_(quantity) {}
  ReadCoilsResponse() {}
  ~ReadCoilsResponse() {}

  std::vector<char> toByteArray() {
    std::vector<char> byteArray;
    std::string s = "coiladdress:" + std::to_string(coilAddress_) +
                    ",quantity:" + std::to_string(quantity_);
    byteArray.resize(s.size());
    byteArray.assign(s.begin(), s.end());
    return byteArray;
  }
  bool isException() {}

  std::error_code fromByteArray(const std::vector<char> &byteArray) {
    std::string str;
    str.resize(byteArray.size());
    str.assign(byteArray.begin(), byteArray.end());

    return std::error_code();
  }

private:
  modbus::CoilAddress coilAddress_;
  modbus::Quantity quantity_;
  std::map<modbus::CoilAddress, modbus::BitValue> coilBitValueMap_;
};
} // namespace pub

} // namespace modbus

int main(int argc, char *argv[]) {
  SerialClient client;

  modbus::Adu adu;
  modbus::Pdu pdu;

  pdu.setFunctionCode(modbus::FunctionCode::kReadCoil);
  pdu.setData(modbus::Data::fromDataGenerator(modbus::pub::ReadCoilsRequest(
      modbus::CoilAddress(1), modbus::Quantity(100))));

  adu.setServerAddress(modbus::ServerAddress(10));
  adu.setPdu(pdu);

  modbus::Response *response =
      client.sendRequest<modbus::pub::ReadCoilsRequest>(adu);
  connect(response, &modbus::finished, [response, &]() {
    auto clientRequest = response->request();
    modbus::pub::ReadCoilsRequest readCoilsRequest =
        clientRequest->dataGenerator<modbus::pub::ReadCoilsRequest>();
    modbus::pub::ReadCoilsResponse readCoilsResponse(
        readCoilsRequest.coilAddress(), readCoilsRequest.quantity());
    readCoilsResponse.fromByteArray(response->dataByteArray());
  });
}
