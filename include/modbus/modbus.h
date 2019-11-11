#ifndef __MODBUS_H_
#define __MODBUS_H_

#include "modbus_data.h"
#include <functional>

namespace modbus {
struct DataChecker {
public:
  enum class Result { kNeedMoreData, kSizeOk };
  using calculateRequiredSizeFunc =
      std::function<Result(size_t &size, const ByteArray &byteArray)>;
  calculateRequiredSizeFunc calculateRequestSize;
  calculateRequiredSizeFunc calculateResponseSize;
};

class Pdu {
public:
  Pdu() {}
  Pdu(FunctionCode functionCode, const DataChecker &dataChecker)
      : functionCode_(functionCode), dataChecker_(dataChecker) {}
  static const uint8_t kExceptionByte = 0x80;

  void setFunctionCode(FunctionCode functionCode) {
    functionCode_ = functionCode;
  }
  FunctionCode functionCode() const {
    return FunctionCode(uint8_t(functionCode_) & ~kExceptionByte);
  }
  void setDataChecker(const DataChecker &dataChecker) {
    dataChecker_ = dataChecker;
  }
  DataChecker dataChecker() const { return dataChecker_; }
  bool isException() { return functionCode_ & kExceptionByte; }
  void setData(const ByteArray &byteArray) { data_ = byteArray; }
  ByteArray data() const { return data_; }

private:
  FunctionCode functionCode_ = FunctionCode::kInvalidCode;
  DataChecker dataChecker_;
  ByteArray data_;
};

class Adu {
public:
  Adu() {}
  Adu(ServerAddress serverAddress, FunctionCode functionCode,
      const DataChecker &dataChecker)
      : serverAddress_(serverAddress), pdu_(functionCode, dataChecker) {}
  Adu(ServerAddress serverAddress, const Pdu &pdu)
      : serverAddress_(serverAddress), pdu_(pdu) {}
  ~Adu() {}

  void setServerAddress(ServerAddress serverAddress) {
    serverAddress_ = serverAddress;
  }
  ServerAddress serverAddress() const { return serverAddress_; }

  void setFunctionCode(FunctionCode functionCode) {
    pdu_.setFunctionCode(functionCode);
  }
  FunctionCode functionCode() const { return pdu_.functionCode(); }

  void setPdu(const Pdu &pdu) {}
  Pdu pdu() const { return pdu_; }

  void setDataChecker(const DataChecker &dataChecker) {
    pdu_.setDataChecker(dataChecker);
  }

  void setData(const ByteArray &byteArray) { pdu_.setData(byteArray); }
  ByteArray data() const { return pdu_.data(); }

  bool isException() { return pdu_.isException(); }

private:
  ServerAddress serverAddress_;
  Pdu pdu_;
};

class Request : public Adu {
public:
  void setUserData(const any &userData) { userData_ = userData; }
  any userData() const { return userData_; }

private:
  any userData_;
};

using Response = Adu;

} // namespace modbus

#endif // __MODBUS_H_
