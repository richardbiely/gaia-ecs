#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

#include "../external/span.hpp"
#include "utils_mem.h"

namespace gaia {
  namespace utils {
    enum class DataLayout {
      SoA, // Structure Of Arrays
      AoS  // Array Of Structures
    };

    // Helper templates
    namespace {
#pragma region "Tuple to struct conversion"

      template <class S, unsigned... Is, class Tupple>
      S TupleToStruct(std::index_sequence<Is...>, Tupple&& tup) {
        using std::get;
        return {get<Is>(std::forward<Tupple>(tup))...};
      }

      template <class S, class Tupple> S TupleToStruct(Tupple&& tup) {
        using T = std::remove_reference_t<Tupple>;

        return TupleToStruct<S>(
            std::make_index_sequence<std::tuple_size<T>{}>{},
            std::forward<Tupple>(tup));
      }

#pragma endregion

#pragma region "Struct to tuple conversion"

      // Check is a type T is constructible such as T{Args...}
      struct AnyType {
        template <class T> constexpr operator T(); // non explicit
      };
      template <class T, class... TArgs>
      decltype(void(T{std::declval<TArgs>()...}), std::true_type{})
      CheckIsBracesConstructible(int);
      template <class, class...>
      std::false_type CheckIsBracesConstructible(...);
      template <class T, class... TArgs>
      using IsBracesConstructible =
          decltype(CheckIsBracesConstructible<T, TArgs...>(0));

      // Converts a struct to a tuple (struct necessary to support
      // initialization via Struct{x,y,...,z})
      template <class T> auto StructToTuple(T&& object) noexcept {
        using type = std::decay_t<T>;
          if constexpr (IsBracesConstructible<
                            type, AnyType, AnyType, AnyType, AnyType,
                            AnyType>{}) {
            auto&& [p1, p2, p3, p4, p5] = object;
            return std::make_tuple(p1, p2, p3, p4, p5);
          } else if constexpr (IsBracesConstructible<
                                   type, AnyType, AnyType, AnyType,
                                   AnyType>{}) {
            auto&& [p1, p2, p3, p4] = object;
            return std::make_tuple(p1, p2, p3, p4);
          } else if constexpr (IsBracesConstructible<
                                   type, AnyType, AnyType, AnyType>{}) {
            auto&& [p1, p2, p3] = object;
            return std::make_tuple(p1, p2, p3);
          } else if constexpr (IsBracesConstructible<
                                   type, AnyType, AnyType>{}) {
            auto&& [p1, p2] = object;
            return std::make_tuple(p1, p2);
          } else if constexpr (IsBracesConstructible<type, AnyType>{}) {
            auto&& [p1] = object;
            return std::make_tuple(p1);
        }
        // Don't support empty structs. They have no data
        // We also want compilation to fail for structs with many members so we
        // can handle them here That shouldn't be necessary, though for we plan
        // tu support only structs with little amount of arguments. else return
        // std::make_tuple();
      }

#pragma endregion

      // Calculates a total size of all types of tuple
      template <typename... Args>
      constexpr unsigned CalculateArgsSize(std::tuple<Args...> const&) {
        return (sizeof(Args) + ...);
      }

#pragma region "Byte offset of memember of a SoA-organized data"

      constexpr static unsigned SoADataAlignment = 16;

      template <unsigned N, typename Tuple>
      constexpr static unsigned
      CalculateSoAByteOffset(const uintptr_t address, const unsigned size) {
          if constexpr (N == 0) {
            // Handle alignment to SoADataAlignment bytes for SSE
            return (
                unsigned)(utils::align<SoADataAlignment>(address) - address);
          } else {
            // Handle alignment to SoADataAlignment bytes for SSE
            const auto offset =
                (unsigned)(utils::align<SoADataAlignment>(address) - address);
            return sizeof(typename std::tuple_element<N - 1, Tuple>::type) *
                       size +
                   offset + CalculateSoAByteOffset<N - 1, Tuple>(address, size);
          }
      }

#pragma endregion
    } // namespace

    template <DataLayout TDataLayout, typename TItem> struct DataViewPolicy;

    /*!
     * DataViewPolicy for accessing and storing data in the AoS way
     *	Good for random access and when acessing data together.
     *
     * struct Foo { int x; int y; int z; };
     * using fooViewPolicy = DataViewPolicy<DataLayout::AoS, Foo>;
     *
     * Memory is going be be organized as:
     *		xyz xyz xyz xyz
     */
    template <typename ValueType>
    struct DataViewPolicy<DataLayout::AoS, ValueType> {
      constexpr static ValueType Get(tcb::span<ValueType> s, unsigned idx) {
        return GetInternal((const ValueType*)s.data(), idx);
      }
      constexpr static void
      Set(tcb::span<ValueType> s, unsigned idx, ValueType&& val) {
        SetInternal((ValueType*)s.data(), idx, std::forward<ValueType>(val));
      }

    private:
      constexpr static ValueType
      GetInternal(const ValueType* data, const unsigned idx) {
        return data[idx];
      }
      constexpr static void
      SetInternal(ValueType* data, const unsigned idx, ValueType&& val) {
        data[idx] = std::forward<ValueType>(val);
      }
    };

    /*!
     * DataViewPolicy for accessing and storing data in the SoA way
     *	Good for SIMD processing.
     *
     * struct Foo { int x; int y; int z; };
     * using fooViewPolicy = DataViewPolicy<DataLayout::SoA, Foo>;
     *
     * Memory is going be be organized as:
     *		xxxx yyyy zzzz
     */
    template <typename ValueType>
    struct DataViewPolicy<DataLayout::SoA, ValueType> {
      constexpr static ValueType
      Get(tcb::span<ValueType> s, const unsigned idx) {
        auto t = StructToTuple(ValueType{});
        return GetInternal(
            t, s, idx,
            std::make_integer_sequence<
                unsigned, std::tuple_size<decltype(t)>::value>());
      }

      constexpr static void
      Set(tcb::span<ValueType> s, const unsigned idx, ValueType&& val) {
        auto t = StructToTuple(std::forward<ValueType>(val));
        SetInternal(
            t, s, idx,
            std::make_integer_sequence<
                unsigned, std::tuple_size<decltype(t)>::value>(),
            std::forward<ValueType>(val));
      }

    private:
      template <typename Tuple, unsigned... Ids>
      constexpr static ValueType GetInternal(
          Tuple& t, tcb::span<ValueType> s, const unsigned idx,
          std::integer_sequence<unsigned, Ids...>) {
        (GetInternal<Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
             t, (const char*)s.data(),
             idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
                 CalculateSoAByteOffset<Ids, Tuple>(
                     (uintptr_t)s.data(), s.size())),
         ...);
        return TupleToStruct<ValueType, Tuple>(std::forward<Tuple>(t));
      }

      template <typename Tuple, unsigned Ids, typename TMemberType>
      constexpr static void
      GetInternal(Tuple& t, const char* data, const unsigned idx) {
        std::get<Ids>(t) = *(TMemberType*)&data[idx];
      }

      template <typename Tuple, typename TValue, unsigned... Ids>
      constexpr static void SetInternal(
          Tuple& t, tcb::span<TValue> s, const unsigned idx,
          std::integer_sequence<unsigned, Ids...>, TValue&& val) {
        (SetInternal(
             (char*)s.data(),
             idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
                 CalculateSoAByteOffset<Ids, Tuple>(
                     (uintptr_t)s.data(), s.size()),
             std::get<Ids>(t)),
         ...);
      }

      template <typename TMemberValue>
      constexpr static void
      SetInternal(char* data, const unsigned idx, TMemberValue val) {
        // memcpy((void*)&data[idx], (const void*)&val, sizeof(val));
        *(TMemberValue*)&data[idx] = val;
      }
    };
  } // namespace utils
} // namespace gaia
