#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "modbus.h"
#include "modbus_tool.h"
#include "smart_assert.h"
#include <algorithm>
#include <unordered_map>

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
  Address startAddress() const { return startAddress_; }
  void setQuantity(Quantity quantity) { quantity_ = quantity; }
  Quantity quantity() const { return quantity_; }

  /**
   * this will set the value to startAddress
   */
  void setValue(bool value) { setValue(startAddress_, value); }
  /**
   * set value to address
   */
  void setValue(Address address, bool value) { valueMap_[address] = value; }

  /**
   * Conversion to modbus protocol format
   * function code 0x01,0x02
   */
  ByteArray marshalReadRequest() const { return marshalAddressQuantity(); }

  ByteArray marshalAddressQuantity() const {
    return ByteArray({static_cast<uint8_t>(startAddress_ / 256),
                      static_cast<uint8_t>(startAddress_ % 256),
                      static_cast<uint8_t>(quantity_ / 256),
                      static_cast<uint8_t>(quantity_ % 256)});
  }

  bool unmarshalReadRequest(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data.data(), data.size());
    if (result != CheckSizeResult::kSizeOk) {
      return false;
    }

    startAddress_ = data[0] * 256 + data[1];
    quantity_ = data[2] * 256 + data[3];
    return true;
  }

  /**
   * Conversion to modbus protocol format
   * function code 0x05
   */
  ByteArray marshalSingleWriteRequest() const {
    ByteArray data;

    data.reserve(4);

    auto it = valueMap_.find(startAddress_);
    smart_assert(it != valueMap_.end() && "has no value set")(startAddress_);

    data.push_back(startAddress_ / 256);
    data.push_back(startAddress_ % 256);

    data.push_back(it->second ? 0xff : 0x00);
    data.push_back(0x00);

    return data;
  }

  /**
   * Conversion to modbus protocol format
   * function code 0x0f
   */
  ByteArray marshalMultipleWriteRequest() {
    ByteArray data;

    data.reserve(48);

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

      bool value = it->second ? true : false;
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

  ByteArray marshalReadResponse() {
    auto take = [&](int address, int n) {
      std::vector<int> bitValueList;

      for (int i = address; i < address + n; i++) {
        int temp = 0;
        auto v = valueMap_.find(i);
        if (v == valueMap_.end()) {
          temp = false;
        } else {
          temp = v->second ? true : false;
        }
        bitValueList.push_back(temp);
      }
      return bitValueList;
    };

    ByteArray data;

    uint8_t bytesNumber =
        quantity_ % 8 == 0 ? quantity_ / 8 : quantity_ / 8 + 1;
    data.push_back(bytesNumber);

    Address address = startAddress_;
    for (int remainingNumber = quantity_; remainingNumber > 0;) {
      int numbers = std::min(8, remainingNumber);
      auto bitValueList = take(address, numbers);
      uint8_t byte = 0;
      for (int offset = 0; offset < numbers; offset++) {
        byte |= bitValueList[offset] << offset;
      }
      data.push_back(byte);
      remainingNumber -= numbers;
      address += numbers;
    }
    return data;
  }

  bool unmarshalReadResponse(const ByteArray &array) {
    return unmarshalValueArray(array);
  }

  bool unmarshalSingleWriteRequest(const ByteArray &data) {
    size_t size;
    auto result = bytesRequired<4>(size, data.data(), data.size());
    if (result != CheckSizeResult::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    quantity_ = 1;
    if (data[2] == 0xff && data[3] == 0x00) {
      setValue(true);
    } else if (data[2] == 0x00 && data[3] == 0x00) {
      setValue(false);
    } else {
      return false;
    }
    return true;
  }

  bool unmarshalMultipleWriteRequest(const ByteArray &data) {
    size_t size;
    auto result =
        bytesRequiredStoreInArrayIndex<4>(size, data.data(), data.size());
    if (result != CheckSizeResult::kSizeOk) {
      return false;
    }
    startAddress_ = data[0] * 256 + data[1];
    quantity_ = data[2] * 256 + data[3];
    auto valueArray = tool::subArray(data, 4);
    return unmarshalValueArray(valueArray);
  }

  bool value(Address address) const {
    auto it = valueMap_.find(address);
    if (it != valueMap_.end()) {
      return it->second;
    }

    return false;
  }

private:
  bool unmarshalValueArray(const ByteArray &array) {
    size_t size = 0;
    auto result =
        bytesRequiredStoreInArrayIndex<0>(size, array.data(), array.size());
    if (result != CheckSizeResult::kSizeOk) {
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
        valueMap_[address] = status;
      }
      quantity -= remainQuantity;
    }
    return true;
  }

  Address startAddress_ = 0xff;
  Quantity quantity_ = 0;
  mutable std::unordered_map<Address, bool> valueMap_;
};

bool processReadSingleBit(const Request &request, const Response &response,
                          SingleBitAccess *access,
                          const std::string &log_prefix = "");
} // namespace modbus

#endif /* FUNCTIONS_H */
