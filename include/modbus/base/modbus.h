#ifndef __MODBUS_H_
#define __MODBUS_H_

#include "bytes/buffer.h"
#include "modbus/base/modbus_types.h"
#include "modbus_data.h"
#include <array>
#include <bits/stdint-uintn.h>
#include <cstddef>
#include <functional>
#include <mutex>

namespace modbus {

/**
 *Different function codes carry different data in the data in the pdu. So we
 *use this class to check whether the size of the data in the received request
 *or response is valid
 */
struct DataChecker {
public:
  enum class Result { kNeedMoreData, kSizeOk, kFailed };
  using calculateRequiredSizeFunc =
      std::function<Result(size_t &size, const ByteArray &byteArray)>;

  calculateRequiredSizeFunc calculateSize;
};

using CheckSizeFunc = std::function<DataChecker::Result(
    size_t &size, const uint8_t *buffer, int len)>;

/**
 * some function code, data is [byte0,byte1|...|byte-n]
 * so, the expected size is n
 */
template <int nbytes>
static inline DataChecker::Result bytesRequired(size_t &size,
                                                const ByteArray &data) {
  if (data.size() < nbytes) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = nbytes;
  return DataChecker::Result::kSizeOk;
}

template <int nbytes>
static inline DataChecker::Result
bytesRequired2(size_t &size, const uint8_t *buffer, int len) {
  if (len < nbytes) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = nbytes;
  return DataChecker::Result::kSizeOk;
}

/**
 * some function code, data is [byte-number | byte-1|byte-2|...|byte-n]
 * the byte-number == n.
 * so, the expected size is byte-number + 1
 */
template <int index>
static inline DataChecker::Result
bytesRequiredStoreInArrayIndex(size_t &size, const ByteArray &data) {
  int preSize = index + 1;
  if (static_cast<int>(data.size()) < preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size_t bytes = data[index];
  if (static_cast<size_t>(data.size()) < bytes + preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = bytes + preSize;
  return DataChecker::Result::kSizeOk;
}

template <int index>
static inline DataChecker::Result
bytesRequiredStoreInArrayIndex2(size_t &size, const uint8_t *buffer, int len) {
  int preSize = index + 1;
  if (len < preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size_t bytes = buffer[index];
  if ((size_t)len < bytes + preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = bytes + preSize;
  return DataChecker::Result::kSizeOk;
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
  Adu(ServerAddress serverAddress, FunctionCode functionCode,
      const DataChecker &dataChecker)
      : serverAddress_(serverAddress), functionCode_(functionCode),
        dataChecker_(dataChecker) {}

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

  void setDataChecker(const DataChecker &dataChecker) {
    dataChecker_ = dataChecker;
  }

  void setCheckSizeFun(const CheckSizeFunc &dataChecker) {
    calculateSize_ = dataChecker;
  }

  DataChecker dataChecker() const { return dataChecker_; }
  const CheckSizeFunc &checkSizeFunc() const { return calculateSize_; }

  void setData(const ByteArray &byteArray) { data_ = byteArray; }
  void setData(const uint8_t *data, int n) {
    if (data_.capacity() < static_cast<size_t>(n)) {
      data_.resize(n);
    }
    std::copy(data, data + n, data_.begin());
  }

  const ByteArray &data() const { return data_; }

  bool isException() const { return functionCode_ & kExceptionByte; }

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
  ServerAddress serverAddress_;
  // Pdu pdu_;
  FunctionCode functionCode_ = FunctionCode::kInvalidCode;
  DataChecker dataChecker_;
  CheckSizeFunc calculateSize_;
  ByteArray data_;

  uint16_t transactionId_ = 0;
};

/**
 * Frame is a complete modbus frame.
 * in rtu mode:
 *      frame is adu + crc
 * in ascii mode:
 *      frame is ":" + ascii(adu) + lrc + "\r\n"
 * in mbap(tcp) mode:
 *     frame is mbap + adu
 */
class Frame {
public:
  virtual ~Frame() {}
  /**
   * marshal the adu to a complete modbus frame.
   */
  virtual ByteArray marshal(const uint16_t *frameId = nullptr) = 0;
  virtual size_t marshalSize() = 0;
  /**
   * unmarshal a bytearray to modbus::Adu
   * the Frame error stored in error
   */
  virtual DataChecker::Result unmarshal(const ByteArray &data,
                                        Error *error) = 0;

  virtual DataChecker::Result unmarshal(const pp::bytes::Buffer &data,
                                        Error *error) {
    return DataChecker::Result::kNeedMoreData;
  }

  /** only unmarshal server address and the functioncode
   * if success return DataChecker::Result::kSizeOk
   */
  virtual DataChecker::Result
  unmarshalServerAddressFunctionCode(const ByteArray &data,
                                     ServerAddress *serverAddress,
                                     FunctionCode *functionCode) = 0;

  void setAdu(const Adu &adu) { adu_ = adu; }
  Adu adu() const { return adu_; }
  uint16_t frameId() const { return id_; }

protected:
  using TransactionId = uint16_t;

  static TransactionId nextTransactionId() {
    static std::mutex mutex_;
    static TransactionId nextId = 0;

    std::lock_guard<std::mutex> l(mutex_);
    return nextId++;
  }

  Adu adu_;
  TransactionId id_ = 0;
};

// the index of array will be used as the functionCode
using CheckSizeFuncTable = std::array<CheckSizeFunc, 256>;

class ModbusFrameDecoder {
public:
  explicit ModbusFrameDecoder(const CheckSizeFuncTable &table)
      : checkSizeFuncTable_(table) {}

  virtual ~ModbusFrameDecoder() = default;

  virtual DataChecker::Result Decode(pp::bytes::Buffer &buffer, Adu *adu) = 0;
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
          const DataChecker &dataChecker, const any &userData,
          const ByteArray &data)
      : Adu(serverAddress, functionCode, dataChecker), userData_(userData) {
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
