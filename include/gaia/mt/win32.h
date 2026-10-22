#pragma once

#include "gaia/config/config.h"

#if GAIA_PLATFORM_WINDOWS

	#include <cstdint>

//! \cond
// The Win32 API Gaia calls, without <windows.h>. Including <windows.h> from a public header would
// leak its macros (near, far, small, min, max, ERROR, CreateWindow, ...) into every translation unit
// of the program. Each function is declared exactly as the Windows SDK declares it, so programs
// may still include <windows.h> before or after Gaia.
struct _SECURITY_ATTRIBUTES;
struct HINSTANCE__;

extern "C" {
__declspec(dllimport) void* __stdcall CreateSemaphoreExW(
		_SECURITY_ATTRIBUTES* lpSemaphoreAttributes, long lInitialCount, long lMaximumCount, const wchar_t* lpName,
		unsigned long dwFlags, unsigned long dwDesiredAccess);
__declspec(dllimport) int __stdcall ReleaseSemaphore(void* hSemaphore, long lReleaseCount, long* lpPreviousCount);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
__declspec(dllimport) HINSTANCE__* __stdcall GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) intptr_t(__stdcall* __stdcall GetProcAddress(HINSTANCE__* hModule, const char* lpProcName))();
}
//! \endcond

namespace gaia {
	namespace mt {
		namespace win32 {
			//! SEMAPHORE_ALL_ACCESS
			inline constexpr unsigned long SemaphoreAllAccess = 0x1F0003UL;
			//! INFINITE
			inline constexpr unsigned long Infinite = 0xFFFFFFFFUL;
			//! WAIT_OBJECT_0
			inline constexpr unsigned long WaitObject0 = 0UL;

			//! RelationProcessorCore of LOGICAL_PROCESSOR_RELATIONSHIP
			inline constexpr int RelationProcessorCore = 0;
			//! Byte offset of PROCESSOR_RELATIONSHIP::EfficiencyClass in SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX.
			//! The relationship and the size come first, and the union after them is 8-byte aligned.
			inline constexpr uint32_t ProcessorInfoEfficiencyClassOffset = 9;

			//! ThreadPowerThrottling of THREAD_INFORMATION_CLASS
			inline constexpr int ThreadPowerThrottling = 3;
			//! THREAD_POWER_THROTTLING_CURRENT_VERSION
			inline constexpr unsigned long ThreadPowerThrottlingCurrentVersion = 1UL;
			//! THREAD_POWER_THROTTLING_EXECUTION_SPEED
			inline constexpr unsigned long ThreadPowerThrottlingExecutionSpeed = 0x1UL;

			//! Mirrors THREAD_POWER_THROTTLING_STATE.
			struct ThreadPowerThrottlingState {
				unsigned long Version;
				unsigned long ControlMask;
				unsigned long StateMask;
			};

			//! GetLogicalProcessorInformationEx with its enum and struct parameters as int and void*.
			using GetLogicalProcessorInformationExFunc = int(__stdcall*)(int, void*, unsigned long*);
			//! SetThreadInformation with its enum parameter as int.
			using SetThreadInformationFunc = int(__stdcall*)(void*, int, void*, unsigned long);
			//! SetThreadDescription.
			using SetThreadDescriptionFunc = long(__stdcall*)(void*, const wchar_t*);

			//! Looks a function up in kernel32.dll. Functions that take Win32 enums are called through
			//! these lookups, because an enum without a fixed underlying type cannot be declared here
			//! the way the Windows SDK declares it.
			//! \tparam Func Function pointer type to return.
			//! \param name Exported function name.
			//! \return The function, or nullptr when this Windows version does not export it.
			template <typename Func>
			inline Func kernel32_func(const char* name) {
				HINSTANCE__* module = ::GetModuleHandleA("kernel32.dll");
				if (module == nullptr)
					return nullptr;
				return reinterpret_cast<Func>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
			}
		} // namespace win32
	} // namespace mt
} // namespace gaia

#endif
