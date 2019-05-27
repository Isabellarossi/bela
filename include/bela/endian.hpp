// C++17 constexpr endian.hpp
#ifndef QUARANTINE_ENDIAN_HPP
#define QUARANTINE_ENDIAN_HPP
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(__linux__) || defined(__GNU__) || defined(__HAIKU__)
#include <endian.h>
#elif defined(_AIX)
#include <sys/machine.h>
#elif defined(__sun)
/* Solaris provides _BIG_ENDIAN/_LITTLE_ENDIAN selector in sys/types.h */
#include <sys/types.h>
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#if defined(_BIG_ENDIAN)
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#else
#if !defined(BYTE_ORDER) && !defined(_WIN32)
#include <machine/endian.h>
#endif
#endif

namespace bela {
#if defined(BYTE_ORDER) && defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDIAN
#define IS_BIG_ENDIAN 1
constexpr bool IsBigEndianHost = true;
constexpr bool IsLittleEndianHost = false;
#else
#define IS_BIG_ENDIAN 0
constexpr bool IsBigEndianHost = false;
constexpr bool IsLittleEndianHost = true;
#endif

constexpr inline bool IsBigEndian() { return IsBigEndianHost; }
constexpr inline bool IsLittleEndian() { return IsLittleEndianHost; }

constexpr inline uint16_t swap16(uint16_t value) {
#if defined(_MSC_VER) && !defined(_DEBUG)
  // The DLL version of the runtime lacks these functions (bug!?), but in a
  // release build they're replaced with BSWAP instructions anyway.
  return _byteswap_ushort(value);
#else
  uint16_t Hi = value << 8;
  uint16_t Lo = value >> 8;
  return Hi | Lo;
#endif
}
// We use C++17. so GCC version must > 8.0. __builtin_bswap32 awayls exists
constexpr inline uint32_t swap32(uint32_t value) {
#if defined(__llvm__) || (defined(__GNUC__) && !defined(__ICC))
  return __builtin_bswap32(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
  return _byteswap_ulong(value);
#else
  uint32_t Byte0 = value & 0x000000FF;
  uint32_t Byte1 = value & 0x0000FF00;
  uint32_t Byte2 = value & 0x00FF0000;
  uint32_t Byte3 = value & 0xFF000000;
  return (Byte0 << 24) | (Byte1 << 8) | (Byte2 >> 8) | (Byte3 >> 24);
#endif
}

constexpr inline uint64_t swap64(uint64_t value) {
#if defined(__llvm__) || (defined(__GNUC__) && !defined(__ICC))
  return __builtin_bswap64(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
  return _byteswap_uint64(value);
#else
  uint64_t Hi = bswap32(uint32_t(value));
  uint32_t Lo = bswap32(uint32_t(value >> 32));
  return (Hi << 32) | Lo;
#endif
}

constexpr inline uint8_t bswap(uint8_t v) { return v; }
constexpr inline uint16_t bswap(uint16_t v) { return swap16(v); }
constexpr inline unsigned int bswap(unsigned int v) {
  return static_cast<unsigned int>(swap32(static_cast<uint32_t>(v)));
}
constexpr inline signed int bswap(signed int v) {
  return static_cast<signed int>(swap32(static_cast<uint32_t>(v)));
}

constexpr unsigned long bswap(unsigned long v) {
  if constexpr (sizeof(unsigned long) == 8) {
    return static_cast<unsigned long>(swap64(static_cast<uint64_t>(v)));
  } else if constexpr (sizeof(unsigned long) != 4) {
    // BAD long size
    return v;
  }
  return static_cast<unsigned long>(swap32(static_cast<uint32_t>(v)));
}
constexpr signed long bswap(signed long v) {
  if constexpr (sizeof(signed long) == 8) {
    return static_cast<signed long>(swap64(static_cast<uint64_t>(v)));
  } else if constexpr (sizeof(signed long) != 4) {
    // BAD long size
    return v;
  }
  return static_cast<signed long>(swap32(static_cast<uint32_t>(v)));
}

constexpr unsigned long long bswap(unsigned long long v) {
  return static_cast<unsigned long long>(swap64(static_cast<uint64_t>(v)));
}
constexpr signed long long bswap(signed long long v) {
  return static_cast<signed long long>(swap64(static_cast<uint64_t>(v)));
}

// SO Network order is BigEndian
template <typename T> inline T htons(T v) {
  static_assert(std::is_integral_v<T>, "must integer");
  if constexpr (IsBigEndianHost) {
    return v;
  }
  return bswap(v);
}
template <typename T> inline T ntohs(T v) {
  static_assert(std::is_integral_v<T>, "must integer");
  if constexpr (IsBigEndianHost) {
    return v;
  }
  return bswap(v);
}

template <typename T> constexpr T swaple(T i) {
  static_assert(std::is_integral<T>::value, "Integral required.");
  if constexpr (IsBigEndianHost) {
    return bswap(i);
  }
  return i;
}

template <typename T> constexpr T swapbe(T i) {
  static_assert(std::is_integral<T>::value, "Integral required.");
  if constexpr (IsBigEndianHost) {
    return i;
  }
  return bswap(i);
}

template <typename T> inline T unalignedloadT(const void *p) {
  static_assert(std::is_integral_v<T>, "must integer");
  T t;
  memcpy(&t, p, sizeof(T));
  return t;
}

template <typename T> inline T readle(const void *p) {
  auto v = unalignedloadT<T>(p);
  if constexpr (IsLittleEndianHost) {
    return v;
  }
  return bswap(v);
}

template <typename T> inline T readbe(const void *p) {
  auto v = unalignedloadT<T>(p);
  if constexpr (IsBigEndianHost) {
    return v;
  }
  return bswap(v);
}

} // namespace bela

#endif
