#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "modbus.h"
#include "modbus_tool.h"
#include "smart_assert.h"
#include <map>

namespace modbus {
/**
 * modbus data model
 * single bit access.
 * include Discrete input(r),coil(rw)
 * before use SingleBitAccess,must set startAddress, quantity
 */
class SingleBitAccess {
public:
  SingleBitAccess() {}

  void setStartAddress(Address startAddress) { startAddress_ = startAddress; }
  Address startAddress() { return startAddress_; }
  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  Quantity quantity() { return quantity_; }

  void setDeviceName(const std::string &name) { deviceName_ = name; }
  std::string deviceName() const { return deviceName_; }

  void setDescription(Address address, const std::string &description) {
    BitValueEx valueEx;

    valueEx.description = description;
    valueMap_[address] = valueEx;
  }
  /**
   * this will set the value to startAddress
   */
  void setValue(BitValue value) { setValue(startAddress_, value); }
  /**
   * set value to address
   */
  void setValue(Address address, BitValue value) {
    if (valueMap_.find(address) != valueMap_.end()) {
      auto &valueEx = valueMap_[address];
      valueEx.value = value;
    } else {
      BitValueEx valueEx;

      valueEx.value = value;
      valueMap_[address] = valueEx;
    }
  }

  /**
   * Conversion to modbus protocol format
   * function code 0x01,0x02
   */
  ByteArray marshalReadRequest() {
    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity_ / 256);
    array.push_back(quantity_ % 256);
    return array;
  }

  /**
   * Conversion to modbus protocol format
   * function code 0x05
   */
  ByteArray marshalSingleWriteRequest() {
    ByteArray data;

    auto it = valueMap_.find(startAddress_);
    smart_assert(it != valueMap_.end() && "has no value set")(startAddress_);

    data.push_back(startAddress_ / 256);
    data.push_back(startAddress_ % 256);

    data.push_back(it->second.value == BitValue::kOn ? 0xff : 0x00);
    data.push_back(0x00);

    return data;
  }

  /**
   * Conversion to modbus protocol format
   * function code 0x0f
   */
  ByteArray marshalMultipleWriteRequest() {
    ByteArray data;

    data.push_back(startAddress_ / 256);
    data.push_back(startAddress_ % 256);

    data.push_back(quantity_ / 256);
    data.push_back(quantity_ % 256);
    data.push_back(quantity_ % 8 == 0 ? quantity_ / 8 : quantity_ / 8 + 1);

    Address nextAddress = startAddress_;
    Address endAddress = startAddress_ + quantity_;
    uint8_t byte = 0;
    int offset = 0;
    for (; nextAddress < endAddress; nextAddress++) {
      auto it = valueMap_.find(nextAddress);
      smart_assert(it != valueMap_.end() &&
                   "some value of address not set, bad operation!")(
          nextAddress);

      bool value = it->second.value == BitValue::kOn ? true : false;
      byte |= value << offset++;
      if (offset == 8) {
        offset = 0;
        data.push_back(byte);
        byte = 0;
      }
    }
    if (quantity_ % 8 != 0) {
      data.push_back(byte);
    }

    return data;
  }

  bool unmarshalReadResponse(const ByteArray &array) {
    size_t size = 0;
    auto result = bytesRequiredStoreInArrayIndex0(size, array);
    if (result != DataChecker::Result::kSizeOk) {
      return false;
    }

    ByteArray bitvalues(tool::subArray(array, 1));
    Quantity quantity = quantity_;
    Address nextAddress = startAddress_;
    for (const auto &n : bitvalues) {
      Quantity remainQuantity = quantity >= 8 ? 8 : quantity % 8;
      for (int i = 0; i < remainQuantity; i++) {
        Address address = nextAddress++;
        bool status = n & (0x01 << i);

        if (valueMap_.find(address) != valueMap_.end()) {
          auto &valueEx = valueMap_[address];
          valueEx.value = status ? BitValue::kOn : BitValue::kOff;
        } else {
          BitValueEx valueEx;
          valueEx.value = status ? BitValue::kOn : BitValue::kOff;
          valueMap_[address] = valueEx;
        }
      }
      quantity -= remainQuantity;
    }
    return true;
  }

  BitValue value(Address address) {
    auto value = valueEx(address);
    return value.value;
  }

  BitValueEx valueEx(Address address) {
    BitValueEx value;

    auto it = valueMap_.find(address);
    if (it != valueMap_.end()) {
      value = it->second;
    }

    return value;
  }

private:
  Address startAddress_;
  Quantity quantity_ = 0;
  std::string deviceName_;
  std::map<Address, BitValueEx> valueMap_;
};

} // namespace modbus

#endif /* FUNCTIONS_H */
