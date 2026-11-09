#pragma once

#include <core/core_defines.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>
#define FURI_NORETURN noreturn

#ifndef _ATTRIBUTE
#define _ATTRIBUTE(attrs) __attribute__(attrs)
#endif

#ifndef FURI_WARN_UNUSED
#define FURI_WARN_UNUSED __attribute__((warn_unused_result))
#endif
#ifndef FURI_DEPRECATED
#define FURI_DEPRECATED __attribute__((deprecated))
#endif
#ifndef FURI_WEAK
#define FURI_WEAK __attribute__((weak))
#endif
#ifndef FURI_PACKED
#define FURI_PACKED __attribute__((packed))
#endif
#ifndef FURI_ALWAYS_INLINE
#define FURI_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif
#define FURI_IS_IRQ_MASKED() (0)
#define FURI_IS_IRQ_MODE()   (0)
#define FURI_IS_ISR()        (0)
#define FURI_CRITICAL_ENTER()
#define FURI_CRITICAL_EXIT()
#ifndef FURI_CHECK_RETURN
#define FURI_CHECK_RETURN __attribute__((__warn_unused_result__))
#endif
