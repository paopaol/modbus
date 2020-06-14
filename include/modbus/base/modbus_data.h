#ifndef __MODBUS_DATA_GENERATOR_H_
#define __MODBUS_DATA_GENERATOR_H_

#include "modbus_types.h"
#include <iostream>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace modbus {
/// only store subclass of AbstractData
class any {
public:
  any() : content(0) {}

  template <typename ValueType>
  any(const ValueType &value) : content(new holder<ValueType>(value)) {}

  any(const any &other) : content(other.content ? other.content->clone() : 0) {}

  ~any() { delete content; }

  any &swap(any &rhs) {
    std::swap(content, rhs.content);
    return *this;
  }

  any &operator=(const any &rhs) {
    any(rhs).swap(*this);
    return *this;
  }

  bool empty() const { return !content; }

  const std::type_info &type() const {
    return content ? content->type() : typeid(void);
  }

  template <typename ValueType>
  static inline ValueType *any_cast(any *operand) {
    return &static_cast<any::holder<ValueType> *>(operand->content)->held;
  }

  template <typename ValueType>
  static inline const ValueType *any_cast(const any *operand) {
    return any_cast<ValueType>(const_cast<any *>(operand));
  }

  template <typename ValueType> static inline ValueType any_cast(any &operand) {
    using nonref = typename std::remove_reference<ValueType>::type;
    nonref *result = any_cast<nonref>(&operand);
    if (!result)
      throw std::bad_cast();
    return *result;
  }

  template <typename ValueType>
  static inline ValueType any_cast(const any &operand) {
    using nonref = typename std::remove_reference<ValueType>::type;
    return any_cast<const nonref &>(const_cast<any &>(operand));
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

} // namespace modbus

#endif // __MODBUS_DATA_GENERATOR_H_
