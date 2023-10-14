#pragma once

namespace gaia {
	namespace utils {
		//! Gaia-ECS is a header-only library which means we want to avoid using global
		//! static variables because they would get copied to each translation units.
		//! At the same time the goal is for users to not see any memory allocation used
		//! by the library. Therefore, the only solution is a static variable with local
		//! scope.
		//!
		//! Being a static variable with local scope which means the singleton is guaranteed
		//! to be younger than its caller. Because static variables are released in the reverse
		//! order in which they are created, if used with a static World it would mean we first
		//! release the singleton and only then proceed with the world itself. As a result, in
		//! its destructor the world could access memory that has already been released.
		//!
		//! Instead, we let the singleton allocate the object on the heap and once singleton's
		//! destructor is called we tell the internal object it should destroy itself. This way
		//! there are no memory leaks or access-after-freed issues on app exit reported.
		template <typename T>
		class dyn_singleton final {
			T* m_obj = new T();

			dyn_singleton() = default;

		public:
			static T& get() noexcept {
				static dyn_singleton<T> singleton;
				return *singleton.m_obj;
			}

			dyn_singleton(dyn_singleton&& world) = delete;
			dyn_singleton(const dyn_singleton& world) = delete;
			dyn_singleton& operator=(dyn_singleton&&) = delete;
			dyn_singleton& operator=(const dyn_singleton&) = delete;

			~dyn_singleton() {
				get().Done();
			}
		};
	} // namespace utils
} // namespace gaia