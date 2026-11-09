// Mounts the input as a FAT/exFAT volume image (RAM disk) and walks, reads and modifies it with FatFs.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ff.h>
#include <ff_gen_drv.h>

static uint8_t* g_disk;
static size_t g_sectors;

DWORD get_fattime(void) {
    return ((DWORD)(2024 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}

static DSTATUS ram_init(BYTE lun) {
    (void)lun;
    return 0;
}
static DSTATUS ram_status(BYTE lun) {
    (void)lun;
    return 0;
}
static DRESULT ram_read(BYTE lun, BYTE* buf, DWORD sector, UINT count) {
    (void)lun;
    if((size_t)sector + count > g_sectors) return RES_PARERR;
    memcpy(buf, g_disk + (size_t)sector * 512, (size_t)count * 512);
    return RES_OK;
}
static DRESULT ram_write(BYTE lun, const BYTE* buf, DWORD sector, UINT count) {
    (void)lun;
    if((size_t)sector + count > g_sectors) return RES_PARERR;
    memcpy(g_disk + (size_t)sector * 512, buf, (size_t)count * 512);
    return RES_OK;
}
static DRESULT ram_ioctl(BYTE lun, BYTE cmd, void* buf) {
    (void)lun;
    switch(cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(DWORD*)buf = (DWORD)g_sectors;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD*)buf = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD*)buf = 1;
        return RES_OK;
    }
    return RES_PARERR;
}

static const Diskio_drvTypeDef ram_drv = {ram_init, ram_status, ram_read, ram_write, ram_ioctl};

static void walk(const char* path, int depth) {
    DIR dir;
    FILINFO fno;
    if(f_opendir(&dir, path) != FR_OK) return;
    int entries = 0;
    while(entries++ < 64 && f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        char sub[256];
        size_t n = strlen(path);
        if(n + strlen(fno.fname) + 2 >= sizeof(sub)) continue;
        memcpy(sub, path, n);
        if(n && sub[n - 1] != '/') sub[n++] = '/';
        strcpy(sub + n, fno.fname);
        if(fno.fattrib & AM_DIR) {
            if(depth < 4) walk(sub, depth + 1);
        } else {
            FIL f;
            if(f_open(&f, sub, FA_READ) == FR_OK) {
                BYTE buf[512];
                UINT br;
                for(int i = 0; i < 8; i++) {
                    if(f_read(&f, buf, sizeof(buf), &br) != FR_OK || br == 0) break;
                }
                f_lseek(&f, f_size(&f) / 2);
                f_read(&f, buf, sizeof(buf), &br);
                f_close(&f);
            }
        }
    }
    f_closedir(&dir);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if(size < 512 || size > (1u << 22)) return 0;
    // Zero-extend to 1 MiB so seeds can be stored with their empty tail trimmed.
    size_t total = size < (1u << 20) ? (1u << 20) : (size + 511) / 512 * 512;
    g_sectors = total / 512;
    g_disk = (uint8_t*)calloc(g_sectors, 512);
    memcpy(g_disk, data, size);

    char path[4] = "0:/";
    if(FATFS_LinkDriver(&ram_drv, path) == 0) {
        static FATFS fs;
        memset(&fs, 0, sizeof(fs));
        if(f_mount(&fs, "0:/", 1) == FR_OK) {
            DWORD nclst;
            FATFS* fsp;
            f_getfree("0:/", &nclst, &fsp);
            char label[34];
            DWORD vsn;
            f_getlabel("0:/", label, &vsn);
            walk("0:/", 0);
            FIL f;
            if(f_open(&f, "0:/MAYHEM.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
                UINT bw;
                f_write(&f, data, size < 2048 ? size : 2048, &bw);
                f_close(&f);
            }
            f_mkdir("0:/mdir");
            f_rename("0:/MAYHEM.TXT", "0:/mdir/M2.TXT");
            f_unlink("0:/mdir/M2.TXT");
            f_unlink("0:/mdir");
        }
        f_mount(NULL, "0:/", 0);
        FATFS_UnLinkDriver(path);
    }
    free(g_disk);
    return 0;
}
