#pragma once

#include "gaia/config/config.h"

#include <cstdint>

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		//! Observer event types.
		//! `OnSet` is emitted for explicit value writes to an already present component,
		//! such as `set<T>(entity)`, `set<T>(entity, object)`, `acc_mut(entity).set<T>(...)`,
		//! `modify<T, true>(entity)`, or `modify<T, true>(entity, object)`.
		//! Setter-style APIs emit the event after the new value has been written back.
		//! Query and observer write callbacks emit the event after the callback finishes.
		//! It is not emitted by silent writes such as `sset(...)`, and it is not emitted just because
		//! a component was added for the first time.
		enum class ObserverEvent : uint8_t {
			OnAdd = 0, //!< Add-side event. Negative terms map removal of the excluded id here.
			OnDel = 1, //!< Delete-side event. Negative terms map addition of the excluded id here.
			OnSet = 2, //!< Component value changed on an already present component.
			None = UINT8_MAX //!< Iterator sentinel used outside observer callbacks.
		};
	} // namespace ecs
} // namespace gaia
#endif
