#ifndef SIXTEEN_BIT_ACCESS_H
#define SIXTEEN_BIT_ACCESS_H

#include <map>
#include <modbus/base/modbus.h>
#include <modbus/base/smart_assert.h>

namespace modbus {

class SixteenBitAccess {
public:
  SixteenBitAccess() = default;
  virtual ~SixteenBitAccess() noexcept = default;

  void setStartAddress(Address address) { startAddress_ = address; }
  Address startAddress() const { return startAddress_; }

  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  Quantity quantoty() const { return quantity_; }

  void setValue(uint16_t value) { valueMap_[startAddress_] = value; }
  void setValue(Address address, uint16_t value) { valueMap_[address] = value; }

  ByteArray marshalMultipleReadRequest() {
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

    array.push_back(valueMap_[startAddress_] / 256);
    array.push_back(valueMap_[startAddress_] % 256);

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
      auto value = valueMap_[nextAddress];
      array.push_back(value / 256);
      array.push_back(value % 256);
    }
    return array;
  }

private:
  Address startAddress_ = 0;
  Quantity quantity_ = 0;
  std::map<Address, uint16_t> valueMap_;
};
} // namespace modbus

#endif /* SIXTEEN_BIT_ACCESS_H */
