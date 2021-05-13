#pragma once

namespace gaia {
	namespace ecs {
		template <typename T>
		class View {
			T* m_data;
			uint32_t m_size;

		public:
			View(T* data): m_data(data) {}

			T* begin() {
				return m_data;
			}
		};

		template <typename T>
		class ViewRO {
			const T* m_data;

		public:
			View(const T* data): m_data(data) {}
		};
	} // namespace ecs
} // namespace gaia