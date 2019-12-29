#ifndef __MODBUS_H_
#define __MODBUS_H_

#include "modbus_data.h"
#include <functional>

namespace modbus {

/**
 *Different function codes carry different data in the data in the pdu. So we
 *use this class to check whether the size of the data in the received request
 *or response is valid
 */
struct DataChecker {
public:
  enum class Result { kNeedMoreData, kSizeOk, kUnkown, kFailed };
  using calculateRequiredSizeFunc =
      std::function<Result(size_t &size, const ByteArray &byteArray)>;
  /**
   * for check request from client
   */
  calculateRequiredSizeFunc calculateRequestSize;
  /**
   * for check response from server
   */
  calculateRequiredSizeFunc calculateResponseSize;
};

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

/**
 * some function code, data is [byte-number | byte-1|byte-2|...|byte-n]
 * the byte-number == n.
 * so, the expected size is byte-number + 1
 */
template <int index>
static inline DataChecker::Result
bytesRequiredStoreInArrayIndex(size_t &size, const ByteArray &data) {
  int preSize = index + 1;
  if (data.size() < preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size_t bytes = data[index];
  if (data.size() < bytes + preSize) {
    return DataChecker::Result::kNeedMoreData;
  }
  size = bytes + preSize;
  return DataChecker::Result::kSizeOk;
}

/**
 * Protocol data unit
 * in modbus frame, it is function Code + data
 */
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

/**
 * Application data unit
 * in modbus frame, it is address field + pdu + error checking.
 * but our adu not include error checking
 */
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

  void setPdu(const Pdu &pdu) { pdu_ = pdu; }
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
   * @brief marshalAduWithoutCrc,that is: serveraddress + fuction code + payload
   */
  ByteArray marshalAduWithoutCrc() {
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
  virtual ByteArray marshal() = 0;
  virtual size_t marshalSize() = 0;
  /**
   * unmarshal a bytearray to modbus::Adu
   * the Frame error stored in error
   */
  virtual DataChecker::Result unmarshal(const ByteArray &data,
                                        Error *error) = 0;

  void setAdu(const Adu &adu) { adu_ = adu; }
  Adu adu() const { return adu_; }

protected:
  Adu adu_;
};

/**
 * a modbus request
 */
class Request : public Adu {
public:
  void setUserData(const any &userData) { userData_ = userData; }
  any userData() const { return userData_; }

private:
  any userData_;
};

/**
 * a modbus response
 */
class Response : public Adu {
public:
  Response() : errorCode_(Error::kNoError), Adu() {}
  Response(const Adu &adu) : errorCode_(Error::kNoError), Adu(adu) {}
  void setError(Error errorCode) { errorCode_ = errorCode; }

  Error error() const { return errorCode_; }
  std::string errorString() const { return std::to_string(errorCode_); }

private:
  Error errorCode_;
};

void registerLogMessage(const LogWriter &logger);

} // namespace modbus

#endif // __MODBUS_H_
