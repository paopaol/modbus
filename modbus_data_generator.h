#ifndef __MODBUS_DATA_GENERATOR_H_
#define __MODBUS_DATA_GENERATOR_H_

#include <modbus_types.h>
#include <system_error>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace modbus {
/// only store subclass of DataGenerator
class DataAnyGenerator {
public:
  DataAnyGenerator() : content(0) {}

  template <typename ValueType>
  DataAnyGenerator(const ValueType &value)
      : content(new holder<ValueType>(value)) {}

  DataAnyGenerator(const DataAnyGenerator &other)
      : content(other.content ? other.content->clone() : 0) {}

  ~DataAnyGenerator() { delete content; }

  DataAnyGenerator &swap(DataAnyGenerator &rhs) {
    std::swap(content, rhs.content);
    return *this;
  }

  DataAnyGenerator &operator=(const DataAnyGenerator &rhs) {
    DataAnyGenerator(rhs).swap(*this);
    return *this;
  }

  bool empty() const { return !content; }

  const std::type_info &type() const {
    return content ? content->type() : typeid(void);
  }

  template <typename ValueType>
  static inline ValueType *any_cast(DataAnyGenerator *operand) {
    static_assert(std::is_base_of<DataGenerator, ValueType>::value,
                  "ValueType must a subclass of DataGenarator");
    return &static_cast<DataAnyGenerator::holder<ValueType> *>(operand->content)
                ->held;
  }

  template <typename ValueType>
  static inline const ValueType *any_cast(const DataAnyGenerator *operand) {
    return any_cast<ValueType>(const_cast<DataAnyGenerator *>(operand));
  }

  template <typename ValueType>
  static inline ValueType any_cast(DataAnyGenerator &operand) {
    using nonref = std::remove_reference<ValueType>::type;
    nonref *result = any_cast<nonref>(&operand);
    if (!result)
      throw std::bad_cast();
    return *result;
  }

  template <typename ValueType>
  static inline ValueType any_cast(const DataAnyGenerator &operand) {
    using nonref = std::remove_reference<ValueType>::type;
    return any_cast<const nonref &>(const_cast<DataAnyGenerator &>(operand));
  }

private:
  class placeholder {
  public:
    virtual ~placeholder() {}

  public:
    virtual const std::type_info &type() const = 0;

    virtual placeholder *clone() const = 0;
  };

  template <typename ValueType> class holder : public placeholder {
  public:
    holder(const ValueType &value) : held(value) {}
    virtual const std::type_info &type() const { return typeid(ValueType); }
    virtual placeholder *clone() const { return new holder(held); }

    ValueType held;

  private:
    holder &operator=(const holder &);
  };

  placeholder *content;
};

/// base class for marshal/unmarshal modbus [data] section
/// modbus protocol:
/// server adderss| function code| data|crc
class DataGenerator {
public:
  virtual ~DataGenerator(){};
  static inline std::string dump(const std::vector<char> &byteArray) {
    /// to hex string
    /// FIXME
    return "";
  }
  /// marshal the data section,output is byteArray
  /// output byte array must be big-endin
  virtual std::vector<char> toByteArray() = 0;
  /// unmarshal data from bytearray
  /// the input bytearray is encoded with big-endin
  virtual std::error_code fromByteArray(const std::vector<char> &byteArray) = 0;
  /// dump the data, like dump(), but output is not hex string,
  /// it is a human readable string
  virtual std::string readableDump() = 0;
};
} // namespace modbus

#endif // __MODBUS_DATA_GENERATOR_H_
