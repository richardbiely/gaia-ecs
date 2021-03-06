

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

#define GAIA_SAFE_CONSTEXPR constexpr

//------------------------------------------------------------------------------
// Features
//------------------------------------------------------------------------------

#define GAIA_ECS_HASH_FNV1A 0
#define GAIA_ECS_HASH_MURMUR2A 1

//------------------------------------------------------------------------------
// Compiler
//------------------------------------------------------------------------------
#define GAIA_COMPILER_CLANG 0
#define GAIA_COMPILER_GCC 0
#define GAIA_COMPILER_MSVC 0
#define GAIA_COMPILER_ICC 0
#define GAIA_COMPILED_DETECTED 0

#if !GAIA_COMPILED_DETECTED && (defined(__clang__))
// Clang check is performed first as it might pretend to be MSVC or GCC by
// defining their predefined macros.
	#undef GAIA_COMPILER_CLANG
	#define GAIA_COMPILER_CLANG 1
	#undef GAIA_COMPILED_DETECTED
	#define GAIA_COMPILED_DETECTED 1
#endif
#if !GAIA_COMPILED_DETECTED &&                                                                                         \
		(defined(__INTEL_COMPILER) || defined(__ICC) || defined(__ICL) || defined(__INTEL_LLVM_COMPILER))
	#undef GAIA_COMPILER_ICC
	#define GAIA_COMPILER_ICC 1
	#undef GAIA_COMPILED_DETECTED
	#define GAIA_COMPILED_DETECTED 1
#endif
#if !GAIA_COMPILED_DETECTED && (defined(__SNC__) || defined(__GNUC__))
	#undef GAIA_COMPILER_GCC
	#define GAIA_COMPILER_GCC 1
	#undef GAIA_COMPILED_DETECTED
	#define GAIA_COMPILED_DETECTED 1
	#if __GNUC__ <= 7
		// In some contexts, e.g. when evaluating PRETTY_FUNCTION, GCC has a bug
		// where the string is not defined a constexpr and thus can't be evaluated
		// in constexpr expressions.
		#undef GAIA_SAFE_CONSTEXPR
		#define GAIA_SAFE_CONSTEXPR const
	#endif
#endif
#if !GAIA_COMPILED_DETECTED && (defined(_MSC_VER))
	#undef GAIA_COMPILER_MSVC
	#define GAIA_COMPILER_MSVC 1
	#undef GAIA_COMPILED_DETECTED
	#define GAIA_COMPILED_DETECTED 1
#endif
#if !GAIA_COMPILED_DETECTED
	#error "Unrecognized compiler"
#endif

//------------------------------------------------------------------------------
// Architecture features
//------------------------------------------------------------------------------
#define GAIA_64 0
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64) || defined(__amd64) ||                \
		defined(__aarch64__)
	#undef GAIA_64
	#define GAIA_64 1
#endif

#define GAIA_ARCH_X86 0
#define GAIA_ARCH_ARM 1
#define GAIA_ARCH GAIA_ARCH_X86

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) || defined(__i386__) || defined(__i386) ||                \
		defined(i386) || defined(__x86_64__) || defined(_X86_)
	#undef GAIA_ARCH
	#define GAIA_ARCH GAIA_ARCH_X86
#elif defined(__arm__) || defined(__aarch64__)
	#undef GAIA_ARCH
	#define GAIA_ARCH GAIA_ARCH_ARM
#else
	#error "Unrecognized target architecture."
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_PRETTY_FUNCTION __FUNCSIG__
	#define GAIA_PRETTY_FUNCTION_PREFIX '<'
	#define GAIA_PRETTY_FUNCTION_SUFFIX '>'
#else
	#define GAIA_PRETTY_FUNCTION __PRETTY_FUNCTION__
	#define GAIA_PRETTY_FUNCTION_PREFIX '='
	#define GAIA_PRETTY_FUNCTION_SUFFIX ']'
#endif

//------------------------------------------------------------------------------

#if (GAIA_COMPILER_MSVC && _MSC_VER >= 1400) || GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG
	#define GAIA_RESTRICT __restrict
#else
	#define GAIA_RESTRICT
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC || GAIA_COMPILER_ICC
	#define GAIA_FORCEINLINE __forceinline
#else
	#define GAIA_FORCEINLINE __attribute__((always_inline))
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_IMPORT __declspec(dllimport)
	#define GAIA_EXPORT __declspec(dllexport)
	#define GAIA_HIDDEN
#elif GAIA_COMPILER_CLANG || GAIA_COMPILER_GCC
	#define GAIA_IMPORT _attribute__((visibility("default")))
	#define GAIA_EXPORT _attribute__((visibility("default")))
	#define GAIA_HIDDEN _attribute__((visibility("hidden")))
#endif

#if defined(GAIA_DLL)
	#define GAIA_API GAIA_IMPORT
#elif defined(GAIA_DLL_EXPORT)
	#define GAIA_API GAIA_EXPORT
#else
	#define GAIA_API
#endif

//------------------------------------------------------------------------------
// Warning-related macros and settings
// We always set warnings as errors and disable ones we don't care about.
// Sometimes in only limited range of code or around 3rd party includes.
//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_MSVC_WARNING_PUSH() __pragma(warning(push))
	#define GAIA_MSVC_WARNING_POP() __pragma(warning(pop))
	#define GAIA_MSVC_WARNING_DISABLE(warningId) __pragma(warning(disable : warningId))
	#define GAIA_MSVC_WARNING_ERROR(warningId) __pragma(warning(error : warningId))
#else
	#define GAIA_MSVC_WARNING_PUSH()
	#define GAIA_MSVC_WARNING_POP()
	#define GAIA_MSVC_WARNING_DISABLE(warningId)
	#define GAIA_MSVC_WARNING_ERROR(warningId)
#endif

#if GAIA_COMPILER_CLANG
	#define DO_PRAGMA_(x) _Pragma(#x)
	#define DO_PRAGMA(x) DO_PRAGMA_(x)
	#define GAIA_CLANG_WARNING_PUSH() _Pragma("clang diagnostic push")
	#define GAIA_CLANG_WARNING_POP() _Pragma("clang diagnostic pop")
	#define GAIA_CLANG_WARNING_DISABLE(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic ignored #warningId))
	#define GAIA_CLANG_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic error #warningId))
	#define GAIA_CLANG_WARNING_ALLOW(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic warning #warningId))
#else
	#define GAIA_CLANG_WARNING_PUSH()
	#define GAIA_CLANG_WARNING_POP()
	#define GAIA_CLANG_WARNING_DISABLE(warningId)
	#define GAIA_CLANG_WARNING_ERROR(warningId)
	#define GAIA_CLANG_WARNING_ALLOW(warningId)
#endif

#if GAIA_COMPILER_GCC
	#define DO_PRAGMA_(x) _Pragma(#x)
	#define DO_PRAGMA(x) DO_PRAGMA_(x)
	#define GAIA_GCC_WARNING_PUSH() _Pragma("GCC diagnostic push")
	#define GAIA_GCC_WARNING_POP() _Pragma("GCC diagnostic pop")
	#define GAIA_GCC_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE_MACRO(GCC diagnostic error warningId))
	#define GAIA_GCC_WARNING_DISABLE(warningId) DO_PRAGMA(GCC diagnostic ignored #warningId)
#else
	#define GAIA_GCC_WARNING_PUSH()
	#define GAIA_GCC_WARNING_POP()
	#define GAIA_GCC_WARNING_DISABLE(warningId)
#endif

#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__pnacl__) || defined(__EMSCRIPTEN__)
	#define GAIA_HAS_NO_INLINE_ASSEMBLY 1
#endif

// Breaking changes and big features
#define GAIA_VERSION_MAJOR 0
// Smaller changes and features
#define GAIA_VERSION_MINOR 4
// Fixes and tweaks
#define GAIA_VERSION_PATCH 0

//------------------------------------------------------------------------------
// Entity settings
//------------------------------------------------------------------------------

// Check entity.h IdBits and GenBits for more info
#define GAIA_ENTITY_IDBITS 20
#define GAIA_ENTITY_GENBITS 12

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

//! If enabled, GAIA_DEBUG is defined despite using a "Release" build configuration for example
#define GAIA_FORCE_DEBUG 0
//! If enabled, no asserts are thrown even in debug builds
#define GAIA_DISABLE_ASSERTS 0

//! If enabled, diagnostics are enabled
#define GAIA_ECS_DIAGS 1
//! If enabled, custom allocator is used for allocating archetype chunks.
#define GAIA_ECS_CHUNK_ALLOCATOR 1

#define GAIA_ECS_HASH GAIA_ECS_HASH_MURMUR2A

//! If enabled, STL containers are going to be used by the framework.
#define GAIA_USE_STL_CONTAINERS 0
//! If enabled, gaia containers stay compatible with STL by sticking to STL iterators.
#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 0

//------------------------------------------------------------------------------
// TODO features
//------------------------------------------------------------------------------

//! If enabled, profiler traces are inserted into generated code
#define GAIA_PROFILER 0
//! If enabled, archetype graph is used to speed up component adding and removal.
//! NOTE: Not ready
#define GAIA_ARCHETYPE_GRAPH 0

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE (GAIA_DEBUG || GAIA_FORCE_DEBUG)
#define GAIA_ECS_VALIDATE_CHUNKS (GAIA_DEBUG || GAIA_FORCE_DEBUG)
#define GAIA_ECS_VALIDATE_ENTITY_LIST (GAIA_DEBUG || GAIA_FORCE_DEBUG)

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

#if GAIA_USE_STL_CONTAINERS && !GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#undef GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 1
#endif
#if GAIA_USE_STL_CONTAINERS || GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_UTIL std
#else
	#define GAIA_UTIL gaia::utils
#endif

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

//! If enabled, additional debug and verification code is used which
//! slows things down but enables better security and diagnostics.
//! Suitable for debug builds first and foremost. Therefore, it is
//! enabled by default for debud builds.
#if !defined(GAIA_DEBUG)
	#if !defined(NDEBUG) || defined(_DEBUG) || GAIA_FORCE_DEBUG
		#define GAIA_DEBUG 1
	#else
		#define GAIA_DEBUG 0
	#endif
#endif

#if defined(GAIA_DISABLE_ASSERTS)
	#undef GAIA_ASSERT
	#define GAIA_ASSERT(condition) (void(0))
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	#if GAIA_DEBUG
		#define GAIA_ASSERT(condition)                                                                                     \
			{                                                                                                                \
				bool cond_ret = condition;                                                                                     \
				assert(cond_ret);                                                                                              \
				DoNotOptimize(cond_ret)                                                                                        \
			}
	#else
		#define GAIA_ASSERT(condition) assert(condition);
	#endif
#endif

#if defined(GAIA_ECS_DIAGS)
	#undef GAIA_ECS_DIAG_ARCHETYPES
	#undef GAIA_ECS_DIAG_REGISTERED_TYPES
	#undef GAIA_ECS_DIAG_DELETED_ENTITIES

	#define GAIA_ECS_DIAG_ARCHETYPES 1
	#define GAIA_ECS_DIAG_REGISTERED_TYPES 1
	#define GAIA_ECS_DIAG_DELETED_ENTITIES 1
#else
	#if !defined(GAIA_ECS_DIAG_ARCHETYPES)
		#define GAIA_ECS_DIAG_ARCHETYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_REGISTERED_TYPES)
		#define GAIA_ECS_DIAG_REGISTERED_TYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_DELETED_ENTITIES)
		#define GAIA_ECS_DIAG_DELETED_ENTITIES 0
	#endif
#endif

#if !defined(GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE)
	#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE GAIA_DEBUG
#endif
#if !defined(GAIA_ECS_VALIDATE_CHUNKS)
	#define GAIA_ECS_VALIDATE_CHUNKS GAIA_DEBUG
#endif
#if !defined(GAIA_ECS_VALIDATE_ENTITY_LIST)
	#define GAIA_ECS_VALIDATE_ENTITY_LIST GAIA_DEBUG
#endif

//------------------------------------------------------------------------------
// Profiling features
//------------------------------------------------------------------------------

#if defined(GAIA_PROFILER)
// ...
#else
// ...
#endif

#include <cstdio> // vsnprintf, sscanf, printf

//! Log - debug
#define LOG_D(...)                                                                                                     \
	{                                                                                                                    \
		fprintf(stdout, "D: ");                                                                                            \
		fprintf(stdout, __VA_ARGS__);                                                                                      \
		fprintf(stdout, "\n");                                                                                             \
	}
//! Log - normal/informational
#define LOG_N(...)                                                                                                     \
	{                                                                                                                    \
		fprintf(stdout, "I: ");                                                                                            \
		fprintf(stdout, __VA_ARGS__);                                                                                      \
		fprintf(stdout, "\n");                                                                                             \
	}
//! Log - warning
#define LOG_W(...)                                                                                                     \
	{                                                                                                                    \
		fprintf(stderr, "W: ");                                                                                            \
		fprintf(stderr, __VA_ARGS__);                                                                                      \
		fprintf(stderr, "\n");                                                                                             \
	}
//! Log - error
#define LOG_E(...)                                                                                                     \
	{                                                                                                                    \
		fprintf(stderr, "E: ");                                                                                            \
		fprintf(stderr, __VA_ARGS__);                                                                                      \
		fprintf(stderr, "\n");                                                                                             \
	}

#include <cinttypes>

#define USE_VECTOR GAIA_USE_STL_CONTAINERS

#if USE_VECTOR == 1

	#include <vector>

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = std::vector<T>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0

	

#include <cstddef>
#include <initializer_list>
#if !GAIA_DISABLE_ASSERTS
	#include <memory>
#endif
#include <utility>

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <iterator>
#else
	#include <cstddef>
	#include <type_traits>

namespace gaia {
	namespace utils {
		struct input_iterator_tag {};
		struct output_iterator_tag {};

		struct forward_iterator_tag: input_iterator_tag {};
		struct reverse_iterator_tag: input_iterator_tag {};
		struct bidirectional_iterator_tag: forward_iterator_tag {};
		struct random_access_iterator_tag: bidirectional_iterator_tag {};

		namespace detail {

			template <typename, typename = void>
			struct iterator_traits_base {}; // empty for non-iterators

			template <typename It>
			struct iterator_traits_base<
					It, std::void_t<
									typename It::iterator_category, typename It::value_type, typename It::difference_type,
									typename It::pointer, typename It::reference>> {
				using iterator_category = typename It::iterator_category;
				using value_type = typename It::value_type;
				using difference_type = typename It::difference_type;
				using pointer = typename It::pointer;
				using reference = typename It::reference;
			};

			template <typename T, bool = std::is_object_v<T>>
			struct iterator_traits_pointer_base {
				using iterator_category = random_access_iterator_tag;
				using value_type = std::remove_cv_t<T>;
				using difference_type = ptrdiff_t;
				using pointer = T*;
				using reference = T&;
			};

			//! Iterator traits for pointers to non-object
			template <typename T>
			struct iterator_traits_pointer_base<T, false> {};

			//! Iterator traits for iterators
			template <typename It>
			struct iterator_traits: iterator_traits_base<It> {};

			// Iterator traits for pointers
			template <typename T>
			struct iterator_traits<T*>: iterator_traits_pointer_base<T> {};

			template <typename It>
			using iterator_cat_t = typename iterator_traits<It>::iterator_category;

			template <typename T, typename = void>
			constexpr bool is_iterator_v = false;

			template <typename T>
			constexpr bool is_iterator_v<T, std::void_t<iterator_cat_t<T>>> = true;

			template <typename T>
			struct is_iterator: std::bool_constant<is_iterator_v<T>> {};

			template <typename It>
			constexpr bool is_input_iter_v = std::is_convertible_v<iterator_cat_t<It>, input_iterator_tag>;

			template <typename It>
			constexpr bool is_fwd_iter_v = std::is_convertible_v<iterator_cat_t<It>, forward_iterator_tag>;

			template <typename It>
			constexpr bool is_rev_iter_v = std::is_convertible_v<iterator_cat_t<It>, reverse_iterator_tag>;

			template <typename It>
			constexpr bool is_bidi_iter_v = std::is_convertible_v<iterator_cat_t<It>, bidirectional_iterator_tag>;

			template <typename It>
			constexpr bool is_random_iter_v = std::is_convertible_v<iterator_cat_t<It>, random_access_iterator_tag>;
		} // namespace detail

		template <typename It>
		using iterator_ref_t = typename detail::iterator_traits<It>::reference;

		template <typename It>
		using iterator_value_t = typename detail::iterator_traits<It>::value_type;

		template <typename It>
		using iterator_diff_t = typename detail::iterator_traits<It>::difference_type;

		template <typename... It>
		using common_diff_t = std::common_type_t<iterator_diff_t<It>...>;

		template <typename It>
		constexpr iterator_diff_t<It> distance(It first, It last) {
			if constexpr (detail::is_random_iter_v<It>)
				return last - first;
			else {
				iterator_diff_t<It> offset{};
				while (first != last) {
					++first;
					++offset;
				}
				return offset;
			}
		}
	} // namespace utils
} // namespace gaia
#endif

namespace gaia {
	namespace containers {
		// Array with variable size allocated on heap.
		// Interface compatiblity with std::vector where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <typename T>
		class darr {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = size_t;

		private:
			T* m_data = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

		public:
			class iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

			private:
				T* m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

				constexpr iterator(const iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
					return m_ptr;
				}
				constexpr iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr iterator& operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator& operator--(int) {
					iterator temp(*this);
					--this;
					return temp;
				}

				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class const_iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

			private:
				const T* m_ptr;

			public:
				constexpr const_iterator(T* ptr): m_ptr(ptr) {}

				constexpr const_iterator(const const_iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const const_iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr const T& operator*() const {
					return *(const T*)m_ptr;
				}
				constexpr const T* operator->() const {
					return (const T*)m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator& operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator& operator--(int) {
					const_iterator temp(*this);
					--this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			darr() noexcept {
				clear();
			}

			darr(size_type count, const T& value) noexcept {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);
				size_type i = 0;
				for (auto it = first; it != last; ++it)
					m_data[i++] = *it;
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept {
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_data = other.m_data;

				other.m_cnt = 0;
				other.m_cap = 0;
				other.m_data = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				auto it = il.begin();
				for (; it != il.end(); ++it)
					push_back(std::move(*it));

				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(std::addressof(other) != this);

				resize(other.size());
				for (size_type i = 0; i < other.size(); ++i)
					m_data[i] = other[i];
				return *this;
			}

			darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(std::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_data = other.m_data;

				other.m_cnt = 0;
				other.m_cap = 0;
				other.m_data = nullptr;
				return *this;
			}

			~darr() {
				if (m_data != nullptr)
					delete[] m_data;
				m_data = nullptr;
			}

			constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_data[pos];
			}

			const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_data[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;
				m_cap = count;
				if (m_data) {
					T* old = m_data;
					m_data = new T[m_cap];
					for (size_type i = 0; i < size(); ++i)
						m_data[i] = old[i];
					delete[] old;
				} else {
					m_data = new T[m_cap];
				}
			}

			void resize(size_type count) {
				if (count <= m_cap)
					m_cnt = count;
				else {
					m_cap = count;
					if (m_data) {
						T* old = m_data;
						m_data = new T[m_cap];
						for (size_type i = 0; i < size(); ++i)
							m_data[i] = old[i];
						delete[] old;
					} else {
						m_data = new T[m_cap];
					}
					m_cnt = count;
				}
			}

		private:
			void push_back_prepare() noexcept {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if (cnt != cap)
					return;

				// If no data is allocated go with at least 4 elements
				if (m_data == nullptr) {
					m_data = new T[m_cap = 4];
				}
				// Increase the size of an existing array in multiples of 1.5
				else {
					T* old = m_data;
					m_data = new T[m_cap = (cap * 3) / 2 + 1];

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(6385)
					for (size_type i = 0; i < cnt; ++i)
						m_data[i] = old[i];
					GAIA_MSVC_WARNING_POP()
					delete[] old;
				}
			}

		public:
			void push_back(const T& arg) noexcept {
				push_back_prepare();
				m_data[m_cnt++] = arg;
			}

			void push_back(T&& arg) noexcept {
				push_back_prepare();
				m_data[m_cnt++] = std::forward<T>(arg);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_cnt;
			}

			iterator erase(iterator pos) {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[m_cap - 1]);

				const auto idxStart = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto items = size() - 1;
				for (size_type i = idxStart; i < items; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return iterator((T*)m_data + idxStart);
			}

			const_iterator erase(const_iterator pos) {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[m_cap - 1]);

				const auto idxStart = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto items = size() - 1;
				for (size_type i = idxStart; i < items; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return iterator((const T*)m_data + idxStart);
				;
			}

			iterator erase(iterator first, iterator last) {
				GAIA_ASSERT(first.m_pos >= 0 && first.m_pos < size());
				GAIA_ASSERT(last.m_pos >= 0 && last.m_pos < size());
				GAIA_ASSERT(last.m_pos >= last.m_pos);

				for (size_type i = first.m_pos; i < last.m_pos; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return {(T*)m_data + size_type(last.m_pos)};
			}

			void clear() noexcept {
				m_cnt = 0;
			}

			void shirk_to_fit() {
				if (m_cnt >= m_cap)
					return;
				T* old = m_data;
				m_data = new T[m_cap = m_cnt];
				for (size_type i = 0; i < size(); ++i)
					m_data[i] = old[i];
				delete[] old;
			}

			[[nodiscard]] constexpr size_type size() const noexcept {
				return m_cnt;
			}

			[[nodiscard]] constexpr size_type capacity() const noexcept {
				return m_cap;
			}

			[[nodiscard]] constexpr bool empty() const noexcept {
				return size() == 0;
			}

			[[nodiscard]] constexpr size_type max_size() const noexcept {
				return 10'000'000;
			}

			reference front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			reference back() noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_cnt - 1];
			}

			const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_cnt - 1];
			}

			iterator begin() const noexcept {
				return {(T*)m_data};
			}

			const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}

			bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}
		};
	} // namespace containers

} // namespace gaia

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = containers::darr<T>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_VECTOR

#endif

#define USE_ARRAY GAIA_USE_STL_CONTAINERS

#if USE_ARRAY == 1
	#include <array>
namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = std::array<T, N>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0
	
#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		// Array with fixed size and capacity allocated on stack.
		// Interface compatiblity with std::array where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <typename T, size_t N>
		class sarr {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

			T m_data[N ? N : 1]; // support zero-size arrays

			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				T* m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

				constexpr iterator(const iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
					return m_ptr;
				}
				constexpr iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr iterator& operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator& operator--(int) {
					iterator temp(*this);
					--this;
					return temp;
				}

				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				const T* m_ptr;

			public:
				constexpr const_iterator(T* ptr): m_ptr(ptr) {}

				constexpr const_iterator(const const_iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const const_iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr const T& operator*() const {
					return *(const T*)m_ptr;
				}
				constexpr const T* operator->() const {
					return (const T*)m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator& operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator& operator--(int) {
					const_iterator temp(*this);
					--this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			constexpr reference operator[](size_type pos) noexcept {
				return (reference)m_data[pos];
			}

			constexpr const_reference operator[](size_type pos) const noexcept {
				return (const_reference)m_data[pos];
			}

			[[nodiscard]] constexpr size_type size() const noexcept {
				return N;
			}

			[[nodiscard]] constexpr bool empty() const noexcept {
				return begin() == end();
			}

			[[nodiscard]] constexpr size_type max_size() const noexcept {
				return N;
			}

			constexpr reference front() noexcept {
				return *begin();
			}

			constexpr const_reference front() const noexcept {
				return *begin();
			}

			constexpr reference back() noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr const_reference back() const noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			constexpr iterator end() const noexcept {
				return {(T*)m_data + N};
			}

			constexpr const_iterator cend() const noexcept {
				return {(const T*)m_data + N};
			}

			bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...>) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + sizeof...(U)>;

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_ARRAY

#endif

// TODO: There is no quickly achievable std alternative so go with gaia container
#if 1// USE_VECTOR == 0
	
#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		// SArray with fixed capacity and variable size allocated on stack.
		// Interface compatiblity with std::array where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <typename T, size_t N>
		class sarr_ext {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

		private:
			T m_data[N ? N : 1]; // support zero-size arrays
			size_type m_pos;

		public:
			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				T* m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

				constexpr iterator(const iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
					return m_ptr;
				}
				constexpr iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr iterator& operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator& operator--(int) {
					iterator temp(*this);
					--this;
					return temp;
				}

				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				const T* m_ptr;

			public:
				constexpr const_iterator(T* ptr): m_ptr(ptr) {}

				constexpr const_iterator(const const_iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const const_iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr const T& operator*() const {
					return *(const T*)m_ptr;
				}
				constexpr const T* operator->() const {
					return (const T*)m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator& operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator& operator--(int) {
					const_iterator temp(*this);
					--this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			sarr_ext() noexcept {
				clear();
			}

			constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			constexpr reference operator[](size_type pos) noexcept {
				return (reference)m_data[pos];
			}

			constexpr const_reference operator[](size_type pos) const noexcept {
				return (const_reference)m_data[pos];
			}

			void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[++m_pos] = arg;
			}

			constexpr void push_back_ct(const T& arg) noexcept {
				m_data[++m_pos] = arg;
			}

			void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[++m_pos] = std::forward<T>(arg);
			}

			constexpr void push_back_ct(T&& arg) noexcept {
				m_data[++m_pos] = std::forward<T>(arg);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_pos;
			}

			constexpr void pop_back_ct() noexcept {
				--m_pos;
			}

			constexpr void clear() noexcept {
				m_pos = size_type(-1);
			}

			void resize(size_type size) noexcept {
				GAIA_ASSERT(size < N);
				resize_ct(size);
			}

			constexpr void resize_ct(size_type size) noexcept {
				m_pos = size - 1;
			}

			[[nodiscard]] constexpr size_type size() const noexcept {
				return m_pos + 1;
			}

			[[nodiscard]] constexpr bool empty() const noexcept {
				return size() == 0;
			}

			[[nodiscard]] constexpr size_type max_size() const noexcept {
				return N;
			}

			constexpr reference front() noexcept {
				return *begin();
			}

			constexpr const_reference front() const noexcept {
				return *begin();
			}

			constexpr reference back() noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr const_reference back() const noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}

			bool operator==(const sarr_ext& other) const {
				if (m_pos != other.m_pos)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...>) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr_ext<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr_ext<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray_ext = containers::sarr_ext<T, N>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_ARRAY

#endif

#include <cstdint>
#include <type_traits>
#if GAIA_USE_STL_CONTAINERS
	#include <functional>
#endif

namespace gaia {
	namespace utils {

		//! Combines values via OR.
		template <typename... T>
		constexpr auto combine_or([[maybe_unused]] T... t) {
			return (... | t);
		}

		struct direct_hash_key {
			uint64_t hash;
			bool operator==(direct_hash_key other) const {
				return hash == other.hash;
			}
			bool operator!=(direct_hash_key other) const {
				return hash != other.hash;
			}
		};

		//-----------------------------------------------------------------------------------

		namespace detail {

			constexpr void hash_combine2_out(uint32_t& lhs, uint32_t rhs) {
				lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
			}
			constexpr void hash_combine2_out(uint64_t& lhs, uint64_t rhs) {
				lhs ^= rhs + 0x9e3779B97f4a7c15ULL + (lhs << 6) + (lhs >> 2);
			}

			template <typename T>
			[[nodiscard]] constexpr T hash_combine2(T lhs, T rhs) {
				hash_combine2_out(lhs, rhs);
				return lhs;
			}
		} // namespace detail

		//! Combines hashes into another complex one
		template <typename T, typename... Rest>
		constexpr T hash_combine(T first, T next, Rest... rest) {
			auto h = detail::hash_combine2(first, next);
			(detail::hash_combine2_out(h, rest), ...);
			return h;
		}

#if GAIA_ECS_HASH == GAIA_ECS_HASH_FNV1A

		namespace detail {
			namespace fnv1a {
				constexpr uint64_t val_64_const = 0xcbf29ce484222325;
				constexpr uint64_t prime_64_const = 0x100000001b3;
			} // namespace fnv1a
		} // namespace detail

		constexpr uint64_t calculate_hash64(const char* const str) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			size_t i = 0;
			while (str[i] != '\0') {
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;
				++i;
			}

			return hash;
		}

		constexpr uint64_t calculate_hash64(const char* const str, const size_t length) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			for (size_t i = 0; i < length; i++)
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;

			return hash;
		}

#elif GAIA_ECS_HASH == GAIA_ECS_HASH_MURMUR2A

		// Thank you https://gist.github.com/oteguro/10538695

		GAIA_MSVC_WARNING_PUSH()
		GAIA_MSVC_WARNING_DISABLE(4592)

		namespace detail {
			namespace murmur2a {
				constexpr uint64_t seed_64_const = 0xe17a1465ull;
				constexpr uint64_t m = 0xc6a4a7935bd1e995ull;
				constexpr uint64_t r = 47;

				constexpr uint64_t Load8(const char* data) {
					return (uint64_t(data[7]) << 56) | (uint64_t(data[6]) << 48) | (uint64_t(data[5]) << 40) |
								 (uint64_t(data[4]) << 32) | (uint64_t(data[3]) << 24) | (uint64_t(data[2]) << 16) |
								 (uint64_t(data[1]) << 8) | (uint64_t(data[0]) << 0);
				}

				constexpr uint64_t StaticHashValueLast64(uint64_t h) {
					return (((h * m) ^ ((h * m) >> r)) * m) ^ ((((h * m) ^ ((h * m) >> r)) * m) >> r);
				}

				constexpr uint64_t StaticHashValueLast64_(uint64_t h) {
					return (((h) ^ ((h) >> r)) * m) ^ ((((h) ^ ((h) >> r)) * m) >> r);
				}

				constexpr uint64_t StaticHashValue64Tail1(uint64_t h, const char* data) {
					return StaticHashValueLast64((h ^ uint64_t(data[0])));
				}

				constexpr uint64_t StaticHashValue64Tail2(uint64_t h, const char* data) {
					return StaticHashValue64Tail1((h ^ uint64_t(data[1]) << 8), data);
				}

				constexpr uint64_t StaticHashValue64Tail3(uint64_t h, const char* data) {
					return StaticHashValue64Tail2((h ^ uint64_t(data[2]) << 16), data);
				}

				constexpr uint64_t StaticHashValue64Tail4(uint64_t h, const char* data) {
					return StaticHashValue64Tail3((h ^ uint64_t(data[3]) << 24), data);
				}

				constexpr uint64_t StaticHashValue64Tail5(uint64_t h, const char* data) {
					return StaticHashValue64Tail4((h ^ uint64_t(data[4]) << 32), data);
				}

				constexpr uint64_t StaticHashValue64Tail6(uint64_t h, const char* data) {
					return StaticHashValue64Tail5((h ^ uint64_t(data[5]) << 40), data);
				}

				constexpr uint64_t StaticHashValue64Tail7(uint64_t h, const char* data) {
					return StaticHashValue64Tail6((h ^ uint64_t(data[6]) << 48), data);
				}

				constexpr uint64_t StaticHashValueRest64(uint64_t h, size_t len, const char* data) {
					return ((len & 7) == 7)		? StaticHashValue64Tail7(h, data)
								 : ((len & 7) == 6) ? StaticHashValue64Tail6(h, data)
								 : ((len & 7) == 5) ? StaticHashValue64Tail5(h, data)
								 : ((len & 7) == 4) ? StaticHashValue64Tail4(h, data)
								 : ((len & 7) == 3) ? StaticHashValue64Tail3(h, data)
								 : ((len & 7) == 2) ? StaticHashValue64Tail2(h, data)
								 : ((len & 7) == 1) ? StaticHashValue64Tail1(h, data)
																		: StaticHashValueLast64_(h);
				}

				constexpr uint64_t StaticHashValueLoop64(size_t i, uint64_t h, size_t len, const char* data) {
					return (
							i == 0 ? StaticHashValueRest64(h, len, data)
										 : StaticHashValueLoop64(
													 i - 1, (h ^ (((Load8(data) * m) ^ ((Load8(data) * m) >> r)) * m)) * m, len, data + 8));
				}

				constexpr uint64_t hash_murmur2a_64_ct(const char* key, size_t len, uint64_t seed) {
					return StaticHashValueLoop64(len / 8, seed ^ (uint64_t(len) * m), (len), key);
				}
			} // namespace murmur2a
		} // namespace detail

		constexpr uint64_t calculate_hash64(uint64_t value) {
			value ^= value >> 33U;
			value *= 0xff51afd7ed558ccdull;
			value ^= value >> 33U;

			value *= 0xc4ceb9fe1a85ec53ull;
			value ^= value >> 33U;
			return static_cast<size_t>(value);
		}

		constexpr uint64_t calculate_hash64(const char* str) {
			size_t size = 0;
			while (str[size] != '\0')
				++size;

			return detail::murmur2a::hash_murmur2a_64_ct(str, size, detail::murmur2a::seed_64_const);
		}

		constexpr uint64_t calculate_hash64(const char* str, size_t length) {
			return detail::murmur2a::hash_murmur2a_64_ct(str, length, detail::murmur2a::seed_64_const);
		}

		GAIA_MSVC_WARNING_POP()

#else
	#error "Unknown hashing type defined"
#endif

	} // namespace utils
} // namespace gaia

#if GAIA_USE_STL_CONTAINERS
template <>
struct std::hash<gaia::utils::direct_hash_key> {
	size_t operator()(gaia::utils::direct_hash_key value) const noexcept {
		return value.hash;
	}
};
#endif

#include <cstring>
#include <type_traits>

namespace gaia {
	namespace utils {
		/*!
		Align a number to the requested byte alignment
		\param num Number to align
		\param alignment Requested alignment
		\return Aligned number
		*/
		template <typename T, typename V>
		constexpr T align(T num, V alignment) {
			return alignment == 0 ? num : ((num + (alignment - 1)) / alignment) * alignment;
		}

		/*!
		Align a number to the requested byte alignment
		\tparam alignment Requested alignment in bytes
		\param num Number to align
		\return Aligned number
		*/
		template <size_t alignment, typename T>
		constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}

		/*!
		Fill memory with 32 bit integer value
		\param dest pointer to destination
		\param num number of items to be filled (not data size!)
		\param data 32bit data to be filled
		*/
		template <typename T>
		void fill_array(T* dest, int num, const T& data) {
			for (int n = 0; n < num; n++)
				((T*)dest)[n] = data;
		}

		/*!
		Convert form type \tparam From to type \tparam To without causing an
		undefined behavior.

		E.g.:
		int i = {};
		float f = *(*float)&i; // undefined behavior
		memcpy(&f, &i, sizeof(float)); // okay
		*/
		template <
				typename To, typename From,
				typename = std::enable_if_t<
						(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>>>
		To bit_cast(const From& from) {
			To to;
			memmove(&to, &from, sizeof(To));
			return to;
		}

		/*!
		Pointer wrapper for reading memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class const_unaligned_pointer {
			const uint8_t* from;

		public:
			const_unaligned_pointer(): from(nullptr) {}
			const_unaligned_pointer(const void* p): from((const uint8_t*)p) {}

			T operator*() const {
				T to;
				memmove(&to, from, sizeof(T));
				return to;
			}

			T operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			const_unaligned_pointer operator+(ptrdiff_t d) const {
				return const_unaligned_pointer(from + d * sizeof(T));
			}
			const_unaligned_pointer operator-(ptrdiff_t d) const {
				return const_unaligned_pointer(from - d * sizeof(T));
			}
		};

		/*!
		Pointer wrapper for writing memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class unaligned_ref {
			void* m_p;

		public:
			unaligned_ref(void* p): m_p(p) {}

			T operator=(const T& rvalue) {
				memmove(m_p, &rvalue, sizeof(T));
				return rvalue;
			}

			operator T() const {
				T tmp;
				memmove(&tmp, m_p, sizeof(T));
				return tmp;
			}
		};

		/*!
		Pointer wrapper for writing memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class unaligned_pointer {
			uint8_t* m_p;

		public:
			unaligned_pointer(): m_p(nullptr) {}
			unaligned_pointer(void* p): m_p((uint8_t*)p) {}

			unaligned_ref<T> operator*() const {
				return unaligned_ref<T>(m_p);
			}

			unaligned_ref<T> operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			unaligned_pointer operator+(ptrdiff_t d) const {
				return unaligned_pointer(m_p + d * sizeof(T));
			}
			unaligned_pointer operator-(ptrdiff_t d) const {
				return unaligned_pointer(m_p - d * sizeof(T));
			}
		};
	} // namespace utils
} // namespace gaia

#include <cinttypes>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <cinttypes>
#include <type_traits>

#define USE_HASHMAP GAIA_USE_STL_CONTAINERS

#if USE_HASHMAP == 1
	#include <unordered_map>
namespace gaia {
	namespace containers {
		template <typename Key, typename Data>
		using map = std::unordered_map<Key, Data>;
	} // namespace containers
} // namespace gaia
#elif USE_HASHMAP == 0
	//                 ______  _____                 ______                _________
//  ______________ ___  /_ ___(_)_______         ___  /_ ______ ______ ______  /
//  __  ___/_  __ \__  __ \__  / __  __ \        __  __ \_  __ \_  __ \_  __  /
//  _  /    / /_/ /_  /_/ /_  /  _  / / /        _  / / // /_/ // /_/ // /_/ /
//  /_/     \____/ /_.___/ /_/   /_/ /_/ ________/_/ /_/ \____/ \____/ \__,_/
//                                      _/_____/
//
// Fast & memory efficient hashtable based on robin hood hashing for C++11/14/17/20
// https://github.com/martinus/robin-hood-hashing
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2021 Martin Ankerl <http://martin.ankerl.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ROBIN_HOOD_H_INCLUDED
#define ROBIN_HOOD_H_INCLUDED

// see https://semver.org/
#define ROBIN_HOOD_VERSION_MAJOR 3 // for incompatible API changes
#define ROBIN_HOOD_VERSION_MINOR 11 // for adding functionality in a backwards-compatible manner
#define ROBIN_HOOD_VERSION_PATCH 5 // for backwards-compatible bug fixes

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <algorithm>
#endif
#include <tuple>
#include <type_traits>

namespace gaia {
	namespace utils {

		template <typename T>
		constexpr void swap(T& left, T& right) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::swap(left, right);
#else
			T tmp = std::move(left);
			left = std::move(right);
			right = std::move(tmp);
#endif
		}

		template <typename InputIt, typename Func>
		constexpr Func for_each(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::for_each(first, last, func);
#else
			for (; first != last; ++first)
				func(*first);
			return func;
#endif
		}

		template <class ForwardIt, class T>
		void fill(ForwardIt first, ForwardIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			std::fill(first, last, value);
#else
			for (; first != last; ++first) {
				*first = value;
			}
#endif
		}

		template <class T>
		const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template <class T>
		const T& get_max(const T& a, const T& b) {
			return (b > a) ? b : a;
		}

		//----------------------------------------------------------------------
		// Checking if a template arg is unique among the rest
		//----------------------------------------------------------------------

		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> =
				std::bool_constant<(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		namespace detail {
			template <typename T>
			struct type_identity {
				using type = T;
			};
		} // namespace detail

		template <typename T, typename... Ts>
		struct unique: detail::type_identity<T> {}; // TODO: In C++20 we could use std::type_identity

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>, unique<std::tuple<Ts..., U>, Us...>> {};

		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;

		//----------------------------------------------------------------------
		// Calculating total size of all types of tuple
		//----------------------------------------------------------------------

		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const&) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Function helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

		//----------------------------------------------------------------------
		// Type helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		template <typename TypesA, typename TypesB>
		struct type_list_concat;

		template <typename... TypesA, typename... TypesB>
		struct type_list_concat<type_list<TypesA...>, type_list<TypesB...>> {
			using type = type_list<TypesA..., TypesB...>;
		};

		//----------------------------------------------------------------------
		// Compile - time for each
		//----------------------------------------------------------------------

		namespace detail {
			template <typename Func, auto... Is>
			constexpr void for_each_impl(Func func, std::index_sequence<Is...>) {
				(func(std::integral_constant<decltype(Is), Is>{}), ...);
			}

			template <typename Tuple, typename Func, auto... Is>
			void for_each_tuple_impl(Tuple&& tuple, Func func, std::index_sequence<Is...>) {
				(func(std::get<Is>(tuple)), ...);
			}
		} // namespace detail

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example:
		//! sarray<int, 10> arr = { ... };
		//! for_each<arr.size()>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		template <auto Iters, typename Func>
		constexpr void for_each(Func func) {
			detail::for_each_impl(func, std::make_index_sequence<Iters>());
		}

		//! Compile-time for loop over containers.
		//! Iteration starts at \tparam FirstIdx and end at \tparam LastIdx
		//! (excluding) in increments of \tparam Inc.
		//!
		//! Example:
		//! sarray<int, 10> arr;
		//! for_each_ext<0, 10, 1>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		//! print(69, "likes", 420.0f);
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void for_each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				for_each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  for_each_pack([][const auto& value]) {
		//!    std::cout << value << std::endl;
		//!  }
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void for_each_pack(Func func, Args&&... args) {
			(func(std::forward<Args>(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! for_each_tuple(const auto& value) {
		//!  std::cout << value << std::endl;
		//!  }, std::make(69, "likes", 420.0f);
		template <typename Tuple, typename Func>
		constexpr void for_each_tuple(Tuple&& tuple, Func func) {
			detail::for_each_tuple_impl(
					std::forward<Tuple>(tuple), func,
					std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find(first, last, value);
#else
			for (; first != last; ++first) {
				if (*first == value) {
					return first;
				}
			}
			return last;
#endif
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if(first, last, func);
#else
			for (; first != last; ++first) {
				if (func(*first)) {
					return first;
				}
			}
			return last;
#endif
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if_not(first, last, func);
#else
			for (; first != last; ++first) {
				if (!func(*first)) {
					return first;
				}
			}
			return last;
#endif
		}

		//----------------------------------------------------------------------
		// Sorting
		//----------------------------------------------------------------------

		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		template <typename T>
		struct is_greater {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs > rhs;
			}
		};

		template <typename T, typename Func>
		constexpr void swap_if(T& lhs, T& rhs, Func func) noexcept {
			T t = func(lhs, rhs) ? std::move(lhs) : std::move(rhs);
			rhs = func(lhs, rhs) ? std::move(rhs) : std::move(lhs);
			lhs = std::move(t);
		}

		namespace detail {
			template <typename Array, typename TSortFunc>
			constexpr void comb_sort_impl(Array& array_, TSortFunc func) noexcept {
				using size_type = typename Array::size_type;
				size_type gap = array_.size();
				bool swapped = false;
				while ((gap > size_type{1}) || swapped) {
					if (gap > size_type{1}) {
						gap = static_cast<size_type>(gap / 1.247330950103979);
					}
					swapped = false;
					for (size_type i = size_type{0}; gap + i < static_cast<size_type>(array_.size()); ++i) {
						if (func(array_[i], array_[i + gap])) {
							auto swap = array_[i];
							array_[i] = array_[i + gap];
							array_[i + gap] = swap;
							swapped = true;
						}
					}
				}
			}

			template <typename Container>
			int quick_sort_partition(Container& arr, int low, int high) {
				const auto& pivot = arr[high];
				int i = low - 1;
				for (int j = low; j <= high - 1; j++) {
					if (arr[j] < pivot)
						utils::swap(arr[++i], arr[j]);
				}
				utils::swap(arr[++i], arr[high]);
				return i;
			}

			template <typename Container>
			void quick_sort(Container& arr, int low, int high) {
				if (low >= high)
					return;
				auto pos = quick_sort_partition(arr, low, high);
				quick_sort(arr, low, pos - 1);
				quick_sort(arr, pos + 1, high);
			}
		} // namespace detail

		//! Compile-time sort.
		//! Implements a sorting network for \tparam N up to 8
		template <typename Container, typename TSortFunc>
		constexpr void sort_ct(Container& arr, TSortFunc func) noexcept {
			constexpr size_t NItems = std::tuple_size<Container>::value;
			if constexpr (NItems <= 1) {
				return;
			} else if constexpr (NItems == 2) {
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				//! TODO: replace with std::sort for c++20
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				detail::comb_sort_impl(arr, func);
				GAIA_MSVC_WARNING_POP()
#else
				detail::comb_sort_impl(arr, func);
#endif
			}
		}

		//! Sorting including a sorting network for containers up to 8 elements in size.
		//! Ordinary sorting used for bigger containers.
		template <typename Container, typename TSortFunc>
		void sort(Container& arr, TSortFunc func) {
			if (arr.size() <= 1) {
				return;
			} else if (arr.size() == 2) {
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else if (arr.size() <= 32) {
				size_t i, j;
				size_t n = arr.size();
				for (i = 0; i < n - 1; i++) {
					for (j = 0; j < n - i - 1; j++) {
						if (arr[j] > arr[j + 1])
							utils::swap(arr[j], arr[j + 1]);
					}
				}
			} else {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				std::sort(arr.begin(), arr.end(), func);
				GAIA_MSVC_WARNING_POP()
#else
				const int n = (int)arr.size();
				detail::quick_sort(arr, 0, n - 1);

#endif
			}
		}

		// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#if GAIA_HAS_NO_INLINE_ASSEMBLY
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			asm volatile("" : : "r,m"(value) : "memory");
		}

		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T& value) {
	#if defined(__clang__)
			asm volatile("" : "+r,m"(value) : : "memory");
	#else
			asm volatile("" : "+m,r"(value) : : "memory");
	#endif
		}
#else
		namespace internal {
			inline GAIA_FORCEINLINE void UseCharPointer(char const volatile* var) {
				(void)var;
			}
		} // namespace internal

	#if defined(_MSC_VER)
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
			_ReadWriteBarrier();
		}
	#else
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
		}
	#endif
#endif

	} // namespace utils
} // namespace gaia

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

// #define ROBIN_HOOD_STD_SMARTPOINTERS
#ifdef ROBIN_HOOD_STD_SMARTPOINTERS
	#include <memory>
#endif

// #define ROBIN_HOOD_LOG_ENABLED
#ifdef ROBIN_HOOD_LOG_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_LOG(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_LOG(x)
#endif

// #define ROBIN_HOOD_TRACE_ENABLED
#ifdef ROBIN_HOOD_TRACE_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_TRACE(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_TRACE(x)
#endif

// #define ROBIN_HOOD_COUNT_ENABLED
#ifdef ROBIN_HOOD_COUNT_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_COUNT(x) ++counts().x;
namespace robin_hood {
	struct Counts {
		uint64_t shiftUp{};
		uint64_t shiftDown{};
	};
	inline std::ostream& operator<<(std::ostream& os, Counts const& c) {
		return os << c.shiftUp << " shiftUp" << std::endl << c.shiftDown << " shiftDown" << std::endl;
	}

	static Counts& counts() {
		static Counts counts{};
		return counts;
	}
} // namespace robin_hood
#else
	#define ROBIN_HOOD_COUNT(x)
#endif

// all non-argument macros should use this facility. See
// https://www.fluentcpp.com/2019/05/28/better-macros-better-flags/
#define ROBIN_HOOD(x) ROBIN_HOOD_PRIVATE_DEFINITION_##x()

// mark unused members with this macro
#define ROBIN_HOOD_UNUSED(identifier)

// bitness
#if SIZE_MAX == UINT32_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 32
#elif SIZE_MAX == UINT64_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 64
#else
	#error Unsupported bitness
#endif

// endianess
#ifdef _MSC_VER
	#if defined(_M_ARM) || defined(_M_ARM64)
		#define ROBIN_HOOD_PRIVATE_DEFINITION_LITTLE_ENDIAN() 0
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BIG_ENDIAN() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_LITTLE_ENDIAN() 1
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BIG_ENDIAN() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_LITTLE_ENDIAN() (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BIG_ENDIAN() (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#endif

// inline
#ifdef _MSC_VER
	#define ROBIN_HOOD_PRIVATE_DEFINITION_NOINLINE() __declspec(noinline)
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_NOINLINE() __attribute__((noinline))
#endif

// exceptions
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 0
	#define ROBIN_HOOD_STD_OUT_OF_RANGE void
#else
	#include <stdexcept>
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 1
	#define ROBIN_HOOD_STD_OUT_OF_RANGE std::out_of_range
#endif

// count leading/trailing bits
#if !defined(ROBIN_HOOD_DISABLE_INTRINSICS)
	#ifdef _MSC_VER
		#if ROBIN_HOOD(BITNESS) == 32
			#define ROBIN_HOOD_PRIVATE_DEFINITION_BITSCANFORWARD() _BitScanForward
		#else
			#define ROBIN_HOOD_PRIVATE_DEFINITION_BITSCANFORWARD() _BitScanForward64
		#endif
		#if _MSV_VER <= 1916
			#include <intrin.h>
		#endif
		#pragma intrinsic(ROBIN_HOOD(BITSCANFORWARD))
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x)                                                                        \
			[](size_t mask) noexcept -> int {                                                                                \
				unsigned long index;                                                                                           \
				return ROBIN_HOOD(BITSCANFORWARD)(&index, mask) ? static_cast<int>(index) : ROBIN_HOOD(BITNESS);               \
			}(x)
	#else
		#if ROBIN_HOOD(BITNESS) == 32
			#define ROBIN_HOOD_PRIVATE_DEFINITION_CTZ() __builtin_ctzl
			#define ROBIN_HOOD_PRIVATE_DEFINITION_CLZ() __builtin_clzl
		#else
			#define ROBIN_HOOD_PRIVATE_DEFINITION_CTZ() __builtin_ctzll
			#define ROBIN_HOOD_PRIVATE_DEFINITION_CLZ() __builtin_clzll
		#endif
		#define ROBIN_HOOD_COUNT_LEADING_ZEROES(x) ((x) ? ROBIN_HOOD(CLZ)(x) : ROBIN_HOOD(BITNESS))
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x) ((x) ? ROBIN_HOOD(CTZ)(x) : ROBIN_HOOD(BITNESS))
	#endif
#endif

// fallthrough
#ifndef __has_cpp_attribute // For backwards compatibility
	#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(fallthrough)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH() [[fallthrough]]
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH()
#endif

// likely/unlikely
#ifdef _MSC_VER
	#define ROBIN_HOOD_LIKELY(condition) condition
	#define ROBIN_HOOD_UNLIKELY(condition) condition
#else
	#define ROBIN_HOOD_LIKELY(condition) __builtin_expect(condition, 1)
	#define ROBIN_HOOD_UNLIKELY(condition) __builtin_expect(condition, 0)
#endif

// detect if native wchar_t type is availiable in MSVC
#ifdef _MSC_VER
	#ifdef _NATIVE_WCHAR_T_DEFINED
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
#endif

// detect if MSVC supports the pair(std::piecewise_construct_t,...) constructor being constexpr
#ifdef _MSC_VER
	#if _MSC_VER <= 1900
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
#endif

// workaround missing "is_trivially_copyable" in g++ < 5.0
// See https://stackoverflow.com/a/31798726/48181
#if defined(__GNUC__) && __GNUC__ < 5
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) __has_trivial_copy(__VA_ARGS__)
#else
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) std::is_trivially_copyable<__VA_ARGS__>::value
#endif

namespace robin_hood {

	namespace detail {

// make sure we static_cast to the correct type for hash_int
#if ROBIN_HOOD(BITNESS) == 64
		using SizeT = uint64_t;
#else
		using SizeT = uint32_t;
#endif

		template <typename T>
		T rotr(T x, unsigned k) {
			return (x >> k) | (x << (8U * sizeof(T) - k));
		}

		// This cast gets rid of warnings like "cast from 'uint8_t*' {aka 'unsigned char*'} to
		// 'uint64_t*' {aka 'long unsigned int*'} increases required alignment of target type". Use with
		// care!
		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void const* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		// make sure this is not inlined as it is slow and dramatically enlarges code, thus making other
		// inlinings more difficult. Throws are also generally the slow path.
		template <typename E, typename... Args>
		[[noreturn]] ROBIN_HOOD(NOINLINE)
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				void doThrow(Args&&... args) {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
			throw E(std::forward<Args>(args)...);
		}
#else
				void doThrow(Args&&... ROBIN_HOOD_UNUSED(args) /*unused*/) {
			abort();
		}
#endif

		template <typename E, typename T, typename... Args>
		T* assertNotNull(T* t, Args&&... args) {
			if (ROBIN_HOOD_UNLIKELY(nullptr == t)) {
				doThrow<E>(std::forward<Args>(args)...);
			}
			return t;
		}

		template <typename T>
		inline T unaligned_load(void const* ptr) noexcept {
			// using memcpy so we don't get into unaligned load problems.
			// compiler should optimize this very well anyways.
			T t;
			std::memcpy(&t, ptr, sizeof(T));
			return t;
		}

		// Allocates bulks of memory for objects of type T. This deallocates the memory in the destructor,
		// and keeps a linked list of the allocated memory around. Overhead per allocation is the size of a
		// pointer.
		template <typename T, size_t MinNumAllocs = 4, size_t MaxNumAllocs = 256>
		class BulkPoolAllocator {
		public:
			BulkPoolAllocator() noexcept = default;

			// does not copy anything, just creates a new allocator.
			BulkPoolAllocator(const BulkPoolAllocator& ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept:
					mHead(nullptr), mListForFree(nullptr) {}

			BulkPoolAllocator(BulkPoolAllocator&& o) noexcept: mHead(o.mHead), mListForFree(o.mListForFree) {
				o.mListForFree = nullptr;
				o.mHead = nullptr;
			}

			BulkPoolAllocator& operator=(BulkPoolAllocator&& o) noexcept {
				reset();
				mHead = o.mHead;
				mListForFree = o.mListForFree;
				o.mListForFree = nullptr;
				o.mHead = nullptr;
				return *this;
			}

			BulkPoolAllocator&
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			operator=(const BulkPoolAllocator& ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept {
				// does not do anything
				return *this;
			}

			~BulkPoolAllocator() noexcept {
				reset();
			}

			// Deallocates all allocated memory.
			void reset() noexcept {
				while (mListForFree) {
					T* tmp = *mListForFree;
					ROBIN_HOOD_LOG("std::free")
					std::free(mListForFree);
					mListForFree = reinterpret_cast_no_cast_align_warning<T**>(tmp);
				}
				mHead = nullptr;
			}

			// allocates, but does NOT initialize. Use in-place new constructor, e.g.
			//   T* obj = pool.allocate();
			//   ::new (static_cast<void*>(obj)) T();
			T* allocate() {
				T* tmp = mHead;
				if (!tmp) {
					tmp = performAllocation();
				}

				mHead = *reinterpret_cast_no_cast_align_warning<T**>(tmp);
				return tmp;
			}

			// does not actually deallocate but puts it in store.
			// make sure you have already called the destructor! e.g. with
			//  obj->~T();
			//  pool.deallocate(obj);
			void deallocate(T* obj) noexcept {
				*reinterpret_cast_no_cast_align_warning<T**>(obj) = mHead;
				mHead = obj;
			}

			// Adds an already allocated block of memory to the allocator. This allocator is from now on
			// responsible for freeing the data (with free()). If the provided data is not large enough to
			// make use of, it is immediately freed. Otherwise it is reused and freed in the destructor.
			void addOrFree(void* ptr, const size_t numBytes) noexcept {
				// calculate number of available elements in ptr
				if (numBytes < ALIGNMENT + ALIGNED_SIZE) {
					// not enough data for at least one element. Free and return.
					ROBIN_HOOD_LOG("std::free")
					std::free(ptr);
				} else {
					ROBIN_HOOD_LOG("add to buffer")
					add(ptr, numBytes);
				}
			}

			void swap(BulkPoolAllocator<T, MinNumAllocs, MaxNumAllocs>& other) noexcept {
				using std::swap;
				swap(mHead, other.mHead);
				swap(mListForFree, other.mListForFree);
			}

		private:
			// iterates the list of allocated memory to calculate how many to alloc next.
			// Recalculating this each time saves us a size_t member.
			// This ignores the fact that memory blocks might have been added manually with addOrFree. In
			// practice, this should not matter much.
			[[nodiscard]] size_t calcNumElementsToAlloc() const noexcept {
				auto tmp = mListForFree;
				size_t numAllocs = MinNumAllocs;

				while (numAllocs * 2 <= MaxNumAllocs && tmp) {
					auto x = reinterpret_cast<T***>(tmp);
					tmp = *x;
					numAllocs *= 2;
				}

				return numAllocs;
			}

			// WARNING: Underflow if numBytes < ALIGNMENT! This is guarded in addOrFree().
			void add(void* ptr, const size_t numBytes) noexcept {
				const size_t numElements = (numBytes - ALIGNMENT) / ALIGNED_SIZE;

				auto data = reinterpret_cast<T**>(ptr);

				// link free list
				auto x = reinterpret_cast<T***>(data);
				*x = mListForFree;
				mListForFree = data;

				// create linked list for newly allocated data
				auto* const headT = reinterpret_cast_no_cast_align_warning<T*>(reinterpret_cast<char*>(ptr) + ALIGNMENT);

				auto* const head = reinterpret_cast<char*>(headT);

				// Visual Studio compiler automatically unrolls this loop, which is pretty cool
				for (size_t i = 0; i < numElements; ++i) {
					*reinterpret_cast_no_cast_align_warning<char**>(head + i * ALIGNED_SIZE) = head + (i + 1) * ALIGNED_SIZE;
				}

				// last one points to 0
				*reinterpret_cast_no_cast_align_warning<T**>(head + (numElements - 1) * ALIGNED_SIZE) = mHead;
				mHead = headT;
			}

			// Called when no memory is available (mHead == 0).
			// Don't inline this slow path.
			ROBIN_HOOD(NOINLINE) T* performAllocation() {
				size_t const numElementsToAlloc = calcNumElementsToAlloc();

				// alloc new memory: [prev |T, T, ... T]
				size_t const bytes = ALIGNMENT + ALIGNED_SIZE * numElementsToAlloc;
				ROBIN_HOOD_LOG(
						"std::malloc " << bytes << " = " << ALIGNMENT << " + " << ALIGNED_SIZE << " * " << numElementsToAlloc)
				add(assertNotNull<std::bad_alloc>(std::malloc(bytes)), bytes);
				return mHead;
			}

			// enforce byte alignment of the T's
			static constexpr size_t ALIGNMENT =
					gaia::utils::get_max(std::alignment_of<T>::value, std::alignment_of<T*>::value);

			static constexpr size_t ALIGNED_SIZE = ((sizeof(T) - 1) / ALIGNMENT + 1) * ALIGNMENT;

			static_assert(MinNumAllocs >= 1, "MinNumAllocs");
			static_assert(MaxNumAllocs >= MinNumAllocs, "MaxNumAllocs");
			static_assert(ALIGNED_SIZE >= sizeof(T*), "ALIGNED_SIZE");
			static_assert(0 == (ALIGNED_SIZE % sizeof(T*)), "ALIGNED_SIZE mod");
			static_assert(ALIGNMENT >= sizeof(T*), "ALIGNMENT");

			T* mHead{nullptr};
			T** mListForFree{nullptr};
		};

		template <typename T, size_t MinSize, size_t MaxSize, bool IsFlat>
		struct NodeAllocator;

		// dummy allocator that does nothing
		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, true> {

			// we are not using the data, so just free it.
			void addOrFree(void* ptr, size_t ROBIN_HOOD_UNUSED(numBytes) /*unused*/) noexcept {
				ROBIN_HOOD_LOG("std::free")
				std::free(ptr);
			}
		};

		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, false>: public BulkPoolAllocator<T, MinSize, MaxSize> {};

		namespace swappable {
			template <typename T>
			struct nothrow {
				static const bool value = std::is_nothrow_swappable<T>::value;
			};
		} // namespace swappable

	} // namespace detail

	struct is_transparent_tag {};

	// A custom pair implementation is used in the map because std::pair is not is_trivially_copyable,
	// which means it would  not be allowed to be used in std::memcpy. This struct is copyable, which is
	// also tested.
	template <typename T1, typename T2>
	struct pair {
		using first_type = T1;
		using second_type = T2;

		template <
				typename U1 = T1, typename U2 = T2,
				typename = typename std::enable_if<
						std::is_default_constructible<U1>::value && std::is_default_constructible<U2>::value>::type>
		constexpr pair() noexcept(noexcept(U1()) && noexcept(U2())): first(), second() {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2> const& o) noexcept(
				noexcept(T1(std::declval<T1 const&>())) && noexcept(T2(std::declval<T2 const&>()))):
				first(o.first),
				second(o.second) {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2>&& o) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(o.first)),
				second(std::move(o.second)) {}

		constexpr pair(T1&& a, T2&& b) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(a)),
				second(std::move(b)) {}

		template <typename U1, typename U2>
		constexpr pair(U1&& a, U2&& b) noexcept(
				noexcept(T1(std::forward<U1>(std::declval<U1&&>()))) && noexcept(T2(std::forward<U2>(std::declval<U2&&>())))):
				first(std::forward<U1>(a)),
				second(std::forward<U2>(b)) {}

		template <typename... U1, typename... U2>
		// MSVC 2015 produces error "C2476: ???constexpr??? constructor does not initialize all members"
		// if this constructor is constexpr
#if !ROBIN_HOOD(BROKEN_CONSTEXPR)
		constexpr
#endif
				pair(std::piecewise_construct_t /*unused*/, std::tuple<U1...> a, std::tuple<U2...> b) noexcept(noexcept(pair(
						std::declval<std::tuple<U1...>&>(), std::declval<std::tuple<U2...>&>(), std::index_sequence_for<U1...>(),
						std::index_sequence_for<U2...>()))):
				pair(a, b, std::index_sequence_for<U1...>(), std::index_sequence_for<U2...>()) {
		}

		// constructor called from the std::piecewise_construct_t ctor
		template <typename... U1, size_t... I1, typename... U2, size_t... I2>
		pair(
				std::tuple<U1...>& a, std::tuple<U2...>& b, std::index_sequence<I1...> /*unused*/,
				std::index_sequence<
						I2...> /*unused*/) noexcept(noexcept(T1(std::
																												forward<U1>(std::get<I1>(
																														std::declval<std::tuple<
																																U1...>&>()))...)) && noexcept(T2(std::
																																																		 forward<
																																																				 U2>(std::get<
																																																						 I2>(
																																																				 std::declval<std::tuple<
																																																						 U2...>&>()))...))):
				first(std::forward<U1>(std::get<I1>(a))...),
				second(std::forward<U2>(std::get<I2>(b))...) {
			// make visual studio compiler happy about warning about unused a & b.
			// Visual studio's pair implementation disables warning 4100.
			(void)a;
			(void)b;
		}

		void
		swap(pair<T1, T2>& o) noexcept((detail::swappable::nothrow<T1>::value) && (detail::swappable::nothrow<T2>::value)) {
			using std::swap;
			swap(first, o.first);
			swap(second, o.second);
		}

		T1 first; // NOLINT(misc-non-private-member-variables-in-classes)
		T2 second; // NOLINT(misc-non-private-member-variables-in-classes)
	};

	template <typename A, typename B>
	inline void
	swap(pair<A, B>& a, pair<A, B>& b) noexcept(noexcept(std::declval<pair<A, B>&>().swap(std::declval<pair<A, B>&>()))) {
		a.swap(b);
	}

	template <typename A, typename B>
	inline constexpr bool operator==(pair<A, B> const& x, pair<A, B> const& y) {
		return (x.first == y.first) && (x.second == y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator!=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x == y);
	}
	template <typename A, typename B>
	inline constexpr bool operator<(pair<A, B> const& x, pair<A, B> const& y) noexcept(noexcept(
			std::declval<A const&>() <
			std::declval<A const&>()) && noexcept(std::declval<B const&>() < std::declval<B const&>())) {
		return x.first < y.first || (!(y.first < x.first) && x.second < y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator>(pair<A, B> const& x, pair<A, B> const& y) {
		return y < x;
	}
	template <typename A, typename B>
	inline constexpr bool operator<=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x > y);
	}
	template <typename A, typename B>
	inline constexpr bool operator>=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x < y);
	}

	inline size_t hash_bytes(void const* ptr, size_t len) noexcept {
		static constexpr uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
		static constexpr uint64_t seed = UINT64_C(0xe17a1465);
		static constexpr unsigned int r = 47;

		auto const* const data64 = static_cast<uint64_t const*>(ptr);
		uint64_t h = seed ^ (len * m);

		size_t const n_blocks = len / 8;
		for (size_t i = 0; i < n_blocks; ++i) {
			auto k = detail::unaligned_load<uint64_t>(data64 + i);

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;
		}

		auto const* const data8 = reinterpret_cast<uint8_t const*>(data64 + n_blocks);
		switch (len & 7U) {
			case 7:
				h ^= static_cast<uint64_t>(data8[6]) << 48U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 6:
				h ^= static_cast<uint64_t>(data8[5]) << 40U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 5:
				h ^= static_cast<uint64_t>(data8[4]) << 32U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 4:
				h ^= static_cast<uint64_t>(data8[3]) << 24U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 3:
				h ^= static_cast<uint64_t>(data8[2]) << 16U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 2:
				h ^= static_cast<uint64_t>(data8[1]) << 8U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 1:
				h ^= static_cast<uint64_t>(data8[0]);
				h *= m;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			default:
				break;
		}

		h ^= h >> r;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// h *= m;
		// h ^= h >> r;
		return static_cast<size_t>(h);
	}

	inline size_t hash_int(uint64_t x) noexcept {
		// tried lots of different hashes, let's stick with murmurhash3. It's simple, fast, well tested,
		// and doesn't need any special 128bit operations.
		x ^= x >> 33U;
		x *= UINT64_C(0xff51afd7ed558ccd);
		x ^= x >> 33U;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// x *= UINT64_C(0xc4ceb9fe1a85ec53);
		// x ^= x >> 33U;
		return static_cast<size_t>(x);
	}

	// A thin wrapper around std::hash, performing an additional simple mixing step of the result.
	template <typename T, typename Enable = void>
	struct hash: public std::hash<T> {
		size_t operator()(T const& obj) const
				noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>()))) {
			// call base hash
			auto result = std::hash<T>::operator()(obj);
			// return mixed of that, to be save against identity has
			return hash_int(static_cast<detail::SizeT>(result));
		}
	};

	template <typename T>
	struct hash<T*> {
		size_t operator()(T* ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr));
		}
	};

#ifdef ROBIN_HOOD_STD_SMARTPOINTERS
	template <typename T>
	struct hash<std::unique_ptr<T>> {
		size_t operator()(std::unique_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};

	template <typename T>
	struct hash<std::shared_ptr<T>> {
		size_t operator()(std::shared_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};
#endif

	template <typename Enum>
	struct hash<Enum, typename std::enable_if<std::is_enum<Enum>::value>::type> {
		size_t operator()(Enum e) const noexcept {
			using Underlying = typename std::underlying_type<Enum>::type;
			return hash<Underlying>{}(static_cast<Underlying>(e));
		}
	};

	template <>
	struct hash<gaia::utils::direct_hash_key> {
		size_t operator()(const gaia::utils::direct_hash_key& obj) const noexcept {
			return obj.hash;
		}
	};

#define ROBIN_HOOD_HASH_INT(T)                                                                                         \
	template <>                                                                                                          \
	struct hash<T> {                                                                                                     \
		size_t operator()(T const& obj) const noexcept {                                                                   \
			return hash_int(static_cast<uint64_t>(obj));                                                                     \
		}                                                                                                                  \
	}

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
	// see https://en.cppreference.com/w/cpp/utility/hash
	ROBIN_HOOD_HASH_INT(bool);
	ROBIN_HOOD_HASH_INT(char);
	ROBIN_HOOD_HASH_INT(signed char);
	ROBIN_HOOD_HASH_INT(unsigned char);
	ROBIN_HOOD_HASH_INT(char16_t);
	ROBIN_HOOD_HASH_INT(char32_t);
#if ROBIN_HOOD(HAS_NATIVE_WCHART)
	ROBIN_HOOD_HASH_INT(wchar_t);
#endif
	ROBIN_HOOD_HASH_INT(short);
	ROBIN_HOOD_HASH_INT(unsigned short);
	ROBIN_HOOD_HASH_INT(int);
	ROBIN_HOOD_HASH_INT(unsigned int);
	ROBIN_HOOD_HASH_INT(long);
	ROBIN_HOOD_HASH_INT(long long);
	ROBIN_HOOD_HASH_INT(unsigned long);
	ROBIN_HOOD_HASH_INT(unsigned long long);
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif
	namespace detail {

		template <typename T>
		struct void_type {
			using type = void;
		};

		template <typename T, typename = void>
		struct has_is_transparent: public std::false_type {};

		template <typename T>
		struct has_is_transparent<T, typename void_type<typename T::is_transparent>::type>: public std::true_type {};

		// using wrapper classes for hash and key_equal prevents the diamond problem when the same type
		// is used. see https://stackoverflow.com/a/28771920/48181
		template <typename T>
		struct WrapHash: public T {
			WrapHash() = default;
			explicit WrapHash(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		template <typename T>
		struct WrapKeyEqual: public T {
			WrapKeyEqual() = default;
			explicit WrapKeyEqual(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		// A highly optimized hashmap implementation, using the Robin Hood algorithm.
		//
		// In most cases, this map should be usable as a drop-in replacement for std::unordered_map, but
		// be about 2x faster in most cases and require much less allocations.
		//
		// This implementation uses the following memory layout:
		//
		// [Node, Node, ... Node | info, info, ... infoSentinel ]
		//
		// * Node: either a DataNode that directly has the std::pair<key, val> as member,
		//   or a DataNode with a pointer to std::pair<key,val>. Which DataNode representation to use
		//   depends on how fast the swap() operation is. Heuristically, this is automatically choosen
		//   based on sizeof(). there are always 2^n Nodes.
		//
		// * info: Each Node in the map has a corresponding info byte, so there are 2^n info bytes.
		//   Each byte is initialized to 0, meaning the corresponding Node is empty. Set to 1 means the
		//   corresponding node contains data. Set to 2 means the corresponding Node is filled, but it
		//   actually belongs to the previous position and was pushed out because that place is already
		//   taken.
		//
		// * infoSentinel: Sentinel byte set to 1, so that iterator's ++ can stop at end() without the
		//   need for a idx variable.
		//
		// According to STL, order of templates has effect on throughput. That's why I've moved the
		// boolean to the front.
		// https://www.reddit.com/r/cpp/comments/ahp6iu/compile_time_binary_size_reductions_and_cs_future/eeguck4/
		template <bool IsFlat, size_t MaxLoadFactor100, typename Key, typename T, typename Hash, typename KeyEqual>
		class Table:
				public WrapHash<Hash>,
				public WrapKeyEqual<KeyEqual>,
				detail::NodeAllocator<
						typename std::conditional<
								std::is_void<T>::value, Key,
								robin_hood::pair<typename std::conditional<IsFlat, Key, Key const>::type, T>>::type,
						4, 16384, IsFlat> {
		public:
			static constexpr bool is_flat = IsFlat;
			static constexpr bool is_map = !std::is_void<T>::value;
			static constexpr bool is_set = !is_map;
			static constexpr bool is_transparent = has_is_transparent<Hash>::value && has_is_transparent<KeyEqual>::value;

			using key_type = Key;
			using mapped_type = T;
			using value_type = typename std::conditional<
					is_set, Key, robin_hood::pair<typename std::conditional<is_flat, Key, Key const>::type, T>>::type;
			using size_type = size_t;
			using hasher = Hash;
			using key_equal = KeyEqual;
			using Self = Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;

		private:
			static_assert(MaxLoadFactor100 > 10 && MaxLoadFactor100 < 100, "MaxLoadFactor100 needs to be >10 && < 100");

			using WHash = WrapHash<Hash>;
			using WKeyEqual = WrapKeyEqual<KeyEqual>;

			// configuration defaults

			// make sure we have 8 elements, needed to quickly rehash mInfo
			static constexpr size_t InitialNumElements = sizeof(uint64_t);
			static constexpr uint32_t InitialInfoNumBits = 5;
			static constexpr uint8_t InitialInfoInc = 1U << InitialInfoNumBits;
			static constexpr size_t InfoMask = InitialInfoInc - 1U;
			static constexpr uint8_t InitialInfoHashShift = 64U - InitialInfoNumBits;
			using DataPool = detail::NodeAllocator<value_type, 4, 16384, IsFlat>;

			// type needs to be wider than uint8_t.
			using InfoType = uint32_t;

			// DataNode ////////////////////////////////////////////////////////

			// Primary template for the data node. We have special implementations for small and big
			// objects. For large objects it is assumed that swap() is fairly slow, so we allocate these
			// on the heap so swap merely swaps a pointer.
			template <typename M, bool>
			class DataNode {};

			// Small: just allocate on the stack.
			template <typename M>
			class DataNode<M, true> final {
			public:
				template <typename... Args>
				explicit DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, Args&&... args) noexcept(
						noexcept(value_type(std::forward<Args>(args)...))):
						mData(std::forward<Args>(args)...) {}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, true>&& n) noexcept(
						std::is_nothrow_move_constructible<value_type>::value):
						mData(std::move(n.mData)) {}

				// doesn't do anything
				void destroy(M& ROBIN_HOOD_UNUSED(map) /*unused*/) noexcept {}
				void destroyDoNotDeallocate() noexcept {}

				value_type const* operator->() const noexcept {
					return &mData;
				}
				value_type* operator->() noexcept {
					return &mData;
				}

				const value_type& operator*() const noexcept {
					return mData;
				}

				value_type& operator*() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return mData;
				}

				template <typename MT = mapped_type>
				[[nodiscard]] typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData.second;
				}

				template <typename MT = mapped_type>
				[[nodiscard]] typename std::enable_if<is_set, MT const&>::type getSecond() const noexcept {
					return mData.second;
				}

				void
				swap(DataNode<M, true>& o) noexcept(noexcept(std::declval<value_type>().swap(std::declval<value_type>()))) {
					mData.swap(o.mData);
				}

			private:
				value_type mData;
			};

			// big object: allocate on heap.
			template <typename M>
			class DataNode<M, false> {
			public:
				template <typename... Args>
				explicit DataNode(M& map, Args&&... args): mData(map.allocate()) {
					::new (static_cast<void*>(mData)) value_type(std::forward<Args>(args)...);
				}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, false>&& n) noexcept: mData(std::move(n.mData)) {}

				void destroy(M& map) noexcept {
					// don't deallocate, just put it into list of datapool.
					mData->~value_type();
					map.deallocate(mData);
				}

				void destroyDoNotDeallocate() noexcept {
					mData->~value_type();
				}

				value_type const* operator->() const noexcept {
					return mData;
				}

				value_type* operator->() noexcept {
					return mData;
				}

				const value_type& operator*() const {
					return *mData;
				}

				value_type& operator*() {
					return *mData;
				}

				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return *mData;
				}

				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				[[nodiscard]] typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return *mData;
				}

				template <typename MT = mapped_type>
				[[nodiscard]] typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData->second;
				}

				template <typename MT = mapped_type>
				[[nodiscard]] typename std::enable_if<is_map, MT const&>::type getSecond() const noexcept {
					return mData->second;
				}

				void swap(DataNode<M, false>& o) noexcept {
					using std::swap;
					swap(mData, o.mData);
				}

			private:
				value_type* mData;
			};

			using Node = DataNode<Self, IsFlat>;

			// helpers for insertKeyPrepareEmptySpot: extract first entry (only const required)
			[[nodiscard]] key_type const& getFirstConst(Node const& n) const noexcept {
				return n.getFirst();
			}

			// in case we have void mapped_type, we are not using a pair, thus we just route k through.
			// No need to disable this because it's just not used if not applicable.
			[[nodiscard]] key_type const& getFirstConst(key_type const& k) const noexcept {
				return k;
			}

			// in case we have non-void mapped_type, we have a standard robin_hood::pair
			template <typename Q = mapped_type>
			[[nodiscard]] typename std::enable_if<!std::is_void<Q>::value, key_type const&>::type
			getFirstConst(value_type const& vt) const noexcept {
				return vt.first;
			}

			// Cloner //////////////////////////////////////////////////////////

			template <typename M, bool UseMemcpy>
			struct Cloner;

			// fast path: Just copy data, without allocating anything.
			template <typename M>
			struct Cloner<M, true> {
				void operator()(M const& source, M& target) const {
					auto const* const src = reinterpret_cast<char const*>(source.mKeyVals);
					auto* tgt = reinterpret_cast<char*>(target.mKeyVals);
					auto const numElementsWithBuffer = target.calcNumElementsWithBuffer(target.mMask + 1);
					memcpy(tgt, src, src + target.calcNumBytesTotal(numElementsWithBuffer));
				}
			};

			template <typename M>
			struct Cloner<M, false> {
				void operator()(M const& s, M& t) const {
					auto const numElementsWithBuffer = t.calcNumElementsWithBuffer(t.mMask + 1);
					memcpy(t.mInfo, s.mInfo, s.mInfo + t.calcNumBytesInfo(numElementsWithBuffer));

					for (size_t i = 0; i < numElementsWithBuffer; ++i) {
						if (t.mInfo[i]) {
							::new (static_cast<void*>(t.mKeyVals + i)) Node(t, *s.mKeyVals[i]);
						}
					}
				}
			};

			// Destroyer ///////////////////////////////////////////////////////

			template <typename M, bool IsFlatAndTrivial>
			struct Destroyer {};

			template <typename M>
			struct Destroyer<M, true> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
				}
			};

			template <typename M>
			struct Destroyer<M, false> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);

					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroy(m);
							n.~Node();
						}
					}
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);
					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroyDoNotDeallocate();
							n.~Node();
						}
					}
				}
			};

			// Iter ////////////////////////////////////////////////////////////

			struct fast_forward_tag {};

			// generic iterator for both const_iterator and iterator.
			template <bool IsConst>
			// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
			class Iter {
			private:
				using NodePtr = typename std::conditional<IsConst, Node const*, Node*>::type;

			public:
				using difference_type = std::ptrdiff_t;
				using value_type = typename Self::value_type;
				using reference = typename std::conditional<IsConst, value_type const&, value_type&>::type;
				using pointer = typename std::conditional<IsConst, value_type const*, value_type*>::type;
				using iterator_category = GAIA_UTIL::forward_iterator_tag;

				// default constructed iterator can be compared to itself, but WON'T return true when
				// compared to end().
				Iter() = default;

				// Rule of zero: nothing specified. The conversion constructor is only enabled for
				// iterator to const_iterator, so it doesn't accidentally work as a copy ctor.

				// Conversion constructor from iterator to const_iterator.
				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				// NOLINTNEXTLINE(hicpp-explicit-conversions)
				Iter(Iter<OtherIsConst> const& other) noexcept: mKeyVals(other.mKeyVals), mInfo(other.mInfo) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr) noexcept: mKeyVals(valPtr), mInfo(infoPtr) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr, fast_forward_tag ROBIN_HOOD_UNUSED(tag) /*unused*/) noexcept:
						mKeyVals(valPtr), mInfo(infoPtr) {
					fastForward();
				}

				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				Iter& operator=(Iter<OtherIsConst> const& other) noexcept {
					mKeyVals = other.mKeyVals;
					mInfo = other.mInfo;
					return *this;
				}

				// prefix increment. Undefined behavior if we are at end()!
				Iter& operator++() noexcept {
					mInfo++;
					mKeyVals++;
					fastForward();
					return *this;
				}

				Iter operator++(int) noexcept {
					Iter tmp = *this;
					++(*this);
					return tmp;
				}

				reference operator*() const {
					return **mKeyVals;
				}

				pointer operator->() const {
					return &**mKeyVals;
				}

				template <bool O>
				bool operator==(Iter<O> const& o) const noexcept {
					return mKeyVals == o.mKeyVals;
				}

				template <bool O>
				bool operator!=(Iter<O> const& o) const noexcept {
					return mKeyVals != o.mKeyVals;
				}

			private:
				// fast forward to the next non-free info byte
				// I've tried a few variants that don't depend on intrinsics, but unfortunately they are
				// quite a bit slower than this one. So I've reverted that change again. See map_benchmark.
				void fastForward() noexcept {
					size_t n = 0;
					while (0U == (n = detail::unaligned_load<size_t>(mInfo))) {
						GAIA_MSVC_WARNING_PUSH()
						GAIA_MSVC_WARNING_DISABLE(6305)
						mInfo += sizeof(size_t);
						mKeyVals += sizeof(size_t);
						GAIA_MSVC_WARNING_POP()
					}
#if defined(ROBIN_HOOD_DISABLE_INTRINSICS)
					// we know for certain that within the next 8 bytes we'll find a non-zero one.
					if (ROBIN_HOOD_UNLIKELY(0U == detail::unaligned_load<uint32_t>(mInfo))) {
						mInfo += 4;
						mKeyVals += 4;
					}
					if (ROBIN_HOOD_UNLIKELY(0U == detail::unaligned_load<uint16_t>(mInfo))) {
						mInfo += 2;
						mKeyVals += 2;
					}
					if (ROBIN_HOOD_UNLIKELY(0U == *mInfo)) {
						mInfo += 1;
						mKeyVals += 1;
					}
#else
	#if ROBIN_HOOD(LITTLE_ENDIAN)
					auto inc = ROBIN_HOOD_COUNT_TRAILING_ZEROES(n) / 8;
	#else
					auto inc = ROBIN_HOOD_COUNT_LEADING_ZEROES(n) / 8;
	#endif
					mInfo += inc;
					mKeyVals += inc;
#endif
				}

				friend class Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;
				NodePtr mKeyVals{nullptr};
				uint8_t const* mInfo{nullptr};
			};

			////////////////////////////////////////////////////////////////////

			// highly performance relevant code.
			// Lower bits are used for indexing into the array (2^n size)
			// The upper 1-5 bits need to be a reasonable good hash, to save comparisons.
			template <typename HashKey>
			void keyToIdx(HashKey&& key, size_t* idx, InfoType* info) const {
				auto h = static_cast<uint64_t>(WHash::operator()(key));

				// direct_hash_key is expected to a proper hash. No additional hash tricks are required
				if constexpr (!std::is_same_v<HashKey, gaia::utils::direct_hash_key>) {
					// In addition to whatever hash is used, add another mul & shift so we get better hashing.
					// This serves as a bad hash prevention, if the given data is
					// badly mixed.
					h *= mHashMultiplier;
					h ^= h >> 33U;
				}

				// the lower InitialInfoNumBits are reserved for info.
				*info = mInfoInc + static_cast<InfoType>(h >> mInfoHashShift);
				*idx = (static_cast<size_t>(h)) & mMask;
			}

			// forwards the index by one, wrapping around at the end
			void next(InfoType* info, size_t* idx) const noexcept {
				*idx = *idx + 1;
				*info += mInfoInc;
			}

			void nextWhileLess(InfoType* info, size_t* idx) const noexcept {
				// unrolling this by hand did not bring any speedups.
				while (*info < mInfo[*idx]) {
					next(info, idx);
				}
			}

			// Shift everything up by one element. Tries to move stuff around.
			void shiftUp(size_t startIdx, size_t const insertion_idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				auto idx = startIdx;
				::new (static_cast<void*>(mKeyVals + idx)) Node(std::move(mKeyVals[idx - 1]));
				while (--idx != insertion_idx) {
					mKeyVals[idx] = std::move(mKeyVals[idx - 1]);
				}

				idx = startIdx;
				while (idx != insertion_idx) {
					ROBIN_HOOD_COUNT(shiftUp)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx - 1] + mInfoInc);
					if (ROBIN_HOOD_UNLIKELY(mInfo[idx] + mInfoInc > 0xFF)) {
						mMaxNumElementsAllowed = 0;
					}
					--idx;
				}
			}

			void shiftDown(size_t idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				// until we find one that is either empty or has zero offset.
				// TODO(martinus) we don't need to move everything, just the last one for the same
				// bucket.
				mKeyVals[idx].destroy(*this);

				// until we find one that is either empty or has zero offset.
				while (mInfo[idx + 1] >= 2 * mInfoInc) {
					ROBIN_HOOD_COUNT(shiftDown)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx + 1] - mInfoInc);
					mKeyVals[idx] = std::move(mKeyVals[idx + 1]);
					++idx;
				}

				mInfo[idx] = 0;
				// don't destroy, we've moved it
				// mKeyVals[idx].destroy(*this);
				mKeyVals[idx].~Node();
			}

			// copy of find(), except that it returns iterator instead of const_iterator.
			template <typename Other>
			[[nodiscard]] size_t findIdx(Other const& key) const {
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				do {
					// unrolling this twice gives a bit of a speedup. More unrolling did not help.
					if (info == mInfo[idx] && ROBIN_HOOD_LIKELY(WKeyEqual::operator()(key, mKeyVals[idx].getFirst()))) {
						return idx;
					}
					next(&info, &idx);
					if (info == mInfo[idx] && ROBIN_HOOD_LIKELY(WKeyEqual::operator()(key, mKeyVals[idx].getFirst()))) {
						return idx;
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found!
				return mMask == 0 ? 0
													: static_cast<size_t>(
																GAIA_UTIL::distance(mKeyVals, reinterpret_cast_no_cast_align_warning<Node*>(mInfo)));
			}

			void cloneData(const Table& o) {
				Cloner<Table, IsFlat && ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(Node)>()(o, *this);
			}

			// inserts a keyval that is guaranteed to be new, e.g. when the hashmap is resized.
			// @return True on success, false if something went wrong
			void insert_move(Node&& keyval) {
				// we don't retry, fail if overflowing
				// don't need to check max num elements
				if (0 == mMaxNumElementsAllowed && !try_increase_info()) {
					throwOverflowError();
				}

				size_t idx{};
				InfoType info{};
				keyToIdx(keyval.getFirst(), &idx, &info);

				// skip forward. Use <= because we are certain that the element is not there.
				while (info <= mInfo[idx]) {
					idx = idx + 1;
					info += mInfoInc;
				}

				// key not found, so we are now exactly where we want to insert it.
				auto const insertion_idx = idx;
				auto const insertion_info = static_cast<uint8_t>(info);
				if (ROBIN_HOOD_UNLIKELY(insertion_info + mInfoInc > 0xFF)) {
					mMaxNumElementsAllowed = 0;
				}

				// find an empty spot
				while (0 != mInfo[idx]) {
					next(&info, &idx);
				}

				auto& l = mKeyVals[insertion_idx];
				if (idx == insertion_idx) {
					::new (static_cast<void*>(&l)) Node(std::move(keyval));
				} else {
					shiftUp(idx, insertion_idx);
					l = std::move(keyval);
				}

				// put at empty spot
				mInfo[insertion_idx] = insertion_info;

				++mNumElements;
			}

		public:
			using iterator = Iter<false>;
			using const_iterator = Iter<true>;

			Table() noexcept(noexcept(Hash()) && noexcept(KeyEqual())): WHash(), WKeyEqual() {
				ROBIN_HOOD_TRACE(this)
			}

			// Creates an empty hash map. Nothing is allocated yet, this happens at the first insert.
			// This tremendously speeds up ctor & dtor of a map that never receives an element. The
			// penalty is payed at the first insert, and not before. Lookup of this empty map works
			// because everybody points to DummyInfoByte::b. parameter bucket_count is dictated by the
			// standard, but we can ignore it.
			explicit Table(
					size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}) noexcept(noexcept(Hash(h)) && noexcept(KeyEqual(equal))):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this)
			}

			template <typename Iter>
			Table(
					Iter first, Iter last, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this)
				insert(first, last);
			}

			Table(
					std::initializer_list<value_type> initlist, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0,
					const Hash& h = Hash{}, const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this)
				insert(initlist.begin(), initlist.end());
			}

			Table(Table&& o) noexcept:
					WHash(std::move(static_cast<WHash&>(o))), WKeyEqual(std::move(static_cast<WKeyEqual&>(o))),
					DataPool(std::move(static_cast<DataPool&>(o))) {
				ROBIN_HOOD_TRACE(this)
				if (o.mMask) {
					mHashMultiplier = std::move(o.mHashMultiplier);
					mKeyVals = std::move(o.mKeyVals);
					mInfo = std::move(o.mInfo);
					mNumElements = std::move(o.mNumElements);
					mMask = std::move(o.mMask);
					mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
					mInfoInc = std::move(o.mInfoInc);
					mInfoHashShift = std::move(o.mInfoHashShift);
					// set other's mask to 0 so its destructor won't do anything
					o.init();
				}
			}

			Table& operator=(Table&& o) noexcept {
				ROBIN_HOOD_TRACE(this)
				if (&o != this) {
					if (o.mMask) {
						// only move stuff if the other map actually has some data
						destroy();
						mHashMultiplier = std::move(o.mHashMultiplier);
						mKeyVals = std::move(o.mKeyVals);
						mInfo = std::move(o.mInfo);
						mNumElements = std::move(o.mNumElements);
						mMask = std::move(o.mMask);
						mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
						mInfoInc = std::move(o.mInfoInc);
						mInfoHashShift = std::move(o.mInfoHashShift);
						WHash::operator=(std::move(static_cast<WHash&>(o)));
						WKeyEqual::operator=(std::move(static_cast<WKeyEqual&>(o)));
						DataPool::operator=(std::move(static_cast<DataPool&>(o)));

						o.init();

					} else {
						// nothing in the other map => just clear us.
						clear();
					}
				}
				return *this;
			}

			Table(const Table& o):
					WHash(static_cast<const WHash&>(o)), WKeyEqual(static_cast<const WKeyEqual&>(o)),
					DataPool(static_cast<const DataPool&>(o)) {
				ROBIN_HOOD_TRACE(this)
				if (!o.empty()) {
					// not empty: create an exact copy. it is also possible to just iterate through all
					// elements and insert them, but copying is probably faster.

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);

					ROBIN_HOOD_LOG("std::malloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mHashMultiplier = o.mHashMultiplier;
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(std::malloc(numBytesTotal)));
					// no need for calloc because clonData does memcpy
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					mNumElements = o.mNumElements;
					mMask = o.mMask;
					mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
					mInfoInc = o.mInfoInc;
					mInfoHashShift = o.mInfoHashShift;
					cloneData(o);
				}
			}

			// Creates a copy of the given map. Copy constructor of each entry is used.
			// Not sure why clang-tidy thinks this doesn't handle self assignment, it does
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			Table& operator=(Table const& o) {
				ROBIN_HOOD_TRACE(this)
				if (&o == this) {
					// prevent assigning of itself
					return *this;
				}

				// we keep using the old allocator and not assign the new one, because we want to keep
				// the memory available. when it is the same size.
				if (o.empty()) {
					if (0 == mMask) {
						// nothing to do, we are empty too
						return *this;
					}

					// not empty: destroy what we have there
					// clear also resets mInfo to 0, that's sometimes not necessary.
					destroy();
					init();
					WHash::operator=(static_cast<const WHash&>(o));
					WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
					DataPool::operator=(static_cast<DataPool const&>(o));

					return *this;
				}

				// clean up old stuff
				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				if (mMask != o.mMask) {
					// no luck: we don't have the same array size allocated, so we need to realloc.
					if (0 != mMask) {
						// only deallocate if we actually have data!
						ROBIN_HOOD_LOG("std::free")
						std::free(mKeyVals);
					}

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
					ROBIN_HOOD_LOG("std::malloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(std::malloc(numBytesTotal)));

					// no need for calloc here because cloneData performs a memcpy.
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					// sentinel is set in cloneData
				}
				WHash::operator=(static_cast<const WHash&>(o));
				WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
				DataPool::operator=(static_cast<DataPool const&>(o));
				mHashMultiplier = o.mHashMultiplier;
				mNumElements = o.mNumElements;
				mMask = o.mMask;
				mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
				mInfoInc = o.mInfoInc;
				mInfoHashShift = o.mInfoHashShift;
				cloneData(o);

				return *this;
			}

			// Swaps everything between the two maps.
			void swap(Table& o) {
				ROBIN_HOOD_TRACE(this)
				using std::swap;
				swap(o, *this);
			}

			// Clears all data, without resizing.
			void clear() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					// don't do anything! also important because we don't want to write to
					// DummyInfoByte::b, even though we would just write 0 to it.
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);
				// clear everything, then set the sentinel again
				uint8_t const z = 0;
				gaia::utils::fill(mInfo, mInfo + calcNumBytesInfo(numElementsWithBuffer), z);
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// Destroys the map and all it's contents.
			~Table() {
				ROBIN_HOOD_TRACE(this)
				destroy();
			}

			// Checks if both tables contain the same entries. Order is irrelevant.
			bool operator==(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				if (other.size() != size()) {
					return false;
				}
				for (auto const& otherEntry: other) {
					if (!has(otherEntry)) {
						return false;
					}
				}

				return true;
			}

			bool operator!=(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				return !operator==(other);
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](key_type&& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Iter>
			void insert(Iter first, Iter last) {
				for (; first != last; ++first) {
					// value_type ctor needed because this might be called with std::pair's
					insert(value_type(*first));
				}
			}

			void insert(std::initializer_list<value_type> ilist) {
				for (auto&& vt: ilist) {
					insert(std::move(vt));
				}
			}

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				Node n{*this, std::forward<Args>(args)...};
				auto idxAndState = insertKeyPrepareEmptySpot(getFirstConst(n));
				switch (idxAndState.second) {
					case InsertionState::key_found:
						n.destroy(*this);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(*this, std::move(n));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = std::move(n);
						break;

					case InsertionState::overflow_error:
						n.destroy(*this);
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename... Args>
			iterator emplace_hint(const_iterator position, Args&&... args) {
				(void)position;
				return emplace(std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
				return try_emplace_impl(key, std::forward<Args>(args)...);
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(key, std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...).first;
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(const key_type& key, Mapped&& obj) {
				return insertOrAssignImpl(key, std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(key_type&& key, Mapped&& obj) {
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, const key_type& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(key, std::forward<Mapped>(obj)).first;
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, key_type&& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj)).first;
			}

			std::pair<iterator, bool> insert(const value_type& keyval) {
				ROBIN_HOOD_TRACE(this)
				return emplace(keyval);
			}

			iterator insert(const_iterator hint, const value_type& keyval) {
				(void)hint;
				return emplace(keyval).first;
			}

			std::pair<iterator, bool> insert(value_type&& keyval) {
				return emplace(std::move(keyval));
			}

			iterator insert(const_iterator hint, value_type&& keyval) {
				(void)hint;
				return emplace(std::move(keyval)).first;
			}

			// Returns 1 if key is found, 0 otherwise.
			size_t count(const key_type& key) const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			template <typename OtherKey, typename Self_ = Self>
			// NOLINTNEXTLINE(modernize-use-nodiscard)
			typename std::enable_if<Self_::is_transparent, size_t>::type count(const OtherKey& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			bool contains(const key_type& key) const { // NOLINT(modernize-use-nodiscard)
				return 1U == count(key);
			}

			template <typename OtherKey, typename Self_ = Self>
			// NOLINTNEXTLINE(modernize-use-nodiscard)
			typename std::enable_if<Self_::is_transparent, bool>::type contains(const OtherKey& key) const {
				return 1U == count(key);
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			// NOLINTNEXTLINE(modernize-use-nodiscard)
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type at(key_type const& key) {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			// NOLINTNEXTLINE(modernize-use-nodiscard)
			typename std::enable_if<!std::is_void<Q>::value, Q const&>::type at(key_type const& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			const_iterator find(const key_type& key) const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			const_iterator find(const OtherKey& key, is_transparent_tag /*unused*/) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			typename std::enable_if<
					Self_::is_transparent, // NOLINT(modernize-use-nodiscard)
					const_iterator>::type // NOLINT(modernize-use-nodiscard)
			find(const OtherKey& key) const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			iterator find(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			iterator find(const OtherKey& key, is_transparent_tag /*unused*/) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			typename std::enable_if<Self_::is_transparent, iterator>::type find(const OtherKey& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			iterator begin() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return end();
				}
				return iterator(mKeyVals, mInfo, fast_forward_tag{});
			}
			const_iterator begin() const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return cbegin();
			}
			const_iterator cbegin() const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return cend();
				}
				return const_iterator(mKeyVals, mInfo, fast_forward_tag{});
			}

			iterator end() {
				ROBIN_HOOD_TRACE(this)
				// no need to supply valid info pointer: end() must not be dereferenced, and only node
				// pointer is compared.
				return iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}
			const_iterator end() const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return cend();
			}
			const_iterator cend() const { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return const_iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}

			iterator erase(const_iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// its safe to perform const cast here
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				return erase(iterator{const_cast<Node*>(pos.mKeyVals), const_cast<uint8_t*>(pos.mInfo)});
			}

			// Erases element at pos, returns iterator to the next element.
			iterator erase(iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// we assume that pos always points to a valid entry, and not end().
				auto const idx = static_cast<size_t>(pos.mKeyVals - mKeyVals);

				shiftDown(idx);
				--mNumElements;

				if (*pos.mInfo) {
					// we've backward shifted, return this again
					return pos;
				}

				// no backward shift, return next element
				return ++pos;
			}

			size_t erase(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				// check while info matches with the source idx
				do {
					if (info == mInfo[idx] && WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
						shiftDown(idx);
						--mNumElements;
						return 1;
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found to delete
				return 0;
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// exactly the same as reserve(c).
			void rehash(size_t c) {
				// forces a reserve
				reserve(c, true);
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// Exactly the same as rehash(c). Use rehash(0) to shrink to fit.
			void reserve(size_t c) {
				// reserve, but don't force rehash
				reserve(c, false);
			}

			// If possible reallocates the map to a smaller one. This frees the underlying table.
			// Does not do anything if load_factor is too large for decreasing the table's size.
			void compact() {
				ROBIN_HOOD_TRACE(this)
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < mNumElements && newSize != 0) {
					newSize *= 2;
				}
				if (ROBIN_HOOD_UNLIKELY(newSize == 0)) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (newSize < mMask + 1) {
					rehashPowerOfTwo(newSize, true);
				}
			}

			size_type size() const noexcept { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return mNumElements;
			}

			size_type max_size() const noexcept { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return static_cast<size_type>(-1);
			}

			[[nodiscard]] bool empty() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return 0 == mNumElements;
			}

			float max_load_factor() const noexcept { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return MaxLoadFactor100 / 100.0F;
			}

			// Average number of elements per bucket. Since we allow only 1 per bucket
			float load_factor() const noexcept { // NOLINT(modernize-use-nodiscard)
				ROBIN_HOOD_TRACE(this)
				return static_cast<float>(size()) / static_cast<float>(mMask + 1);
			}

			[[nodiscard]] size_t mask() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return mMask;
			}

			[[nodiscard]] size_t calcMaxNumElementsAllowed(size_t maxElements) const noexcept {
				if (ROBIN_HOOD_LIKELY(maxElements <= (size_t(-1) / 100))) {
					return maxElements * MaxLoadFactor100 / 100;
				}

				// we might be a bit inprecise, but since maxElements is quite large that doesn't matter
				return (maxElements / 100) * MaxLoadFactor100;
			}

			[[nodiscard]] size_t calcNumBytesInfo(size_t numElements) const noexcept {
				// we add a uint64_t, which houses the sentinel (first byte) and padding so we can load
				// 64bit types.
				return numElements + sizeof(uint64_t);
			}

			[[nodiscard]] size_t calcNumElementsWithBuffer(size_t numElements) const noexcept {
				auto maxNumElementsAllowed = calcMaxNumElementsAllowed(numElements);
				return numElements + gaia::utils::get_min(maxNumElementsAllowed, (static_cast<size_t>(0xFF)));
			}

			// calculation only allowed for 2^n values
			[[nodiscard]] size_t calcNumBytesTotal(size_t numElements) const {
#if ROBIN_HOOD(BITNESS) == 64
				return numElements * sizeof(Node) + calcNumBytesInfo(numElements);
#else
				// make sure we're doing 64bit operations, so we are at least safe against 32bit overflows.
				auto const ne = static_cast<uint64_t>(numElements);
				auto const s = static_cast<uint64_t>(sizeof(Node));
				auto const infos = static_cast<uint64_t>(calcNumBytesInfo(numElements));

				auto const total64 = ne * s + infos;
				auto const total = static_cast<size_t>(total64);

				if (ROBIN_HOOD_UNLIKELY(static_cast<uint64_t>(total) != total64)) {
					throwOverflowError();
				}
				return total;
#endif
			}

		private:
			template <typename Q = mapped_type>
			[[nodiscard]] typename std::enable_if<!std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				auto it = find(e.first);
				return it != end() && it->second == e.second;
			}

			template <typename Q = mapped_type>
			[[nodiscard]] typename std::enable_if<std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				return find(e) != end();
			}

			void reserve(size_t c, bool forceRehash) {
				ROBIN_HOOD_TRACE(this)
				auto const minElementsAllowed = gaia::utils::get_max(c, mNumElements);
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < minElementsAllowed && newSize != 0) {
					newSize *= 2;
				}
				if (ROBIN_HOOD_UNLIKELY(newSize == 0)) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (forceRehash || newSize > mMask + 1) {
					rehashPowerOfTwo(newSize, false);
				}
			}

			// reserves space for at least the specified number of elements.
			// only works if numBuckets if power of two
			// True on success, false otherwise
			void rehashPowerOfTwo(size_t numBuckets, bool forceFree) {
				ROBIN_HOOD_TRACE(this)

				Node* const oldKeyVals = mKeyVals;
				uint8_t const* const oldInfo = mInfo;

				const size_t oldMaxElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				// resize operation: move stuff
				initData(numBuckets);
				if (oldMaxElementsWithBuffer > 1) {
					for (size_t i = 0; i < oldMaxElementsWithBuffer; ++i) {
						if (oldInfo[i] != 0) {
							// might throw an exception, which is really bad since we are in the middle of
							// moving stuff.
							insert_move(std::move(oldKeyVals[i]));
							// destroy the node but DON'T destroy the data.
							oldKeyVals[i].~Node();
						}
					}

					// this check is not necessary as it's guarded by the previous if, but it helps
					// silence g++'s overeager "attempt to free a non-heap object 'map'
					// [-Werror=free-nonheap-object]" warning.
					if (oldKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
						// don't destroy old data: put it into the pool instead
						if (forceFree) {
							std::free(oldKeyVals);
						} else {
							DataPool::addOrFree(oldKeyVals, calcNumBytesTotal(oldMaxElementsWithBuffer));
						}
					}
				}
			}

			ROBIN_HOOD(NOINLINE) void throwOverflowError() const {
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				throw std::overflow_error("robin_hood::map overflow");
#else
				abort();
#endif
			}

			template <typename OtherKey, typename... Args>
			std::pair<iterator, bool> try_emplace_impl(OtherKey&& key, Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename OtherKey, typename Mapped>
			std::pair<iterator, bool> insertOrAssignImpl(OtherKey&& key, Mapped&& obj) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						mKeyVals[idxAndState.first].getSecond() = std::forward<Mapped>(obj);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			void initData(size_t max_elements) {
				mNumElements = 0;
				mMask = max_elements - 1;
				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(max_elements);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(max_elements);

				// malloc & zero mInfo. Faster than calloc everything.
				auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
				ROBIN_HOOD_LOG("std::calloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
				mKeyVals = reinterpret_cast<Node*>(detail::assertNotNull<std::bad_alloc>(std::malloc(numBytesTotal)));
				mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
				std::memset(mInfo, 0, numBytesTotal - numElementsWithBuffer * sizeof(Node));

				// set sentinel
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			enum class InsertionState { overflow_error, key_found, new_node, overwrite_node };

			// Finds key, and if not already present prepares a spot where to pot the key & value.
			// This potentially shifts nodes out of the way, updates mInfo and number of inserted
			// elements, so the only operation left to do is create/assign a new node at that spot.
			template <typename OtherKey>
			std::pair<size_t, InsertionState> insertKeyPrepareEmptySpot(OtherKey&& key) {
				for (int i = 0; i < 256; ++i) {
					size_t idx{};
					InfoType info{};
					keyToIdx(key, &idx, &info);
					nextWhileLess(&info, &idx);

					// while we potentially have a match
					while (info == mInfo[idx]) {
						if (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							// key already exists, do NOT insert.
							// see http://en.cppreference.com/w/cpp/container/unordered_map/insert
							return std::make_pair(idx, InsertionState::key_found);
						}
						next(&info, &idx);
					}

					// unlikely that this evaluates to true
					if (ROBIN_HOOD_UNLIKELY(mNumElements >= mMaxNumElementsAllowed)) {
						if (!increase_size()) {
							return std::make_pair(size_t(0), InsertionState::overflow_error);
						}
						continue;
					}

					// key not found, so we are now exactly where we want to insert it.
					auto const insertion_idx = idx;
					auto const insertion_info = info;
					if (ROBIN_HOOD_UNLIKELY(insertion_info + mInfoInc > 0xFF)) {
						mMaxNumElementsAllowed = 0;
					}

					// find an empty spot
					while (0 != mInfo[idx]) {
						next(&info, &idx);
					}

					if (idx != insertion_idx) {
						shiftUp(idx, insertion_idx);
					}
					// put at empty spot
					mInfo[insertion_idx] = static_cast<uint8_t>(insertion_info);
					++mNumElements;
					return std::make_pair(
							insertion_idx, idx == insertion_idx ? InsertionState::new_node : InsertionState::overwrite_node);
				}

				// enough attempts failed, so finally give up.
				return std::make_pair(size_t(0), InsertionState::overflow_error);
			}

			bool try_increase_info() {
				ROBIN_HOOD_LOG(
						"mInfoInc=" << mInfoInc << ", numElements=" << mNumElements
												<< ", maxNumElementsAllowed=" << calcMaxNumElementsAllowed(mMask + 1))
				if (mInfoInc <= 2) {
					// need to be > 2 so that shift works (otherwise undefined behavior!)
					return false;
				}
				// we got space left, try to make info smaller
				mInfoInc = static_cast<uint8_t>(mInfoInc >> 1U);

				// remove one bit of the hash, leaving more space for the distance info.
				// This is extremely fast because we can operate on 8 bytes at once.
				++mInfoHashShift;
				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				for (size_t i = 0; i < numElementsWithBuffer; i += 8) {
					auto val = unaligned_load<uint64_t>(mInfo + i);
					val = (val >> 1U) & UINT64_C(0x7f7f7f7f7f7f7f7f);
					std::memcpy(mInfo + i, &val, sizeof(val));
				}
				// update sentinel, which might have been cleared out!
				mInfo[numElementsWithBuffer] = 1;

				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				return true;
			}

			// True if resize was possible, false otherwise
			bool increase_size() {
				// nothing allocated yet? just allocate InitialNumElements
				if (0 == mMask) {
					initData(InitialNumElements);
					return true;
				}

				auto const maxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				if (mNumElements < maxNumElementsAllowed && try_increase_info()) {
					return true;
				}

				ROBIN_HOOD_LOG(
						"mNumElements=" << mNumElements << ", maxNumElementsAllowed=" << maxNumElementsAllowed << ", load="
														<< (static_cast<double>(mNumElements) * 100.0 / (static_cast<double>(mMask) + 1)))

				if (mNumElements * 2 < calcMaxNumElementsAllowed(mMask + 1)) {
					// we have to resize, even though there would still be plenty of space left!
					// Try to rehash instead. Delete freed memory so we don't steadyily increase mem in case
					// we have to rehash a few times
					nextHashMultiplier();
					rehashPowerOfTwo(mMask + 1, true);
				} else {
					// we've reached the capacity of the map, so the hash seems to work nice. Keep using it.
					rehashPowerOfTwo((mMask + 1) * 2, false);
				}
				return true;
			}

			void nextHashMultiplier() {
				// adding an *even* number, so that the multiplier will always stay odd. This is necessary
				// so that the hash stays a mixing function (and thus doesn't have any information loss).
				mHashMultiplier += UINT64_C(0xc4ceb9fe1a85ec54);
			}

			void destroy() {
				if (0 == mMask) {
					// don't deallocate!
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodesDoNotDeallocate(*this);

				// This protection against not deleting mMask shouldn't be needed as it's sufficiently
				// protected with the 0==mMask check, but I have this anyways because g++ 7 otherwise
				// reports a compile error: attempt to free a non-heap object 'fm'
				// [-Werror=free-nonheap-object]
				if (mKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
					ROBIN_HOOD_LOG("std::free")
					std::free(mKeyVals);
				}
			}

			void init() noexcept {
				mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask);
				mInfo = reinterpret_cast<uint8_t*>(&mMask);
				mNumElements = 0;
				mMask = 0;
				mMaxNumElementsAllowed = 0;
				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// members are sorted so no padding occurs
			uint64_t mHashMultiplier = UINT64_C(0xc4ceb9fe1a85ec53); // 8 byte  8
			Node* mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask); // 8 byte 16
			uint8_t* mInfo = reinterpret_cast<uint8_t*>(&mMask); // 8 byte 24
			size_t mNumElements = 0; // 8 byte 32
			size_t mMask = 0; // 8 byte 40
			size_t mMaxNumElementsAllowed = 0; // 8 byte 48
			InfoType mInfoInc = InitialInfoInc; // 4 byte 52
			InfoType mInfoHashShift = InitialInfoHashShift; // 4 byte 56
																											// 16 byte 56 if NodeAllocator
		};

	} // namespace detail

	// map

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_map = detail::Table<true, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_map = detail::Table<false, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_map = detail::Table<
			sizeof(robin_hood::pair<Key, T>) <= sizeof(size_t) * 6 &&
					std::is_nothrow_move_constructible<robin_hood::pair<Key, T>>::value &&
					std::is_nothrow_move_assignable<robin_hood::pair<Key, T>>::value,
			MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	// set

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>, size_t MaxLoadFactor100 = 80>
	using unordered_flat_set = detail::Table<true, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>, size_t MaxLoadFactor100 = 80>
	using unordered_node_set = detail::Table<false, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>, size_t MaxLoadFactor100 = 80>
	using unordered_set = detail::Table<
			sizeof(Key) <= sizeof(size_t) * 6 && std::is_nothrow_move_constructible<Key>::value &&
					std::is_nothrow_move_assignable<Key>::value,
			MaxLoadFactor100, Key, void, Hash, KeyEqual>;

} // namespace robin_hood

#endif

namespace gaia {
	namespace containers {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_HASHMAP

#endif

#include <cinttypes>

namespace gaia {
	namespace ecs {
		//! Number of ticks before empty chunks are removed
		constexpr uint32_t MAX_CHUNK_LIFESPAN = 8u;
		//! Number of ticks before empty archetypes are removed
		constexpr uint32_t MAX_ARCHETYPE_LIFESPAN = 8u;
		//! Maximum number of components on archetype
		constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 32u;

		[[nodiscard]] constexpr bool VerityArchetypeComponentCount(uint32_t count) {
			return count <= MAX_COMPONENTS_PER_ARCHETYPE;
		}

		[[nodiscard]] inline bool DidVersionChange(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered changed.
			if (requiredVersion == 0)
				return true;

			// Supporting wrap-around for version numbers. ChangeVersion must be
			// bigger than requiredVersion (never detect change of something the
			// system itself changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		inline void UpdateVersion(uint32_t& version) {
			++version;
			// Handle wrap-around, 0 is reserved for systems that have never run.
			if (version == 0)
				++version;
		}
	} // namespace ecs
} // namespace gaia

#include <tuple>
#include <type_traits>
#include <utility>

#if __cpp_lib_span
	#include <span>
#else
	// Workaround for pre-C++20 compilers <span>
	
/*
This is an implementation of C++20's std::span
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/n4820.pdf
*/

//          Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef TCB_SPAN_HPP_INCLUDED
#define TCB_SPAN_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifndef TCB_SPAN_NO_EXCEPTIONS
	// Attempt to discover whether we're being compiled with exception support
	#if !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
		#define TCB_SPAN_NO_EXCEPTIONS
	#endif
#endif

#ifndef TCB_SPAN_NO_EXCEPTIONS
	#include <cstdio>
	#include <stdexcept>
#endif

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
	#define TCB_SPAN_NAMESPACE_NAME tcb
#endif

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
	#define TCB_SPAN_HAVE_CPP17
#endif

#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
	#define TCB_SPAN_HAVE_CPP14
#endif

namespace TCB_SPAN_NAMESPACE_NAME {

// Establish default contract checking behavior
#if !defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION) && !defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION) &&            \
		!defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#if defined(NDEBUG) || !defined(TCB_SPAN_HAVE_CPP14)
		#define TCB_SPAN_NO_CONTRACT_CHECKING
	#else
		#define TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION
	#endif
#endif

#if defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION)
	struct contract_violation_error: std::logic_error {
		explicit contract_violation_error(const char* msg): std::logic_error(msg) {}
	};

	inline void contract_violation(const char* msg) {
		throw contract_violation_error(msg);
	}

#elif defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION)
	[[noreturn]] inline void contract_violation(const char* /*unused*/) {
		std::terminate();
	}
#endif

#if !defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#define TCB_SPAN_STRINGIFY(cond) #cond
	#define TCB_SPAN_EXPECT(cond) cond ? (void)0 : contract_violation("Expected " TCB_SPAN_STRINGIFY(cond))
#else
	#define TCB_SPAN_EXPECT(cond)
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_inline_variables)
	#define TCB_SPAN_INLINE_VAR inline
#else
	#define TCB_SPAN_INLINE_VAR
#endif

#if defined(TCB_SPAN_HAVE_CPP14) || (defined(__cpp_constexpr) && __cpp_constexpr >= 201304)
	#define TCB_SPAN_HAVE_CPP14_CONSTEXPR
#endif

#if defined(TCB_SPAN_HAVE_CPP14_CONSTEXPR)
	#define TCB_SPAN_CONSTEXPR14 constexpr
#else
	#define TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP14_CONSTEXPR) && (!defined(_MSC_VER) || _MSC_VER > 1900)
	#define TCB_SPAN_CONSTEXPR_ASSIGN constexpr
#else
	#define TCB_SPAN_CONSTEXPR_ASSIGN
#endif

#if defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#define TCB_SPAN_CONSTEXPR11 constexpr
#else
	#define TCB_SPAN_CONSTEXPR11 TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_deduction_guides)
	#define TCB_SPAN_HAVE_DEDUCTION_GUIDES
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_byte)
	#define TCB_SPAN_HAVE_STD_BYTE
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_array_constexpr)
	#define TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC
#endif

#if defined(TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC)
	#define TCB_SPAN_ARRAY_CONSTEXPR constexpr
#else
	#define TCB_SPAN_ARRAY_CONSTEXPR
#endif

#ifdef TCB_SPAN_HAVE_STD_BYTE
	using byte = std::byte;
#else
	using byte = unsigned char;
#endif

#if defined(TCB_SPAN_HAVE_CPP17)
	#define TCB_SPAN_NODISCARD [[nodiscard]]
#else
	#define TCB_SPAN_NODISCARD
#endif

	TCB_SPAN_INLINE_VAR constexpr std::size_t dynamic_extent = SIZE_MAX;

	template <typename ElementType, std::size_t Extent = dynamic_extent>
	class span;

	namespace detail {

		template <typename E, std::size_t S>
		struct span_storage {
			constexpr span_storage() noexcept = default;

			constexpr span_storage(E* p_ptr, std::size_t /*unused*/) noexcept: ptr(p_ptr) {}

			E* ptr = nullptr;
			static constexpr std::size_t size = S;
		};

		template <typename E>
		struct span_storage<E, dynamic_extent> {
			constexpr span_storage() noexcept = default;

			constexpr span_storage(E* p_ptr, std::size_t p_size) noexcept: ptr(p_ptr), size(p_size) {}

			E* ptr = nullptr;
			std::size_t size = 0;
		};

// Reimplementation of C++17 std::size() and std::data()
#if 0 // defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_nonmember_container_access)
		using std::data;
		using std::size;
#else
		template <typename C>
		constexpr auto size(const C& c) -> decltype(c.size()) {
			return c.size();
		}

		template <typename T, std::size_t N>
		constexpr std::size_t size(const T (&)[N]) noexcept {
			return N;
		}

		template <typename C>
		constexpr auto data(C& c) -> decltype(c.data()) {
			return c.data();
		}

		template <typename C>
		constexpr auto data(const C& c) -> decltype(c.data()) {
			return c.data();
		}

		template <typename T, std::size_t N>
		constexpr T* data(T (&array)[N]) noexcept {
			return array;
		}

		template <typename E>
		constexpr const E* data(std::initializer_list<E> il) noexcept {
			return il.begin();
		}
#endif // TCB_SPAN_HAVE_CPP17

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_void_t)
		using std::void_t;
#else
		template <typename...>
		using void_t = void;
#endif

		template <typename T>
		using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

		template <typename>
		struct is_span: std::false_type {};

		template <typename T, std::size_t S>
		struct is_span<span<T, S>>: std::true_type {};

		template <typename>
		struct is_std_array: std::false_type {};

		template <typename T, auto N>
		struct is_std_array<gaia::containers::sarray<T, N>>: std::true_type {};

		template <typename, typename = void>
		struct has_size_and_data: std::false_type {};

		template <typename T>
		struct has_size_and_data<
				T, void_t<decltype(detail::size(std::declval<T>())), decltype(detail::data(std::declval<T>()))>>:
				std::true_type {};

		template <typename C, typename U = uncvref_t<C>>
		struct is_container {
			static constexpr bool value =
					!is_span<U>::value && !is_std_array<U>::value && !std::is_array<U>::value && has_size_and_data<C>::value;
		};

		template <typename T>
		using remove_pointer_t = typename std::remove_pointer<T>::type;

		template <typename, typename, typename = void>
		struct is_container_element_type_compatible: std::false_type {};

		template <typename T, typename E>
		struct is_container_element_type_compatible<
				T, E,
				typename std::enable_if<!std::is_same<
						typename std::remove_cv<decltype(detail::data(std::declval<T>()))>::type, void>::value>::type>:
				std::is_convertible<remove_pointer_t<decltype(detail::data(std::declval<T>()))> (*)[], E (*)[]> {};

		template <typename, typename = size_t>
		struct is_complete: std::false_type {};

		template <typename T>
		struct is_complete<T, decltype(sizeof(T))>: std::true_type {};

	} // namespace detail

	template <typename ElementType, std::size_t Extent>
	class span {
		static_assert(
				std::is_object<ElementType>::value, "A span's ElementType must be an object type (not a "
																						"reference type or void)");
		static_assert(
				detail::is_complete<ElementType>::value, "A span's ElementType must be a complete type (not a forward "
																								 "declaration)");
		static_assert(!std::is_abstract<ElementType>::value, "A span's ElementType cannot be an abstract class type");

		using storage_type = detail::span_storage<ElementType, Extent>;

	public:
		// constants and types
		using element_type = ElementType;
		using value_type = typename std::remove_cv<ElementType>::type;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = element_type*;
		using const_pointer = const element_type*;
		using reference = element_type&;
		using const_reference = const element_type&;
		using iterator = pointer;
		// using reverse_iterator = std::reverse_iterator<iterator>;

		static constexpr size_type extent = Extent;

		// [span.cons], span constructors, copy, assignment, and destructor
		template <std::size_t E = Extent, typename std::enable_if<(E == dynamic_extent || E <= 0), int>::type = 0>
		constexpr span() noexcept {}

		TCB_SPAN_CONSTEXPR11 span(pointer ptr, size_type count): storage_(ptr, count) {
			TCB_SPAN_EXPECT(extent == dynamic_extent || count == extent);
		}

		TCB_SPAN_CONSTEXPR11 span(pointer first_elem, pointer last_elem): storage_(first_elem, last_elem - first_elem) {
			TCB_SPAN_EXPECT(extent == dynamic_extent || last_elem - first_elem == static_cast<std::ptrdiff_t>(extent));
		}

		template <
				std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) &&
								detail::is_container_element_type_compatible<element_type (&)[N], ElementType>::value,
						int>::type = 0>
		constexpr span(element_type (&arr)[N]) noexcept: storage_(arr, N) {}

		template <
				std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) && detail::is_container_element_type_compatible<
																									 gaia::containers::sarray<value_type, N>&, ElementType>::value,
						int>::type = 0>
		TCB_SPAN_ARRAY_CONSTEXPR span(gaia::containers::sarray<value_type, N>& arr) noexcept: storage_(arr.data(), N) {}

		template <
				std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) && detail::is_container_element_type_compatible<
																									 const gaia::containers::sarray<value_type, N>&, ElementType>::value,
						int>::type = 0>
		TCB_SPAN_ARRAY_CONSTEXPR span(const gaia::containers::sarray<value_type, N>& arr) noexcept:
				storage_(arr.data(), N) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_type_compatible<Container&, ElementType>::value,
						int>::type = 0>
		constexpr span(Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_type_compatible<const Container&, ElementType>::value,
						int>::type = 0>
		constexpr span(const Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		constexpr span(const span& other) noexcept = default;

		template <
				typename OtherElementType, std::size_t OtherExtent,
				typename std::enable_if<
						(Extent == OtherExtent || Extent == dynamic_extent) &&
								std::is_convertible<OtherElementType (*)[], ElementType (*)[]>::value,
						int>::type = 0>
		constexpr span(const span<OtherElementType, OtherExtent>& other) noexcept: storage_(other.data(), other.size()) {}

		~span() noexcept = default;

		TCB_SPAN_CONSTEXPR_ASSIGN span& operator=(const span& other) noexcept = default;

		// [span.sub], span subviews
		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_type, Count> first() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data(), Count};
		}

		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_type, Count> last() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data() + (size() - Count), Count};
		}

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		using subspan_return_t = span<
				ElementType, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		TCB_SPAN_CONSTEXPR11 subspan_return_t<Offset, Count> subspan() const {
			TCB_SPAN_EXPECT(Offset <= size() && (Count == dynamic_extent || Offset + Count <= size()));
			return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent> first(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data(), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent> last(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data() + (size() - count), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent>
		subspan(size_type offset, size_type count = dynamic_extent) const {
			TCB_SPAN_EXPECT(offset <= size() && (count == dynamic_extent || offset + count <= size()));
			return {data() + offset, count == dynamic_extent ? size() - offset : count};
		}

		// [span.obs], span observers
		constexpr size_type size() const noexcept {
			return storage_.size;
		}

		constexpr size_type size_bytes() const noexcept {
			return size() * sizeof(element_type);
		}

		TCB_SPAN_NODISCARD constexpr bool empty() const noexcept {
			return size() == 0;
		}

		// [span.elem], span element access
		TCB_SPAN_CONSTEXPR11 reference operator[](size_type idx) const {
			TCB_SPAN_EXPECT(idx < size());
			return *(data() + idx);
		}

		TCB_SPAN_CONSTEXPR11 reference front() const {
			TCB_SPAN_EXPECT(!empty());
			return *data();
		}

		TCB_SPAN_CONSTEXPR11 reference back() const {
			TCB_SPAN_EXPECT(!empty());
			return *(data() + (size() - 1));
		}

		constexpr pointer data() const noexcept {
			return storage_.ptr;
		}

		// [span.iterators], span iterator support
		constexpr iterator begin() const noexcept {
			return data();
		}

		constexpr iterator end() const noexcept {
			return data() + size();
		}

		// TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rbegin() const noexcept {
		// 	return reverse_iterator(end());
		// }

		// TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rend() const noexcept {
		// 	return reverse_iterator(begin());
		// }

	private:
		storage_type storage_{};
	};

#ifdef TCB_SPAN_HAVE_DEDUCTION_GUIDES

	/* Deduction Guides */
	template <typename T, size_t N>
	span(T (&)[N]) -> span<T, N>;

	template <typename T, size_t N>
	span(gaia::containers::sarray<T, N>&) -> span<T, N>;

	template <typename T, size_t N>
	span(const gaia::containers::sarray<T, N>&) -> span<const T, N>;

	template <typename Container>
	span(Container&) -> span<typename Container::value_type>;

	template <typename Container>
	span(const Container&) -> span<const typename Container::value_type>;

#endif // TCB_HAVE_DEDUCTION_GUIDES

	template <typename ElementType, std::size_t Extent>
	constexpr span<ElementType, Extent> make_span(span<ElementType, Extent> s) noexcept {
		return s;
	}

	template <typename T, std::size_t N>
	constexpr span<T, N> make_span(T (&arr)[N]) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<T, N> make_span(gaia::containers::sarray<T, N>& arr) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<const T, N> make_span(const gaia::containers::sarray<T, N>& arr) noexcept {
		return {arr};
	}

	template <typename Container>
	constexpr span<typename Container::value_type> make_span(Container& cont) {
		return {cont};
	}

	template <typename Container>
	constexpr span<const typename Container::value_type> make_span(const Container& cont) {
		return {cont};
	}

	template <typename ElementType, std::size_t Extent>
	span<const byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
	as_bytes(span<ElementType, Extent> s) noexcept {
		return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
	}

	template <
			class ElementType, size_t Extent, typename std::enable_if<!std::is_const<ElementType>::value, int>::type = 0>
	span<byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
	as_writable_bytes(span<ElementType, Extent> s) noexcept {
		return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
	}

	template <std::size_t N, typename E, std::size_t S>
	constexpr auto get(span<E, S> s) -> decltype(s[N]) {
		return s[N];
	}

} // namespace TCB_SPAN_NAMESPACE_NAME

namespace std {

	template <typename ElementType, size_t Extent>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementType, Extent>>: public integral_constant<size_t, Extent> {};

	template <typename ElementType>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementType, TCB_SPAN_NAMESPACE_NAME::dynamic_extent>>; // not defined

	template <size_t I, typename ElementType, size_t Extent>
	struct tuple_element<I, TCB_SPAN_NAMESPACE_NAME::span<ElementType, Extent>> {
		static_assert(Extent != TCB_SPAN_NAMESPACE_NAME::dynamic_extent && I < Extent, "");
		using type = ElementType;
	};

} // end namespace std

#endif // TCB_SPAN_HPP_INCLUDED

namespace std {
	using tcb::span;
}
#endif

#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace utils {

		//----------------------------------------------------------------------
		// Tuple to struct conversion
		//----------------------------------------------------------------------

		template <typename S, size_t... Is, typename Tuple>
		S tuple_to_struct(std::index_sequence<Is...>, Tuple&& tup) {
			return {std::get<Is>(std::forward<Tuple>(tup))...};
		}

		template <typename S, typename Tuple>
		S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(std::make_index_sequence<std::tuple_size<T>{}>{}, std::forward<Tuple>(tup));
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		// Check if type T is constructible via T{Args...}
		struct any_type {
			template <typename T>
			constexpr operator T(); // non explicit
		};

		template <typename T, typename... TArgs>
		decltype(void(T{std::declval<TArgs>()...}), std::true_type{}) is_braces_constructible(int);

		template <typename, typename...>
		std::false_type is_braces_constructible(...);

		template <typename T, typename... TArgs>
		using is_braces_constructible_t = decltype(is_braces_constructible<T, TArgs...>(0));

		//! Converts a struct to a tuple (struct must support initialization via:
		//! Struct{x,y,...,z})
		template <typename T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = std::decay_t<T>;
			// Don't support empty structs. They have no data.
			// We also want to fail for structs with too many members because it smells with bad usage.
			// Therefore, only 1 to 8 types are supported at the moment.
			if constexpr (is_braces_constructible_t<
												type, any_type, any_type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type>{}) {
				auto&& [p1, p2] = object;
				return std::make_tuple(p1, p2);
			} else if constexpr (is_braces_constructible_t<type, any_type>{}) {
				auto&& [p1] = object;
				return std::make_tuple(p1);
			} else {
				return std::make_tuple();
			}
		}

	} // namespace utils
} // namespace gaia

namespace gaia {
	namespace utils {
		enum class DataLayout {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays
		};

		// Helper templates
		namespace detail {

			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			template <size_t N, size_t Alignment, typename Tuple>
			constexpr static size_t soa_byte_offset(const uintptr_t address, [[maybe_unused]] const size_t size) {
				if constexpr (N == 0) {
					return utils::align<Alignment>(address) - address;
				} else {
					const auto offset = utils::align<Alignment>(address) - address;
					using tt = typename std::tuple_element<N - 1, Tuple>::type;
					return sizeof(tt) * size + offset + soa_byte_offset<N - 1, Alignment, Tuple>(address, size);
				}
			}

		} // namespace detail

		template <DataLayout TDataLayout>
		struct data_layout_properties;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_get;
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_set;

		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_get_idx;
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_set_idx;

		template <>
		struct data_layout_properties<DataLayout::AoS> {
			constexpr static uint32_t PackSize = 1;
		};
		template <>
		struct data_layout_properties<DataLayout::SoA> {
			constexpr static uint32_t PackSize = 4;
		};

		/*!
		 * data_view_policy for accessing and storing data in the AoS way
		 *	Good for random access and when acessing data together.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xyz xyz xyz xyz
		 */
		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t Alignment = alignof(ValueType);

			[[nodiscard]] constexpr static ValueType getc(std::span<const ValueType> s, size_t idx) {
				return s[idx];
			}

			[[nodiscard]] constexpr static ValueType get(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			[[nodiscard]] constexpr static const ValueType& getc_constref(std::span<const ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			[[nodiscard]] constexpr static const ValueType& get_constref(std::span<ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			[[nodiscard]] constexpr static ValueType& get_ref(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType> {
			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			using view_policy = data_view_policy<DataLayout::AoS, ValueType>;

			[[nodiscard]] const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			[[nodiscard]] const ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType> {
			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			using view_policy = data_view_policy<DataLayout::AoS, ValueType>;

			[[nodiscard]] ValueType& operator[](size_t idx) {
				return view_policy::get_ref(m_data, idx);
			}

			[[nodiscard]] const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			[[nodiscard]] ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		using aos_view_policy = data_view_policy<DataLayout::AoS, ValueType>;
		template <typename ValueType>
		using aos_view_policy_get = data_view_policy_get<DataLayout::AoS, ValueType>;
		template <typename ValueType>
		using aos_view_policy_set = data_view_policy_get<DataLayout::AoS, ValueType>;

		/*!
		 * data_view_policy for accessing and storing data in the SoA way
		 *	Good for SIMD processing.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xxxx yyyy zzzz
		 */
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t Alignment = 16;

			template <size_t Ids>
			using value_type = typename std::tuple_element<Ids, decltype(struct_to_tuple(ValueType{}))>::type;
			template <size_t Ids>
			using const_value_type = typename std::add_const<value_type<Ids>>::type;

			[[nodiscard]] constexpr static ValueType get(std::span<const ValueType> s, const size_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			[[nodiscard]] constexpr static auto get(std::span<const ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				const auto* ret = (const uint8_t*)s.data() + idx * sizeof(MemberType) +
													detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(const MemberType*)ret, s.size() - idx};
			}

			constexpr static void set(std::span<ValueType> s, const size_t idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			constexpr static auto set(std::span<ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				auto* ret = (uint8_t*)s.data() + idx * sizeof(MemberType) +
										detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(MemberType*)ret, s.size() - idx};
			}

		private:
			template <typename Tuple, size_t... Ids>
			[[nodiscard]] constexpr static ValueType
			get_internal(Tuple& t, std::span<const ValueType> s, const size_t idx, std::integer_sequence<size_t, Ids...>) {
				(get_internal<Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, size_t Ids, typename TMemberType>
			constexpr static void get_internal(Tuple& t, const uint8_t* data, const size_t idx) {
				unaligned_ref<TMemberType> reader((void*)&data[idx]);
				std::get<Ids>(t) = reader;
			}

			template <typename Tuple, typename TValue, size_t... Ids>
			constexpr static void
			set_internal(Tuple& t, std::span<TValue> s, const size_t idx, std::integer_sequence<size_t, Ids...>) {
				(set_internal(
						 (uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size()),
						 std::get<Ids>(t)),
				 ...);
			}

			template <typename MemberType>
			constexpr static void set_internal(uint8_t* data, const size_t idx, MemberType val) {
				unaligned_ref<MemberType> writer((void*)&data[idx]);
				writer = val;
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType> {
			using view_policy = data_view_policy<DataLayout::SoA, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			[[nodiscard]] constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto get() const {
				return std::span<typename data_view_policy_idx_info<Ids>::const_value_type>(
						view_policy::template get<Ids>(m_data).data(), view_policy::template get<Ids>(m_data).size());
			}

			[[nodiscard]] const ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType> {
			using view_policy = data_view_policy<DataLayout::SoA, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using value_type = typename view_policy::template value_type<Ids>;
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			struct setter {
				const std::span<ValueType>& m_data;
				const size_t m_idx;

				constexpr setter(const std::span<ValueType>& data, const size_t idx): m_data(data), m_idx(idx) {}
				constexpr void operator=(ValueType&& val) {
					view_policy::set(m_data, m_idx, std::forward<ValueType>(val));
				}
			};

			[[nodiscard]] constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}
			[[nodiscard]] constexpr auto operator[](size_t idx) {
				return setter(m_data, idx);
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto get() const {
				using value_type = typename data_view_policy_idx_info<Ids>::const_value_type;
				const std::span<const ValueType> data((const ValueType*)m_data.data(), m_data.size());
				return std::span<value_type>(
						view_policy::template get<Ids>(data).data(), view_policy::template get<Ids>(data).size());
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto set() {
				return std::span<typename data_view_policy_idx_info<Ids>::value_type>(
						view_policy::template set<Ids>(m_data).data(), view_policy::template set<Ids>(m_data).size());
			}

			[[nodiscard]] ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa_view_policy_get = data_view_policy_get<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa_view_policy_set = data_view_policy_get<DataLayout::SoA, ValueType>;

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		template <typename, typename = void>
		struct is_soa_layout: std::false_type {};

		template <typename T>
		struct is_soa_layout<T, typename std::enable_if<T::Layout == DataLayout::SoA>::type>: std::true_type {};

		template <typename T>
		using auto_view_policy = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy<DataLayout::SoA, T>, data_view_policy<DataLayout::AoS, T>>;
		template <typename T>
		using auto_view_policy_get = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy_get<DataLayout::SoA, T>, data_view_policy_get<DataLayout::AoS, T>>;
		template <typename T>
		using auto_view_policy_set = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy_set<DataLayout::SoA, T>, data_view_policy_set<DataLayout::AoS, T>>;

	} // namespace utils
} // namespace gaia

namespace gaia {
	namespace utils {

		//! Provides statically generated unique identifier.
		struct GAIA_API type_seq final {
			[[nodiscard]] static uint32_t next() noexcept {
				static uint32_t value{};
				return value++;
			}
		};

		//! Provides statically generated unique identifier for a given group of types.
		template <typename...>
		class type_group {
			inline static uint32_t identifier{};

		public:
			template <typename... Type>
			inline static const uint32_t id = identifier++;
		};

		template <>
		class type_group<void>;

		//----------------------------------------------------------------------
		// Type meta data
		//----------------------------------------------------------------------

		struct type_info final {
		private:
			constexpr static size_t GetMin(size_t a, size_t b) {
				return b < a ? b : a;
			}

			constexpr static size_t FindFirstOf(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return size_t(-1);
			}

			constexpr static size_t FindLastOf(const char* data, size_t len, char c, size_t startPos = size_t(-1)) {
				for (int64_t i = (int64_t)GetMin(len - 1, startPos); i >= 0; --i) {
					if (data[i] == c)
						return i;
				}
				return size_t(-1);
			}

		public:
			template <typename T>
			static uint32_t index() noexcept {
				return type_group<type_info>::id<T>;
			}

			template <typename T>
			[[nodiscard]] static constexpr const char* full_name() noexcept {
				return GAIA_PRETTY_FUNCTION;
			}

			template <typename T>
			[[nodiscard]] static constexpr auto name() noexcept {
				// MSVC:
				//		const char* __cdecl ecs::ComponentInfo::name<struct ecs::EnfEntity>(void)
				//   -> ecs::EnfEntity
				// Clang/GCC:
				//		const ecs::ComponentInfo::name() [T = ecs::EnfEntity]
				//   -> ecs::EnfEntity

				// Note:
				//		We don't want to use std::string_view here because it would only make it harder on compile-times.
				//		In fact, even if we did, we need to be afraid of compiler issues.
				// 		Clang 8 and older wouldn't compile because their string_view::find_last_of doesn't work
				//		in constexpr context. Tested with and without LIBCPP
				//		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc
				//		As a workaround FindFirstOf and FindLastOf were implemented

				size_t strLen = 0;
				while (GAIA_PRETTY_FUNCTION[strLen] != '\0')
					++strLen;

				std::span<const char> name{GAIA_PRETTY_FUNCTION, strLen};
				const auto prefixPos = FindFirstOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = FindFirstOf(name.data(), name.size(), ' ', prefixPos + 1);
				const auto end = FindLastOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.subspan(start + 1, end - start - 1);
			}

			template <typename T>
			[[nodiscard]] static constexpr auto hash() noexcept {
#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4307)
#endif

				auto n = name<T>();
				return calculate_hash64(n.data(), n.size());

#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
#endif
			}
		};

	} // namespace utils
} // namespace gaia

namespace gaia {
	namespace ecs {

		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* ComponentTypeString[ComponentType::CT_Count] = {"Generic", "Chunk"};

		struct ComponentInfo;

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct AsChunk {
			using __Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using __TypeOriginal = T;
			static constexpr ComponentType __ComponentType = ComponentType::CT_Chunk;
		};

		namespace detail {
			template <typename T>
			struct ExtractComponentType_Generic {
				using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
				using TypeOriginal = T;
			};
			template <typename T>
			struct ExtractComponentType_NonGeneric {
				using Type = typename T::__Type;
				using TypeOriginal = typename T::__TypeOriginal;
			};
		} // namespace detail

		template <typename T, typename = void>
		struct IsGenericComponent: std::true_type {};
		template <typename T>
		struct IsGenericComponent<T, decltype((void)T::__ComponentType, void())>: std::false_type {};

		template <typename T>
		using DeduceComponent = std::conditional_t<
				IsGenericComponent<T>::value, typename detail::ExtractComponentType_Generic<T>,
				typename detail::ExtractComponentType_NonGeneric<T>>;

		template <typename T>
		struct IsReadOnlyType:
				std::bool_constant<
						std::is_const<std::remove_reference_t<std::remove_pointer_t<T>>>::value ||
						(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		struct ComponentSizeValid: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

		template <typename T>
		struct ComponentTypeValid:
				std::bool_constant<std::is_trivially_copyable<T>::value && std::is_default_constructible<T>::value> {};

		template <typename T>
		constexpr void VerifyComponent() {
			using U = typename DeduceComponent<T>::Type;
			// Make sure we only use this for "raw" types
			static_assert(!std::is_const<U>::value);
			static_assert(!std::is_pointer<U>::value);
			static_assert(!std::is_reference<U>::value);
			static_assert(!std::is_volatile<U>::value);
			static_assert(ComponentSizeValid<U>::value, "MAX_COMPONENTS_SIZE in bytes is exceeded");
			static_assert(ComponentTypeValid<U>::value, "Only components of trivial type are allowed");
		}

		//----------------------------------------------------------------------
		// Component hash operations
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr uint64_t CalculateMatcherHash() noexcept {
				return (uint64_t(1) << (utils::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr uint64_t CalculateMatcherHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return detail::CalculateMatcherHash<T>();
			else
				return utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...);
		}

		template <>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			return 0;
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return 0;
			} else {
				uint64_t hash = arr[0];
				utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
					hash = utils::hash_combine(hash, arr[i + 1]);
				});
				return hash;
			}
		}

		template <typename = void, typename...>
		constexpr uint64_t CalculateLookupHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return utils::type_info::hash<T>();
			else
				return utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...);
		}

		template <>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			return 0;
		}

		//----------------------------------------------------------------------
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfoCreate final {
			using FuncConstructor = void(void*);
			using FuncDestructor = void(void*);

			//! [ 0-15] Component name
			std::span<const char> name;
			//! [16-31] Constructor to call when the component is being constructed
			FuncConstructor* constructor;
			//! [32-47] Destructor to call when the component is being destroyed
			FuncDestructor* destructor;
			//! [48-51] Unique component identifier
			uint32_t infoIndex;

			template <typename T>
			[[nodiscard]] static constexpr ComponentInfoCreate Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfoCreate info{};
				info.name = utils::type_info::name<U>();
				info.infoIndex = utils::type_info::index<U>();

				if constexpr (!std::is_empty<U>::value && !utils::is_soa_layout<U>::value) {
					info.constructor = [](void* ptr) {
						new (ptr) T{};
					};
					info.destructor = [](void* ptr) {
						((T*)ptr)->~T();
					};
				}

				return info;
			}

			template <typename T>
			static ComponentInfoCreate Create() {
				using U = std::decay_t<T>;
				return ComponentInfoCreate::Calculate<U>();
			}
		};

		//----------------------------------------------------------------------
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfo final {
			//! [0-7] Complex hash used for look-ups
			uint64_t lookupHash;
			//! [8-15] Simple hash used for matching component
			uint64_t matcherHash;
			//! [16-19] Unique component identifier
			uint32_t infoIndex;
			//! [20-23]
			struct {
				//! Component alignment
				uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! Tells if the component is laid out in SoA style
				uint32_t soa : 1;
			} properties{};

			[[nodiscard]] bool operator==(const ComponentInfo& other) const {
				return lookupHash == other.lookupHash && infoIndex == other.infoIndex;
			}
			[[nodiscard]] bool operator!=(const ComponentInfo& other) const {
				return lookupHash != other.lookupHash || infoIndex != other.infoIndex;
			}
			[[nodiscard]] bool operator<(const ComponentInfo& other) const {
				return lookupHash < other.lookupHash;
			}

			template <typename T>
			[[nodiscard]] static constexpr ComponentInfo Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfo info{};
				info.lookupHash = utils::type_info::hash<U>();
				info.matcherHash = CalculateMatcherHash<U>();
				info.infoIndex = utils::type_info::index<U>();

				if constexpr (!std::is_empty<U>::value) {
					info.properties.alig = utils::auto_view_policy<U>::Alignment;
					info.properties.size = (uint32_t)sizeof(U);
					info.properties.soa = utils::is_soa_layout<U>::value;
				}

				return info;
			}

			template <typename T>
			static const ComponentInfo* Create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{Calculate<U>()};
			}
		};

		[[nodiscard]] inline uint64_t CalculateMatcherHash(uint64_t hashA, uint64_t hashB) noexcept {
			return utils::combine_or(hashA, hashB);
		}

		[[nodiscard]] inline uint64_t CalculateMatcherHash(std::span<const ComponentInfo*> infos) noexcept {
			uint64_t hash = infos.empty() ? 0 : infos[0]->matcherHash;
			for (size_t i = 1; i < infos.size(); ++i)
				hash = utils::combine_or(hash, infos[i]->matcherHash);
			return hash;
		}

		[[nodiscard]] inline uint64_t CalculateLookupHash(std::span<const ComponentInfo*> infos) noexcept {
			uint64_t hash = infos.empty() ? 0 : infos[0]->lookupHash;
			for (size_t i = 1; i < infos.size(); ++i)
				hash = utils::hash_combine(hash, infos[i]->lookupHash);
			return hash;
		}

		//----------------------------------------------------------------------

		struct ComponentLookupData final {
			//! Component info index. A copy of the value in ComponentInfo
			uint32_t infoIndex;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ComponentInfoList = containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ComponentLookupList = containers::sarray_ext<ComponentLookupData, MAX_COMPONENTS_PER_ARCHETYPE>;

	} // namespace ecs
} // namespace gaia

#include <cstddef>
#include <cstdint>

namespace gaia {
	namespace utils {
		constexpr size_t BadIndex = size_t(-1);

		template <typename C, typename Func>
		constexpr auto for_each(const C& arr, Func func) {
			return utils::for_each(arr.begin(), arr.end(), func);
		}

		template <typename C>
		constexpr auto find(const C& arr, typename C::const_reference item) {
			return utils::find(arr.begin(), arr.end(), item);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			return utils::find_if(arr.begin(), arr.end(), predicate);
		}

		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return (std::ptrdiff_t)BadIndex;

			return GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return GAIA_UTIL::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return GAIA_UTIL::distance(arr.begin(), find_if(arr, predicate));
		}

		template <typename C>
		constexpr bool has(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		template <typename UnaryPredicate, typename C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		template <typename C>
		void erase_fast(C& arr, size_t idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				utils::swap(arr[idx], arr[arr.size() - 1]);

			arr.pop_back();
		}
	} // namespace utils
} // namespace gaia

#include <cstdint>

#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#include <alloca.h>
	#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#elif defined(_WIN32)
	#include <malloc.h>
	// Clang with MSVC codegen needs some remapping
	#if !defined(aligned_alloc)
		#define GAIA_ALIGNED_ALLOC(alig, size) _aligned_malloc(size, alig)
	#else
		#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#endif
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE _aligned_free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#else
	#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#endif

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
	
#endif

namespace gaia {
	namespace ecs {
		static constexpr uint32_t MemoryBlockSize = 16384;
		static constexpr uint32_t MemoryBlockUsableOffset = 16;
		static constexpr uint32_t ChunkMemorySize = MemoryBlockSize - MemoryBlockUsableOffset;

		struct ChunkAllocatorStats final {
			//! Total allocated memory
			uint64_t AllocatedMemory;
			//! Memory actively used
			uint64_t UsedMemory;
			//! Number of allocated pages
			uint32_t NumPages;
			//! Number of free pages
			uint32_t NumFreePages;
		};

		/*!
		Allocator for ECS Chunks. Memory is organized in pages of chunks.
		*/
		class ChunkAllocator {
			struct MemoryBlock {
				//! For active block: Index of the block within page.
				//! For passive block: Index of the next free block in the implicit list.
				uint16_t idx;
			};

			struct MemoryPage {
				static constexpr uint32_t NBlocks = 64;
				static constexpr uint32_t Size = NBlocks * MemoryBlockSize;
				static constexpr uint16_t InvalidBlockId = (uint16_t)-1;
				using iterator = containers::darray<MemoryPage*>::iterator;

				//! Pointer to data managed by page
				void* m_data;
				//! Implicit list of blocks
				containers::sarray_ext<MemoryBlock, (uint16_t)NBlocks> m_blocks;
				//! Index in the list of pages
				uint32_t m_idx;
				//! Number of used blocks out of NBlocks
				uint16_t m_usedBlocks;
				//! Index of the next block to recycle
				uint16_t m_nextFreeBlock;
				//! Number of blocks to recycle
				uint16_t m_freeBlocks;

				MemoryPage(void* ptr): m_data(ptr), m_idx(0), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

				[[nodiscard]] void* AllocChunk() {
					if (!m_freeBlocks) {
						// We don't want to go out of range for new blocks
						GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

						++m_usedBlocks;

						const size_t index = m_blocks.size();
						GAIA_ASSERT(index < 16536U);
						m_blocks.push_back({(uint16_t)m_blocks.size()});

						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by UsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
						*(uintptr_t*)pMemoryBlock = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					} else {
						GAIA_ASSERT(m_nextFreeBlock < m_blocks.size() && "Block allocator recycle containers::list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const size_t index = m_nextFreeBlock;
						m_nextFreeBlock = m_blocks[m_nextFreeBlock].idx;

						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by UsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
						*(uintptr_t*)pMemoryBlock = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					}
				}

				void FreeChunk(void* chunk) {
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					// Offset the chunk memory so we get the real block address
					const uint8_t* pMemoryBlock = (uint8_t*)chunk - MemoryBlockUsableOffset;

					const auto blckAddr = (uintptr_t)pMemoryBlock;
					const auto dataAddr = (uintptr_t)m_data;
					GAIA_ASSERT(blckAddr >= dataAddr && blckAddr < dataAddr + MemoryPage::Size);
					MemoryBlock block = {uint16_t((blckAddr - dataAddr) / MemoryBlockSize)};

					auto& blockContainer = m_blocks[block.idx];

					// Update our implicit containers::list
					if (!m_freeBlocks) {
						m_nextFreeBlock = block.idx;
						blockContainer.idx = InvalidBlockId;
					} else {
						blockContainer.idx = m_nextFreeBlock;
						m_nextFreeBlock = block.idx;
					}

					++m_freeBlocks;
					--m_usedBlocks;
				}

				[[nodiscard]] uint32_t GetUsedBlocks() const {
					return m_usedBlocks;
				}
				[[nodiscard]] bool IsFull() const {
					return m_usedBlocks == NBlocks;
				}
				[[nodiscard]] bool IsEmpty() const {
					return m_usedBlocks == 0;
				}
			};

			//! List of available pages
			//! Note, this currently only contains at most 1 item
			containers::darray<MemoryPage*> m_pagesFree;
			//! List of full pages
			containers::darray<MemoryPage*> m_pagesFull;
			//! Allocator statistics
			ChunkAllocatorStats m_stats{};

		public:
			~ChunkAllocator() {
				FreeAll();
			}

			/*!
			Allocates memory
			*/
			void* Allocate() {
				void* pChunk = nullptr;

				if (m_pagesFree.empty()) {
					// Initial allocation
					auto pPage = AllocPage();
					m_pagesFree.push_back(pPage);
					pPage->m_idx = (uint32_t)m_pagesFree.size() - 1;
					pChunk = pPage->AllocChunk();
				} else {
					auto pPage = m_pagesFree[0];
					GAIA_ASSERT(!pPage->IsFull());
					// Allocate a new chunk
					pChunk = pPage->AllocChunk();

					// Handle full pages
					if (pPage->IsFull()) {
						// Remove the page from the open list and update the swapped page's pointer
						utils::erase_fast(m_pagesFree, 0);
						if (!m_pagesFree.empty())
							m_pagesFree[0]->m_idx = 0;

						// Move our page to the full list
						m_pagesFull.push_back(pPage);
						pPage->m_idx = (uint32_t)m_pagesFull.size() - 1;
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill allocated memory with 0xbaadf00d.
				// This way we always know if we treat the memory correctly.
				utils::fill_array((uint32_t*)pChunk, (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t)), 0x7fcdf00dU);
#endif

				return pChunk;
			}

			/*!
			Releases memory allocated for pointer
			*/
			void Release(void* chunk) {
				// Decode the page from the address
				uintptr_t pageAddr = *(uintptr_t*)((uint8_t*)chunk - MemoryBlockUsableOffset);
				auto* pPage = (MemoryPage*)pageAddr;

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill freed memory with 0xfeeefeee.
				// This way we always know if we treat the memory correctly.
				utils::fill_array((uint32_t*)chunk, (int)(ChunkMemorySize / sizeof(uint32_t)), 0xfeeefeeeU);
#endif

				const bool pageFull = pPage->IsFull();

#if GAIA_DEBUG
				if (pageFull) {
					[[maybe_unused]] auto it = utils::find_if(m_pagesFull.begin(), m_pagesFull.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																				 "in the full pages containers::list");
				} else {
					[[maybe_unused]] auto it = utils::find_if(m_pagesFree.begin(), m_pagesFree.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																				 "the free pages containers::list");
				}
#endif

				// Update lists
				if (pageFull) {
					// Our page is no longer full. Remove it from the list and update the swapped page's pointer
					if (m_pagesFull.size() > 1)
						m_pagesFull.back()->m_idx = pPage->m_idx;
					utils::erase_fast(m_pagesFull, pPage->m_idx);

					// Move our page to the open list
					pPage->m_idx = (uint32_t)m_pagesFree.size();
					m_pagesFree.push_back(pPage);
				}

				// Free the chunk
				pPage->FreeChunk(chunk);
			}

			/*!
			Releases all allocated memory
			*/
			void FreeAll() {
				// Release free pages
				for (auto* page: m_pagesFree)
					FreePage(page);
				// Release full pages
				for (auto* page: m_pagesFull)
					FreePage(page);

				m_pagesFree = {};
				m_pagesFull = {};
				m_stats = {};
			}

			/*!
			Returns allocator statistics
			*/
			void GetStats(ChunkAllocatorStats& stats) const {
				stats.NumPages = (uint32_t)m_pagesFree.size() + (uint32_t)m_pagesFull.size();
				stats.NumFreePages = (uint32_t)m_pagesFree.size();
				stats.AllocatedMemory = stats.NumPages * (size_t)MemoryPage::Size;
				stats.UsedMemory = m_pagesFull.size() * (size_t)MemoryPage::Size;
				for (auto* page: m_pagesFree)
					stats.UsedMemory += page->GetUsedBlocks() * (size_t)MemoryBlockSize;
			}

			/*!
			Flushes unused memory
			*/
			void Flush() {
				for (size_t i = 0; i < m_pagesFree.size();) {
					auto* pPage = m_pagesFree[i];

					// Skip non-empty pages
					if (!pPage->IsEmpty()) {
						++i;
						continue;
					}

					utils::erase_fast(m_pagesFree, i);
					FreePage(pPage);
					if (!m_pagesFree.empty())
						m_pagesFree[i]->m_idx = (uint32_t)i;
				}
			}

		private:
			MemoryPage* AllocPage() {
				auto* pageData = (uint8_t*)GAIA_ALIGNED_ALLOC(16, MemoryPage::Size);
				return new MemoryPage(pageData);
			}

			void FreePage(MemoryPage* page) {
				GAIA_ALIGNED_FREE(page->m_data);
				delete page;
			}
		};

	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <cstdint>

#include <cinttypes>

namespace gaia {
	namespace ecs {
		struct Entity;

		class Chunk;
		class Archetype;
		class World;

		class EntityQuery;

		struct ComponentInfo;
		class CommandBuffer;
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		uint32_t GetWorldVersionFromArchetype(const Archetype& archetype);

		struct ChunkHeader final {
		public:
			//! [0-7] Archetype that created this chunk.
			const Archetype& owner;
#if !GAIA_64
			uint32_t owner_padding;
#endif
			// [8-11]
			struct {
				//! Number of items in the chunk.
				uint32_t count : 16;
				//! Capacity (copied from the owner archetype).
				uint32_t capacity : 16;
			} items{};

			//! [12-15] Chunk index in its archetype list
			uint32_t index{};

			// [16-19]
			struct {
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint32_t lifespan : 31;
				//! If true this chunk stores disabled entities
				uint32_t disabled : 1;
			} info{};

			//! [20-275] Versions of individual components on chunk.
			uint32_t versions[ComponentType::CT_Count][MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(const Archetype& archetype): owner(archetype) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType type, uint32_t componentIdx) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx != UINT32_MAX && componentIdx < MAX_COMPONENTS_PER_ARCHETYPE);

				// Update all components' version
				versions[type][componentIdx] = gv;
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType type) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Update all components' version
				for (size_t i = 0; i < MAX_COMPONENTS_PER_ARCHETYPE; i++)
					versions[type][i] = gv;
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		using EntityInternalType = uint32_t;
		using EntityId = EntityInternalType;
		using EntityGenId = EntityInternalType;

		struct Entity final {
			static constexpr EntityInternalType IdBits = GAIA_ENTITY_IDBITS;
			static constexpr EntityInternalType GenBits = GAIA_ENTITY_GENBITS;
			static constexpr EntityInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr EntityInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;

			using EntitySizeType = std::conditional_t<(IdBits + GenBits > 32), uint64_t, uint32_t>;

			static_assert(IdBits + GenBits <= 64, "Entity IdBits and GenBits must fit inside 64 bits");
			static_assert(IdBits <= 31, "Entity IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Entity GenBits is recommended to be at least 10 bits long");

		private:
			struct EntityData {
				//! Index in entity array
				EntityInternalType id: IdBits;
				//! Generation index. Incremented every time an entity is deleted
				EntityInternalType gen: GenBits;
			};

			union {
				EntityData data;
				EntitySizeType val;
			};

		public:
			Entity() = default;
			Entity(EntityId id, EntityGenId gen) {
				data.id = id;
				data.gen = gen;
			}

			Entity(Entity&&) = default;
			Entity& operator=(Entity&&) = default;
			Entity(const Entity&) = default;
			Entity& operator=(const Entity&) = default;

			[[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept {
				return val == other.val;
			}
			[[nodiscard]] constexpr bool operator!=(const Entity& other) const noexcept {
				return val != other.val;
			}

			auto id() const {
				return data.id;
			}
			auto gen() const {
				return data.gen;
			}
			auto value() const {
				return val;
			}
		};

		struct EntityNull_t {
			[[nodiscard]] operator Entity() const noexcept {
				return Entity(Entity::IdMask, Entity::GenMask);
			}

			[[nodiscard]] constexpr bool operator==(const EntityNull_t&) const noexcept {
				return true;
			}
			[[nodiscard]] constexpr bool operator!=(const EntityNull_t&) const noexcept {
				return false;
			}
		};

		[[nodiscard]] inline bool operator==(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() == entity.id();
		}

		[[nodiscard]] inline bool operator!=(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() != entity.id();
		}

		[[nodiscard]] inline bool operator==(const Entity& entity, const EntityNull_t& null) noexcept {
			return null == entity;
		}

		[[nodiscard]] inline bool operator!=(const Entity& entity, const EntityNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr EntityNull_t EntityNull{};

		class Chunk;
		struct EntityContainer {
			//! Chunk the entity currently resides in
			Chunk* pChunk;
#if !GAIA_64
			uint32_t pChunk_padding;
#endif
			//! For allocated entity: Index of entity within chunk.
			//! For deleted entity: Index of the next entity in the implicit list.
			uint32_t idx : 31;
			//! Tells if the entity is disabled. Borrows one bit from idx because it's unlikely to cause issues there
			uint32_t disabled : 1;
			//! Generation ID
			EntityGenId gen;
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		const ComponentInfo* GetComponentInfoFromArchetype(const Archetype& archetype, uint32_t componentIdx);
		const ComponentInfoList& GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type);
		const ComponentLookupList& GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type);

		class Chunk final {
		public:
			//! Size of data at the end of the chunk reserved for special purposes
			//! (alignment, allocation overhead etc.)
			static constexpr size_t DATA_SIZE_RESERVED = 128;
			//! Size of one chunk's data part with components
			static constexpr size_t DATA_SIZE = ChunkMemorySize - sizeof(ChunkHeader) - DATA_SIZE_RESERVED;
			//! Size of one chunk's data part with components without the serve part
			static constexpr size_t DATA_SIZE_NORESERVE = ChunkMemorySize - sizeof(ChunkHeader);

		private:
			friend class World;
			friend class Archetype;
			friend class CommandBuffer;

			//! Archetype header with info about the archetype
			ChunkHeader header;
			//! Archetype data. Entities first, followed by a lists of components.
			uint8_t data[DATA_SIZE_NORESERVE];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			Chunk(const Archetype& archetype): header(archetype) {}

			GAIA_MSVC_WARNING_POP()

			/*!
			Checks if a component is present in the archetype based on the provided \param infoIndex.
			\param type Component type
			\return True if found. False otherwise.
			*/
			[[nodiscard]] bool HasComponent_Internal(ComponentType type, uint32_t infoIndex) const {
				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				return utils::has_if(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
			}

			/*!
			Make the \param entity entity a part of the chunk.
			\return Index of the entity within the chunk.
			*/
			[[nodiscard]] uint32_t AddEntity(Entity entity) {
				const auto index = header.items.count++;
				SetEntity(index, entity);

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				return index;
			}

			void RemoveEntity(uint32_t index, containers::darray<EntityContainer>& entities) {
				// Ignore requests on empty chunks
				if (header.items.count == 0)
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index < header.items.count);

				// If there are at least two entities inside and it's not already the
				// last one let's swap our entity with the last one in chunk.
				if (header.items.count > 1 && header.items.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(header.items.count - 1);
					SetEntity(index, entity);

					const auto& componentInfos = GetArchetypeComponentInfoList(header.owner, ComponentType::CT_Generic);
					const auto& lookupList = GetArchetypeComponentLookupList(header.owner, ComponentType::CT_Generic);

					for (size_t i = 0; i < componentInfos.size(); i++) {
						const auto& info = componentInfos[i];
						const auto& look = lookupList[i];

						// Skip tag components
						if (!info->properties.size)
							continue;

						const uint32_t idxFrom = look.offset + index * info->properties.size;
						const uint32_t idxTo = look.offset + (header.items.count - 1) * info->properties.size;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxFrom != idxTo);

						memcpy(&data[idxFrom], &data[idxTo], info->properties.size);
					}

					// Entity has been replaced with the last one in chunk.
					// Update its index so look ups can find it.
					entities[entity.id()].idx = index;
					entities[entity.id()].gen = entity.gen();
				}

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				--header.items.count;
			}

			/*!
			Makes the entity a part of a chunk on a given index.
			\param index Index of the entity
			\param entity Entity to store in the chunk
			*/
			void SetEntity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < header.items.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				mem = entity;
			}

			/*!
			Returns the entity on a given index in the chunk.
			\param index Index of the entity
			\return Entity on a given index within the chunk.
			*/
			[[nodiscard]] const Entity GetEntity(uint32_t index) const {
				GAIA_ASSERT(index < header.items.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				return mem;
			}

			/*!
			Returns a read-only span of the component data.
			\tparam T Component
			\return Const span of the component data.
			*/
			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE auto View_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				using UConst = typename std::add_const_t<U>;

				if constexpr (std::is_same<U, Entity>::value) {
					return std::span<const Entity>{(const Entity*)&data[0], GetItemCount()};
				} else {
					static_assert(!std::is_empty<U>::value, "Attempting to get value of an empty component");

					const auto infoIndex = utils::type_info::index<U>();

					if constexpr (IsGenericComponent<T>::value)
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Generic, infoIndex), GetItemCount()};
					else
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Chunk, infoIndex), 1};
				}
			}

			/*!
			Returns a read-write span of the component data. Also updates the world version for the component.
			\tparam T Component
			\return Span of the component data.
			*/
			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE auto ViewRW_Internal() {
				using U = typename DeduceComponent<T>::Type;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
				// even in context where it shouldn't.
				// Unfortunatelly, even runtime assert can't be used...
				// GAIA_ASSERT(!std::is_same<U, Entity>::value);
#else
				static_assert(!std::is_same<U, Entity>::value);
#endif
				static_assert(!std::is_empty<U>::value, "Attempting to set value of an empty component");

				const auto infoIndex = utils::type_info::index<U>();

				if constexpr (IsGenericComponent<T>::value)
					return std::span<U>{(U*)GetDataPtrRW(ComponentType::CT_Generic, infoIndex), GetItemCount()};
				else
					return std::span<U>{(U*)GetDataPtrRW(ComponentType::CT_Chunk, infoIndex), 1};
			}

			/*!
			Returns a pointer do component data with read-only access.
			\param type Component type
			\param infoIndex Index of the component in the archetype
			\return Const pointer to component data.
			*/
			[[nodiscard]] GAIA_FORCEINLINE const uint8_t* GetDataPtr(ComponentType type, uint32_t infoIndex) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(type, infoIndex));

				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				const auto componentIdx = utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				return (const uint8_t*)&data[infos[componentIdx].offset];
			}

			/*!
			Returns a pointer do component data with read-write access. Also updates the world version for the component.
			\param type Component type
			\param infoIndex Index of the component in the archetype
			\return Pointer to component data.
			*/
			[[nodiscard]] GAIA_FORCEINLINE uint8_t* GetDataPtrRW(ComponentType type, uint32_t infoIndex) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(type, infoIndex));
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(GetComponentInfoFromArchetype(header.owner, infoIndex)->properties.size != 0);

				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				const auto componentIdx = (uint32_t)utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				// Update version number so we know RW access was used on chunk
				header.UpdateWorldVersion(type, componentIdx);

				return (uint8_t*)&data[infos[componentIdx].offset];
			}

		public:
			/*!
			Returns the parent archetype.
			\return Parent archetype
			*/
			const Archetype& GetArchetype() const {
				return header.owner;
			}

			/*!
			Returns a read-only entity or component view.
			\return Component view with read-only access
			*/
			template <typename T>
			[[nodiscard]] auto View() const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				static_assert(IsReadOnlyType<UOriginal>::value);

				return utils::auto_view_policy_get<std::add_const_t<U>>{View_Internal<T>()};
			}

			/*!
			Returns a mutable component view.
			\return Component view with read-write access
			*/
			template <typename T>
			[[nodiscard]] auto ViewRW() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same<U, Entity>::value);

				return utils::auto_view_policy_set<U>{ViewRW_Internal<T>()};
			}

			/*!
			Returns the internal index of a component based on the provided \param infoIndex.
			\param type Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			[[nodiscard]] uint32_t GetComponentIdx(ComponentType type, uint32_t infoIndex) const {
				const auto& list = GetArchetypeComponentLookupList(header.owner, type);
				const auto idx = (uint32_t)utils::get_index_if_unsafe(list, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
				GAIA_ASSERT(idx != (uint32_t)-1);
				return (uint32_t)idx;
			}

			/*!
			Checks if component is present on the chunk.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				if constexpr (IsGenericComponent<T>::value) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					const auto infoIndex = utils::type_info::index<U>();
					return HasComponent_Internal(ComponentType::CT_Generic, infoIndex);
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					const auto infoIndex = utils::type_info::index<U>();
					return HasComponent_Internal(ComponentType::CT_Chunk, infoIndex);
				}
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			template <typename T>
			void SetComponent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>::value,
						"SetComponent providing an index in chunk is only available for generic components");

				ViewRW<T>()[index] = std::forward<U>(value);
			}

			template <typename T>
			void SetComponent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>::value,
						"SetComponent not providing an index in chunk is only available for non-generic components");

				ViewRW<T>()[0] = std::forward<U>(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			template <typename T>
			auto GetComponent(uint32_t index) const {
				static_assert(
						IsGenericComponent<T>::value, "GetComponent providing an index is only available for generic components");
				return View<T>()[index];
			}

			template <typename T>
			auto GetComponent() const {
				static_assert(
						!IsGenericComponent<T>::value,
						"GetComponent not providing an index is only available for non-generic components");
				return View<T>()[0];
			}

			//----------------------------------------------------------------------

			//! Checks is this chunk is disabled
			[[nodiscard]] bool IsDisabled() const {
				return header.info.disabled;
			}

			//! Checks is the full capacity of the has has been reached
			[[nodiscard]] bool IsFull() const {
				return header.items.count >= header.items.capacity;
			}

			//! Checks is there are any entities in the chunk
			[[nodiscard]] bool HasEntities() const {
				return header.items.count > 0;
			}

			//! Returns the number of entities in the chunk
			[[nodiscard]] uint32_t GetItemCount() const {
				return header.items.count;
			}

			//! Returns true if the provided version is newer than the one stored internally
			[[nodiscard]] bool DidChange(ComponentType type, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(header.versions[type][componentIdx], version);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::map<uint32_t, const ComponentInfo*> m_infoByIndex;
			containers::map<uint32_t, ComponentInfoCreate> m_infoCreateByIndex;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_infoByIndex.reserve(2048);
				m_infoCreateByIndex.reserve(2048);
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			~ComponentCache() {
				ClearRegisteredInfoCache();
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* GetOrCreateComponentInfo() {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();

				if (m_infoCreateByIndex.find(index) == m_infoCreateByIndex.end())
					m_infoCreateByIndex.emplace(index, ComponentInfoCreate::Create<U>());

				{
					const auto res = m_infoByIndex.emplace(index, nullptr);
					if (res.second)
						res.first->second = ComponentInfo::Create<U>();

					return res.first->second;
				}
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* FindComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;

				const auto index = utils::type_info::hash<U>();
				const auto it = m_infoByIndex.find(index);
				return it != m_infoByIndex.end() ? it->second : (const ComponentInfo*)nullptr;
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();
				return GetComponentInfoFromIdx(index);
			}

			[[nodiscard]] const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoByIndex.find(componentIndex) != m_infoByIndex.end());
				return m_infoByIndex.at(componentIndex);
			}

			[[nodiscard]] const ComponentInfoCreate& GetComponentCreateInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoCreateByIndex.find(componentIndex) != m_infoCreateByIndex.end());
				return m_infoCreateByIndex.at(componentIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();
				return m_infoCreateByIndex.find(index) != m_infoCreateByIndex.end();
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_infoCreateByIndex.size();
				LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& pair: m_infoCreateByIndex) {
					const auto& info = pair.second;
					LOG_N("  (%p) index:%010u, %.*s", (void*)&info, info.infoIndex, (uint32_t)info.name.size(), info.name.data());
				}
			}

		private:
			void ClearRegisteredInfoCache() {
				for (auto& pair: m_infoByIndex)
					delete pair.second;
				m_infoByIndex.clear();
				m_infoCreateByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class World;

		ComponentCache& GetComponentCache(World& world);
		const ComponentCache& GetComponentCache(const World& world);
		uint32_t GetWorldVersionFromWorld(const World& world);
		void* AllocateChunkMemory(World& world);
		void ReleaseChunkMemory(World& world, void* mem);

		class Archetype final {
		private:
			friend class World;
			friend class CommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

#if GAIA_ARCHETYPE_GRAPH
			struct ArchetypeGraphEdge {
				const ComponentInfo* info;
				Archetype* archetype;
			};
#endif

			//! World to which this chunk belongs to
			const World* parentWorld = nullptr;

			//! List of active chunks allocated by this archetype
			containers::darray<Chunk*> chunks;
			//! List of disabled chunks allocated by this archetype
			containers::darray<Chunk*> chunksDisabled;

#if GAIA_ARCHETYPE_GRAPH
			//! List of edges in the archetype graph when adding components
			containers::darray<ArchetypeGraphEdge> edgesAdd[ComponentType::CT_Count];
			//! List of edges in the archetype graph when removing components
			containers::darray<ArchetypeGraphEdge> edgesDel[ComponentType::CT_Count];
#endif

			//! Description of components within this archetype
			containers::sarray<ComponentInfoList, ComponentType::CT_Count> componentInfos;
			//! Lookup hashes of components within this archetype
			containers::sarray<ComponentLookupList, ComponentType::CT_Count> componentLookupData;

			uint64_t genericHash = 0;
			uint64_t chunkHash = 0;

			//! Hash of components within this archetype - used for lookups
			utils::direct_hash_key lookupHash{};
			//! Hash of components within this archetype - used for matching
			uint64_t matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t id = 0;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint32_t capacity : 16;
				//! True if there's a component that requires custom construction or destruction
				uint32_t hasComponentWithCustomCreation : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 4;
			} info{};

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

			/*!
			Allocates memory for a new chunk.
			\param archetype Archetype of the chunk we want to allocate
			\return Newly allocated chunk
			*/
			[[nodiscard]] static Chunk* AllocateChunk(const Archetype& archetype) {
				auto& world = const_cast<World&>(*archetype.parentWorld);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto pChunk = (Chunk*)AllocateChunkMemory(world);
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif

				// Call default constructors for components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Generic];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.constructor == nullptr)
							continue;
						infoCreate.constructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}
				// Call default constructors for chunk components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Chunk];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.constructor == nullptr)
							continue;
						infoCreate.constructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}

				pChunk->header.items.capacity = archetype.info.capacity;
				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void ReleaseChunk(Chunk* pChunk) {
				const auto& archetype = pChunk->header.owner;
				auto& world = const_cast<World&>(*archetype.parentWorld);

				// Call destructors for types that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Generic];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.destructor == nullptr)
							continue;
						infoCreate.destructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}
				// Call destructors for chunk components which need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Chunk];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.destructor == nullptr)
							continue;
						infoCreate.destructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				ReleaseChunkMemory(world, pChunk);
#else
				delete pChunk;
#endif
			}

			[[nodiscard]] static Archetype*
			Create(World& pWorld, std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				auto newArch = new Archetype();
				newArch->parentWorld = &pWorld;

#if GAIA_ARCHETYPE_GRAPH
				// Preallocate arrays for graph edges
				// Generic components are going to be more common so we prepare bigger arrays for them.
				// Chunk components are expected to be very rare so only a small buffer is preallocated.
				newArch->edgesAdd[ComponentType::CT_Generic].reserve(8);
				newArch->edgesAdd[ComponentType::CT_Chunk].reserve(1);
				newArch->edgesDel[ComponentType::CT_Generic].reserve(8);
				newArch->edgesDel[ComponentType::CT_Chunk].reserve(1);
#endif

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Size of the entity + all of its generic components
				size_t genericComponentListSize = sizeof(Entity);
				for (const auto* info: infosGeneric) {
					genericComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= info->properties.size != 0 && info->properties.soa != 0;
				}

				// Size of chunk components
				size_t chunkComponentListSize = 0;
				for (const auto* info: infosChunk) {
					chunkComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= info->properties.size != 0 && info->properties.soa != 0;
				}

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset = sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = sizeof(ChunkHeader) + componentOffset;

				// Add generic infos
				for (size_t i = 0; i < infosGeneric.size(); i++) {
					const auto* info = infosGeneric[i];
					const auto alignment = info->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{info->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += info->properties.size * maxGenericItemsInArchetype;
						alignedOffset += info->properties.size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{info->infoIndex, (uint32_t)componentOffset});
					}
				}

				// Add chunk infos
				for (size_t i = 0; i < infosChunk.size(); i++) {
					const auto* info = infosChunk[i];
					const auto alignment = info->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{info->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += info->properties.size;
						alignedOffset += info->properties.size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{info->infoIndex, (uint32_t)componentOffset});
					}
				}

				newArch->info.capacity = maxGenericItemsInArchetype;
				newArch->matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(infosGeneric);
				newArch->matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(infosChunk);

				return newArch;
			}

			[[nodiscard]] Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) {
				if (!chunkArray.empty()) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = chunkArray.size() - 1;
					do {
						auto pChunk = chunkArray[i];
						GAIA_ASSERT(pChunk != nullptr);
						if (!pChunk->IsFull())
							return pChunk;
					} while (i-- > 0);
				}

				// No free space found anywhere. Let's create a new one.
				auto* pChunk = AllocateChunk(*this);
				pChunk->header.index = (uint32_t)chunkArray.size();
				chunkArray.push_back(pChunk);
				return pChunk;
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created
			[[nodiscard]] Chunk* FindOrCreateFreeChunk() {
				return FindOrCreateFreeChunk_Internal(chunks);
			}

			//! Tries to locate a chunk for disabled entities that has some space left for a new one.
			//! If not found a new chunk is created
			[[nodiscard]] Chunk* FindOrCreateFreeChunkDisabled() {
				if (auto* pChunk = FindOrCreateFreeChunk_Internal(chunksDisabled)) {
					pChunk->header.info.disabled = true;
					return pChunk;
				}

				return nullptr;
			}

			/*!
			Removes a chunk from the list of chunks managed by their achetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void RemoveChunk(Chunk* pChunk) {
				const bool isDisabled = pChunk->IsDisabled();
				const auto chunkIndex = pChunk->header.index;

				ReleaseChunk(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->header.index = chunkIndex;
					GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
					utils::erase_fast(chunkArray, chunkIndex);
				};

				if (isDisabled)
					remove(chunksDisabled);
				else
					remove(chunks);
			}

#if GAIA_ARCHETYPE_GRAPH
			Archetype* FindDelEdgeArchetype(ComponentType type, const ComponentInfo* info) {
				// Breath-first lookup.
				// Go through all edges first. If nothing is found check each leaf and repeat until there is a match.
				const auto& edges = edgesDel[type];
				const auto it = utils::find_if(edges, [info](const auto& edge) {
					return edge.info == info;
				});
				if (it != edges.end())
					return it->archetype;

				// Not found right away, search deeper
				for (const auto& edge: edges) {
					auto ret = edge.archetype->FindDelEdgeArchetype(type, info);
					if (ret != nullptr)
						return edge.archetype;
				}

				return nullptr;
			}
#endif

		public:
			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: chunks)
					ReleaseChunk(pChunk);
				for (auto* pChunk: chunksDisabled)
					ReleaseChunk(pChunk);
			}

			[[nodiscard]] const World& GetWorld() const {
				return *parentWorld;
			}

			[[nodiscard]] uint32_t GetWorldVersion() const {
				return GetWorldVersionFromWorld(*parentWorld);
			}

			[[nodiscard]] uint32_t GetCapacity() const {
				return info.capacity;
			}

			[[nodiscard]] uint64_t GetMatcherHash(ComponentType type) const {
				return matcherHash[type];
			}

			[[nodiscard]] const ComponentInfoList& GetComponentInfoList(ComponentType type) const {
				return componentInfos[type];
			}

			[[nodiscard]] const ComponentLookupList& GetComponentLookupList(ComponentType type) const {
				return componentLookupData[type];
			}

			/*!
			Checks if a given component is present on the archetype.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				return HasComponent_Internal<T>();
			}

		private:
			template <typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				const auto infoIndex = utils::type_info::index<U>();

				if constexpr (IsGenericComponent<T>::value) {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Generic), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				} else {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Chunk), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				}
			}
		};

		[[nodiscard]] inline uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}
		[[nodiscard]] inline uint64_t GetArchetypeMatcherHash(const Archetype& archetype, ComponentType type) {
			return archetype.GetMatcherHash(type);
		}
		[[nodiscard]] inline const ComponentInfo*
		GetComponentInfoFromArchetype(const Archetype& archetype, uint32_t componentIdx) {
			const auto& cc = GetComponentCache(archetype.GetWorld());
			return cc.GetComponentInfoFromIdx(componentIdx);
		}
		[[nodiscard]] inline const ComponentInfoList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentInfoList(type);
		}
		[[nodiscard]] inline const ComponentLookupList&
		GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentLookupList(type);
		}
	} // namespace ecs
} // namespace gaia

#include <cinttypes>

#include <cinttypes>
#include <type_traits>

#include <tuple>

namespace gaia {
	namespace ecs {
		class EntityQuery final {
			friend class World;

		public:
			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };
			//! Query constraints
			enum class Constraints { EnabledOnly, DisabledOnly, AcceptAll };
			//! Query matching result
			enum class MatchArchetypeQueryRet { Fail, Ok, Skip };
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;

		private:
			//! Array of component type indices
			using ComponentIndexArray = containers::sarray_ext<uint32_t, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component type indices reserved for filtering
			using ChangeFilterArray = ComponentIndexArray;

			struct ComponentListData {
				ComponentIndexArray list[ListType::LT_Count]{};
				uint64_t hash[ListType::LT_Count]{};
			};
			//! List of querried components
			ComponentListData m_list[ComponentType::CT_Count]{};
			//! List of filtered components
			ChangeFilterArray m_listChangeFiltered[ComponentType::CT_Count]{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 0;
			//! Lookup hash for this query
			utils::direct_hash_key m_hashLookup{};
			//! List of cached archetypes
			containers::darray<Archetype*> m_archetypeCache;
			//! Tell what kinds of chunks are going to be accepted by the query
			Constraints m_constraints = Constraints::EnabledOnly;
			//! If true, we need to recalculate hashes
			bool m_recalculate = true;
			//! If true, sorting infos is necessary
			bool m_sort = true;

			template <typename T>
			bool HasComponent_Internal([[maybe_unused]] const ComponentIndexArray& arr) const {
				if constexpr (std::is_same<T, Entity>::value) {
					// Skip Entity input args
					return true;
				} else {
					const auto infoIndex = utils::type_info::index<T>();
					return utils::has(arr, infoIndex);
				}
			}

			template <typename T>
			void AddComponent_Internal([[maybe_unused]] ComponentIndexArray& arr) {
				if constexpr (std::is_same<T, Entity>::value) {
					// Skip Entity input args
					return;
				} else {
					const auto infoIndex = utils::type_info::index<T>();

					// Unique infos only
					if (utils::has(arr, infoIndex))
						return;

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(
								false && "Trying to create an ECS query with too many "
												 "components!");

						constexpr auto typeName = utils::type_info::name<T>();
						LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)typeName.size(),
								typeName.data());

						return;
					}
#endif

					arr.push_back(infoIndex);
					m_recalculate = true;
					m_sort = true;

					// Any updates to components in the query invalidate the cache.
					// If possible, we should first prepare the query and not touch if afterwards.
					GAIA_ASSERT(m_archetypeCache.empty());
					m_archetypeCache.clear();
				}
			}

			template <typename T>
			void SetChangedFilter_Internal(ChangeFilterArray& arrFilter, ComponentListData& componentListData) {
				static_assert(!std::is_same<T, Entity>::value, "It doesn't make sense to use ChangedFilter with Entity");

				const auto infoIndex = utils::type_info::index<T>();

				// Unique infos only
				GAIA_ASSERT(!utils::has(arrFilter, infoIndex) && "Filter doesn't contain unique infos");

#if GAIA_DEBUG
				// There's a limit to the amount of components which we can store
				if (arrFilter.size() >= MAX_COMPONENTS_IN_QUERY) {
					GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

					constexpr auto typeName = utils::type_info::name<T>();
					LOG_E(
							"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)typeName.size(),
							typeName.data());

					return;
				}
#endif

				// Component has to be present in anyList or allList.
				// NoneList makes no sense because we skip those in query processing anyway.
				if (utils::has_if(componentListData.list[ListType::LT_Any], [infoIndex](auto idx) {
							return idx == infoIndex;
						})) {
					arrFilter.push_back(infoIndex);
					return;
				}
				if (utils::has_if(componentListData.list[ListType::LT_All], [infoIndex](auto idx) {
							return idx == infoIndex;
						})) {
					arrFilter.push_back(infoIndex);
					return;
				}

				GAIA_ASSERT("SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
				constexpr auto typeName = utils::type_info::name<T>();
				LOG_E(
						"SetChangeFilter trying to filter ECS component %.*s but "
						"it's not a part of the query!",
						(uint32_t)typeName.size(), typeName.data());
#endif
			}

			template <typename... T>
			void SetChangedFilter(ChangeFilterArray& arr, ComponentListData& componentListData) {
				(SetChangedFilter_Internal<T>(arr, componentListData), ...);
			}

			//! Sorts internal component arrays by their type indices
			void SortComponentArrays() {
				for (auto& l: m_list) {
					for (auto& arr: l.list) {
						utils::sort(arr, [](uint32_t left, uint32_t right) {
							return left < right;
						});
					}
				}
			}

			void CalculateLookupHash(const World& world) {
				// Sort the arrays if necessary
				if (m_sort) {
					SortComponentArrays();
					m_sort = false;
				}

				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(m_hashLookup.hash == 0);

				const auto& cc = GetComponentCache(world);

				// Contraints
				uint64_t hashLookup = utils::hash_combine(m_hashLookup.hash, (uint64_t)m_constraints);

				// Filters
				{
					uint64_t hash = 0;
					for (auto infoIndex: m_listChangeFiltered[ComponentType::CT_Generic])
						hash = utils::hash_combine(hash, (uint64_t)infoIndex);
					hash = utils::hash_combine(hash, (uint64_t)m_listChangeFiltered[ComponentType::CT_Generic].size());
					for (auto infoIndex: m_listChangeFiltered[ComponentType::CT_Chunk])
						hash = utils::hash_combine(hash, (uint64_t)infoIndex);
					hash = utils::hash_combine(hash, (uint64_t)m_listChangeFiltered[ComponentType::CT_Chunk].size());
				}

				// Generic components lookup hash
				{
					uint64_t hash = 0;
					const auto& l = m_list[ComponentType::CT_Generic];
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						const auto& arr = l.list[i];
						hash = utils::hash_combine(hash, (uint64_t)i);
						for (size_t j = 0; j < arr.size(); ++j) {
							const auto* info = cc.GetComponentInfoFromIdx(arr[j]);
							GAIA_ASSERT(info != nullptr);
							hash = utils::hash_combine(hash, info->lookupHash);
						}
					}
					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				// Chunk components lookup hash
				{
					uint64_t hash = 0;
					const auto& l = m_list[ComponentType::CT_Chunk];
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						const auto& arr = l.list[i];
						hash = utils::hash_combine(hash, (uint64_t)i);
						for (size_t j = 0; j < arr.size(); ++j) {
							const auto* info = cc.GetComponentInfoFromIdx(arr[j]);
							GAIA_ASSERT(info != nullptr);
							hash = utils::hash_combine(hash, info->lookupHash);
						}
					}
					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				m_hashLookup = {utils::calculate_hash64(hashLookup)};
			}

			void CalculateMatcherHashes(const World& world) {
				if (!m_recalculate)
					return;
				m_recalculate = false;

				// Sort the arrays if necessary
				if (m_sort) {
					m_sort = false;
					SortComponentArrays();
				}

				// Calculate the matcher hash
				{
					const auto& cc = GetComponentCache(world);

					for (auto& l: m_list) {
						for (size_t i = 0; i < ListType::LT_Count; ++i) {
							auto& arr = l.list[i];

							if (!arr.empty()) {
								const auto* info = cc.GetComponentInfoFromIdx(arr[0]);
								GAIA_ASSERT(info != nullptr);
								l.hash[i] = info->matcherHash;
							}
							for (size_t j = 1; j < arr.size(); ++j) {
								const auto* info = cc.GetComponentInfoFromIdx(arr[j]);
								GAIA_ASSERT(info != nullptr);
								l.hash[i] = utils::combine_or(l.hash[i], info->matcherHash);
							}
						}
					}
				}
			}

			/*!
				Tries to match \param componentInfos with a given \param matcherHash.
				\return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
								MatchArchetypeQueryRet::Skip is not relevant.
				*/
			template <ComponentType TComponentType>
			[[nodiscard]] MatchArchetypeQueryRet Match(const ComponentInfoList& componentInfos, uint64_t matcherHash) const {
				const auto& queryList = GetData(TComponentType);
				const uint64_t withNoneTest = matcherHash & queryList.hash[ListType::LT_None];
				const uint64_t withAnyTest = matcherHash & queryList.hash[ListType::LT_Any];
				const uint64_t withAllTest = matcherHash & queryList.hash[ListType::LT_All];

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hash[ListType::LT_All] != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hash[ListType::LT_Any] != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with the withNoneList we quit
				if (withNoneTest != 0) {
					for (const auto infoIndex: queryList.list[ListType::LT_None]) {
						for (const auto& info: componentInfos) {
							if (info->infoIndex == infoIndex) {
								return MatchArchetypeQueryRet::Fail;
							}
						}
					}
				}

				// If there is any match with the withAnyTest
				if (withAnyTest != 0) {
					for (const auto infoIndex: queryList.list[ListType::LT_Any]) {
						for (const auto& info: componentInfos) {
							if (info->infoIndex == infoIndex)
								goto checkWithAllMatches;
						}
					}

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					// If the number of queried components is greater than the
					// number of components in archetype there's no need to search
					if (queryList.list[ListType::LT_All].size() <= componentInfos.size()) {
						uint32_t matches = 0;

						// m_list[ListType::LT_All] first because we usually request for less
						// components than there are components in archetype
						for (const auto infoIndex: queryList.list[ListType::LT_All]) {
							for (const auto& info: componentInfos) {
								if (info->infoIndex != infoIndex)
									continue;

								// All requirements are fulfilled. Let's iterate
								// over all chunks in archetype
								if (++matches == queryList.list[ListType::LT_All].size())
									return MatchArchetypeQueryRet::Ok;

								break;
							}
						}
					}

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

			[[nodiscard]] bool operator==(const EntityQuery& other) const {
				// Lookup hash must match
				if (m_hashLookup != other.m_hashLookup)
					return false;

				if (m_constraints != other.m_constraints)
					return false;

				for (size_t j = 0; j < ComponentType::CT_Count; ++j) {
					const auto& queryList = m_list[j];
					const auto& otherList = other.m_list[j];

					// Component count needes to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.list[i].size() != otherList.list[i].size())
							return false;
					}

					// Matches hashes need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.hash[i] != otherList.hash[i])
							return false;
					}

					// Filter count needs to be the same
					if (m_listChangeFiltered[j].size() != other.m_listChangeFiltered[j].size())
						return false;

					// Components need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						const auto ret = std::memcmp(
								(const void*)&queryList.list[i], (const void*)&otherList.list[i],
								queryList.list[i].size() * sizeof(queryList.list[0]));
						if (ret != 0)
							return false;
					}

					// Filters need to be the same
					{
						const auto ret = std::memcmp(
								(const void*)&m_listChangeFiltered[j], (const void*)&other.m_listChangeFiltered[j],
								m_listChangeFiltered[j].size() * sizeof(m_listChangeFiltered[0]));
						if (ret != 0)
							return false;
					}
				}

				return false;
			}

			[[nodiscard]] bool operator!=(const EntityQuery& other) const {
				return !operator==(other);
			}

			/*!
			Tries to match the query against \param archetypes. For each matched archetype the archetype is cached.
			This is necessary so we do not iterate all chunks over and over again when running queries.
			*/
			void Match(const containers::darray<Archetype*>& archetypes) {
				if (m_recalculate && !archetypes.empty()) {
					const auto& world = archetypes[0]->GetWorld();
					CalculateMatcherHashes(world);
				}

				for (size_t i = m_lastArchetypeId; i < archetypes.size(); i++) {
					auto* pArchetype = archetypes[i];
#if GAIA_DEBUG
					auto& archetype = *pArchetype;
#else
					const auto& archetype = *pArchetype;
#endif

					// Early exit if generic query doesn't match
					const auto retGeneric = Match<ComponentType::CT_Generic>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Generic),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Generic));
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = Match<ComponentType::CT_Chunk>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Chunk),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Chunk));
					if (retChunk == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// If at least one query succeeded run our logic
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Ok ||
							retChunk == EntityQuery::MatchArchetypeQueryRet::Ok)
						m_archetypeCache.push_back(pArchetype);
				}

				m_lastArchetypeId = (uint32_t)archetypes.size();
			}

			void SetWorldVersion(uint32_t worldVersion) {
				m_worldVersion = worldVersion;
			}

			template <typename T>
			EntityQuery& AddComponent_Internal(ListType listType) {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>::value)
					AddComponent_Internal<U>(m_list[ComponentType::CT_Generic].list[listType]);
				else
					AddComponent_Internal<U>(m_list[ComponentType::CT_Chunk].list[listType]);
				return *this;
			}

			template <typename T>
			bool HasComponent_Internal(ListType listType) const {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>::value)
					return HasComponent_Internal<U>(m_list[ComponentType::CT_Generic].list[listType]);
				else
					return HasComponent_Internal<U>(m_list[ComponentType::CT_Chunk].list[listType]);
			}

			template <typename T>
			EntityQuery& WithChanged_Internal() {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>::value)
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Generic], m_list[ComponentType::CT_Generic]);
				else
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Chunk], m_list[ComponentType::CT_Chunk]);
				return *this;
			}

		public:
			[[nodiscard]] const ComponentListData& GetData(ComponentType type) const {
				return m_list[type];
			}

			[[nodiscard]] const ChangeFilterArray& GetFiltered(ComponentType type) const {
				return m_listChangeFiltered[type];
			}

			EntityQuery& SetConstraints(Constraints value) {
				m_constraints = value;
				return *this;
			}

			[[nodiscard]] bool CheckConstraints(bool enabled) const {
				return m_constraints == Constraints::AcceptAll || (enabled && m_constraints == Constraints::EnabledOnly) ||
							 (!enabled && m_constraints == Constraints::DisabledOnly);
			}

			[[nodiscard]] bool HasFilters() const {
				return !m_listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !m_listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& Any() {
				(AddComponent_Internal<T>(ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& All() {
				(AddComponent_Internal<T>(ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& None() {
				(AddComponent_Internal<T>(ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			bool HasAny() const {
				return (HasComponent_Internal<T>(ListType::LT_Any) || ...);
			}

			template <typename... T>
			bool HasAll() const {
				return (HasComponent_Internal<T>(ListType::LT_All) && ...);
			}

			template <typename... T>
			bool HasNone() const {
				return (!HasComponent_Internal<T>(ListType::LT_None) && ...);
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& WithChanged() {
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			[[nodiscard]] uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}

			[[nodiscard]] containers::darray<Archetype*>::iterator begin() {
				return m_archetypeCache.begin();
			}

			[[nodiscard]] containers::darray<Archetype*>::iterator end() {
				return m_archetypeCache.end();
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {

		//----------------------------------------------------------------------

		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			template <typename T>
			ComponentSetter& SetComponent(typename DeduceComponent<T>::Type&& data) {
				if constexpr (IsGenericComponent<T>::value) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					m_pChunk->template SetComponent<T>(m_idx, std::forward<U>(data));
					return *this;
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					m_pChunk->template SetComponent<T>(std::forward<U>(data));
					return *this;
				}
			}
		};

		//----------------------------------------------------------------------

		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Allocator used to allocate chunks
			ChunkAllocator m_chunkAllocator;
			//! Cache of components used by the world
			ComponentCache m_componentCache;

			containers::map<utils::direct_hash_key, containers::darray<EntityQuery>> m_cachedQueries;
			//! Map or archetypes mapping to the same hash - used for lookups
			containers::map<utils::direct_hash_key, containers::darray<Archetype*>> m_archetypeMap;
			//! List of archetypes - used for iteration
			containers::darray<Archetype*> m_archetypes;
			//! Root archetype
			Archetype* m_rootArchetype;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			containers::darray<EntityContainer> m_entities;
			//! Index of the next entity to recycle
			uint32_t m_nextFreeEntity = Entity::IdMask;
			//! Number of entites to recycle
			uint32_t m_freeEntities = 0;

			//! List of chunks to delete
			containers::darray<Chunk*> m_chunksToRemove;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

			void* AllocateChunkMemory() {
				return m_chunkAllocator.Allocate();
			}

			void ReleaseChunkMemory(void* mem) {
				m_chunkAllocator.Release(mem);
			}

		public:
			ComponentCache& GetComponentCache() {
				return m_componentCache;
			}

			const ComponentCache& GetComponentCache() const {
				return m_componentCache;
			}

			void UpdateWorldVersion() {
				UpdateVersion(m_worldVersion);
			}

			[[nodiscard]] bool IsEntityValid(Entity entity) const {
				// Entity ID has to fit inside entity array
				if (entity.id() >= m_entities.size())
					return false;

				auto& entityContainer = m_entities[entity.id()];
				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;
				// If chunk information is present the entity at the pointed index has
				// to match our entity
				if (entityContainer.pChunk && entityContainer.pChunk->GetEntity(entityContainer.idx) != entity)
					return false;

				return true;
			}

			void Cleanup() {
				// Clear entities
				{
					m_entities = {};
					m_nextFreeEntity = 0;
					m_freeEntities = 0;
				}

				// Clear archetypes
				{
					m_chunksToRemove = {};

					// Delete all allocated chunks and their parent archetypes
					for (auto archetype: m_archetypes)
						delete archetype;

					m_archetypes = {};
					m_archetypeMap = {};
				}
			}

		private:
			/*!
			Remove an entity from chunk.
			\param pChunk Chunk we remove the entity from
			\param entityChunkIndex Index of entity within its chunk
			*/
			void RemoveEntity(Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_ASSERT(
						!pChunk->header.owner.info.structuralChangesLocked &&
						"Entities can't be removed while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				pChunk->RemoveEntity(entityChunkIndex, m_entities);

				if (
						// Skip chunks which already requested removal
						pChunk->header.info.lifespan > 0 ||
						// Skip non-empty chunks
						pChunk->HasEntities())
					return;

				// When the chunk is emptied we want it to be removed. We can't do it
				// right away and need to wait for world's GC to be called.
				//
				// However, we need to prevent the following:
				//    1) chunk is emptied + add to some removal list
				//    2) chunk is reclaimed
				//    3) chunk is emptied again + add to some removal list again
				//
				// Therefore, we have a flag telling us the chunk is already waiting to
				// be removed. The chunk might be reclaimed before GC happens but it
				// simply ignores such requests. This way GC always has at most one
				// record for removal for any given chunk.
				pChunk->header.info.lifespan = MAX_CHUNK_LIFESPAN;

				m_chunksToRemove.push_back(pChunk);
			}

			/*!
			Searches for archetype with a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\param lookupHash Archetype lookup hash
			\return Pointer to archetype or nullptr
			*/
			[[nodiscard]] Archetype* FindArchetype(
					std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk,
					utils::direct_hash_key lookupHash) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;

				auto checkInfos = [&](const ComponentInfoList& list, const std::span<const ComponentInfo*>& infos) {
					for (uint32_t j = 0; j < infos.size(); j++) {
						// Different components. We need to search further
						if (list[j] != infos[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (const auto archetype: archetypeArray) {
					const auto& genericComponentList = archetype->componentInfos[ComponentType::CT_Generic];
					if (genericComponentList.size() != infosGeneric.size())
						continue;
					const auto& chunkComponentList = archetype->componentInfos[ComponentType::CT_Chunk];
					if (chunkComponentList.size() != infosChunk.size())
						continue;

					if (checkInfos(genericComponentList, infosGeneric) && checkInfos(chunkComponentList, infosChunk))
						return archetype;
				}

				return nullptr;
			}

#if GAIA_ARCHETYPE_GRAPH
			/*!
			Creates a new archetype from a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			[[nodiscard]] Archetype*
			CreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				// Make sure to sort the component infos so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				utils::sort(infosGeneric, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});
				utils::sort(infosChunk, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});

				const auto genericHash = CalculateLookupHash(infosGeneric);
				const auto chunkHash = CalculateLookupHash(infosChunk);
				const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, chunkHash});

				auto newArch = Archetype::Create(*this, infosGeneric, infosChunk);

				newArch->genericHash = genericHash;
				newArch->chunkHash = chunkHash;
				newArch->lookupHash = lookupHash;

				return newArch;
			}
#else
			/*!
			Creates a new archetype from a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			[[nodiscard]] Archetype*
			CreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				return Archetype::Create(*this, infosGeneric, infosChunk);
			}

			void
			InitArchetype(Archetype* archetype, uint64_t genericHash, uint64_t chunkHash, utils::direct_hash_key lookupHash) {
				archetype->genericHash = genericHash;
				archetype->chunkHash = chunkHash;
				archetype->lookupHash = lookupHash;
			}

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is
			created. \param infosGeneric Span of generic component infos \param infosChunk Span of chunk component infos
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindOrCreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				// Make sure to sort the component infos so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				utils::sort(infosGeneric, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});
				utils::sort(infosChunk, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});

				// Calculate hash for our combination of components
				const auto genericHash = CalculateLookupHash(infosGeneric);
				const auto chunkHash = CalculateLookupHash(infosChunk);
				utils::direct_hash_key lookupHash = {
						CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, chunkHash})};

				Archetype* archetype = FindArchetype(infosGeneric, infosChunk, lookupHash);
				if (archetype == nullptr) {
					archetype = CreateArchetype(infosGeneric, infosChunk);
					InitArchetype(archetype, genericHash, chunkHash, lookupHash);
					RegisterArchetype(archetype);
				}

				return archetype;
			}
#endif

			void RegisterArchetype(Archetype* archetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(archetype == m_rootArchetype || (archetype->genericHash != 0 || archetype->chunkHash != 0));
				GAIA_ASSERT(archetype == m_rootArchetype || archetype->lookupHash.hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!utils::has(m_archetypes, archetype));

				// Register the archetype
				archetype->id = (uint32_t)m_archetypes.size();
				m_archetypes.push_back(archetype);

				auto it = m_archetypeMap.find(archetype->lookupHash);
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[archetype->lookupHash] = {archetype};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, archetype));
					archetypes.push_back(archetype);
				}
			}

#if GAIA_DEBUG
			void VerifyAddComponent(Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToAdd) {
				const auto& lookup = archetype.componentLookupData[type];
				const size_t oldInfosCount = lookup.size();

				// Make sure not to add too many infos
				if (!VerityArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					LOG_W("Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					LOG_W("Already present:");
					for (size_t i = 0; i < oldInfosCount; i++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					LOG_W("Trying to add:");
					{
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToAdd->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (size_t i = 0; i < lookup.size(); ++i) {
					const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
					if (info.infoIndex == infoToAdd->infoIndex) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentTypeString[type], entity.id(),
								entity.gen());
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			void VerifyRemoveComponent(
					Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToRemove) {

	#if GAIA_ARCHETYPE_GRAPH
				auto ret = archetype.FindDelEdgeArchetype(type, infoToRemove);
				if (ret == nullptr) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					const auto& infos = archetype.componentInfos[type];
					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#else
				const auto& infos = archetype.componentInfos[type];
				if (!utils::has_if(infos, [&](const auto* info) {
							return info == infoToRemove;
						})) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#endif
			}
#endif

#if GAIA_ARCHETYPE_GRAPH
			GAIA_FORCEINLINE void
			BuildGraphEdges(ComponentType type, Archetype* left, Archetype* right, const ComponentInfo* info) {
				left->edgesAdd[type].push_back({info, right});
				right->edgesDel[type].push_back({info, left});
			}

			/*!
			Checks if archetype \param left is a superset of archetype \param right (contains all its infos)
			\param left Entity to delete
			\param right Entity to delete
			\return Returns true if left is a superset of right
			*/
			bool IsSuperSet(ComponentType type, Archetype& left, Archetype& right) {
				size_t i = 0;
				size_t j = 0;

				const auto& infosLeft = left.componentInfos[type];
				const auto& infosRight = right.componentInfos[type];
				if (infosLeft.size() < infosRight.size())
					return false;

				// Arrays are sorted so we can do linear intersection lookup
				while (i < infosLeft.size() && j < infosRight.size()) {
					const auto* infoLeft = infosLeft[i];
					const auto* infoRight = infosRight[j];

					if (infoLeft == infoRight) {
						++i;
						++j;
					} else if (infoLeft->infoIndex < infoRight->infoIndex)
						++i;
					else
						return false;
				}

				return j == infosRight.size();
			}
#endif

			/*!
			Searches for an archetype based on the given set of components. If no archetype is found a new one is created.
			\param oldArchetype Original archetype
			\param type Component infos
			\param infoToAdd Span of chunk components
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindOrCreateArchetype(Archetype* oldArchetype, ComponentType type, const ComponentInfo* infoToAdd) {
				auto* node = oldArchetype;

#if GAIA_ARCHETYPE_GRAPH
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we simply make a lookup.
				if (node == m_rootArchetype) {
					if (type == ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd->lookupHash;
						const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, 0});
						node = FindArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {}, lookupHash);
						if (node == nullptr) {
							node = CreateArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {});
							RegisterArchetype(node);
							node->edgesDel[type].push_back({infoToAdd, m_rootArchetype});
						}
					} else {
						const auto chunkHash = infoToAdd->lookupHash;
						const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{0, chunkHash});
						node = FindArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1), lookupHash);
						if (node == nullptr) {
							node = CreateArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1));
							RegisterArchetype(node);
							node->edgesDel[type].push_back({infoToAdd, m_rootArchetype});
						}
					}
				}

				const auto it = utils::find_if(node->edgesAdd[type], [infoToAdd](const auto& edge) {
					return edge.info == infoToAdd;
				});

				// Not found among edges, create a new archetype
				if (it == node->edgesAdd[type].end())
#endif
				{
					const auto& archetype = *node;

					const auto& componentInfos = archetype.componentInfos[type];
					const auto& componentInfos2 = archetype.componentInfos[(type + 1) & 1];

					// Prepare a joint array of component infos of old + the newly added component
					containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> newInfos;
					{
						const size_t newSize = componentInfos.size();
						newInfos.resize(newSize + 1);

						size_t j = 0;
						for (; j < newSize; ++j)
							newInfos[j] = componentInfos[j];
						newInfos[j] = infoToAdd;
					}

#if GAIA_ARCHETYPE_GRAPH
					auto newArchetype =
							type == ComponentType::CT_Generic
									? CreateArchetype(
												{newInfos.data(), newInfos.size()}, {componentInfos2.data(), componentInfos2.size()})
									: CreateArchetype(
												{componentInfos2.data(), componentInfos2.size()}, {newInfos.data(), newInfos.size()});

					RegisterArchetype(newArchetype);
					BuildGraphEdges(type, node, newArchetype, infoToAdd);
#else
					// Prepare an array of old component infos for our other type. This is a simple copy.
					containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> otherInfos;
					{
						const size_t otherSize = componentInfos2.size();
						otherInfos.resize(otherSize);

						for (size_t j = 0; j < otherSize; ++j)
							otherInfos[j] = componentInfos2[j];
					}

					auto newArchetype =
							type == ComponentType::CT_Generic
									? FindOrCreateArchetype({newInfos.data(), newInfos.size()}, {otherInfos.data(), otherInfos.size()})
									: FindOrCreateArchetype({otherInfos.data(), otherInfos.size()}, {newInfos.data(), newInfos.size()});
#endif

					node = newArchetype;
#if GAIA_ARCHETYPE_GRAPH
				} else {
					node = it->archetype;

#endif
				}

				return node;
			}

			/*!
			Searches for a parent archetype that contains the given component of \param type.
			\param archetype Archetype to search from
			\param type Component type
			\param typesToRemove Span of component infos we want to remove
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindArchetype_RemoveComponents(Archetype* archetype, ComponentType type, const ComponentInfo* intoToRemove) {
#if GAIA_ARCHETYPE_GRAPH
				// Follow the graph to the next archetype
				auto* node = archetype->FindDelEdgeArchetype(type, intoToRemove);
				GAIA_ASSERT(node != nullptr);
				return node;
#else
				const auto& infosFirst = archetype->componentInfos[type];
				const auto& infosOther = archetype->componentInfos[(type + 1) & 1];

				containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				{
					// Find the intersection
					for (const auto* info: infosFirst) {
						if (info == intoToRemove)
							goto nextIter;

						infosNew.push_back(info);

					nextIter:
						continue;
					}

					// Return if there's no change
					if (infosNew.size() == infosFirst.size())
						return nullptr;
				}

				auto newArchetype =
						type == ComponentType::CT_Generic
								? FindOrCreateArchetype({infosNew.data(), infosNew.size()}, {infosOther.data(), infosOther.size()})
								: FindOrCreateArchetype({infosOther.data(), infosOther.size()}, {infosNew.data(), infosNew.size()});

				return newArchetype;
#endif
			}

			/*!
			Returns an array of archetypes registered in the world
			\return Array or archetypes
			*/
			const auto& GetArchetypes() const {
				return m_archetypes;
			}

			/*!
			Returns the archetype the entity belongs to.
			\param entity Entity
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk ? const_cast<Archetype*>(&pChunk->header.owner) : nullptr;
			}

			/*!
			Allocates a new entity.
			\return Entity
			*/
			[[nodiscard]] Entity AllocateEntity() {
				if (!m_freeEntities) {
					const auto entityCnt = m_entities.size();
					// We don't want to go out of range for new entities
					GAIA_ASSERT(entityCnt < Entity::IdMask && "Trying to allocate too many entities!");

					m_entities.push_back({});
					return {(EntityId)entityCnt, 0};
				} else {
					// Make sure the list is not broken
					GAIA_ASSERT(m_nextFreeEntity < m_entities.size() && "ECS recycle list broken!");

					--m_freeEntities;
					const auto index = m_nextFreeEntity;
					m_nextFreeEntity = m_entities[m_nextFreeEntity].idx;
					return {index, m_entities[index].gen};
				}
			}

			/*!
			Deallocates a new entity.
			\param entityToDelete Entity to delete
			*/
			void DeallocateEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities[entityToDelete.id()];
				entityContainer.pChunk = nullptr;

				// New generation
				const auto gen = ++entityContainer.gen;

				// Update our implicit list
				if (!m_freeEntities) {
					m_nextFreeEntity = entityToDelete.id();
					entityContainer.idx = Entity::IdMask;
					entityContainer.gen = gen;
				} else {
					entityContainer.idx = m_nextFreeEntity;
					entityContainer.gen = gen;
					m_nextFreeEntity = entityToDelete.id();
				}
				++m_freeEntities;
			}

			/*!
			Associates an entity with a chunk.
			\param entity Entity to associate with a chunk
			\param pChunk Chunk the entity is to become a part of
			*/
			void StoreEntity(Entity entity, Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->header.owner.info.structuralChangesLocked &&
						"Entities can't be added while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->AddEntity(entity);
				entityContainer.gen = entity.gen();
			}

			/*!
			Moves an entity along with all its generic components from its current to another
			chunk in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void MoveEntity(Entity oldEntity, Archetype& newArchetype) {
				auto& entityContainer = m_entities[oldEntity.id()];
				auto oldChunk = entityContainer.pChunk;
				const auto oldIndex = entityContainer.idx;
				const auto& oldArchetype = oldChunk->header.owner;

				// Find a new chunk for the entity and move it inside.
				// Old entity ID needs to remain valid or lookups would break.
				auto newChunk = newArchetype.FindOrCreateFreeChunk();
				const auto newIndex = newChunk->AddEntity(oldEntity);

				// Find intersection of the two component lists.
				// We ignore chunk components here because they should't be influenced
				// by entities moving around.
				const auto& oldInfos = oldArchetype.componentInfos[ComponentType::CT_Generic];
				const auto& newInfos = newArchetype.componentInfos[ComponentType::CT_Generic];
				const auto& oldLook = oldArchetype.componentLookupData[ComponentType::CT_Generic];
				const auto& newLook = newArchetype.componentLookupData[ComponentType::CT_Generic];

				// Arrays are sorted so we can do linear intersection lookup
				{
					size_t i = 0;
					size_t j = 0;
					while (i < oldInfos.size() && j < newInfos.size()) {
						const auto* infoOld = oldInfos[i];
						const auto* infoNew = newInfos[j];

						if (infoOld == infoNew) {
							// Let's move all type data from oldEntity to newEntity
							const auto idxFrom = oldLook[i++].offset + infoOld->properties.size * oldIndex;
							const auto idxTo = newLook[j++].offset + infoOld->properties.size * newIndex;

							GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

							memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], infoOld->properties.size);
						} else if (infoOld->infoIndex > infoNew->infoIndex)
							++j;
						else
							++i;
					}
				}

				// Remove entity from the previous chunk
				RemoveEntity(oldChunk, oldIndex);

				// Update entity's chunk and index so look-ups can find it
				entityContainer.pChunk = newChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();

				ValidateChunk(oldChunk);
				ValidateChunk(newChunk);
				ValidateEntityList();
			}

			//! Verifies than the implicit linked list of entities is valid
			void ValidateEntityList() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				bool hasThingsToRemove = m_freeEntities > 0;
				if (!hasThingsToRemove)
					return;

				// If there's something to remove there has to be at least one
				// entity left
				GAIA_ASSERT(!m_entities.empty());

				auto freeEntities = m_freeEntities;
				auto nextFreeEntity = m_nextFreeEntity;
				while (freeEntities > 0) {
					GAIA_ASSERT(nextFreeEntity < m_entities.size() && "ECS recycle list broken!");

					nextFreeEntity = m_entities[nextFreeEntity].idx;
					--freeEntities;
				}

				// At this point the index of the last index in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeEntity == Entity::IdMask);
#endif
			}

			//! Verifies than the chunk is valid
			void ValidateChunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (pChunk->HasEntities()) {
					// Make sure a proper amount of entities reference the chunk
					size_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->GetItemCount());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			EntityContainer& AddComponent_Internal(ComponentType type, Entity entity, const ComponentInfo* infoToAdd) {
				auto& entityContainer = m_entities[entity.id()];

				// Adding a component to an entity which already is a part of some chunk
				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, type, infoToAdd);
#endif

					auto* newArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					MoveEntity(entity, *newArchetype);
				}
				// Adding a component to an empty entity
				else {
					auto& archetype = const_cast<Archetype&>(*m_rootArchetype);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, type, infoToAdd);
#endif

					auto* newArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					StoreEntity(entity, newArchetype->FindOrCreateFreeChunk());
				}

				return entityContainer;
			}

			ComponentSetter RemoveComponent_Internal(ComponentType type, Entity entity, const ComponentInfo* infoToRemove) {
				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

				GAIA_ASSERT(
						!archetype.info.structuralChangesLocked && "Components can't be removed while chunk is being iterated "
																											 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
				VerifyRemoveComponent(archetype, entity, type, infoToRemove);
#endif

				auto newArchetype = FindArchetype_RemoveComponents(&archetype, type, infoToRemove);
				GAIA_ASSERT(newArchetype != nullptr);
				MoveEntity(entity, *newArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				m_rootArchetype = CreateArchetype({}, {});
				InitArchetype(m_rootArchetype, 0, 0, {CalculateLookupHash(containers::sarray<uint64_t, 2>{0, 0})});
				RegisterArchetype(m_rootArchetype);
			}

			void Done() {
				Cleanup();
				m_chunkAllocator.Flush();

#if GAIA_DEBUG
				// Make sure there are no leaks
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
				if (memstats.AllocatedMemory != 0) {
					GAIA_ASSERT(false && "ECS leaking memory");
					LOG_W("ECS leaking memory!");
					DiagMemory();
				}
#endif
			}

			/*!
			Creates a new entity from archetype
			\return Entity
			*/
			Entity CreateEntity(Archetype& archetype) {
				Entity entity = AllocateEntity();

				auto* pChunk = m_entities[entity.id()].pChunk;
				if (pChunk == nullptr)
					pChunk = archetype.FindOrCreateFreeChunk();

				StoreEntity(entity, pChunk);

				return entity;
			}

		public:
			World() {
				Init();
			}

			~World() {
				Done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			/*!
			Returns the current version of the world.
			\return World version number
			*/
			[[nodiscard]] uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			/*!
			Creates a new empty entity
			\return Entity
			*/
			[[nodiscard]] Entity CreateEntity() {
				return AllocateEntity();
			}

			/*!
			Creates a new entity by cloning an already existing one.
			\param entity Entity
			\return Entity
			*/
			Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];
				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

					const auto newEntity = CreateEntity(archetype);
					auto& newEntityContainer = m_entities[newEntity.id()];
					auto newChunk = newEntityContainer.pChunk;

					// By adding a new entity m_entities array might have been reallocated.
					// We need to get the new address.
					auto& oldEntityContainer = m_entities[entity.id()];
					auto oldChunk = oldEntityContainer.pChunk;

					// Copy generic component data from reference entity to our new ntity
					const auto& infos = archetype.componentInfos[ComponentType::CT_Generic];
					const auto& looks = archetype.componentLookupData[ComponentType::CT_Generic];

					for (size_t i = 0; i < infos.size(); i++) {
						const auto* info = infos[i];
						if (!info->properties.size)
							continue;

						const auto offset = looks[i].offset;
						const auto idxFrom = offset + info->properties.size * oldEntityContainer.idx;
						const auto idxTo = offset + info->properties.size * newEntityContainer.idx;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

						memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], info->properties.size);
					}

					return newEntity;
				} else
					return CreateEntity();
			}

			/*!
			Removes an entity along with all data associated with it.
			\param entity Entity
			*/
			void DeleteEntity(Entity entity) {
				if (m_entities.empty() || entity == EntityNull)
					return;

				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					RemoveEntity(pChunk, entityContainer.idx);

					// Return entity to pool
					DeallocateEntity(entity);

					ValidateChunk(pChunk);
					ValidateEntityList();
				} else {
					// Return entity to pool
					DeallocateEntity(entity);
				}
			}

			/*!
			Enables or disables an entire entity.
			\param entity Entity
			\param enable Enable or disable the entity
			*/
			void EnableEntity(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->header.owner.info.structuralChangesLocked) &&
						"Entities can't be enabled/disabled while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (enable != (bool)entityContainer.disabled)
					return;
				entityContainer.disabled = !enable;

				if (auto* pChunkFrom = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunkFrom->header.owner);

					// Create a spot in the new chunk
					auto* pChunkTo = enable ? archetype.FindOrCreateFreeChunk() : archetype.FindOrCreateFreeChunkDisabled();
					const auto idxNew = pChunkTo->AddEntity(entity);

					// Copy generic component data from the reference entity to our new entity
					{
						const auto& infos = archetype.componentInfos[ComponentType::CT_Generic];
						const auto& looks = archetype.componentLookupData[ComponentType::CT_Generic];

						for (size_t i = 0; i < infos.size(); i++) {
							const auto* info = infos[i];
							if (!info->properties.size)
								continue;

							const auto offset = looks[i].offset;
							const auto idxFrom = offset + info->properties.size * entityContainer.idx;
							const auto idxTo = offset + info->properties.size * idxNew;

							GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

							memcpy(&pChunkTo->data[idxTo], &pChunkFrom->data[idxFrom], info->properties.size);
						}
					}

					// Remove the entity from the old chunk
					pChunkFrom->RemoveEntity(entityContainer.idx, m_entities);

					// Update the entity container with new info
					entityContainer.pChunk = pChunkTo;
					entityContainer.idx = idxNew;
				}
			}

			/*!
			Returns the number of active entities
			\return Entity
			*/
			[[nodiscard]] uint32_t GetEntityCount() const {
				return (uint32_t)m_entities.size() - m_freeEntities;
			}

			/*!
			Returns an entity at a given position
			\return Entity
			*/
			[[nodiscard]] Entity GetEntity(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			/*!
			Returns a chunk containing the given entity.
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk* GetChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			/*!
			Returns a chunk containing the given entity.
			Index of the entity is stored in \param indexInChunk
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk* GetChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			/*!
			Attaches a new component to \param entity.
			\warning It is expected the component is not there yet and that \param
			entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter AddComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>::value) {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Chunk, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Attaches a component to \param entity. Also sets its value.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter AddComponent(Entity entity, typename DeduceComponent<T>::Type&& data) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>::value) {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(entityContainer.idx, std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Removes a component from \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter RemoveComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>::value) {
					return RemoveComponent_Internal(ComponentType::CT_Generic, entity, info);
				} else {
					return RemoveComponent_Internal(ComponentType::CT_Chunk, entity, info);
				}
			}

			/*!
			Sets the value of component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter SetComponent(Entity entity, typename DeduceComponent<T>::Type&& data) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponent<T>(
						std::forward<typename DeduceComponent<T>::Type>(data));
			}

			/*!
			Returns the value stored in the component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\return Value stored in the component.
			*/
			template <typename T>
			auto GetComponent(Entity entity) const {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;

				if constexpr (IsGenericComponent<T>::value)
					return pChunk->GetComponent<T>(entityContainer.idx);
				else
					return pChunk->GetComponent<T>();
			}

			//----------------------------------------------------------------------

			/*!
			Tells if \param entity contains the component.
			\return True if the component is present on entity.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk)
					return pChunk->HasComponent<T>();

				return false;
			}

			//----------------------------------------------------------------------

		private:
			template <typename T>
			constexpr GAIA_FORCEINLINE auto GetComponentView(Chunk& chunk) const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				if constexpr (IsReadOnlyType<UOriginal>::value)
					return chunk.View_Internal<U>();
				else
					return chunk.ViewRW_Internal<U>();
			}

			//--------------------------------------------------------------------------------

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void
			ForEachEntityInChunk([[maybe_unused]] utils::func_type_list<T...> types, Chunk& chunk, Func func) {
				// Pointers to the respective component types in the chunk, e.g
				// 		w.ForEach(q, [&](Position& p, const Velocity& v) {...}
				// Translates to:
				//  	auto p = chunk.ViewRW_Internal<Position>();
				//		auto v = chunk.View_Internal<Velocity>();
				auto dataPointerTuple = std::make_tuple(GetComponentView<T>(chunk)...);

				// Iterate over each entity in the chunk.
				// Translates to:
				//		for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				//			func(p[i], v[i]);
				const size_t size = chunk.GetItemCount();
				for (size_t i = 0; i < size; ++i)
					func(std::get<decltype(GetComponentView<T>(chunk))>(dataPointerTuple)[i]...);
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachArchetype(EntityQuery& query, Func func) {
				query.Match(m_archetypes);
				for (auto* pArchetype: query)
					func(*pArchetype);
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			void UnpackArgsIntoQuery([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				if constexpr (sizeof...(T) > 0)
					query.All<T...>();
			}

			template <typename... T>
			bool UnpackArgsIntoQuery_Check([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				if constexpr (sizeof...(T) > 0)
					return query.HasAll<T...>();
			}

			template <typename Func>
			static void ResolveQuery(World& world, EntityQuery& query) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				world.UnpackArgsIntoQuery(InputArgs{}, query);
			}

			template <typename Func>
			static bool CheckQuery(World& world, EntityQuery& query) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				return world.UnpackArgsIntoQuery_Check(InputArgs{}, query);
			}

			//--------------------------------------------------------------------------------

			[[nodiscard]] static bool CheckFilters(const EntityQuery& query, const Chunk& chunk) {
				GAIA_ASSERT(chunk.HasEntities() && "CheckFilters called on an empty chunk");

				const auto lastWorldVersion = query.GetWorldVersion();

				// See if any generic component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Generic);
					for (auto infoIndex: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(ComponentType::CT_Generic, infoIndex);
						if (chunk.DidChange(ComponentType::CT_Generic, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// See if any chunk component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Chunk);
					for (auto infoIndex: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(ComponentType::CT_Chunk, infoIndex);
						if (chunk.DidChange(ComponentType::CT_Chunk, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// Skip unchanged chunks.
				return false;
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			static void RunQueryOnChunks_Internal(World& world, EntityQuery& query, Func func) {
				constexpr size_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				// Update the world version
				world.UpdateWorldVersion();

				const bool hasFilters = query.HasFilters();

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](Archetype& archetype) {
					GAIA_ASSERT(archetype.info.structuralChangesLocked < 8);
					++archetype.info.structuralChangesLocked;

					auto exec = [&](const auto& chunksList) {
						size_t chunkOffset = 0;
						size_t indexInBatch = 0;

						size_t itemsLeft = chunksList.size();
						while (itemsLeft > 0) {
							const size_t batchSize = itemsLeft > BatchSize ? BatchSize : itemsLeft;

							// Prepare a buffer to iterate over
							for (size_t j = chunkOffset; j < chunkOffset + batchSize; ++j) {
								auto* pChunk = chunksList[j];

								if (!pChunk->HasEntities())
									continue;
								if (!query.CheckConstraints(!pChunk->IsDisabled()))
									continue;
								if (hasFilters && !CheckFilters(query, *pChunk))
									continue;

								tmp[indexInBatch++] = pChunk;
							}

							// Execute functors in batches
							if (indexInBatch == BatchSize || batchSize != BatchSize) {
								for (size_t chunkIdx = 0; chunkIdx < indexInBatch; ++chunkIdx)
									func(*tmp[chunkIdx]);
								indexInBatch = 0;
							}

							// Prepeare for the next loop
							itemsLeft -= batchSize;
						}
					};

					if (query.CheckConstraints(true))
						exec(archetype.chunks);
					if (query.CheckConstraints(false))
						exec(archetype.chunksDisabled);

					GAIA_ASSERT(archetype.info.structuralChangesLocked > 0);
					--archetype.info.structuralChangesLocked;
				});

				query.SetWorldVersion(world.GetWorldVersion());
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			static void RegisterComponents_Internal([[maybe_unused]] utils::func_type_list<T...> types, World& world) {
				static_assert(sizeof...(T) > 0, "Empty EntityQuery is not supported in this context");
				auto& cc = world.GetComponentCache();
				((void)cc.GetOrCreateComponentInfo<T>(), ...);
			}

			template <typename Func>
			static void RegisterComponents(World& world) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{}, world);
			}

			//--------------------------------------------------------------------------------

			EntityQuery& AddOrFindEntityQueryInCache(World& world, EntityQuery& queryTmp) {
				EntityQuery* query = nullptr;

				auto it = world.m_cachedQueries.find(queryTmp.m_hashLookup);
				if (it == world.m_cachedQueries.end()) {
					const auto hash = queryTmp.m_hashLookup;
					world.m_cachedQueries[hash] = {std::move(queryTmp)};
					query = &world.m_cachedQueries[hash].back();
				} else {
					auto& queries = it->second;

					// Make sure the same hash gets us to the proper query
					for (const auto& q: queries) {
						if (q != queryTmp)
							continue;
						query = &queries.back();
						return *query;
					}

					queries.push_back(std::move(queryTmp));
					query = &queries.back();
				}

				return *query;
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			GAIA_FORCEINLINE void ForEachChunk_External(World& world, EntityQuery& query, Func func) {
				RunQueryOnChunks_Internal(world, query, [&](Chunk& chunk) {
					func(chunk);
				});
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachChunk_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>(world);
				queryTmp.CalculateLookupHash(world);
				RunQueryOnChunks_Internal(world, AddOrFindEntityQueryInCache(world, queryTmp), [&](Chunk& chunk) {
					func(chunk);
				});
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			GAIA_FORCEINLINE void ForEach_External(World& world, EntityQuery& query, Func func) {
#if GAIA_DEBUG
				// Make sure we only use components specificed in the query
				GAIA_ASSERT(CheckQuery<Func>(world, query));
#endif

				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RunQueryOnChunks_Internal(world, query, [&](Chunk& chunk) {
					world.ForEachEntityInChunk(InputArgs{}, chunk, func);
				});
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEach_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>(world);
				queryTmp.CalculateLookupHash(world);

				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RunQueryOnChunks_Internal(world, AddOrFindEntityQueryInCache(world, queryTmp), [&](Chunk& chunk) {
					world.ForEachEntityInChunk(InputArgs{}, chunk, func);
				});
			}

			//--------------------------------------------------------------------------------

		public:
			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Iterating using ecs::Chunk makes it possible to perform optimizations otherwise not possible with
							other methods of iteration as it exposes the chunk itself. On the other hand, it is more verbose
							and takes more lines of code when used.
			*/
			template <typename Func>
			void ForEach(EntityQuery& query, Func func) {
				if constexpr (std::is_invocable<Func, Chunk&>::value)
					ForEachChunk_External((World&)*this, query, func);
				else
					ForEach_External((World&)*this, query, func);
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Iterating using ecs::Chunk makes it possible to perform optimizations otherwise not possible with
							other methods of iteration as it exposes the chunk itself. On the other hand, it is more verbose
							and takes more lines of code when used.
			*/
			template <typename Func>
			void ForEach(EntityQuery&& query, Func func) {
				if constexpr (std::is_invocable<Func, Chunk&>::value)
					ForEachChunk_Internal((World&)*this, std::forward<EntityQuery>(query), func);
				else
					ForEach_Internal((World&)*this, std::forward<EntityQuery>(query), func);
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			EntityQuery instance is generated internally from the input arguments of \param func.
			\warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable ForEach passing
							in a query because it needs to do cached query lookups on each invocation. However, it is easier to use
							and for non-critical code paths it is the most elegant way of iterating your data.
			*/
			template <typename Func>
			void ForEach(Func func) {
				static_assert(
						!std::is_invocable<Func, Chunk&>::value, "Calling query-less ForEach is not supported for chunk iteration");

				EntityQuery query;
				ResolveQuery<Func>((World&)*this, query);
				ForEach_Internal<Func>((World&)*this, std::move(query), func);
			}

			/*!
			Collect garbage. Check all chunks and archetypes which are empty and have not been used for a while
			and tries to delete them and release memory allocated by them.
			*/
			void GC() {
				// Handle chunks
				for (size_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->HasEntities()) {
						pChunk->header.info.lifespan = MAX_CHUNK_LIFESPAN;
						utils::erase_fast(m_chunksToRemove, i);
						continue;
					}

					GAIA_ASSERT(pChunk->header.info.lifespan > 0);
					--pChunk->header.info.lifespan;
					if (pChunk->header.info.lifespan > 0) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove)
					const_cast<Archetype&>(pChunk->header.owner).RemoveChunk(pChunk);
				m_chunksToRemove.clear();
			}

			/*!
			Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			\param archetype Archetype to run diagnostics on
			*/
			void DiagArchetype(const Archetype& archetype) const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (!DiagArchetypes)
					return;
				DiagArchetypes = false;

				// Caclulate the number of entites in archetype
				uint32_t entityCount = 0;
				uint32_t entityCountDisabled = 0;
				for (const auto* chunk: archetype.chunks)
					entityCount += chunk->GetItemCount();
				for (const auto* chunk: archetype.chunksDisabled) {
					entityCountDisabled += chunk->GetItemCount();
					entityCount += chunk->GetItemCount();
				}

				// Print archetype info
				{
					const auto& genericComponents = archetype.componentInfos[ComponentType::CT_Generic];
					const auto& chunkComponents = archetype.componentInfos[ComponentType::CT_Chunk];
					uint32_t genericComponentsSize = 0;
					uint32_t chunkComponentsSize = 0;
					for (const auto& component: genericComponents)
						genericComponentsSize += component->properties.size;
					for (const auto& component: chunkComponents)
						chunkComponentsSize += component->properties.size;

					LOG_N(
							"Archetype ID:%u, "
							"lookupHash:%016" PRIx64 ", "
							"mask:%016" PRIx64 "/%016" PRIx64 ", "
							"chunks:%u, data size:%3u B (%u/%u), "
							"entities:%u/%u (disabled:%u)",
							archetype.id, archetype.lookupHash.hash, archetype.matcherHash[ComponentType::CT_Generic],
							archetype.matcherHash[ComponentType::CT_Chunk], (uint32_t)archetype.chunks.size(),
							genericComponentsSize + chunkComponentsSize, genericComponentsSize, chunkComponentsSize, entityCount,
							archetype.info.capacity, entityCountDisabled);

					auto logComponentInfo = [](const ComponentInfo* info, const ComponentInfoCreate& infoStatic) {
						LOG_N(
								"    (%p) lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s", (void*)info,
								info->lookupHash, info->matcherHash, info->properties.size, info->properties.alig,
								(uint32_t)infoStatic.name.size(), infoStatic.name.data());
					};

					if (!genericComponents.empty()) {
						LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
						for (const auto* component: genericComponents)
							logComponentInfo(component, m_componentCache.GetComponentCreateInfoFromIdx(component->infoIndex));
					}
					if (!chunkComponents.empty()) {
						LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
						for (const auto* component: chunkComponents)
							logComponentInfo(component, m_componentCache.GetComponentCreateInfoFromIdx(component->infoIndex));
					}

#if GAIA_ARCHETYPE_GRAPH
					{
						const auto& edgesG = archetype.edgesAdd[ComponentType::CT_Generic];
						const auto& edgesC = archetype.edgesAdd[ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							LOG_N("  Add edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}

							if (!edgesC.empty()) {
								LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}
						}
					}

					{
						const auto& edgesG = archetype.edgesDel[ComponentType::CT_Generic];
						const auto& edgesC = archetype.edgesDel[ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							LOG_N("  Del edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}

							if (!edgesC.empty()) {
								LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}
						}
					}
#endif

					auto logChunks = [](const auto& chunks) {
						for (size_t i = 0; i < chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							const auto& header = pChunk->header;
							LOG_N(
									"  Chunk #%04u, entities:%u/%u, lifespan:%u", (uint32_t)i, header.items.count,
									header.owner.info.capacity, header.info.lifespan);
						}
					};

					{
						const auto& chunks = archetype.chunks;
						if (!chunks.empty())
							LOG_N("  Enabled chunks");

						logChunks(chunks);
					}

					{
						const auto& chunks = archetype.chunksDisabled;
						if (!chunks.empty())
							LOG_N("  Disabled chunks");

						logChunks(chunks);
					}
				}
			}

			/*!
			Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			*/
			void DiagArchetypes() const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (!DiagArchetypes)
					return;

				// Print archetype info
				LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes) {
					DiagArchetype(*archetype);
					DiagArchetypes = true;
				}

				DiagArchetypes = false;
			}

			/*!
			Performs diagnostics on registered components.
			Prints basic info about them and reports and detected issues.
			*/
			void DiagRegisteredTypes() const {
				static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
				if (!DiagRegisteredTypes)
					return;
				DiagRegisteredTypes = false;

				m_componentCache.Diag();
			}

			/*!
			Performs diagnostics on entites of the world.
			Also performs validation of internal structures which hold the entities.
			*/
			void DiagEntities() const {
				static bool DiagDeletedEntities = GAIA_ECS_DIAG_DELETED_ENTITIES;
				if (!DiagDeletedEntities)
					return;
				DiagDeletedEntities = false;

				ValidateEntityList();

				LOG_N("Deleted entities: %u", m_freeEntities);
				if (m_freeEntities) {
					LOG_N("  --> %u", m_nextFreeEntity);

					uint32_t iters = 0;
					auto fe = m_entities[m_nextFreeEntity].idx;
					while (fe != Entity::IdMask) {
						LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if (!iters || iters > m_freeEntities) {
							LOG_E("  Entities recycle list contains inconsistent "
										"data!");
							break;
						}
					}
				}
			}

			/*!
			Performs diagnostics of the memory used by the world.
			*/
			void DiagMemory() const {
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
				LOG_N("ChunkAllocator stats");
				LOG_N("  Allocated: %" PRIu64 " B", memstats.AllocatedMemory);
				LOG_N("  Used: %" PRIu64 " B", memstats.AllocatedMemory - memstats.UsedMemory);
				LOG_N("  Overhead: %" PRIu64 " B", memstats.UsedMemory);
				LOG_N("  Utilization: %.1f%%", 100.0 * ((double)memstats.UsedMemory / (double)memstats.AllocatedMemory));
				LOG_N("  Pages: %u", memstats.NumPages);
				LOG_N("  Free pages: %u", memstats.NumFreePages);
			}

			/*!
			Performs all diagnostics.
			*/
			void Diag() const {
				DiagArchetypes();
				DiagRegisteredTypes();
				DiagEntities();
				DiagMemory();
			}
		};

		inline ComponentCache& GetComponentCache(World& world) {
			return world.GetComponentCache();
		}
		inline const ComponentCache& GetComponentCache(const World& world) {
			return world.GetComponentCache();
		}
		inline uint32_t GetWorldVersionFromWorld(const World& world) {
			return world.GetWorldVersion();
		}
		inline void* AllocateChunkMemory(World& world) {
			return world.AllocateChunkMemory();
		}
		inline void ReleaseChunkMemory(World& world, void* mem) {
			world.ReleaseChunkMemory(mem);
		}
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {

		struct TempEntity final {
			uint32_t id;
		};

		/*!
		Buffer for deferred execution of some operations on entities.

		Adding and removing components and entities inside World::ForEach or can result
		in changes of archetypes or chunk structure. This would lead to undefined behavior.
		Therefore, such operations have to be executed after the loop is done.
		*/
		class CommandBuffer final {
			enum CommandBufferCmd : uint8_t {
				CREATE_ENTITY,
				CREATE_ENTITY_FROM_ARCHETYPE,
				CREATE_ENTITY_FROM_ENTITY,
				DELETE_ENTITY,
				ADD_COMPONENT,
				ADD_COMPONENT_DATA,
				ADD_COMPONENT_TO_TEMPENTITY,
				ADD_COMPONENT_TO_TEMPENTITY_DATA,
				SET_COMPONENT,
				SET_COMPONENT_FOR_TEMPENTITY,
				REMOVE_COMPONENT
			};

			friend class World;

			World& m_world;
			containers::darray<uint8_t> m_data;
			uint32_t m_entities;

			template <typename TEntity, typename T>
			void AddComponent_Internal(TEntity entity) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(TEntity));

					utils::unaligned_ref<TEntity> to(&m_data[lastIndex]);
					to = entity;
				}
				// Components
				{
					const auto* infoToAdd = GetComponentCache(m_world).GetOrCreateComponentInfo<T>();

					// Component info
					auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(uint32_t));

					utils::unaligned_ref<uint32_t> to(&m_data[lastIndex]);
					to = infoToAdd->infoIndex;

					lastIndex += sizeof(uint32_t);
				}
			}

			template <typename T>
			void SetComponentFinal_Internal(uint32_t& index, T&& data) {
				using U = std::decay_t<T>;

				// Component info
				{
					utils::unaligned_ref<uint32_t> mem((void*)&m_data[index]);
					mem = utils::type_info::index<U>();
				}

				// Component data
				{
					utils::unaligned_ref<U> mem((void*)&m_data[index + sizeof(uint32_t)]);
					mem = std::forward<U>(data);
				}

				index += (uint32_t)(sizeof(uint32_t) + sizeof(U));
			}

			template <typename T>
			void SetComponentNoEntityNoSize_Internal(T&& data) {
				// Data size
				auto lastIndex = (uint32_t)m_data.size();

				constexpr auto ComponentsSize = sizeof(T);
				constexpr auto ComponentTypeIdxSize = sizeof(uint32_t);
				constexpr auto AddSize = ComponentsSize + ComponentTypeIdxSize;
				m_data.resize(m_data.size() + AddSize);

				// Component data
				this->SetComponentFinal_Internal<T>(lastIndex, std::forward<T>(data));
			}

			template <typename T>
			void SetComponentNoEntity_Internal(T&& data) {
				// Register components
				(void)GetComponentCache(m_world).GetOrCreateComponentInfo<T>();

				// Data
				SetComponentNoEntityNoSize_Internal(std::forward<T>(data));
			}

			template <typename TEntity, typename T>
			void SetComponent_Internal(TEntity entity, T&& data) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(TEntity));

					utils::unaligned_ref<TEntity> to(&m_data[lastIndex]);
					to = entity;
				}

				// Components
				SetComponentNoEntity_Internal(std::forward<T>(data));
			}

			template <typename T>
			void RemoveComponent_Internal(Entity entity) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(Entity));

					utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
					to = entity;
				}
				// Components
				{
					const auto* typeToRemove = GetComponentCache(m_world).GetComponentInfo<T>();
					GAIA_ASSERT(typeToRemove != nullptr);

					// Component info
					auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(uint32_t));

					utils::unaligned_ref<uint32_t> to(&m_data[lastIndex]);
					to = typeToRemove->infoIndex;

					lastIndex += sizeof(uint32_t);
				}
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(Archetype& archetype) {
				m_data.push_back(CREATE_ENTITY_FROM_ARCHETYPE);
				const auto archetypeSize = sizeof(void*); // we'll serialize just the pointer
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + archetypeSize);

				utils::unaligned_ref<uintptr_t> to(&m_data[lastIndex]);
				to = (uintptr_t)&archetype;

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_world(world), m_entities(0) {
				m_data.reserve(256);
			}

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity() {
				m_data.push_back(CREATE_ENTITY);
				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(Entity entityFrom) {
				m_data.push_back(CREATE_ENTITY_FROM_ENTITY);
				const auto entitySize = sizeof(entityFrom);
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + entitySize);

				utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
				to = entityFrom;

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				m_data.push_back(DELETE_ENTITY);
				const auto entitySize = sizeof(entity);
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + entitySize);

				utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
				to = entity;
			}

			/*!
			Requests a component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<Entity, U>(entity);
				return true;
			}

			/*!
			Requests a component to be added to temporary entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(TempEntity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_TO_TEMPENTITY);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TempEntity, U>(entity);
				return true;
			}

			/*!
			Requests a component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(Entity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_DATA);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<Entity, U>(entity);
				SetComponentNoEntityNoSize_Internal(std::forward<U>(data));
				return true;
			}

			/*!
			Requests a component to be added to temporary entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(TempEntity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_TO_TEMPENTITY_DATA);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TempEntity, U>(entity);
				SetComponentNoEntityNoSize_Internal(std::forward<U>(data));
				return true;
			}

			/*!
			Requests component data to be set to given values for a given entity.

			\warning Just like World::SetComponent, this function expects the
			given component infos to exist. Undefined behavior otherwise.
			*/
			template <typename T>
			void SetComponent(Entity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(SET_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.

			\warning Just like World::SetComponent, this function expects the
			given component infos to exist. Undefined behavior otherwise.
			*/
			template <typename T>
			void SetComponent(TempEntity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(SET_COMPONENT_FOR_TEMPENTITY);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(REMOVE_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				RemoveComponent_Internal<U>(entity);
			}

			/*!
			Commits all queued changes.
			*/
			void Commit() {
				containers::map<uint32_t, Entity> entityMap;
				uint32_t entities = 0;

				// Extract data from the buffer
				for (size_t i = 0; i < m_data.size();) {
					const auto cmd = m_data[i++];
					switch (cmd) {
						case CREATE_ENTITY: {
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity());
							GAIA_ASSERT(res.second);
						} break;
						case CREATE_ENTITY_FROM_ARCHETYPE: {
							uintptr_t ptr = utils::unaligned_ref<uintptr_t>((void*)&m_data[i]);
							Archetype* archetype = (Archetype*)ptr;
							i += sizeof(void*);
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity(*archetype));
							GAIA_ASSERT(res.second);
						} break;
						case CREATE_ENTITY_FROM_ENTITY: {
							Entity entityFrom = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity(entityFrom));
							GAIA_ASSERT(res.second);
						} break;
						case DELETE_ENTITY: {
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);
							m_world.DeleteEntity(entity);
						} break;
						case ADD_COMPONENT:
						case ADD_COMPONENT_DATA: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);
							// Entity
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache(m_world).GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);
							m_world.AddComponent_Internal(type, entity, newInfo);

							uint32_t indexInChunk;
							auto* pChunk = m_world.GetChunk(entity, indexInChunk);
							GAIA_ASSERT(pChunk != nullptr);

							if (type == ComponentType::CT_Chunk)
								indexInChunk = 0;

							if (cmd == ADD_COMPONENT_DATA) {
								// Skip the component index
								// TODO: Don't include the component index here
								uint32_t infoIndex2 = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								(void)infoIndex2;
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW(type, newInfo->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * newInfo->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], newInfo->properties.size);
								i += newInfo->properties.size;
							}
						} break;
						case ADD_COMPONENT_TO_TEMPENTITY:
						case ADD_COMPONENT_TO_TEMPENTITY_DATA: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);
							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = entityMap.find(e.id());
							// Link has to exist!
							GAIA_ASSERT(it != entityMap.end());

							Entity entity = it->second;

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache(m_world).GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);
							m_world.AddComponent_Internal(type, entity, newInfo);

							uint32_t indexInChunk;
							auto* pChunk = m_world.GetChunk(entity, indexInChunk);
							GAIA_ASSERT(pChunk != nullptr);

							if (type == ComponentType::CT_Chunk)
								indexInChunk = 0;

							if (cmd == ADD_COMPONENT_TO_TEMPENTITY_DATA) {
								// Skip the type index
								// TODO: Don't include the type index here
								uint32_t infoIndex2 = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								(void)infoIndex2;
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW(type, newInfo->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * newInfo->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], newInfo->properties.size);
								i += newInfo->properties.size;
							}
						} break;
						case SET_COMPONENT: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);

							// Entity
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							const auto& entityContainer = m_world.m_entities[entity.id()];
							auto* pChunk = entityContainer.pChunk;
							const auto indexInChunk = type == ComponentType::CT_Chunk ? 0 : entityContainer.idx;

							// Components
							{
								const auto infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								const auto* info = GetComponentCache(m_world).GetComponentInfoFromIdx(infoIndex);
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW(type, info->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * info->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], info->properties.size);
								i += info->properties.size;
							}
						} break;
						case SET_COMPONENT_FOR_TEMPENTITY: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);

							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = entityMap.find(e.id());
							// Link has to exist!
							GAIA_ASSERT(it != entityMap.end());

							Entity entity = it->second;

							const auto& entityContainer = m_world.m_entities[entity.id()];
							auto* pChunk = entityContainer.pChunk;
							const auto indexInChunk = type == ComponentType::CT_Chunk ? 0 : entityContainer.idx;

							// Components
							{
								const auto infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								const auto* info = GetComponentCache(m_world).GetComponentInfoFromIdx(infoIndex);
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW(type, info->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * info->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], info->properties.size);
								i += info->properties.size;
							}
						} break;
						case REMOVE_COMPONENT: {
							// Type
							ComponentType type = utils::unaligned_ref<ComponentType>((void*)&m_data[i]);
							i += sizeof(ComponentType);

							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache(m_world).GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);

							m_world.RemoveComponent_Internal(type, e, newInfo);
						} break;
					}
				}

				m_entities = 0;
				m_data.clear();
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>

#include <cinttypes>
#include <cstring>

#if GAIA_DEBUG
	
#endif

namespace gaia {
	namespace ecs {
		class World;
		class BaseSystemManager;

		constexpr size_t MaxSystemNameLength = 64;

		class BaseSystem {
			friend class BaseSystemManager;

			// A world this system belongs to
			World* m_world = nullptr;
			//! System's name
			char m_name[MaxSystemNameLength]{};
			//! System's hash code
			uint64_t m_hash = 0;
			//! If true, the system is enabled and running
			bool m_enabled = true;
			//! If true, the system is to be destroyed
			bool m_destroy = false;

		protected:
			BaseSystem() = default;
			virtual ~BaseSystem() = default;

			BaseSystem(BaseSystem&&) = delete;
			BaseSystem(const BaseSystem&) = delete;
			BaseSystem& operator=(BaseSystem&&) = delete;
			BaseSystem& operator=(const BaseSystem&) = delete;

		public:
			[[nodiscard]] World& GetWorld() {
				return *m_world;
			}
			[[nodiscard]] const World& GetWorld() const {
				return *m_world;
			}

			//! Enable/disable system
			void Enable(bool enable) {
				bool prev = m_enabled;
				m_enabled = enable;
				if (prev == enable)
					return;

				if (enable)
					OnStarted();
				else
					OnStopped();
			}

			//! Returns true if system is enabled
			[[nodiscard]] bool IsEnabled() const {
				return m_enabled;
			}

		protected:
			//! Called when system is first created
			virtual void OnCreated() {}
			//! Called every time system is started (before the first run and after
			//! Enable(true) is called
			virtual void OnStarted() {}

			//! Called right before every OnUpdate()
			virtual void BeforeOnUpdate() {}
			//! Called every time system is allowed to tick
			virtual void OnUpdate() {}
			//! Called aright after every OnUpdate()
			virtual void AfterOnUpdate() {}

			//! Called every time system is stopped (after Enable(false) is called and
			//! before OnDestroyed when system is being destroyed
			virtual void OnStopped() {}
			//! Called when system are to be cleaned up.
			//! This always happens before OnDestroyed is called or at any point when
			//! simulation decides to bring the system back to the initial state
			//! without actually destroying it.
			virtual void OnCleanup() {}
			//! Called when system is being destroyed
			virtual void OnDestroyed() {}

			//! Returns true for systems this system depends on. False otherwise
			virtual bool DependsOn([[maybe_unused]] const BaseSystem* system) const {
				return false;
			}

		private:
			void SetDestroyed(bool destroy) {
				m_destroy = destroy;
			}
			bool IsDestroyed() const {
				return m_destroy;
			}
		};

		class BaseSystemManager {
		protected:
			World& m_world;
			//! Map of all systems - used for look-ups only
			containers::map<utils::direct_hash_key, BaseSystem*> m_systemsMap;
			//! List of system - used for iteration
			containers::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			containers::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			containers::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			~BaseSystemManager() {
				Clear();
			}

			void Clear() {
				for (auto* pSystem: m_systems)
					pSystem->Enable(false);
				for (auto* pSystem: m_systems)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systems)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systems)
					delete pSystem;

				m_systems.clear();
				m_systemsMap.clear();

				m_systemsToCreate.clear();
				m_systemsToRemove.clear();
			}

			void Cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void Update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToRemove)
					pSystem->Enable(false);
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToRemove)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToRemove) {
					m_systems.erase(utils::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToRemove)
					delete pSystem;
				m_systemsToRemove.clear();

				if (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					SortSystems();

					// Create all new systems
					for (auto* pSystem: m_systemsToCreate) {
						pSystem->OnCreated();
						if (pSystem->IsEnabled())
							pSystem->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnBeforeUpdate();

				for (auto* pSystem: m_systems) {
					if (!pSystem->IsEnabled())
						continue;

					pSystem->BeforeOnUpdate();
					pSystem->OnUpdate();
					pSystem->AfterOnUpdate();
				}

				OnAfterUpdate();
			}

			template <typename T>
			T* CreateSystem(const char* name) {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.emplace(utils::direct_hash_key{hash}, nullptr);
				if (!res.second)
					return (T*)res.first->second;

				BaseSystem* pSystem = new T();
				pSystem->m_world = &m_world;

#if GAIA_COMPILER_MSVC || defined(_WIN32)
				strncpy_s(pSystem->m_name, name, (size_t)-1);
#else
				strncpy(pSystem->m_name, name, MaxSystemNameLength - 1);
#endif
				pSystem->m_name[MaxSystemNameLength - 1] = 0;

				pSystem->m_hash = hash;
				res.first->second = pSystem;

				m_systems.push_back(pSystem);
				// Request initialization of the pSystem
				m_systemsToCreate.push_back(pSystem);

				return (T*)pSystem;
			}

			template <typename T>
			void RemoveSystem() {
				auto pSystem = FindSystem<T>();
				if (pSystem == nullptr || pSystem->IsDestroyed())
					return;

				pSystem->SetDestroyed(true);

				// Request removal of the system
				m_systemsToRemove.push_back(pSystem);
			}

			template <typename T>
			[[nodiscard]] T* FindSystem() {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find({hash});
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void SortSystems() {
				for (size_t l = 0; l < m_systems.size() - 1; l++) {
					auto min = l;
					for (size_t p = l + 1; p < m_systems.size(); p++) {
						const auto* sl = m_systems[l];
						const auto* pl = m_systems[p];
						if (sl->DependsOn(pl))
							min = p;
					}

					auto tmp = m_systems[min];
					m_systems[min] = m_systems[l];
					m_systems[l] = tmp;
				}

#if GAIA_DEBUG
				// Make sure there are no circular dependencies
				for (auto j = 1U; j < m_systems.size(); j++) {
					if (!m_systems[j - 1]->DependsOn(m_systems[j]))
						continue;
					GAIA_ASSERT(false && "Wrong systems dependencies!");
					LOG_E("Wrong systems dependencies!");
				}
#endif
			}
		};

	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class System: public BaseSystem {
			friend class World;
			friend struct ExecutionContextBase;

			//! Last version of the world the system was updated.
			uint32_t m_lastSystemVersion = 0;

		private:
			void BeforeOnUpdate() final {
				GetWorld().UpdateWorldVersion();
			}
			void AfterOnUpdate() final {
				m_lastSystemVersion = GetWorld().GetWorldVersion();
			}

		public:
			//! Returns the world version when the system was updated
			[[nodiscard]] uint32_t GetLastSystemVersion() const {
				return m_lastSystemVersion;
			}
		};

		class SystemManager final: public BaseSystemManager {
			CommandBuffer m_beforeUpdateCmdBuffer;
			CommandBuffer m_afterUpdateCmdBuffer;

		public:
			SystemManager(World& world):
					BaseSystemManager(world), m_beforeUpdateCmdBuffer(world), m_afterUpdateCmdBuffer(world) {}

			CommandBuffer& BeforeUpdateCmdBufer() {
				return m_beforeUpdateCmdBuffer;
			}
			CommandBuffer& AfterUpdateCmdBufer() {
				return m_afterUpdateCmdBuffer;
			}

		protected:
			void OnBeforeUpdate() final {
				m_beforeUpdateCmdBuffer.Commit();
			}

			void OnAfterUpdate() final {
				m_afterUpdateCmdBuffer.Commit();
				m_world.GC();
			}
		};
	} // namespace ecs
} // namespace gaia

