#ifndef SIXTEEN_BIT_ACCESS_H
#define SIXTEEN_BIT_ACCESS_H

#include <map>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/modbus_types.h>
#include <modbus/base/smart_assert.h>

namespace modbus {
class SixteenBitAccess {
public:
  SixteenBitAccess() = default;
  virtual ~SixteenBitAccess() = default;

  void setDeviceName(const std::string &name) { deviceName_ = name; }
  std::string deviceName() const { return deviceName_; }

  void setStartAddress(Address address) { startAddress_ = address; }
  Address startAddress() const { return startAddress_; }

  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  Quantity quantity() const { return quantity_; }

  void setDescription(Address address, const std::string &description) {
    SixteenBitValueEx valueEx;

    valueEx.description = description;
    valueMap_[address] = valueEx;
  }

  void setValue(uint16_t value) { setValue(startAddress_, value); }
  void setValue(Address address, uint16_t value) {
    if (address > startAddress_ + quantity_ || address < startAddress_) {
      /// out of range
      return;
    }
    if (valueMap_.find(address) != valueMap_.end()) {
      auto &valueEx = valueMap_[address];
      valueEx.value = value;
    } else {
      SixteenBitValueEx valueEx;

      valueEx.value = value;
      valueMap_[address] = valueEx;
    }
  }
  SixteenBitValue value(Address address, bool *ok = nullptr) const {
    auto v = valueEx(address, ok);
    return v.value;
  }

  SixteenBitValueEx valueEx(Address address, bool *ok = nullptr) const {
    SixteenBitValueEx value;

    bool isFound = true;
    auto it = valueMap_.find(address);
    if (it == valueMap_.end()) {
      isFound = false;
    } else {
      isFound = true;
      value = it->second;
    }
    if (ok) {
      *ok = isFound;
    }
    return value;
  }

  ByteArray marshalMultipleReadRequest() const {
    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity_ / 256);
    array.push_back(quantity_ % 256);

    return array;
  }

  bool unmarshalAddressQuantity(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    quantity_ = data[2] * 256 + data[3];
    return true;
  }

  bool unmarshalSingleWriteRequest(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    quantity_ = 1;
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
    quantity_ = data[2] * 256 + data[3];
    auto lenght = data[4];
    if (lenght % 2 != 0) {
      return false;
    }
    if (quantity_ != lenght / 2) {
      return false;
    }
    auto valueArray = tool::subArray(data, 5);
    for (size_t i = 0; i < lenght; i += 2) {
      SixteenBitValue value(valueArray[i], valueArray[i + 1]);
      Address address = startAddress_ + i / 2;
      setValue(address, value.toUint16());
    }
    return true;
  }

  ByteArray marshalSingleWriteRequest() const {
    smart_assert(valueMap_.find(startAddress_) != valueMap_.end() &&
                 "no set value of start address")(startAddress_);

    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(valueMap_[startAddress_].value.toUint16() / 256);
    array.push_back(valueMap_[startAddress_].value.toUint16() % 256);

    return array;
  }

  ByteArray marshalMultipleWriteRequest() const {
    smart_assert(quantity_ == valueMap_.size() &&
                 "invalid data, no set some value,or more")(quantity_)(
        valueMap_.size());

    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity_ / 256);
    array.push_back(quantity_ % 256);

    array.push_back(quantity_ * 2);

    for (Address nextAddress = startAddress_;
         nextAddress < startAddress_ + quantity_; nextAddress++) {
      smart_assert(valueMap_.find(nextAddress) != valueMap_.end() &&
                   "no set value of address")(nextAddress);
      auto valueEx = valueMap_[nextAddress];
      array.push_back(valueEx.value.toUint16() / 256);
      array.push_back(valueEx.value.toUint16() % 256);
    }
    return array;
  }

  ByteArray marshalMultipleReadResponse() {
    ByteArray array;

    array.push_back(quantity_ * 2);

    for (Address nextAddress = startAddress_;
         nextAddress < startAddress_ + quantity_; nextAddress++) {
      smart_assert(valueMap_.find(nextAddress) != valueMap_.end() &&
                   "no set value of address")(nextAddress);
      auto valueEx = valueMap_[nextAddress];
      array.push_back(valueEx.value.toUint16() / 256);
      array.push_back(valueEx.value.toUint16() % 256);
    }
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

    auto valueArray = tool::subArray(data, 1);
    Address nextAddress = startAddress();
    for (size_t i = 0; i < valueArray.size(); i += 2) {
      uint16_t v = 0;
      v = valueArray[i] * 256 + valueArray[i + 1];

      if (valueMap_.find(nextAddress) != valueMap_.end()) {
        auto &vEx = valueMap_[nextAddress];
        vEx.value = v;
      } else {
        valueMap_[nextAddress].value = v;
      }
      nextAddress++;
    }
    return true;
  }

private:
  Address startAddress_ = 0;
  Quantity quantity_ = 0;
  std::string deviceName_;
  mutable std::map<Address, SixteenBitValueEx> valueMap_;
};

bool processReadRegisters(const Request &request, const Response &response,
                          SixteenBitAccess *access);
} // namespace modbus

#endif /* SIXTEEN_BIT_ACCESS_H */
