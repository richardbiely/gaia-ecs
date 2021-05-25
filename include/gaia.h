// TODO: Temporary until the allocator situation is sorted out
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include "gaia/config/config.h"
#include "gaia/config/logging.h"

#include "gaia/ecs/archetype.h"
#include "gaia/ecs/chunk.h"
#include "gaia/ecs/chunk_allocator.h"
#include "gaia/ecs/command_buffer.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/creation_query.h"
#include "gaia/ecs/entity.h"
#include "gaia/ecs/entity_query.h"
#include "gaia/ecs/world.h"

#include "gaia/utils/data_layout_policy.h"
#include "gaia/utils/hashing_policy.h"
#include "gaia/utils/signals.h"
#include "gaia/utils/type_info.h"
#include "gaia/utils/utils_mem.h"
#include "gaia/utils/utils_std.h"

#define GAIA_INIT GAIA_ECS_COMPONENT_CACHE_H_INIT