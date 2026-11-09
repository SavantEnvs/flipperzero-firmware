// Executes an arbitrary buffer as source in the mJS JavaScript engine (parser, bytecode, GC, builtins).
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <mjs_core_public.h>
#include <mjs_exec_public.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if(size > 4096) return 0;
    char* src = (char*)malloc(size + 1);
    memcpy(src, data, size);
    src[size] = 0;
    struct mjs* mjs = mjs_create(NULL);
    mjs_val_t res = 0;
    mjs_exec(mjs, src, &res);
    mjs_destroy(mjs);
    free(src);
    return 0;
}
