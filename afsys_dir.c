/* See LICENSE.txt for license details */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE (1)
#endif

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "os_proto.h"
#include "qio.h"
#include "fsys.h"

#define AFSYS_DIR_Q	(0x01)		/* No totals */
#define AFSYS_DIR_F	(0x02)		/* command file format */
#define AFSYS_DIR_L	(0x04)		/* extended report */

extern FsysVolume volumes[];
extern int is_boot_file(const char *fname);
extern int have_checksum(U32 fid);

static void dmp_line(FsysRamFH *rfh, int fmt, U32 fid, const char *path, const char *name, int copies, FILE *ofp)
{
    struct tm *ftv;
    char ftim[64];

    ftv = localtime((time_t *)&rfh->mtime);
    ftim[0] = 0;
    if (ftv)
    {
        strftime(ftim, sizeof(ftim), "%b %d, %Y %H:%M:%S", ftv);
    }
    if ((fmt&AFSYS_DIR_L))
    {
        fprintf(ofp, "%d %08X %8d %8d %s %s%s%c\n", 
                copies,
                fid,
                rfh->clusters,
                rfh->size,
                ftim,
                path, name,
                rfh->directory ? '/' : ' ');
    }
    else
    {
        fprintf(ofp, "%d %9d %s %s%s%c\n", 
                copies,
                rfh->size,
                ftim,
                path, name,
                rfh->directory ? '/' : ' ');
    }
}

static void dump_dir(FsysRamFH *rfh, const char *path, int oformat, FILE *ofp)
{
    FsysRamRP *rp;
    FsysDirEnt **hash, *dir;
    int copies;
    int jj, dire;
    struct tm *ftv;
    char ftim[64];

    for (dire=0; dire < FSYS_DIR_HASH_SIZE; ++dire)
    {
        hash = rfh->directory + dire;
        dir = *hash;
        while (dir)
        {
            FsysRamFH *lrfh;

            lrfh = fsys_find_ramfh(volumes, (dir->gen_fid & FSYS_DIR_FIDMASK));
            copies = 0;
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
            {
                rp = lrfh->ramrp+jj;
                if (!rp || !rp->rptrs) break;
                ++copies;
            }
            if (!lrfh->directory ||
                (strcmp(dir->name, ".") != 0 &&
                 strcmp(dir->name, "..") != 0))
            {
                if (!(AFSYS_DIR_F&oformat))
                {
                    dmp_line(lrfh, oformat, dir->gen_fid, path, dir->name, copies, ofp);
                }
                else if (!lrfh->directory)
                {
                    char tname[256], *ab = tname, *cmd="FILE", *gu="UNIX", *nocs="";
                    int slen, sts;
                    slen = strlen(path)+strlen(dir->name);
                    if (slen > sizeof(tname)-1)
                    {
                        ab = malloc(slen+1);
                        if (!ab)
                        {
                            fprintf(ofp, "# %s%s ****ERROR_MALLOCING_%d_BYTES****\n", path, dir->name, slen);
                        }
                    }
                    if (!have_checksum(dir->gen_fid)) nocs = " NO_CHECKSUM=1";
                    if (ab)
                    {
                        strcpy(ab, path);
                        strcat(ab, dir->name);
                        if (!strcmp(ab, "diags/checksums"))
                        {
                            cmd = "CHECKSUM";
                            gu = "GAME";
                            nocs = "";
                        }
                        sts = is_boot_file(ab);
                        if (ab != tname) free(ab);
                        if (sts >= 0)
                        {
                            if (sts == 0) cmd = "BOOT0";
                            else if (sts == 1) cmd = "BOOT1";
                            else if (sts == 2) cmd = "BOOT2";
                            else if (sts == 3) cmd = "BOOT3";
                        }
                    }
                    if (copies != 1)
                    {
                        fprintf(ofp, "%s %s=%s%s COPIES=%d%s\n", cmd, gu, path, dir->name, copies, nocs);
                    }
                    else
                    {
                        fprintf(ofp, "%s %s=%s%s%s\n", cmd, gu, path, dir->name, nocs);
                    }
                }
            }
            if (lrfh->directory &&
                (strcmp(dir->name, ".") != 0 &&
                 strcmp(dir->name, "..") != 0))
            {
                char *newpath;
                int len;
                len = strlen(path) + strlen(dir->name) + 2;
                newpath = malloc(len);
                if (newpath)
                {
                    strcpy(newpath, path);
                    strcat(newpath, dir->name);
                    strcat(newpath, "/");
                    dump_dir(lrfh, newpath, oformat, ofp);
                    free(newpath);
                }
                else
                {
                    ftv = localtime((time_t *)&lrfh->mtime);
                    ftim[0] = 0;
                    if (ftv)
                    {
                        strftime(ftim, sizeof(ftim), "%b %d, %Y %H:%M:%S", ftv);
                    }
                    if ((oformat&AFSYS_DIR_L))
                    {
                        fprintf(ofp, "0 00000000        0 00000000 %s ****ERROR_MALLOCING_%d_BYTES****\n", ftim, len);
                    }
                    else
                    {
                        fprintf(ofp, "0 00000000 %s ****ERROR_MALLOCING_%d_BYTES****\n", ftim, len);
                    }
                }
            }
            dir = dir->next;
        }
    }
}

#define DUMP_IDX (0)

int afsys_dir(int format, FILE *ofp)
{
    int used, used_mb;
    int free, free_mb, sts=0;

    if ((format&AFSYS_DIR_F))
    {
        fprintf(ofp, "VOLUME PHYS=/rd0 VIRT=/d0\n");
        fprintf(ofp, "DEFAULT UNIX=/d0 GAME=/d0 COPIES=1\n");
    }
    else if ((format&AFSYS_DIR_L))
    {
        FsysRamFH *rfh;
#if DUMP_IDX
        FsysIndexLink *fl;
        U32 *lba;
        int ii, idx, grp;

        fl = volumes[0].indexp;
        grp = idx = 0;
        while (fl)
        {
            lba = (U32*)(fl+1);
            for (ii=0; ii < fl->items; ++ii)
            {
                rfh = fsys_find_ramfh(volumes, idx);
                printf("%4d, %5d.%2d: %08lX %08lX %08lX, rfh=%08lX Dir=%08lX\n",
                       grp, idx, ii, lba[0], lba[1], lba[2], (U32)rfh, rfh ? (U32)rfh->directory : 0);
                lba += FSYS_MAX_ALTS;
                ++idx;
            }
            fl = fl->next;
            ++grp;
        }
#endif
        rfh = fsys_find_ramfh(volumes, 0);  /* root dir */
        dmp_line(rfh, format, 0x01000000, "", "<Index file>", 3, ofp);
        rfh = fsys_find_ramfh(volumes, 1);  /* freelist */
        dmp_line(rfh, format, 0x01000001, "", "<Freelist file>", 3, ofp);
        rfh = fsys_find_ramfh(volumes, 2);  /* root directory */
        dmp_line(rfh, format, 0x01000002, "", "<Root directory>", 3, ofp);
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
        if ((rfh=volumes[0].journ_rfh))
        {
            dmp_line(rfh, format, volumes[0].journ_id | (rfh->generation<<FSYS_DIR_GENSHF),
                     "", "<Journal file>", 1, ofp);
        }
#endif
    }
    dump_dir(fsys_find_ramfh(volumes, 2), "", format, ofp); /* start dumping the root directory */
    used = volumes[0].total_alloc_clusters;
    used_mb = (used + 1024-1)/(2*1024);
    free = volumes[0].total_free_clusters;
    free_mb = (free + 1024-1)/(2*1024);
    if (!(format&(AFSYS_DIR_Q|AFSYS_DIR_F)))
    {
        fprintf(ofp, "A total of %d file headers allocated\n", volumes[0].files_ffree);
        fprintf(ofp, "A total of %d sectors (%'dMB, %d%%) used, %d sectors (%'dMB, %d%%) available\n",
                used, used_mb, used*100/(used+free),
                free, free_mb, free*100/(used+free));
    }
    if ((volumes[0].maxlba - (used+free)))
    {
        sts = 1;
        if (!(format&(AFSYS_DIR_Q|AFSYS_DIR_F)))
        {
            fprintf(ofp, "A total of %d sectors unaccounted for\n", volumes[0].maxlba - (used+free));
        }
    }
    fflush(ofp);
    return sts;
}
