#ifndef __MODBUS_DATA_H_
#define __MODBUS_DATA_H_

#include <modbus_data_generator.h>

namespace modbus {
class Data {
public:
  template <typename T> std::string dump() {
    auto *base = modbus::DataAnyGenerator::any_cast<T>(&dataGenerator_);
    return DataGenerator::dump(base->toByteArray());
  }

  template <typename T> T dataGenerator() {
    static_assert(std::is_base_of<DataGenerator, T>::value &&
                      !std::is_same<DataGenerator, T>::value,
                  "dataGenerator must be a subclass of modbus::DataGenerator");
    return modbus::DataAnyGenerator::any_cast<T>(dataGenerator_);
  }

  template <typename T>
  static inline Data fromDataGenerator(const T &dataGenerator) {
    static_assert(std::is_base_of<DataGenerator, T>::value &&
                      !std::is_same<DataGenerator, T>::value,
                  "dataGenerator must be a subclass of modbus::DataGenerator");

    Data data;
    data.setDataGenerator(dataGenerator);

    return data;
  }

  void setDataGenerator(const modbus::DataAnyGenerator &dataGenerator) {
    dataGenerator_ = dataGenerator;
  }

private:
  modbus::DataAnyGenerator dataGenerator_;
};
} // namespace modbus

#endif // __MODBUS_DATA_H_
