#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <core/check.h>

void __furi_crash_implementation(void) {
    abort();
}

void __furi_halt_implementation(void) {
    abort();
}

#include <furi_hal_rtc.h>

FuriHalRtcLocaleUnits furi_hal_rtc_get_locale_units(void) {
    return FuriHalRtcLocaleUnitsMetric;
}

char* itoa(int value, char* str, int base) {
    snprintf(str, 34, base == 16 ? "%x" : "%d", value);
    return str;
}

// <malloc.h> would redeclare malloc/realloc under the compat.h macros.
extern size_t malloc_usable_size(void* ptr);

void* mayhem_realloc(void* ptr, size_t size) {
    size_t old = ptr ? malloc_usable_size(ptr) : 0;
    void* p = (realloc)(ptr, size);
    if(p && size > old) memset((char*)p + old, 0, size - old);
    return p;
}
