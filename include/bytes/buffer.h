#ifndef HHT_BYTES_BUFFER_H
#define HHT_BYTES_BUFFER_H
#include <algorithm>
#include <assert.h>
#include <cstddef>
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
  size_t Read(std::vector<uint8_t> &p);

  // Read One Byte
  uint8_t ReadByte();

  // Read N Bytes from buffer
  size_t ReadBytes(std::vector<uint8_t> &p, size_t n);

  size_t Read(uint8_t *buffer, size_t n);
  size_t Read(char *buffer, size_t n);
  size_t ZeroCopyRead(uint8_t **ptr, size_t n);
  size_t ZeroCopyRead(char **ptr, size_t n);

  // write data into buffer
  size_t Write(const uint8_t d);
  size_t Write(const uint8_t *d, size_t len);
  size_t Write(const char *d, size_t len);

  size_t Write(const std::string &s);

  size_t Write(const std::vector<uint8_t> &p);
  size_t Write(const std::vector<char> &p);

  void UnReadByte(/*error*/);

  void UnReadBytes(size_t n /*,error &e*/);

  // return unreaded data size
  size_t Len() const;

  size_t Cap() const;

  void Reset();

  bool PeekAt(std::vector<uint8_t> &p, size_t index, size_t size) const;
  bool ZeroCopyPeekAt(uint8_t **p, size_t index, size_t size) const;
  bool ZeroCopyPeekAt(char **p, size_t index, size_t size) const;

  void Optimization();

  void Resize(size_t len);

  // ReadFrom
  // WriteTo

private:
  void growSpace(size_t len);

  size_t leftSpace();

  void hasWritten(size_t len);

  void hasReaded(size_t len);

  uint8_t *beginWrite();

  const uint8_t *beginWrite() const;

  uint8_t *lastRead();

  const uint8_t *beginRead() const;

  uint8_t *begin();

  const uint8_t *begin() const;

  std::vector<uint8_t> b;
  size_t ridx;
  size_t widx;
};

} // namespace bytes
} // namespace pp

#endif

// void enable_wakeup(errors::error_code &error);
