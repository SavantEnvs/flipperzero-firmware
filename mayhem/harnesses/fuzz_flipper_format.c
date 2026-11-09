// Parses an arbitrary buffer as a Flipper File Format (.fff) document and reads every value type.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FlipperFormat* ff = flipper_format_string_alloc();
    Stream* stream = flipper_format_get_raw_stream(ff);
    stream_write(stream, data, size);
    stream_rewind(stream);

    FuriString* s = furi_string_alloc();
    uint32_t version = 0;
    flipper_format_read_header(ff, s, &version);

    // Keys are taken from the first line of the input, so the fuzzer can reach the value parsers
    // without guessing a fixed name.
    char key[32] = "Key";
    size_t n = 0;
    while(n < size && n < sizeof(key) - 1 && data[n] != ':' && data[n] != '\n') {
        key[n] = (char)data[n];
        n++;
    }
    key[n] = 0;
    const char* keys[] = {key, "Frequency", "Protocol", "Bit", "Key", "Data", "Filetype"};
    for(size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        uint32_t cnt = 0;
        flipper_format_rewind(ff);
        flipper_format_get_value_count(ff, keys[k], &cnt);
        flipper_format_rewind(ff);
        flipper_format_read_string(ff, keys[k], s);
        uint32_t u32[4];
        int32_t i32[4];
        float f[4];
        uint8_t hex[16];
        bool b[4];
        flipper_format_rewind(ff);
        flipper_format_read_uint32(ff, keys[k], u32, 4);
        flipper_format_rewind(ff);
        flipper_format_read_int32(ff, keys[k], i32, 4);
        flipper_format_rewind(ff);
        flipper_format_read_float(ff, keys[k], f, 4);
        flipper_format_rewind(ff);
        flipper_format_read_bool(ff, keys[k], b, 4);
        flipper_format_rewind(ff);
        flipper_format_read_hex(ff, keys[k], hex, sizeof(hex));
        flipper_format_rewind(ff);
        flipper_format_key_exist(ff, keys[k]);
    }
    flipper_format_rewind(ff);
    flipper_format_update_string_cstr(ff, "Key", "updated");
    flipper_format_rewind(ff);
    flipper_format_insert_or_update_uint32(ff, "Num", (uint32_t[]){1, 2}, 2);
    flipper_format_rewind(ff);
    flipper_format_delete_key(ff, key);

    furi_string_free(s);
    flipper_format_free(ff);
    return 0;
}
