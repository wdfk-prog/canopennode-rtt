/**
 * @file CO_402_log.h
 * @brief Optional platform-neutral logging hooks for the CiA 402 profile.
 *
 * Define CO_402_LOG_CUSTOM_HEADER to a quoted header name at compile time to
 * provide one or more CO_402_LOG_E/W/I/D macros. Undefined hooks fall back to
 * no-op macros, so the Pure-C profile keeps no stdio, RTOS, or BSP dependency.
 */

#ifndef CO_402_LOG_H
#define CO_402_LOG_H

/** Optional compile-time include that provides platform log hook macros. */
#if defined(CO_402_LOG_CUSTOM_HEADER)
#include CO_402_LOG_CUSTOM_HEADER
#endif

/** Error-level profile log hook. */
#ifndef CO_402_LOG_E
#define CO_402_LOG_E(...) do { } while (0)
#endif

/** Warning-level profile log hook. */
#ifndef CO_402_LOG_W
#define CO_402_LOG_W(...) do { } while (0)
#endif

/** Informational profile log hook. */
#ifndef CO_402_LOG_I
#define CO_402_LOG_I(...) do { } while (0)
#endif

/** Debug-level profile log hook. */
#ifndef CO_402_LOG_D
#define CO_402_LOG_D(...) do { } while (0)
#endif

#endif /* CO_402_LOG_H */
