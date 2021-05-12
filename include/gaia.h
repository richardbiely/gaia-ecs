#include "gaia/config/config.h"
#include "gaia/config/logging.h"

#include "gaia/ecs/archetype.h"
#include "gaia/ecs/chunk.h"
#include "gaia/ecs/chunk_allocator.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/creation_query.h"
#include "gaia/ecs/entity.h"
#include "gaia/ecs/entity_command_buffer.h"
#include "gaia/ecs/entity_query.h"
#include "gaia/ecs/world.h"

#include "gaia/utils/data_layout_policy.h"
#include "gaia/utils/hashing_policy.h"
#include "gaia/utils/signals.h"
#include "gaia/utils/type_info.h"
#include "gaia/utils/utils_mem.h"
#include "gaia/utils/utils_std.h"

GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4996)
#include "gaia/external/stack_allocator.h"
GAIA_MSVC_WARNING_POP()
#include "gaia/external/span.hpp"

#define GAIA_INIT                                                              \
	GAIA_ECS_WORLD_H_INIT                                                        \
	GAIA_ECS_META_TYPES_H_INIT