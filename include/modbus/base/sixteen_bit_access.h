#ifndef SIXTEEN_BIT_ACCESS_H
#define SIXTEEN_BIT_ACCESS_H

#include <map>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/smart_assert.h>

namespace modbus {
class SixteenBitAccess {
public:
  SixteenBitAccess() = default;
  virtual ~SixteenBitAccess() noexcept = default;

  static inline DataChecker::Result
  calculateSizeOfReadResponse(size_t &size, const ByteArray &data) {
    if (data.empty()) {
      return DataChecker::Result::kNeedMoreData;
    }
    size_t bytes = data[0];
    if (bytes + 1 != data.size()) {
      return DataChecker::Result::kNeedMoreData;
    }
    /// Must be a multiple of 2
    if (bytes % 2 != 0) {
      return DataChecker::Result::kNeedMoreData;
    }
    size = bytes + 1;
    return DataChecker::Result::kSizeOk;
  }

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
    if (valueMap_.find(address) != valueMap_.end()) {
      auto &valueEx = valueMap_[address];
      valueEx.value = value;
    } else {
      SixteenBitValueEx valueEx;

      valueEx.value = value;
      valueMap_[address] = valueEx;
    }
  }
  uint16_t value(Address address, bool *ok = nullptr) {
    auto v = valueEx(address, ok);
    return v.value;
  }

  SixteenBitValueEx valueEx(Address address, bool *ok = nullptr) {
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

  ByteArray marshalSingleWriteRequest() {
    smart_assert(valueMap_.find(startAddress_) != valueMap_.end() &&
                 "no set value of start address")(startAddress_);

    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(valueMap_[startAddress_].value / 256);
    array.push_back(valueMap_[startAddress_].value % 256);

    return array;
  }

  ByteArray marshalMultipleWriteRequest() {
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
      array.push_back(valueEx.value / 256);
      array.push_back(valueEx.value % 256);
    }
    return array;
  }

  bool unmarshalReadResponse(const ByteArray &data) {
    size_t size = 0;
    auto result = calculateSizeOfReadResponse(size, data);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }

    auto valueArray = tool::subArray(data, 1);
    Address nextAddress = startAddress();
    for (int i = 0; i < valueArray.size(); i += 2) {
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
  Address startAddress_;
  Quantity quantity_ = 0;
  std::string deviceName_;
  std::map<Address, SixteenBitValueEx> valueMap_;
};
} // namespace modbus

#endif /* SIXTEEN_BIT_ACCESS_H */
