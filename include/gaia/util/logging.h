#pragma once
#include "gaia/config/config_core.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include "gaia/cnt/darray_ext.h"

// Controls how logs can grow in bytes before flush is triggered
#ifndef GAIA_LOG_BUFFER_SIZE
	#define GAIA_LOG_BUFFER_SIZE 32 * 1024
#endif
// Controls how many log entries are possible before flush
#ifndef GAIA_LOG_BUFFER_ENTRIES
	#define GAIA_LOG_BUFFER_ENTRIES 2048
#endif

namespace gaia {
	namespace util {
		using LogLevelType = uint8_t;

		enum class LogLevel : LogLevelType { Debug = 0x1, Info = 0x2, Warning = 0x4, Error = 0x8 };
		inline LogLevelType g_logLevelMask = (LogLevelType)LogLevel::Debug | (LogLevelType)LogLevel::Info |
																				 (LogLevelType)LogLevel::Warning | (LogLevelType)LogLevel::Error;

		//! Enables logging for a given log level
		//! \param level Logging level
		//! \param value True to enable the given logging level. False otherwise.
		inline void log_enable(LogLevel level, bool value) {
			if (value)
				gaia::util::g_logLevelMask |= ((LogLevelType)level);
			else
				gaia::util::g_logLevelMask &= ~((LogLevelType)level);
		}

		//! Returns true if a given logging level is enabled. False otherwise.
		inline bool is_logging_enabled(LogLevel level) {
			return ((LogLevelType)level & g_logLevelMask) != 0;
		}

		using LogLineFunc = void (*)(LogLevel, const char*);
		using LogFunc = void (*)(LogLevel, const char*, va_list);
		using LogFlushFunc = void (*)();

		namespace detail {
			inline constexpr uint32_t LOG_BUFFER_SIZE = GAIA_LOG_BUFFER_SIZE;
			inline constexpr uint32_t LOG_RECORD_LIMIT = GAIA_LOG_BUFFER_ENTRIES;

			inline FILE* get_log_out(LogLevel level) {
				const auto mask = (LogLevelType)level & ((LogLevelType)LogLevel::Error | (LogLevelType)LogLevel::Warning);
				// If a warning or error level is set we will use stderr for output.
				return mask != 0 ? stderr : stdout;
			}

			//! Default implementation of logging a line
			inline void log_line(LogLevel level, const char* msg) {
				FILE* out = get_log_out(level);

				static constexpr const char* colors[] = {
						"\033[1;32mD: ", // Debug
						"\033[0mI: ", // Info
						"\033[1;33mW: ", // Warning
						"\033[1;31mE: " // Error
				};
				// LogLevel is a bitmask. Calculate what bit is and use it as an index.
				const auto lvl = (uint32_t)level;
				const auto idx = GAIA_CLZ(lvl);
				fprintf(out, "%s%s\033[0m\n", colors[idx], msg);
			}
			inline LogLineFunc g_log_line_func = log_line;

			struct LogBuffer {
				struct LogRecord {
					uint32_t offset : 29;
					uint32_t level : 3; // 3 bits for LogLevel mask
				};

				char m_buffer[LOG_BUFFER_SIZE];
				LogRecord m_recs[LOG_RECORD_LIMIT];
				uint32_t m_buffer_pos = 0;
				uint32_t m_recs_pos = 0;

				//! Logs a message.
				//! We implement a buffering strategy. Warnings and errors flush immediately.
				//! Otherwise, we flush once the buffer is filled or on-demand manually.
				//! \param level Logging level
				//! \param len Length of the messagage (including the null-terminating character)
				//! \param msg Message to log
				void log(LogLevel level, uint32_t len, const char* msg) {
					FILE* out = get_log_out(level);
					const bool is_assert = out == stderr;

					// Big message? Write directly
					if (is_assert || len >= detail::LOG_BUFFER_SIZE) {
						// Flush existing buffer first
						flush();

						// Print message directly (bypass cache)
						g_log_line_func(level, msg);
						fflush(out);
						return;
					}

					// Normal caching path. If the message doesn't fit, or if there are too many records, flush.
					if (m_buffer_pos + len > detail::LOG_BUFFER_SIZE || m_recs_pos >= detail::LOG_RECORD_LIMIT)
						flush();

					// Append message to cache
					auto& rec = m_recs[m_recs_pos];
					rec.offset = m_buffer_pos;
					rec.level = (LogLevelType)level;
					memcpy(m_buffer + m_buffer_pos, msg, len);

					m_buffer_pos += len;
					++m_recs_pos;
				}

				void flush() {
					if (m_recs_pos == 0)
						return;

					for (size_t i = 0; i < m_recs_pos; ++i) {
						const auto& rec = m_recs[i];
						g_log_line_func((LogLevel)rec.level, &m_buffer[rec.offset]);
					}

					m_recs_pos = 0;
					m_buffer_pos = 0;
					fflush(stdout);
				}

				LogBuffer() {
					// Disable flushing on the new lines. We will control flushing fully.
					// To avoid issues with Windows’ UCRT we set some reasonable non-zero value.
					setvbuf(stdout, nullptr, _IOFBF, 4096);
					setvbuf(stderr, nullptr, _IOFBF, 4096);
				}
				~LogBuffer() {
					// Flush before the object disappears
					flush();
				}

				LogBuffer(const LogBuffer&) = delete;
				LogBuffer(LogBuffer&&) = delete;
				LogBuffer& operator=(const LogBuffer&) = delete;
				LogBuffer& operator=(LogBuffer&&) = delete;
			};

			inline LogBuffer* g_log() {
				static LogBuffer* s_log = nullptr;
				if (s_log == nullptr) {
					s_log = new LogBuffer();
					// Register automatic cleanup
					static struct LogAtExit {
						LogAtExit() = default;
						~LogAtExit() {
							if (s_log != nullptr) {
								s_log->flush();
								delete s_log;
								s_log = nullptr;
							}
						}
					} s_logDeleter;
				}
				return s_log;
			}

			//! Default implementation of log handler
			inline void log_cached(LogLevel level, const char* fmt, va_list args) {
				// Early exit if there is nothing to write
				va_list args_copy;
				va_copy(args_copy, args);
				int l = vsnprintf(nullptr, 0, fmt, args_copy);
				va_end(args_copy);
				if (l < 0)
					return;

				const auto len = (uint32_t)l;

				cnt::darray_ext<char, 1024> msg(len + 1);
				vsnprintf(msg.data(), msg.size(), fmt, args);
				// Always null-terminate logs
				msg[len] = 0;

				// Log a message.
				// We implement a buffering strategy. Warnings and errors flush immediately.
				// Otherwise, we flush once the buffer is filled or on-demand manually.
				g_log()->log(level, msg.size(), msg.data());
			}

			//! Default implementation of log flushing
			inline void log_flush_cached() {
				g_log()->flush();
			}

			//! Implementation of log handler that logs data directly (no caching)
			inline void log_default(LogLevel level, const char* fmt, va_list args) {
				// Early exit if there is nothing to write
				va_list args_copy;
				va_copy(args_copy, args);
				int l = vsnprintf(nullptr, 0, fmt, args_copy);
				va_end(args_copy);
				if (l < 0)
					return;

				const auto len = (uint32_t)l;

				cnt::darray_ext<char, 1024> msg(len + 1);
				vsnprintf(msg.data(), msg.size(), fmt, args);
				// Always null-terminate logs
				msg[len] = 0;

				g_log_line_func(level, msg.data());
			}

			//! Implementation of log handler that flushes directly (no cachce)
			inline void log_flush_default() {}

			inline LogFunc g_log_func = log_default;
			inline LogFlushFunc g_log_flush_func = log_flush_default;
		} // namespace detail

		//! Set the default function for handling of logs.
		//! This will fully override the default implementation or logging from gaia (format of logs, caching, etc.).
		//! \param func Function pointer. If set to nullptr, default implementation is used.
		inline void set_log_func(LogFunc func) {
			detail::g_log_func = func != nullptr ? func : detail::log_default;
		}

		//! Set the default function for handling one line of log.
		//! \param func Function pointer. If set to nullptr, default implementation is used.
		inline void set_log_line_func(LogLineFunc func) {
			detail::g_log_line_func = func != nullptr ? func : detail::log_line;
		}

		//! Set the default function for handling of log flushing.
		//! \param func Function pointer. If set to nullptr, default implementation is used.
		inline void set_log_flush_func(LogFlushFunc func) {
			detail::g_log_flush_func = func != nullptr ? func : detail::log_flush_default;
		}

		//! Logs a message with a given log level
		//! \param level Logging level
		//! \param fmt Formated message in a format you would use for printf of std::format
		inline void log(LogLevel level, const char* fmt, ...) {
			if (!is_logging_enabled(level))
				return;

			va_list args;
			va_start(args, fmt);
			detail::g_log_func(level, fmt, args);
			va_end(args);
		}

		inline void log_flush() {
			detail::g_log_flush_func();
		}
	} // namespace util
} // namespace gaia

extern "C" {

typedef void (*gaia_log_line_func_t)(gaia::util::LogLevelType level, const char* msg);
inline void gaia_set_log_func(gaia_log_line_func_t func) {
	gaia::util::set_log_line_func((gaia::util::LogLineFunc)func);
}

inline void gaia_log(uint8_t level, const char* msg) {
	gaia::util::log((gaia::util::LogLevel)level, "%s", msg);
}

typedef void (*gaia_log_flush_func_t)();
inline void gaia_set_flush_func(gaia_log_flush_func_t func) {
	gaia::util::set_log_flush_func((gaia::util::LogFlushFunc)func);
}

inline void gaia_flush_logs() {
	gaia::util::log_flush();
}

inline void gaia_log_enable(gaia::util::LogLevelType level, bool value) {
	gaia::util::log_enable((gaia::util::LogLevel)level, value);
}

inline bool gaia_is_logging_enabled(gaia::util::LogLevelType level) {
	return gaia::util::is_logging_enabled((gaia::util::LogLevel)level);
}
}

#define GAIA_LOG_D(...) gaia::util::log(gaia::util::LogLevel::Debug, __VA_ARGS__)
#define GAIA_LOG_N(...) gaia::util::log(gaia::util::LogLevel::Info, __VA_ARGS__)
#define GAIA_LOG_W(...) gaia::util::log(gaia::util::LogLevel::Warning, __VA_ARGS__)
#define GAIA_LOG_E(...) gaia::util::log(gaia::util::LogLevel::Error, __VA_ARGS__)
