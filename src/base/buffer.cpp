#include <bits/stdint-uintn.h>
#include <bytes/buffer.h>
#include <cstddef>

namespace pp {
namespace bytes {

Buffer::Buffer() : b(8192), ridx(0), widx(0), size_(b.size()) {}

// Read All
size_t Buffer::Read(std::vector<uint8_t> &p) { return ReadBytes(p, Len()); }

// Read One Byte
uint8_t Buffer::ReadByte() {
  uint8_t *ch = lastRead();
  hasReaded(1);
  return *ch;
}

// Read N Bytes from buffer
size_t Buffer::ReadBytes(std::vector<uint8_t> &p, size_t n) {
  assert(n >= 0 && "buffer::readbytes(), bad input paramer");

  p.clear();
  n = n > Len() ? Len() : n;
  std::copy(lastRead(), lastRead() + n, std::back_inserter(p));
  hasReaded(n);
  return n;
}

size_t Buffer::Read(uint8_t *buffer, size_t n) {
  assert(n >= 0 && "buffer::read(), bad input paramer");
  n = n > Len() ? Len() : n;
  std::copy(lastRead(), lastRead() + n, buffer);
  hasReaded(n);
  return n;
}

size_t Buffer::Read(char *buffer, size_t n) {
  return Read(reinterpret_cast<uint8_t *>(buffer), n);
}

size_t Buffer::ZeroCopyRead(uint8_t **ptr, size_t n) {
  assert(n >= 0 && "buffer::read(), bad input paramer");
  n = n > Len() ? Len() : n;
  *ptr = lastRead();
  hasReaded(n);
  return n;
}

size_t Buffer::ZeroCopyRead(char **ptr, size_t n) {
  return ZeroCopyRead(reinterpret_cast<uint8_t **>(ptr), n);
}

size_t Buffer::Write(const uint8_t d) { return Write(&d, 1); }

// write data into buffer
size_t Buffer::Write(const uint8_t *d, size_t len) {
  if (leftSpace() < len) {
    Optimization();
  }
  if (leftSpace() < len) {
    growSpace(static_cast<size_t>(size_ + len));
  }
  std::copy(d, d + len, beginWrite());
  hasWritten(len);
  return len;
}

size_t Buffer ::Write(const char *d, size_t len) {
  return Write(reinterpret_cast<const uint8_t *>(d), len);
}
size_t Buffer::Write(const std::string &s) {
  return Write(s.c_str(), static_cast<size_t>(s.size()));
}

size_t Buffer::Write(const std::vector<uint8_t> &p) {
  return Write(p.data(), static_cast<size_t>(p.size()));
}

size_t Buffer::Write(const std::vector<char> &p) {
  return Write(p.data(), static_cast<size_t>(p.size()));
}

void Buffer::UnReadByte(/*error*/) { UnReadBytes(1); }

void Buffer::UnReadBytes(size_t n /*,error &e*/) {
  assert(static_cast<size_t>(lastRead() - begin()) >= n &&
         "buffer::unreadbytes too much data size");
  ridx -= n;
}

// return unreaded data size
size_t Buffer::Len() const { return widx - ridx; }

size_t Buffer::Cap() const { return size_; }

void Buffer::Reset() {
  ridx = 0;
  widx = 0;
}

bool Buffer::PeekAt(std::vector<uint8_t> &p, size_t index, size_t size) const {
  if (index < 0 || index >= Len()) {
    return false;
  }
  if (size <= 0) {
    return false;
  }
  index = ridx + index;
  size_t len = widx - index;
  if (size > len) {
    return false;
  }

  p.clear();
  std::copy(b.data() + index, b.data() + index + size, std::back_inserter(p));
  return true;
}

bool Buffer::ZeroCopyPeekAt(uint8_t **p, size_t index, size_t size) const {
  if (index < 0 || index >= Len()) {
    return false;
  }
  if (size <= 0) {
    return false;
  }
  index = ridx + index;
  size_t len = widx - index;
  if (size > len) {
    return false;
  }

  *p = (uint8_t *)b.data() + index;
  return true;
}

bool Buffer::ZeroCopyPeekAt(char **p, size_t index, size_t size) const {
  return ZeroCopyPeekAt(reinterpret_cast<uint8_t **>(p), index, size);
}

void Buffer::Optimization() {
  if (ridx == 0) {
    return;
  }

  size_t len = Len();
  std::copy(begin() + ridx, begin() + widx, begin());
  ridx = 0;
  widx = ridx + len;
  assert(widx < size_);
}

void Buffer::Resize(size_t len) {
  if (leftSpace() < len) {
    Optimization();
  }
  if (leftSpace() < len) {
    growSpace(static_cast<size_t>(size_ + len));
  }
  hasWritten(len);
}

// ReadFrom
// WriteTo

void Buffer::growSpace(size_t len) {
  b.resize(widx + len);
  size_ = b.size();
}

size_t Buffer::leftSpace() { return size_ - widx; }

void Buffer::hasWritten(size_t len) {
  widx += len;
  assert(widx <= size_);
}
void Buffer::hasReaded(size_t len) { ridx += len; }

uint8_t *Buffer::beginWrite() { return begin() + widx; }

const uint8_t *Buffer::beginWrite() const { return begin() + widx; }

uint8_t *Buffer::lastRead() { return begin() + ridx; }

const uint8_t *Buffer::beginRead() const { return begin() + ridx; }

uint8_t *Buffer::begin() { return &b[0]; }

const uint8_t *Buffer::begin() const { return &b[0]; }

// BufferRef NewBuffer();
//  {
// BufferRef b = std::make_shared<Buffer>();
// return b;
//  }
} // namespace bytes
} // namespace pp
