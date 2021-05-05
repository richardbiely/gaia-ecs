#pragma once
#include <ctype.h>
#include <functional>

namespace gaia {
namespace utils {
template <class __key> struct StdHashingPolicy {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const __key &val) {
    return std::hash<__key>()(val);
  }
};

template <class __key> struct PrehashedKeyPolicy {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const __key &val) { return (unsigned int)val; }
};

template <class __key> struct HashMethodPolicy {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const __key &val) { return val.GetHash(); }
};

template <class __key> struct StringHashingPolicyCS {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const __key &val) { return val.GetHashValue(); }
};
template <> struct StringHashingPolicyCS<const char *> {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const char *str) {
    int c;
    unsigned int hashval = 0;

    while ((c = *str++) != 0)
      hashval = hashval * 37 + c;

    return hashval;
  }
};

template <class __key> struct StringHashingPolicyCI {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const __key &val) {
    return val.GetHashValueCI();
  }
};
template <> struct StringHashingPolicyCI<const char *> {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const char *str) {
    int c;
    unsigned int hashval = 0;

    while ((c = *str++) != 0)
      hashval = hashval * 37 + tolower(c);

    return hashval;
  }
};

template <class K> struct SequentialKeyHash {
  typedef unsigned int HashValueType;

  static HashValueType GetHash(const K &val) {
    // Relies on implementation of GenericHashSet that uses all but 7 least
    // significant bits for initial probe position. 128 sequential keys would
    // thus fall into same bucket. Avoid this by rotating bits.
    HashValueType bottom = uintptr_t(val) << 7;
    HashValueType top = (uintptr_t(val) >> (sizeof(K) * 8 - 7)) & 0x7F;
    return top | bottom;
  }
};

//! Hash bytes of the KeyT type
template <class KeyT> struct MemoryHashingPolicy {
  typedef unsigned int HashValueType;

  static unsigned int GetHash(const KeyT &desc) {
    constexpr unsigned int count = sizeof(KeyT);
    auto bytes = reinterpret_cast<const char *>(&desc);
    unsigned int val = 1806131521;
    for (unsigned int i = 0; i < count; ++i) {
      val ^= static_cast<unsigned int>(bytes[i]);
      val *= 1674261049;
    }

    return val;
  }
};

#pragma region "Fowler-Noll-Vo hash"
// Fowler-Noll-Vo hash is a fast public-domain hash function good for checksums

constexpr unsigned int val_32_const = 0x811c9dc5;
constexpr unsigned int prime_32_const = 0x1000193;

#if 1
constexpr unsigned int
hash_fnv1a_32(const char *const str,
              const unsigned int value = val_32_const) noexcept {
  return (str[0] == '\0')
             ? value
             : hash_fnv1a_32(&str[1],
                             (value ^ (unsigned int)(str[0])) * prime_32_const);
}
#else
inline const unsigned int hash_fnv1a_32(const char *str) {
  unsigned int hash = val_32_const;
  unsigned int prime = prime_32_const;

  unsigned int i = 0;
  while (str[i] != '\0') {
    unsigned int8 value = str[i++];
    hash = hash ^ value;
    hash *= prime;
  }

  return hash;
}
#endif

constexpr unsigned long long val_64_const = 0xcbf29ce484222325;
constexpr unsigned long long prime_64_const = 0x100000001b3;

#if 1
constexpr unsigned long long
hash_fnv1a_64(const char *const str,
              const unsigned long long value = val_64_const) noexcept {
  return (str[0] == '\0')
             ? value
             : hash_fnv1a_64(&str[1], (value ^ (unsigned long long)(str[0])) *
                                          prime_64_const);
}
#else
inline const unsigned long long hash_fnv1a(const char *str) {
  unsigned long long hash = val_64_const;
  unsigned long long prime = prime_64_const;

  unsigned int i = 0;
  while (str[i] != '\0') {
    unsigned int8 value = str[i++];
    hash = hash ^ value;
    hash *= prime;
  }

  return hash;
}
#endif

#pragma region "Type meta data"

struct TypeMetaData {
  template <typename T>
  [[nodiscard]] static constexpr const char *GetMetaName() noexcept {
    return GAIA_PRETTY_FUNCTION;
  }

  template <typename T>
  [[nodiscard]] static constexpr unsigned long long CalculateNameHash() {
    return hash_fnv1a_64(GetMetaName<std::decay_t<T>>());
  }
};

#pragma endregion

template <class __key> struct Fnv1a32HashingPolicy {
  using HashValueType = unsigned int;

  static HashValueType GetHash(const __key &val) { return val.GetHashValue(); }
};
template <> struct Fnv1a32HashingPolicy<const char *> {
  using HashValueType = unsigned int;

  static HashValueType GetHash(const char *str) { return hash_fnv1a_32(str); }
};

template <class __key> struct Fnv1a64HashingPolicy {
  using HashValueType = unsigned long long;

  static HashValueType GetHash(const __key &val) { return val.GetHashValue(); }
};
template <> struct Fnv1a64HashingPolicy<const char *> {
  using HashValueType = unsigned long long;

  static HashValueType GetHash(const char *str) { return hash_fnv1a_64(str); }
};

#pragma endregion
} // namespace utils
} // namespace gaia
