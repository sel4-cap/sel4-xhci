#include <sddf/fs/fat/AsyncFATFs.h>

FRESULT fat_mount (FATFS* fs, const TCHAR* path, BYTE opt);			/* Mount/Unmount a logical drive */

FRESULT fat_f_open (FIL* fp, const TCHAR* path, BYTE mode);

FRESULT fat_f_pread (FIL* fp, void* buff, FSIZE_t ofs, UINT btr, UINT* br);

FRESULT fat_f_pwrite (FIL* fp, void* buff, FSIZE_t ofs, UINT btw, UINT* bw);

// f_close would sync the file to disk
FRESULT fat_f_close (FIL* fp);

FRESULT fat_unmount(const TCHAR* path);

void mymalloc_init();

void* mymalloc(uint64_t buffer_size);
