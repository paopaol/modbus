#ifndef __MODBUS_H_
#define __MODBUS_H_

#include <QObject>
#include <modbus_data.h>

namespace modbus {

class Pdu {
public:
  void setFunctionCode(FunctionCode functionCode);
  void setData(const Data &data);
  Data data();

private:
  FunctionCode functionCode_;
  Data data_;
};

class Adu {
public:
  Adu() {}
  ~Adu() {}
  void setServerAddress(ServerAddress serverAddress) {}
  void setPdu(const Pdu &pdu) {}
  Pdu pdu() const { return pdu_; }

private:
  ServerAddress serverAddress_;
  Pdu pdu_;
};

class Request {
public:
  void setAdu(const modbus::Adu &adu) { adu_ = adu; }
  modbus::Adu adu() const { return adu_; }
  modbus::Pdu pdu() const { return adu_.pdu(); }
  modbus::Data data() const {
    modbus::Pdu p = pdu();
    auto data = p.data();
    return data;
  }
  template <typename T> T dataGenerator() const {
    auto data = data();
    return data.dataGenerator<T>();
  }

private:
  modbus::Adu adu_;
};

class Response : public Request {
public:
  Request *request() const { return request_; }
  std::vector<char> dataByteArray() const { return dataByteArray_; }

private:
  Request *request_;
  std::vector<char> dataByteArray_;
};
} // namespace modbus

#endif // __MODBUS_H_
