#ifndef __MODBUS_H_
#define __MODBUS_H_

#include "bytes/buffer.h"
#include "modbus/base/modbus_types.h"
#include "modbus_data.h"
#include <array>
#include <cstddef>
#include <functional>
#include <mutex>

namespace modbus {

/**
 *Different function codes carry different data in the data in the pdu. So we
 *use this class to check whether the size of the data in the received request
 *or response is valid
 */
enum class CheckSizeResult { kNeedMoreData, kSizeOk, kFailed };

using CheckSizeFunc = std::function<CheckSizeResult(
    size_t &size, const uint8_t *buffer, int len)>;

template <int nbytes>
static inline CheckSizeResult
bytesRequired(size_t &size, const uint8_t * /*buffer*/, int len) {
  if (len < nbytes) {
    return CheckSizeResult::kNeedMoreData;
  }
  size = nbytes;
  return CheckSizeResult::kSizeOk;
}

template <int index>
static inline CheckSizeResult
bytesRequiredStoreInArrayIndex(size_t &size, const uint8_t *buffer, int len) {
  int preSize = index + 1;
  if (len < preSize) {
    return CheckSizeResult::kNeedMoreData;
  }
  size_t bytes = buffer[index];
  if ((size_t)len < bytes + preSize) {
    return CheckSizeResult::kNeedMoreData;
  }
  size = bytes + preSize;
  return CheckSizeResult::kSizeOk;
}

/**
 * Application data unit
 * in modbus frame, it is address field + pdu + error checking.
 * but our adu not include error checking
 */
class Adu {
public:
  static const ServerAddress kBrocastAddress = 0;
  static const uint8_t kExceptionByte = 0x80;

  Adu() {}
  Adu(ServerAddress serverAddress, FunctionCode functionCode)
      : serverAddress_(serverAddress), functionCode_(functionCode) {}

  ~Adu() {}

  void setServerAddress(ServerAddress serverAddress) {
    serverAddress_ = serverAddress;
  }
  ServerAddress serverAddress() const { return serverAddress_; }
  bool isBrocast() { return serverAddress_ == kBrocastAddress; }

  void setFunctionCode(FunctionCode functionCode) {
    functionCode_ = functionCode;
  }
  FunctionCode functionCode() const {
    return FunctionCode(uint8_t(functionCode_) & ~kExceptionByte);
  }

  void setData(const ByteArray &byteArray) { data_ = byteArray; }
  void setData(const uint8_t *data, int n) {
    data_.resize(n);
    std::copy(data, data + n, data_.begin());
  }

  const ByteArray &data() const { return data_; }

  bool isException() const { return functionCode_ & kExceptionByte; }

  Error error() const {
    if (!isException()) {
      return Error::kNoError;
    }
    return Error(data_[0]);
  }

  bool isValid() const { return !data_.empty(); }

  size_t marshalSize() const { return 1 + 1 + data_.size(); }

  void setTransactionId(uint16_t transactionId) {
    transactionId_ = transactionId;
  }

  uint16_t transactionId() const { return transactionId_; }
  /**
   * @brief marshalAduWithoutCrc,that is: serveraddress + fuction code + payload
   */
  ByteArray marshalAduWithoutCrc() {
    ByteArray array;
    array.reserve(2 + data_.size());

    array.push_back(serverAddress());
    if (isException()) {
      array.push_back(FunctionCode(functionCode() | kExceptionByte));
    } else {
      array.push_back(functionCode());
    }
    for (int i = 0, size = data_.size(); i < size; i++) {
      array.push_back(data_[i]);
    }
    return array;
  }

private:
  ServerAddress serverAddress_ = 0;
  // Pdu pdu_;
  FunctionCode functionCode_ = FunctionCode::kInvalidCode;
  ByteArray data_;
  uint16_t transactionId_ = 0;
};

// the index of array will be used as the functionCode
using CheckSizeFuncTable = std::array<CheckSizeFunc, 256>;

class ModbusFrameDecoder {
public:
  explicit ModbusFrameDecoder(const CheckSizeFuncTable &table)
      : checkSizeFuncTable_(table) {}

  virtual ~ModbusFrameDecoder() = default;

  virtual CheckSizeResult Decode(pp::bytes::Buffer &buffer, Adu *adu) = 0;
  virtual bool IsDone() const = 0;
  virtual void Clear() = 0;
  virtual Error LasError() const = 0;

protected:
  CheckSizeFuncTable checkSizeFuncTable_;
};

class ModbusFrameEncoder {
public:
  virtual ~ModbusFrameEncoder() = default;

  virtual void Encode(const Adu *adu, pp::bytes::Buffer &buffer) = 0;
};

/**
 * a modbus request
 */
class Request : public Adu {
public:
  Request() {}
  Request(ServerAddress serverAddress, FunctionCode functionCode,
          const any &userData, const ByteArray &data)
      : Adu(serverAddress, functionCode), userData_(userData) {
    setData(data);
  }
  Request(const Adu &adu) : Adu(adu) {}
  void setUserData(const any &userData) { userData_ = userData; }
  any userData() const { return userData_; }
  const any &userData() { return userData_; }

private:
  any userData_;
};

/**
 * a modbus response
 */
class Response : public Adu {
public:
  Response() : Adu(), errorCode_(Error::kNoError) {}
  Response(const Adu &adu) : Adu(adu), errorCode_(Error::kNoError) {}
  void setError(Error errorCode) { errorCode_ = errorCode; }

  Error error() const { return errorCode_; }
  std::string errorString() const { return std::to_string(errorCode_); }

private:
  Error errorCode_;
};

void registerLogMessage(const LogWriter &logger);

} // namespace modbus

#endif // __MODBUS_H_
