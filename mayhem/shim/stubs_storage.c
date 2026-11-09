// Storage-backed streams are not reachable from the in-memory harnesses; abort if they ever are.
#include <stdlib.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>

Stream* file_stream_alloc(Storage* storage) {
    (void)storage;
    abort();
}
bool file_stream_open(Stream* stream, const char* path, FS_AccessMode access_mode, FS_OpenMode open_mode) {
    (void)stream; (void)path; (void)access_mode; (void)open_mode;
    abort();
}
bool file_stream_close(Stream* stream) {
    (void)stream;
    abort();
}
Stream* buffered_file_stream_alloc(Storage* storage) {
    (void)storage;
    abort();
}
bool buffered_file_stream_open(Stream* stream, const char* path, FS_AccessMode access_mode, FS_OpenMode open_mode) {
    (void)stream; (void)path; (void)access_mode; (void)open_mode;
    abort();
}
bool buffered_file_stream_close(Stream* stream) {
    (void)stream;
    abort();
}

// mjs_exec_file() reads from the SD card; a host process has none.
char* cs_read_file(const char* path, size_t* size) {
    (void)path;
    if(size) *size = 0;
    return NULL;
}

// No SD card on the host: the record service has no storage, and country lookups behave as if the
// resource file were missing.
#include <lfrfid/tools/iso_3166.h>

void* furi_record_open(const char* name) {
    (void)name;
    return NULL;
}
void furi_record_close(const char* name) {
    (void)name;
}
bool iso_3166_get_two_letter(Storage* storage, uint16_t country_code, FuriString* out) {
    (void)storage; (void)country_code; (void)out;
    return false;
}
bool iso_3166_get_three_letter(Storage* storage, uint16_t country_code, FuriString* out) {
    (void)storage; (void)country_code; (void)out;
    return false;
}
bool iso_3166_get_full_name(Storage* storage, uint16_t country_code, FuriString* out) {
    (void)storage; (void)country_code; (void)out;
    return false;
}
