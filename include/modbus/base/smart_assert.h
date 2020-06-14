#ifndef __SMART_ASSERT_H_
#define __SMART_ASSERT_H_
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

struct functor {
  template <typename type> functor &operator()(const type &val) {
    std::cout << val << std::endl;
    return (*this);
  }
};

struct smart_asserter {
  smart_asserter()
      : smart_asserter_a(*this), smart_asserter_b(*this), m_first_value(true) {}

  ~smart_asserter() { exit(1); }

  smart_asserter &smart_asserter_a;
  smart_asserter &smart_asserter_b;

  smart_asserter &print_error(const char *file, int line, const char *exp) {
    std::cout << "smart assert failed: " << exp << ", "
              << "file " << file << ", "
              << "line " << line << std::endl;
    return (*this);
  }

  template <typename type>
  smart_asserter &print_context(const char *name, type val) {
    if (m_first_value) {
      m_first_value = false;
      std::cout << "the context: " << std::endl;
    }
    std::cout << '\t' << name << ':' << val << std::endl;
    return (*this);
  }

  bool m_first_value;
};

#define smart_asserter_a(exp) smart_asserter_op(b, exp)
#define smart_asserter_b(exp) smart_asserter_op(a, exp)
#define smart_asserter_op(n, exp)                                              \
  smart_asserter_a.print_context(#exp, exp).smart_asserter_##n
#define smart_assert(exp)                                                      \
  if (exp)                                                                     \
    ;                                                                          \
  else                                                                         \
    smart_asserter().print_error(__FILE__, __LINE__, #exp).smart_asserter_a

#endif // __SMART_ASSERT_H_
