#ifndef __MODBUS_H_
#define __MODBUS_H_

#include "modbus_data.h"
#include <functional>

namespace modbus {
struct DataChecker {
public:
  enum class Result { kNeedMoreData, kSizeOk, kUnkown };
  using calculateRequiredSizeFunc =
      std::function<Result(size_t &size, const ByteArray &byteArray)>;
  calculateRequiredSizeFunc calculateRequestSize;
  calculateRequiredSizeFunc calculateResponseSize;
};

template <int nbytes>
static inline DataChecker::Result bytesRequired(size_t &size,
                                                const ByteArray &data) {
  if (data.size() < nbytes) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = nbytes;
  return DataChecker::Result::kSizeOk;
}

static inline DataChecker::Result
bytesRequiredStoreInArrayIndex0(size_t &size, const ByteArray &data) {
  if (data.empty()) {
    return DataChecker::Result::kNeedMoreData;
  }
  size_t bytes = data[0];
  if (data.size() < bytes + 1) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = bytes + 1;
  return DataChecker::Result::kSizeOk;
}

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
  bool isException() const { return functionCode_ & kExceptionByte; }
  void setData(const ByteArray &byteArray) { data_ = byteArray; }

  /**
   * @brief this data is only payload, not include function code
   */
  ByteArray data() const { return data_; }
  /**
   * @brief return the size of payload
   */
  size_t size() const { return data_.size(); }

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
  bool isBrocast() { return serverAddress_ == kBrocastAddress; }

  void setFunctionCode(FunctionCode functionCode) {
    pdu_.setFunctionCode(functionCode);
  }
  FunctionCode functionCode() const { return pdu_.functionCode(); }

  void setPdu(const Pdu &pdu) {}
  Pdu pdu() const { return pdu_; }

  void setDataChecker(const DataChecker &dataChecker) {
    pdu_.setDataChecker(dataChecker);
  }
  DataChecker dataChecker() const { return pdu_.dataChecker(); }

  void setData(const ByteArray &byteArray) { pdu_.setData(byteArray); }
  ByteArray data() const { return pdu_.data(); }

  bool isException() const { return pdu_.isException(); }

  size_t marshalSize() const { return 1 + 1 + pdu_.size(); }
  /**
   * @brief marshalData,that is: serveraddress + fuction code + payload
   */
  ByteArray marshalData() {
    ByteArray array;

    array.push_back(serverAddress());
    array.push_back(pdu_.functionCode());
    const auto &data = pdu_.data();
    array.insert(array.end(), data.begin(), data.end());
    return array;
  }

  static const ServerAddress kBrocastAddress = 0;

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

class Response : public Adu {
public:
  Response() : errorCode_(Error::kNoError), Adu() {}
  void setError(Error errorCode, const std::string &errorString) {
    errorCode_ = errorCode;
    errorString_ = errorString;
  }

  Error error() const { return errorCode_; }
  std::string errorString() const { return errorString_; }

private:
  Error errorCode_;
  std::string errorString_;
};

} // namespace modbus

#endif // __MODBUS_H_
