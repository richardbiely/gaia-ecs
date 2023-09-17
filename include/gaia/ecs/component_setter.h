#pragma once
#include <cstdint>

#include "chunk.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				component::VerifyComponent<T>();

				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponent<T>(std::forward<U>(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				component::VerifyComponent<T>();

				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponentSilent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponentSilent<T>(std::forward<U>(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia