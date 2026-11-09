// Known-answer tests for the host-built firmware libraries fuzzed from mayhem/harnesses.
// Prints one "PASS <name>" / "FAIL <name>" line per check; test.sh counts them.
//   kat                      run all checks
//   kat --mkfat <type> <out> write a small FAT/exFAT image holding HELLO.TXT (seed generator)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <infrared.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <mjs_core_public.h>
#include <mjs_exec_public.h>
#include <mjs_primitive_public.h>
#include <mjs_string_public.h>
#include <ff.h>
#include <ff_gen_drv.h>

static int g_pass, g_fail;

static void check(const char* name, bool ok) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? g_pass++ : g_fail++;
}

/* ---------------------------------------------------------------- infrared */

static void ir_roundtrip(InfraredProtocol proto, uint32_t addr, uint32_t cmd, uint32_t first_us) {
    char name[64];
    snprintf(name, sizeof(name), "infrared %s encode->decode", infrared_get_protocol_name(proto));
    InfraredMessage in = {.protocol = proto, .address = addr, .command = cmd, .repeat = false};
    InfraredEncoderHandler* enc = infrared_alloc_encoder();
    InfraredDecoderHandler* dec = infrared_alloc_decoder();
    infrared_reset_encoder(enc, &in);
    uint32_t duration, first = 0, n = 0;
    bool level;
    InfraredMessage out = {.protocol = InfraredProtocolUnknown};
    bool got = false;
    InfraredStatus st;
    // Merge back-to-back same-level timings (Manchester halves) the way a receiver sees them.
    bool cur_level = false;
    uint32_t cur_dur = 0;
    while((st = infrared_encode(enc, &duration, &level)) != InfraredStatusError && n < 1000) {
        n++;
        if(level && first == 0) first = duration;
        if(cur_dur && level == cur_level) {
            cur_dur += duration;
        } else {
            if(cur_dur) {
                const InfraredMessage* m = infrared_decode(dec, cur_level, cur_dur);
                if(m && !got) out = *m, got = true;
            }
            cur_level = level;
            cur_dur = duration;
        }
        if(st == InfraredStatusDone) break;
    }
    if(cur_dur) {
        const InfraredMessage* m = infrared_decode(dec, cur_level, cur_dur);
        if(m && !got) out = *m, got = true;
    }
    if(!got) {
        const InfraredMessage* m = infrared_check_decoder_ready(dec);
        if(!m) m = infrared_decode(dec, !cur_level, 200000);
        if(m) out = *m, got = true;
    }
    check(name, got && out.protocol == proto && out.address == addr && out.command == cmd);
    snprintf(name, sizeof(name), "infrared %s first pulse", infrared_get_protocol_name(proto));
    check(name, first == first_us);
    infrared_free_encoder(enc);
    infrared_free_decoder(dec);
}

static void test_infrared(void) {
    ir_roundtrip(InfraredProtocolNEC, 0x07, 0x15, 9000);
    ir_roundtrip(InfraredProtocolSamsung32, 0x07, 0x15, 4500);
    ir_roundtrip(InfraredProtocolRC5, 0x05, 0x15, 888);
    ir_roundtrip(InfraredProtocolRC6, 0x12, 0x34, 2666);
    ir_roundtrip(InfraredProtocolSIRC, 0x01, 0x15, 2400);
    ir_roundtrip(InfraredProtocolRCA, 0x0A, 0xA5, 4000);
    ir_roundtrip(InfraredProtocolPioneer, 0x12, 0x34, 8500);
    check("infrared protocol by name", infrared_get_protocol_by_name("Samsung32") == InfraredProtocolSamsung32);
    check("infrared NEC address length", infrared_get_protocol_address_length(InfraredProtocolNEC) == 8);
    check("infrared frequency", infrared_get_protocol_frequency(InfraredProtocolNEC) == 38000);
}

/* ---------------------------------------------------------- flipper format */

static void test_flipper_format(void) {
    static const char doc[] =
        "Filetype: IR signals file\n"
        "Version: 1\n"
        "# comment line\n"
        "Frequency: 38000\n"
        "Name: Power\n"
        "Counts: 10 20 -30\n"
        "Blob: DE AD BE EF\n"
        "Ratio: 0.5\n"
        "Flag: true\n";
    FlipperFormat* ff = flipper_format_string_alloc();
    stream_write(flipper_format_get_raw_stream(ff), (const uint8_t*)doc, sizeof(doc) - 1);
    stream_rewind(flipper_format_get_raw_stream(ff));

    FuriString* s = furi_string_alloc();
    uint32_t version = 0;
    check(
        "flipper_format header",
        flipper_format_read_header(ff, s, &version) && version == 1 &&
            strcmp(furi_string_get_cstr(s), "IR signals file") == 0);
    uint32_t freq = 0;
    check("flipper_format uint32", flipper_format_read_uint32(ff, "Frequency", &freq, 1) && freq == 38000);
    flipper_format_rewind(ff);
    check(
        "flipper_format string",
        flipper_format_read_string(ff, "Name", s) && strcmp(furi_string_get_cstr(s), "Power") == 0);
    flipper_format_rewind(ff);
    int32_t counts[3] = {0};
    check(
        "flipper_format int32 array",
        flipper_format_read_int32(ff, "Counts", counts, 3) && counts[0] == 10 &&
            counts[1] == 20 && counts[2] == -30);
    flipper_format_rewind(ff);
    uint8_t hex[4] = {0};
    check(
        "flipper_format hex",
        flipper_format_read_hex(ff, "Blob", hex, 4) && hex[0] == 0xDE && hex[1] == 0xAD &&
            hex[2] == 0xBE && hex[3] == 0xEF);
    flipper_format_rewind(ff);
    float ratio = 0;
    check("flipper_format float", flipper_format_read_float(ff, "Ratio", &ratio, 1) && ratio == 0.5f);
    flipper_format_rewind(ff);
    bool flag = false;
    check("flipper_format bool", flipper_format_read_bool(ff, "Flag", &flag, 1) && flag);
    flipper_format_rewind(ff);
    check("flipper_format missing key", !flipper_format_read_uint32(ff, "Nope", &freq, 1));
    flipper_format_rewind(ff);
    uint32_t cnt = 0;
    check("flipper_format value count", flipper_format_get_value_count(ff, "Counts", &cnt) && cnt == 3);
    flipper_format_free(ff);

    ff = flipper_format_string_alloc();
    uint32_t v[2] = {7, 4294967295u};
    flipper_format_write_header_cstr(ff, "Test", 3);
    flipper_format_write_uint32(ff, "Vals", v, 2);
    flipper_format_write_string_cstr(ff, "Label", "hello world");
    flipper_format_rewind(ff);
    uint32_t r[2] = {0};
    version = 0;
    check(
        "flipper_format write->read",
        flipper_format_read_header(ff, s, &version) && version == 3 &&
            flipper_format_read_uint32(ff, "Vals", r, 2) && r[0] == 7 && r[1] == 4294967295u);
    flipper_format_rewind(ff);
    check(
        "flipper_format write string",
        flipper_format_read_string(ff, "Label", s) &&
            strcmp(furi_string_get_cstr(s), "hello world") == 0);
    flipper_format_free(ff);
    furi_string_free(s);
}

/* --------------------------------------------------------------------- mjs */

static void js_int(const char* name, const char* src, int expect) {
    struct mjs* mjs = mjs_create(NULL);
    mjs_val_t res = 0;
    mjs_err_t err = mjs_exec(mjs, src, &res);
    check(name, err == MJS_OK && mjs_is_number(res) && mjs_get_int(mjs, res) == expect);
    mjs_destroy(mjs);
}

static void js_str(const char* name, const char* src, const char* expect) {
    struct mjs* mjs = mjs_create(NULL);
    mjs_val_t res = 0;
    mjs_err_t err = mjs_exec(mjs, src, &res);
    bool ok = err == MJS_OK && mjs_is_string(res) && strcmp(mjs_get_cstring(mjs, &res), expect) == 0;
    check(name, ok);
    mjs_destroy(mjs);
}

static void test_mjs(void) {
    js_int("mjs arithmetic", "1 + 2 * 3", 7);
    js_int("mjs function", "function f(a, b) { return a * b + 1; } f(6, 7);", 43);
    js_int("mjs loop", "let s = 0; for (let i = 1; i <= 10; i++) { s += i; } s;", 55);
    js_int("mjs array", "let a = [3, 4, 5]; a[0] + a[2] + a.length;", 11);
    js_int("mjs object", "let o = {x: 4, y: 5}; o.x * o.y;", 20);
    js_str("mjs string concat", "'foo' + 'bar'", "foobar");
    js_str("mjs string slice", "'abcdef'.slice(1, 3)", "bc");
    struct mjs* mjs = mjs_create(NULL);
    mjs_val_t res = 0;
    check("mjs syntax error", mjs_exec(mjs, "let = = ;", &res) != MJS_OK);
    mjs_destroy(mjs);
}

/* ------------------------------------------------------------------- fatfs */

static uint8_t* g_disk;
static size_t g_sectors;

DWORD get_fattime(void) {
    return ((DWORD)(2024 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
static DSTATUS ram_init(BYTE l) { (void)l; return 0; }
static DSTATUS ram_status(BYTE l) { (void)l; return 0; }
static DRESULT ram_read(BYTE l, BYTE* b, DWORD s, UINT c) {
    (void)l;
    if((size_t)s + c > g_sectors) return RES_PARERR;
    memcpy(b, g_disk + (size_t)s * 512, (size_t)c * 512);
    return RES_OK;
}
static DRESULT ram_write(BYTE l, const BYTE* b, DWORD s, UINT c) {
    (void)l;
    if((size_t)s + c > g_sectors) return RES_PARERR;
    memcpy(g_disk + (size_t)s * 512, b, (size_t)c * 512);
    return RES_OK;
}
static DRESULT ram_ioctl(BYTE l, BYTE cmd, void* b) {
    (void)l;
    switch(cmd) {
    case CTRL_SYNC: return RES_OK;
    case GET_SECTOR_COUNT: *(DWORD*)b = (DWORD)g_sectors; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD*)b = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD*)b = 1; return RES_OK;
    }
    return RES_PARERR;
}
static const Diskio_drvTypeDef ram_drv = {ram_init, ram_status, ram_read, ram_write, ram_ioctl};

static const char k_hello[] = "Hello from the Flipper Zero FatFs known-answer test.\n";

// Formats a fresh RAM volume of `sectors` sectors, writes HELLO.TXT, leaves the driver linked.
static bool fat_make(BYTE fmt, size_t sectors, FATFS* fs) {
    g_sectors = sectors;
    g_disk = calloc(sectors, 512);
    static char path[4] = "0:/";
    if(FATFS_LinkDriver(&ram_drv, path) != 0) return false;
    static BYTE work[4096];
    if(f_mount(fs, "0:/", 1) != FR_NO_FILESYSTEM) return false;
    FRESULT mk = f_mkfs("0:/", fmt, 0, work, sizeof(work));
    if(mk != FR_OK) return false;
    if(f_mount(fs, "0:/", 1) != FR_OK) return false;
    FIL f;
    UINT bw;
    if(f_open(&f, "0:/HELLO.TXT", FA_CREATE_NEW | FA_WRITE) != FR_OK) return false;
    if(f_write(&f, k_hello, sizeof(k_hello) - 1, &bw) != FR_OK || bw != sizeof(k_hello) - 1) return false;
    return f_close(&f) == FR_OK;
}

static void fat_teardown(void) {
    static char path[4] = "0:/";
    f_mount(NULL, "0:/", 0);
    FATFS_UnLinkDriver(path);
    free(g_disk);
}

static void fat_roundtrip(const char* name, BYTE fmt, size_t sectors, BYTE expect_fs) {
    static FATFS fs;
    memset(&fs, 0, sizeof(fs));
    char label[64];
    bool made = fat_make(fmt, sectors, &fs);
    snprintf(label, sizeof(label), "fatfs %s mkfs+write", name);
    check(label, made && fs.fs_type == expect_fs);
    if(made) {
        FIL f;
        char buf[128] = {0};
        UINT br = 0;
        bool ok = f_open(&f, "0:/HELLO.TXT", FA_READ) == FR_OK &&
                  f_size(&f) == sizeof(k_hello) - 1 &&
                  f_read(&f, buf, sizeof(buf) - 1, &br) == FR_OK && br == sizeof(k_hello) - 1 &&
                  strcmp(buf, k_hello) == 0;
        f_close(&f);
        snprintf(label, sizeof(label), "fatfs %s read back", name);
        check(label, ok);
        FILINFO fno;
        DIR dir;
        bool listed = f_opendir(&dir, "0:/") == FR_OK && f_readdir(&dir, &fno) == FR_OK &&
                      strcmp(fno.fname, "HELLO.TXT") == 0 && fno.fsize == sizeof(k_hello) - 1;
        f_closedir(&dir);
        snprintf(label, sizeof(label), "fatfs %s directory listing", name);
        check(label, listed);
        snprintf(label, sizeof(label), "fatfs %s mkdir/unlink", name);
        check(
            label,
            f_mkdir("0:/SUB") == FR_OK && f_stat("0:/SUB", &fno) == FR_OK &&
                (fno.fattrib & AM_DIR) && f_unlink("0:/SUB") == FR_OK &&
                f_stat("0:/SUB", &fno) == FR_NO_FILE);
    }
    fat_teardown();
}

// Fills the volume with a directory tree, long names, multi-cluster and fragmented files and a
// deleted entry, so fuzzer seeds reach the directory walk, f_read and f_lseek past the mount.
static void fat_populate(void) {
    static const char* dirs[] = {"0:/SUB", "0:/Documents and Settings", "0:/Documents and Settings/deep",
                                 "0:/Documents and Settings/deep/er"};
    for(size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) f_mkdir(dirs[i]);
    static const char* files[] = {
        "0:/SUB/short.txt", "0:/A long file name for the test (1).txt",
        "0:/Documents and Settings/deep/er/\xc3\xbcnicode-\xe6\x97\xa5\xe6\x9c\xac.bin",
        "0:/Documents and Settings/readme.md", "0:/gone.tmp", "0:/frag.a", "0:/frag.b", "0:/big.dat"};
    static const size_t sizes[] = {17, 300, 700, 5000, 900, 9000, 9000, 40000};
    for(size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        FIL f;
        if(f_open(&f, files[i], FA_CREATE_NEW | FA_WRITE) != FR_OK) continue;
        uint8_t buf[512];
        for(size_t off = 0; off < sizes[i]; off += sizeof(buf)) {
            for(size_t j = 0; j < sizeof(buf); j++) buf[j] = (uint8_t)(off + j + i * 31);
            UINT bw, n = sizes[i] - off < sizeof(buf) ? sizes[i] - off : sizeof(buf);
            f_write(&f, buf, n, &bw);
        }
        f_close(&f);
    }
    f_unlink("0:/gone.tmp");
    f_unlink("0:/frag.a");
    FIL f;
    if(f_open(&f, "0:/frag.c", FA_CREATE_NEW | FA_WRITE) == FR_OK) {
        uint8_t buf[512] = {7};
        UINT bw;
        for(int i = 0; i < 30; i++) f_write(&f, buf, sizeof(buf), &bw);
        f_close(&f);
    }
    f_rename("0:/SUB/short.txt", "0:/Documents and Settings/moved.txt");
    f_setlabel("0:MAYHEM");
}

static int mkfat_image(const char* type, const char* out, bool populated) {
    static FATFS fs;
    BYTE fmt = FM_FAT;
    size_t sectors = 4096;
    if(!strcmp(type, "fat32")) fmt = FM_FAT32, sectors = 70000;
    if(!strcmp(type, "exfat")) fmt = FM_EXFAT, sectors = 16384;
    if(!fat_make(fmt, sectors, &fs)) return 1;
    if(populated) fat_populate();
    else f_mkdir("0:/SUB");
    FILE* fp = fopen(out, "wb");
    if(!fp) return 1;
    size_t n = sectors * 512;
    while(n > 512 && !memcmp(g_disk + n - 512, g_disk + n - 1024, 512) && g_disk[n - 1] == 0) n -= 512;
    fwrite(g_disk, 1, n, fp);
    fclose(fp);
    return 0;
}

static void test_fatfs(void) {
    fat_roundtrip("FAT12/16", FM_FAT, 4096, FS_FAT12);
    fat_roundtrip("exFAT", FM_EXFAT, 16384, FS_EXFAT);
}

/* ------------------------------------------------------------------ lfrfid */

// Manchester-encodes an EM4100 frame (header, 10 rows with even parity, column parity, stop bit).
static size_t em4100_pulses(const uint8_t id[5], bool* levels, uint32_t* durs, bool invert) {
    uint8_t bits[192];
    size_t nb = 0;
    for(int i = 0; i < 9; i++) bits[nb++] = 1;
    uint8_t col[4] = {0};
    for(int i = 0; i < 10; i++) {
        uint8_t nib = (i & 1) ? (id[i / 2] & 0xF) : (id[i / 2] >> 4);
        uint8_t par = 0;
        for(int b = 3; b >= 0; b--) {
            uint8_t v = (nib >> b) & 1;
            bits[nb++] = v;
            par ^= v;
            col[3 - b] ^= v;
        }
        bits[nb++] = par;
    }
    for(int c = 0; c < 4; c++) bits[nb++] = col[c];
    bits[nb++] = 0;
    size_t frame = nb;
    for(int rep = 1; rep < 3; rep++) memcpy(bits + rep * frame, bits, frame), nb += frame;
    size_t np = 0;
    bool cur = false;
    uint32_t acc = 0;
    for(size_t i = 0; i < nb; i++) {
        bool first = (bits[i] != 0) ^ invert; // Manchester: 1 -> low,high ; 0 -> high,low
        bool halves[2] = {!first, first};
        for(int h = 0; h < 2; h++) {
            if(acc && halves[h] == cur) {
                acc += 256;
            } else {
                if(acc) { levels[np] = cur; durs[np++] = acc; }
                cur = halves[h];
                acc = 256;
            }
        }
    }
    levels[np] = cur;
    durs[np++] = acc;
    return np;
}

static bool em4100_try(bool invert, const uint8_t id[5], char* uid_out, size_t uid_len) {
    ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    protocol_dict_decoders_start(dict);
    bool levels[400];
    uint32_t durs[400];
    size_t np = em4100_pulses(id, levels, durs, invert);
    bool found = false;
    {
        for(size_t i = 0; i < np && !found; i++) {
            ProtocolId pid = protocol_dict_decoders_feed(dict, levels[i], durs[i]);
            if(pid == (ProtocolId)LFRFIDProtocolEM4100) {
                uint8_t got[5];
                protocol_dict_get_data(dict, pid, got, 5);
                found = memcmp(got, id, 5) == 0;
                FuriString* s = furi_string_alloc();
                protocol_dict_render_data(dict, s, pid);
                snprintf(uid_out, uid_len, "%s", furi_string_get_cstr(s));
                furi_string_free(s);
            }
        }
    }
    protocol_dict_free(dict);
    return found;
}

static void test_lfrfid(void) {
    static const uint8_t id[5] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    char uid[64] = {0};
    bool ok = em4100_try(false, id, uid, sizeof(uid)) || em4100_try(true, id, uid, sizeof(uid));
    check("lfrfid EM4100 pulse train decodes to ID", ok);
    check("lfrfid EM4100 rendered data", strstr(uid, "FC: 086") && strstr(uid, "Card: 30874 (RF/64)"));

    ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    check(
        "lfrfid protocol lookup",
        protocol_dict_get_protocol_by_name(dict, "EM4100") == (ProtocolId)LFRFIDProtocolEM4100 &&
            protocol_dict_get_data_size(dict, LFRFIDProtocolEM4100) == 5);
    static const uint8_t h10301[3] = {0x12, 0x34, 0x56};
    protocol_dict_set_data(dict, LFRFIDProtocolH10301, h10301, 3);
    FuriString* s = furi_string_alloc();
    protocol_dict_render_brief_data(dict, s, LFRFIDProtocolH10301);
    check("lfrfid H10301 rendered", strstr(furi_string_get_cstr(s), "FC: 18") != NULL);
    furi_string_free(s);
    protocol_dict_free(dict);
}

int main(int argc, char** argv) {
    if(argc == 4 && !strcmp(argv[1], "--mkfat")) return mkfat_image(argv[2], argv[3], false);
    if(argc == 4 && !strcmp(argv[1], "--mkfat-full")) return mkfat_image(argv[2], argv[3], true);
    test_infrared();
    test_flipper_format();
    test_mjs();
    test_fatfs();
    test_lfrfid();
    printf("KAT_SUMMARY pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
