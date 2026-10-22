#include "test_common.h"

//------------------------------------------------------------------------------
// Win32
//------------------------------------------------------------------------------

// Gaia must not hand the macros of <windows.h> to the programs that include it.
#if defined(near) || defined(far) || defined(small) || defined(ERROR) || defined(CreateWindow)
	#error "gaia.h leaks <windows.h> macros"
#endif

#include <cstddef>
#include <windows.h>

// <windows.h> after Gaia compiles only while Gaia declares each Win32 function the way the SDK
// does. The constants and layouts Gaia mirrors must match the SDK too.
static_assert(mt::win32::SemaphoreAllAccess == SEMAPHORE_ALL_ACCESS);
static_assert(mt::win32::Infinite == INFINITE);
static_assert(mt::win32::WaitObject0 == WAIT_OBJECT_0);
static_assert(mt::win32::RelationProcessorCore == RelationProcessorCore);
static_assert(
		mt::win32::ProcessorInfoEfficiencyClassOffset ==
		offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor) + offsetof(PROCESSOR_RELATIONSHIP, EfficiencyClass));
static_assert(mt::win32::ThreadPowerThrottling == ThreadPowerThrottling);
static_assert(mt::win32::ThreadPowerThrottlingCurrentVersion == THREAD_POWER_THROTTLING_CURRENT_VERSION);
static_assert(mt::win32::ThreadPowerThrottlingExecutionSpeed == THREAD_POWER_THROTTLING_EXECUTION_SPEED);
static_assert(sizeof(mt::win32::ThreadPowerThrottlingState) == sizeof(THREAD_POWER_THROTTLING_STATE));
