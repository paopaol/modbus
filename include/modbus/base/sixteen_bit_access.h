#ifndef SIXTEEN_BIT_ACCESS_H
#define SIXTEEN_BIT_ACCESS_H

#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/modbus_types.h>
#include <modbus/base/smart_assert.h>
#include <unordered_map>

namespace modbus {
class SixteenBitAccess {
public:
  SixteenBitAccess() = default;
  virtual ~SixteenBitAccess() = default;

  void setStartAddress(Address address) {
    startAddress_ = address;
    if (quantity() == 0) {
      setQuantity(1);
    }
  }
  Address startAddress() const { return startAddress_; }

  void setQuantity(Quantity quantity) {
    quantity_ = quantity;
    value_array_.resize(quantity_ * 2);
  }

  Quantity quantity() const { return quantity_; }

  void setValue(uint16_t value) { setValue(startAddress_, value); }
  void setValue(Address address, uint16_t value) {
    if (address >= startAddress_ + quantity() || address < startAddress_) {
      /// out of range
      return;
    }
    SixteenBitValue v(value);
    size_t i = (address - startAddress_) * 2;
    value_array_[i] = v.firstByte();
    value_array_[i + 1] = v.secondByte();
  }

  ByteArray value() const { return value_array_; }

  SixteenBitValue value(Address address, bool *ok = nullptr) const {
    Address start_address = startAddress();
    Quantity quan = quantity();
    int size = value_array_.size();
    for (int i = (address - startAddress_) * 2;
         i >= 0 && address < start_address + quan; i += 2) {
      if (ok) {
        *ok = true;
      }
      if (i >= size) {
        break;
      }
      return SixteenBitValue(value_array_[i], value_array_[i + 1]);
    }

    if (ok) {
      *ok = false;
    }
    return SixteenBitValue();
  }

  ByteArray marshalMultipleReadRequest() const {
    return ByteArray({static_cast<uint8_t>(startAddress_ / 256),
                      static_cast<uint8_t>(startAddress_ % 256),
                      static_cast<uint8_t>(quantity() / 256),
                      static_cast<uint8_t>(quantity() % 256)});
  }

  bool unmarshalAddressQuantity(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    setQuantity(data[2] * 256 + data[3]);
    return true;
  }

  bool unmarshalSingleWriteRequest(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    setQuantity(1);
    SixteenBitValue value(data[2], data[3]);
    setValue(startAddress_, value.toUint16());
    return true;
  }

  bool unmarshalMulitpleWriteRequest(const ByteArray &data) {
    size_t size;
    auto result = bytesRequiredStoreInArrayIndex<4>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    setQuantity(data[2] * 256 + data[3]);
    auto lenght = data[4];
    if (lenght % 2 != 0) {
      return false;
    }
    if (quantity() != lenght / 2) {
      return false;
    }
    value_array_ = tool::subArray(data, 5);
    return true;
  }

  ByteArray marshalSingleWriteRequest() const {
    ByteArray array;

    array.reserve(4);

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(value_array_[0]);
    array.push_back(value_array_[1]);

    return array;
  }

  ByteArray marshalMultipleWriteRequest() const {
    ByteArray array;
    array.reserve(5 + value_array_.size());

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity() / 256);
    array.push_back(quantity() % 256);

    array.push_back(quantity() * 2);

    array.insert(array.end(), value_array_.begin(), value_array_.end());
    return array;
  }

  ByteArray marshalMultipleReadResponse() {
    ByteArray array;
    array.reserve(1 + value_array_.size());

    array.push_back(quantity() * 2);
    array.insert(array.end(), value_array_.begin(), value_array_.end());
    return array;
  }

  bool unmarshalReadResponse(const ByteArray &data) {
    size_t size = 0;
    auto result = bytesRequiredStoreInArrayIndex<0>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }

    /**
     * Sixteen bit value, one value use 2 bytes.so, index0 + numberOfValues * 2
     */
    if (size % 2 != 1) {
      return false;
    }

    value_array_ = tool::subArray(data, 1);
    return true;
  }

private:
  Address startAddress_ = 0;
  Quantity quantity_ = 0;
  ByteArray value_array_;
};

bool processReadRegisters(const Request &request, const Response &response,
                          SixteenBitAccess *access);
} // namespace modbus

#endif /* SIXTEEN_BIT_ACCESS_H */
