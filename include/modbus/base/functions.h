#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "modbus.h"
#include "modbus_tool.h"
#include <assert.h>
#include <map>

namespace modbus {
class SingleBitAccess {
public:
  SingleBitAccess() {}

  void buildFromArray(const ByteArray &array) {}
  void setStartAddress(Address startAddress) { startAddress_ = startAddress; }
  Address startAddress() { return startAddress_; }
  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  Quantity quantity() { return quantity_; }
  void setValue(BitValue value) { valueMap_[startAddress_] = value; }
  void setValue(Address address, BitValue value) { valueMap_[address] = value; }

  ByteArray marshalReadRequest() {
    ByteArray array;

    array.push_back(startAddress_ / 256);
    array.push_back(startAddress_ % 256);

    array.push_back(quantity_ / 256);
    array.push_back(quantity_ % 256);
    return array;
  }

  ByteArray marshalSingleWriteRequest(bool *result = nullptr) {
    ByteArray data;
    bool ok = true;
    if (valueMap_.size() != 1) {
      ok = false;
    }
    auto it = valueMap_.find(startAddress_);
    assert(it != valueMap_.end() && "has no value set");
    data.push_back(startAddress_ / 256);
    data.push_back(startAddress_ % 256);

    if (it->second == BitValue::kOn) {
      data.push_back(0xff);
    } else {
      data.push_back(0x00);
    }
    data.push_back(0x00);

    if (result) {
      *result = ok;
    }

    return data;
  }

  bool unmarshalResponse(const ByteArray &array) {
    if (array.empty()) {
      return false;
    }
    /**
     * bytes bumber
     */
    int bytes = array[0];
    if (bytes + 1 != array.size()) {
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
        valueMap_[address] = status ? BitValue::kOn : BitValue::kOff;
      }
      quantity -= remainQuantity;
    }
    return true;
  }

  BitValue value(Address address) {
    BitValue value = BitValue::kBadValue;

    auto it = valueMap_.find(address);
    if (it != valueMap_.end()) {
      value = it->second;
    }

    return value;
  }

private:
  Address startAddress_ = 0;
  Quantity quantity_ = 0;
  std::map<Address, BitValue> valueMap_;
};

} // namespace modbus

#endif /* FUNCTIONS_H */