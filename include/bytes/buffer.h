#ifndef HHT_BYTES_BUFFER_H
#define HHT_BYTES_BUFFER_H
#include <algorithm>
#include <assert.h>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace pp {
namespace bytes {
class Buffer;
typedef std::shared_ptr<Buffer> BufferRef;

class Buffer {
public:
  Buffer();
  ~Buffer() {}

  // Read All
  size_t Read(std::vector<char> &p);

  // Read One Byte
  char ReadByte();

  // Read N Bytes from buffer
  size_t ReadBytes(std::vector<char> &p, size_t n);

  size_t Read(char *buffer, size_t n);
  size_t ZeroCopyRead(char *&ptr, size_t n);

  // write data into buffer
  size_t Write(const char *d, size_t len);

  size_t Write(const std::string &s);

  size_t Write(const std::vector<char> &p);

  void UnReadByte(/*error*/);

  void UnReadBytes(size_t n /*,error &e*/);

  // return unreaded data size
  size_t Len();

  size_t Cap();

  void Reset();

  bool PeekAt(std::vector<char> &p, size_t index, size_t size);

  void Optimization();

  // ReadFrom
  // WriteTo

private:
  void growSpace(size_t len);

  size_t leftSpace();

  void hasWritten(size_t len);

  void hasReaded(size_t len);

  char *beginWrite();

  const char *beginWrite() const;

  char *lastRead();

  const char *beginRead() const;

  char *begin();

  const char *begin() const;

  std::vector<char> b;
  size_t ridx;
  size_t widx;
};

} // namespace bytes
} // namespace pp

#endif

// void enable_wakeup(errors::error_code &error);
