/* See LICENSE.txt for license details */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE (1)
#endif

#include <stdio.h>
#include <locale.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>

#include "config.h"
#include "os_proto.h"
#include "st_proto.h"
#include "qio.h"
#include "fsys.h"
#include "nsprintf.h"
#include "h_hdparse.h"

char *image_name;
int default_copies = 1;
int debug_level;
int debug_mode;     /* Keep from sucking in all of libos */

void exit(int why)
{
    _exit(why);
}

const struct st_envar *st_getenv (const char *__name, const struct st_envar *__hook)
{
    return 0;
}

VU32 __real_eer_rtc;

#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
int real_errno;
int *__errno_location(void)
{
    return &real_errno;
}
#endif

int eer_gets(unsigned int what)
{
    return 0;
}

int eer_puts(unsigned int where, unsigned int what)
{
    return 0;
}

int prc_get_astlvl(void)
{
    return -1;
}

static struct act_q ast_q;
static int running_ast;

int prc_q_ast(int level, struct act_q *new)
{
    struct act_q **prev = &ast_q.next, *nxt;

    if (new && !new->que)
    {
        while ((nxt = *prev))
        {
            prev = &nxt->next;
        }
        *prev = new;
        new->next = 0;
        new->que = &ast_q;
        if (!running_ast)
        {
            running_ast = 1;
            while ((nxt = ast_q.next))
            {
                ast_q.next = nxt->next;
                nxt->next = 0;
                nxt->que = 0;
                if (nxt->action) nxt->action(nxt->param);
            }
            running_ast = 0;
        }
    }
    else
    {
        return 1;
    }
    return 0;
}

void prc_panic(const char *msg)
{
    fprintf(stderr, "prc_panic: %s\n", msg);
    exit (1);
}

void prc_wait_n_usecs(int amt)
{
    int loop = amt*166/4;
    while (--loop > 0)
    {
        ;
    }
}

#if 0
int nsprintf(char *buf, int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vsnprintf(buf, size, fmt, ap);
}
#endif

static int mk_paths(const char *orig_path)
{
    char *path, *strt, *end;
    int sts=0;
    FsysOpenT ot;
    QioIOQ *ioq;

    path = (char*)malloc(strlen(orig_path)+1);
    if (!path) return 0;            /* can't make any dirs */
    ioq = qio_getioq();
    if (!ioq)
    {
        free(path);
        return QIO_NOIOQ;
    }
    ot.spc.path = path;
    strcpy(path, orig_path);            /* make a copy of input path */
    strt = path+4;              /* beginning of pathname (skip the /d0/) */
    while (1)
    {
        end = strchr(strt, '/');        /* look for next '/' */
        if (!end) break;            /* there are no more directories */
        *end = 0;               /* zap the delimiter */
        if (!strcmp(strt, "..") || !strcmp(strt, "."))    /* skip filenames of .. and . */
        {
            *end = '/';             /* put delimiter back */
            strt = end+1;           /* and skip it */
            continue;
        }
        ot.spc.mode = O_RDONLY;
        ot.fid = 0;
        sts = qiow_openspc(ioq, &ot.spc);   /* try to open the file */
        if (QIO_ERR_CODE(sts) && sts != FSYS_LOOKUP_NOPATH && sts != FSYS_LOOKUP_FNF)
        {
            break;              /* something is very bad */
        }
        if (QIO_ERR_CODE(sts))        /* if file not there, create it */
        {
            FsysOpenT spc;

            if (debug_level) printf("Creating directory: %s\n", path);
            memset(&spc, 0, sizeof(spc));
            spc.spc.path = path;
            spc.spc.mode = O_WRONLY|O_CREAT|O_TRUNC;
            spc.alloc = 50*512;
            spc.copies = FSYS_MAX_ALTS;
            spc.def_extend = 50;
            spc.ctime = time(0);
            spc.mtime = spc.ctime;
            spc.mkdir = 1;
            sts = qiow_openspc(ioq, &spc.spc);
            if (QIO_ERR_CODE(sts))
            {
                break;              /* something is very bad */
            }
        }
        qiow_close(ioq);            /* close the file */
        *end = '/';             /* replace the delimiter */
        strt = end+1;               /* start looking at next one */
    }
    free(path);
    qiow_close(ioq);
    qio_freeioq(ioq);
    return QIO_ERR_CODE(sts) ? sts : 0;     /* return succes if appropriate */
}

static int mk_upaths(const char *orig_path)
{
    int sts=0, len;
    char lbuf[256], *lbp = lbuf, *end;

    if (strrchr(orig_path, '/'))
    {
#define MKDIR_COMMAND "mkdir -p "
        len = strlen(orig_path)+1+strlen(MKDIR_COMMAND);
        if (len > sizeof(lbuf))
        {
            lbp = (char*)malloc(len);
            if (!lbp) return 0;         /* can't make any dirs */
        }
        strcpy(lbp, orig_path);
        end = strrchr(lbp, '/');
        if (end)
        {
            struct stat st;
            *end = 0;
            sts = stat(lbp, &st);
            if (sts >= 0)
            {
                if (S_ISDIR(st.st_mode))  /* if file exists and is a directory */
                {
                    if (lbp != lbuf) free(lbp); /* done with the buffer */
                    return 0;           /* success */
                }
                if (lbp != lbuf) free(lbp);
                return ENOTDIR;
            }
        }
        strcpy(lbp, MKDIR_COMMAND);
        strcat(lbp, orig_path);         /* make a copy of input path */
        end = strrchr(lbp, '/');        /* look backwards */
        if (end)
        {
            *end = 0;               /* null terminate */
            sts = system(lbp);          /* create intervening directories */
        }
        if (lbp != lbuf) free(lbp);
    }
    return 0;                   /* return succes if appropriate */
}

#define DO_INIT		(0x0001)
#define DO_TOATARI	(0x0002)
#define DO_TOUNIX	(0x0004)
#define DO_DIR		(0x0008)
#define DO_DIR_Q	(0x0010)
#define DO_DIR_F	(0x0020)
#define DO_DIR_L	(0x0040)
#define DO_DELETE	(0x0080)
#define DO_FORCE	(0x0100)
#define DO_QUIETLY	(0x0200)
#define DO_VERIFY	(0x0400)
#define DO_CHKSUM	(0x0800)

#define AFSYS_DIR_Q	(0x01)		/* No totals */
#define AFSYS_DIR_F	(0x02)		/* command file format */
#define AFSYS_DIR_L	(0x04)		/* extended report */

extern int afsys_dir(int format, FILE *ofp);

static FILE *err_status;

static int sho_qio_err(int sts, const char *str)
{
    char qio_err[132];
    qio_errmsg(sts, qio_err, sizeof(qio_err));
    fputs(str, err_status);
    fprintf(err_status, ": %s\n", qio_err);
    return sts;
}

#if 0
static int sho_err(int sts, const char *msg)
{
    fprintf(err_status, "%s\n", msg);
    return sts;
}
#endif

U32 *cksums;
int cksums_size;
int cksums_limit;
int cksums_patch;

static void patch_checksums(U32 fid, U32 csum)
{
    int ii;

    if (cksums)
    {
        for (ii=0; ii < cksums_size/4; ii += 2)
        {
            if (fid == cksums[ii])
            {
                if (cksums[ii+1] != csum)
                {
                    cksums[ii+1] = csum;
                    ++cksums_patch;
                }
                break;
            }
        }
        if (ii >= cksums_size/4)
        {
            if (ii >= cksums_limit/4)
            {
                cksums_limit += 4096;
                cksums = (U32 *)realloc(cksums, cksums_limit);
                if (!cksums)
                {
                    fprintf(stderr, "Ran out of memory trying to realloc %d bytes for checksums\n", cksums_limit);
                    return;
                }
            }
            cksums[ii] = fid;
            cksums[ii+1] = csum;
            cksums_size += 8;
            ++cksums_patch;
        }
    }
}

static void del_checksum(U32 fid, U32 csum)
{
    int ii;

    if (cksums && cksums_size)
    {
        for (ii=0; ii < cksums_size/4; ii += 2)
        {
            if (fid == cksums[ii])
            {
                int jj;
                for (jj=ii+2; jj < cksums_size/4; jj += 2)
                {
                    cksums[jj-2] = cksums[jj];
                    cksums[jj-1] = cksums[jj+1];
                }
                cksums_size -= 8;
                ++cksums_patch;
                break;
            }
        }
    }
}

int have_checksum(U32 fid)
{
    int ii;

    if (cksums && cksums_size)
    {
        for (ii=0; ii < cksums_size/4; ii += 2)
        {
            if (fid == cksums[ii])
            {
                return 1;
            }
        }
    }
    return 0;
}

static ParseUnion tcks_puv;
static ParseUnion *cks_puv;
static int cks_been_here;

static int get_checksums(void)
{
    FsysOpenT spc;
    QioIOQ *ioq;
    int sts;

    if (cks_puv)
    {
        if (!cks_been_here)
        {
            ioq = qio_getioq();
            memset(&spc, 0, sizeof(spc));
            spc.spc.path = cks_puv->f.game_name;
            spc.spc.mode = O_RDONLY;
            sts = qiow_openspc(ioq, &spc.spc);
            if (!QIO_ERR_CODE(sts))
            {
                cksums_limit = spc.eof + 4096;
                cksums = malloc(cksums_limit);
                cksums_size = spc.eof;
                if (debug_level) printf("Reading %d byte checksum file\n", cksums_size);
                sts = qiow_read(ioq, cksums, cksums_size);
                if (QIO_ERR_CODE(sts))
                {
                    free(cksums);
                    cksums_size = 0;
                    cksums = 0;
                }
                qiow_close(ioq);
            }
            else if (debug_level)
            {
                printf("No checksum file found\n");
            }
            qio_freeioq(ioq);
            cks_been_here = 1;
        }
    }
    return 0;
}

static int put_checksums(void)
{
    FsysOpenT spc;
    QioIOQ *ioq;
    int sts;

    if (cks_puv && cksums && cksums_patch)
    {
        ioq = qio_getioq();
        memset(&spc, 0, sizeof(spc));
        spc.spc.path = cks_puv->f.game_name;
        spc.spc.mode = O_WRONLY|O_CREAT|O_TRUNC;
        spc.alloc = cksums_size;
        spc.copies = FSYS_MAX_ALTS;
        if (debug_level) printf("Writing %d byte checksum file\n", cksums_size);
        sts = qiow_openspc(ioq, &spc.spc);
        if (!QIO_ERR_CODE(sts))
        {
            sts = qiow_write(ioq, cksums, cksums_size);
            qiow_close(ioq);
        }
        if (QIO_ERR_CODE(sts))
        {
            sho_qio_err(sts, "Error writing checksum file\n");
        }
        qio_freeioq(ioq);
    }
    else if (debug_level)
    {
        printf("No changes to checksum file\n");
    }
    return 0;
}

static int get_lbas(const char *fname, U32 *lbas)
{
    QioIOQ *ioq;
    int sts, retv = -1;

    ioq = qio_getioq();
    sts = qiow_open(ioq, fname, O_RDONLY);
    if (!QIO_ERR_CODE(sts))
    {
        sts = qiow_ioctl(ioq, FSYS_IOC_GETFHLBA, lbas); /* get lbas */
        if (!QIO_ERR_CODE(sts))
        {
#if 0
            printf("LBAs for %s: %08lX, %08lX, %08lX\n", fname, lbas[0], lbas[1], lbas[2]);
#endif
            retv = ioq->iocount;
        }
        qiow_close(ioq);        /* done with file */
    }
    qio_freeioq(ioq);
    return retv;
}

static FsysGSetHB home_block;

static int get_home_block(QioIOQ *ioq, const char *virt)
{
    int sts;
    int gotioq=0;

    if (!ioq)
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            printf("No more IOQ's\n");
            return -1;
        }
        sts = qiow_open(ioq, "/rd0", O_RDWR);
        if (QIO_ERR_CODE(sts))
        {
            sho_qio_err(sts, "Error opening /rd0:");
            qio_freeioq(ioq);
            return sts;
        }
        gotioq = 1;
    }
    sts = fsys_gethomeblk(ioq, "/d0", &home_block);
    while (!sts) sts = ioq->iostatus;
    if (QIO_ERR_CODE(sts))
    {
        sho_qio_err(sts, "Found no valid home blocks:");
    }
    if (gotioq)
    {
        qiow_close(ioq);
        qio_freeioq(ioq);
    }
    return sts;
}

int is_boot_file(const char *fname)
{
    U32 lbas[FSYS_MAX_ALTS];
    int sts;
    sts = get_lbas(fname, lbas);
    if (sts >= 0)
    {
        FsysHomeBlock *hb = &home_block.hbu.homeblk;
        if (lbas[0] == hb->boot[0]) return 0;
        if (lbas[0] == hb->boot1[0]) return 1;
        if (lbas[0] == hb->boot2[0]) return 2;
        if (lbas[0] == hb->boot3[0]) return 3;
        sts = -1;
    }
    return sts;
}

static int mark_home_block( unsigned long time, U32 *lbas, int which_boot )
{
    FsysHomeBlock *hb;
    U32 *lp, cksum;
    int ii, jj, sts;
    QioIOQ *ioq;
    U32 *bootlbas;

    ioq = qio_getioq();
    if (!ioq)
    {
        printf("No more IOQ's\n");
        return -1;
    }
    ii = qiow_open(ioq, "/rd0", O_RDWR);
    if (QIO_ERR_CODE(ii))
    {
        sho_qio_err(ii, "Error opening /rd0:");
        qio_freeioq(ioq);
        return ii;
    }
    ioq->timeout = 1000000;                /* apply a 1 second timeout */
    ii = get_home_block(ioq, "/d0");
    if (QIO_ERR_CODE(ii))
    {
        sho_qio_err(ii, "Found no valid home blocks:");
        qio_freeioq(ioq);
        return ii;
    }
    hb = &home_block.hbu.homeblk;
    if ( lbas && which_boot >= 0 && which_boot < 4)
    {
        if (which_boot == 0)
        {
            bootlbas = hb->boot;
        }
        else
        {
            bootlbas = hb->boot1 + (which_boot-1)*FSYS_MAX_ALTS;
        }
        for ( jj=0; jj < FSYS_MAX_ALTS; ++jj )
        {
            bootlbas[jj] = lbas[jj];   
        }
    }
    hb->chksum = 0;
    hb->upd_flag = time;
    for (jj=0,cksum=0,lp=(U32*)hb; jj < 512/4; ++jj)
    {
        cksum += *lp++;         /* checksum the homeblock */
    }
    hb->chksum = 0-cksum;
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        int retry; 
        for (retry=0; retry < 8; ++retry)
        {
            sts = qiow_writewpos(ioq, home_block.abs_lba[ii], hb, 512);   /* write HB */
            if (!QIO_ERR_CODE(sts)) break;
        }
    }
    if (!ii && QIO_ERR_CODE(sts))
    {
        sho_qio_err( sts, "Home block write failed" );
        qio_freeioq(ioq);
        return sts;
    }
    qio_freeioq(ioq);
    return 0;
}

static void print_mkb(const char *prefix, int amt, const char *suffix)
{
    char c;
    float famt;   

    if (amt < 1024)
    {
        printf("%s%5d B%s", prefix, amt, suffix);
        return;
    }
    if (amt < 995*1024)
    {
        famt = (float)amt/1024.0;
        c = 'K';
    }
    else if (amt < 995*1024*1024)
    {
        famt = (float)amt/(1024.0*1024.0);
        c = 'M';  
    }
    else
    {
        famt = (float)amt/(1024.0*1024.0*1024.0);
        c = 'G';
    }
    printf("%s%5.1f%cB%s", prefix, famt, c, suffix);
    return;
}

static void upd_boot(const char *fname, int which_boot)
{
    U32 lbas[3];
    int sts;
    sts = get_lbas(fname, lbas);
    if (sts == sizeof(lbas))
    {
        sts = mark_home_block(0, lbas, which_boot);
        if (sts)
        {
            sho_qio_err(sts, "Error writing home block\n");
        }
    }
    else
    {
        sho_qio_err(sts, "Error getting boot file LBAs\n");
    }
}

static ParseUnion *puv;
#define DISK_BUFF_SIZE (128*1024)
static U32 disk_buff[DISK_BUFF_SIZE/4];

static int unix_to_game(const char *game_name, const char *unix_name, int ncopies, int extnd, int dowhat)
{
    FILE *ufp;
    int len, sts;
    QioIOQ *ioq;
    char emsg[132];
    FsysOpenT spc;
    struct stat st;
    U32 cksum;

    if ((dowhat&DO_CHKSUM)) get_checksums();
    sts = stat(unix_name, &st);         /* get Unix file details */
    if (sts < 0)              /* No Unix file! */
    {
        char *emsg;
        emsg = strerror(errno);
        fprintf(err_status, "Unable to stat unix file: %s:\n\t%s\n", unix_name, emsg);
        return 1;
    }
    ioq = qio_getioq();
    do
    {
        if ((dowhat&DO_FORCE)) break;       /* go ahead and do it anyway */
        memset(&spc, 0, sizeof(spc));
        spc.spc.path = game_name;
        spc.spc.mode = O_RDONLY;
        sts = qiow_openspc(ioq, &spc.spc);  /* open game file for read */
        if (QIO_ERR_CODE(sts))        /* no game file! */
        {
            break;              /* go ahead and copy */
        }
        qiow_close(ioq);            /* don't need this anymore */
        if (spc.mkdir)            /* game file is a directory. It won't work */
        {
            break;
        }
        if (ncopies > 0)
        {
            if (spc.copies != ncopies) break;   /* if new number of copies, copy */
        }
        else if (spc.copies != FSYS_MAX_ALTS)
        {
            break;              /* new number of copies, copy */
        }
        if (spc.mtime < st.st_mtime)      /* if game file is older than unix file */
        {
            break;              /* go ahead and copy */
        }
        if (spc.mtime == st.st_mtime && spc.eof != st.st_size)
        {
            break;
        }
        if (debug_level) printf("Skipped copy of %s\n", unix_name);
        qio_freeioq(ioq);
        return 0;
    } while (0);
    ufp = fopen(unix_name, "rb");
    if (!ufp)
    {
        char *emsg = strerror(errno);
        fprintf(err_status, "Unable to open unix file %s:\b%s\n", unix_name, emsg);
        qio_freeioq(ioq);
        return 1;
    }
    if ((sts=mk_paths(game_name)))
    {
        snprintf(emsg, sizeof(emsg), "Could not create game path: %s\n", game_name);
        sho_qio_err(sts, emsg);
        fclose(ufp);
        qio_freeioq(ioq);
        return 2;
    }
    memset(&spc, 0, sizeof(spc));
    spc.spc.path = game_name;
    spc.spc.mode = O_WRONLY|O_CREAT|O_TRUNC;
    spc.alloc = st.st_size;
    spc.copies = FSYS_MAX_ALTS;     /* assume max copies */
    if (ncopies > 0 && ncopies < FSYS_MAX_ALTS)
    {
        spc.copies = ncopies;
    }
    spc.def_extend = extnd;
    spc.ctime = st.st_ctime;
    spc.mtime = st.st_mtime;
    if (debug_level)
    {
        printf("Opening %s for create. size=%ld, copies=%d\n", spc.spc.path, spc.alloc, spc.copies);
    }
    sts = qiow_openspc(ioq, &spc.spc);
    if (QIO_ERR_CODE(sts))
    {
        snprintf(emsg, sizeof(emsg), "Error creating game file %s\n", game_name);
        sho_qio_err(sts, emsg);
        fclose(ufp);
        qio_freeioq(ioq);
        return 3;
    }
    if (!(dowhat&DO_QUIETLY))
    {
        print_mkb("Writing ", spc.alloc, ", ");
        printf("FID=%08X, %s\n", spc.fid, game_name);
        fflush(stdout);
    }
    cksum = 0;
    while ((len=fread(disk_buff, 1, sizeof(disk_buff), ufp)) > 0)
    {
        if ((dowhat&DO_CHKSUM))
        {
            int ii, clen;
            U32 *ulp = disk_buff;
            clen = (len+3)&-4;
            if ((len&3))
            {
                memset((U8*)disk_buff+len, 0, clen-len);
            }
            for (ii=0; ii < clen; ii += 4)
            {
                cksum += *ulp++;
            }
        }
        sts = qiow_write(ioq, disk_buff, len);
        if (QIO_ERR_CODE(sts))
        {
            snprintf(emsg, sizeof(emsg), "Error writing to %s", game_name);
            sho_qio_err(sts, emsg);
            qiow_close(ioq);
            fclose(ufp);
            qio_freeioq(ioq);
            return 4;
        }
    }
    if (len < 0)
    {
        char *emsg;
        emsg = strerror(errno);
        fprintf(err_status, "Error reading unix file: %s\n\t%s\n", unix_name, emsg);
    }
    if (cksums)
    {
        if ((dowhat&DO_CHKSUM))
        {
            patch_checksums(spc.fid, cksum);
        }
        else
        {
            del_checksum(spc.fid, 0);
        }
    }
    fclose(ufp);
    qiow_close(ioq);
    qio_freeioq(ioq);
    if (ncopies <= 0)
    {
        if (!(dowhat&DO_QUIETLY))
        {
            printf("Updating boot block. Entry %d\n", -ncopies);
        }
        upd_boot(game_name, -ncopies);
    }
    return 0;
}

static int game_to_unix(const char *unix_name, const char *game_name, int dowhat)
{
    FILE *ufp;
    int len, sts;
    QioIOQ *ioq;
    FsysOpenT spc;
    struct utimbuf ut;
    char emsg[132];
    char *emsgp;
    struct stat st;

    ioq = qio_getioq();
    memset(&spc, 0, sizeof(spc));
    spc.spc.path = game_name;
    spc.spc.mode = O_RDONLY;
    sts = qiow_openspc(ioq, &spc.spc);
    if (QIO_ERR_CODE(sts))
    {
        snprintf(emsg, sizeof(emsg), "Error opening game file %s\n", game_name);
        sho_qio_err(sts, emsg);
        qio_freeioq(ioq);
        return 3;
    }
    do
    {
        if ((dowhat&DO_FORCE)) break;       /* copy it no matter what */
        sts = stat(unix_name, &st);
        if (sts < 0) break;         /* no unix file, copy it */
        if (st.st_mtime < spc.mtime) break; /* unix file is older, copy */
        if (st.st_mtime == spc.mtime && st.st_size != spc.eof) break; /* sizes don't match, copy */
        if (debug_level) printf("Skipped copy of game %s\n", game_name);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 0;
    } while (0);
    if ((sts=mk_upaths(unix_name)))
    {
        snprintf(emsg, sizeof(emsg), "Could not create unix path: %s\n", unix_name);
        sho_qio_err(sts, emsg);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 2;
    }
    ufp = fopen(unix_name, "wb");
    if (!ufp)
    {
        emsgp = strerror(errno);
        fprintf(err_status, "Could not create unix file: %s\n\t%s\n", unix_name, emsgp);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 1;
    }
    if (!(dowhat&DO_QUIETLY))
    {
        print_mkb("Reading ", spc.alloc, ", ");
        printf("FID=%08X, %s\n", spc.fid, game_name);
        fflush(stdout);
    }
    while (1)
    {
        sts = qiow_read(ioq, disk_buff, sizeof(disk_buff));
        if (QIO_ERR_CODE(sts))
        {
            if (sts == QIO_EOF)
            {
                sts = 0;
                break;
            }
            snprintf(emsg, sizeof(emsg), "Error reading game file %s\n", game_name);
            sho_qio_err(sts, emsg);
            break;
        }
        len = fwrite(disk_buff, 1, ioq->iocount, ufp);
        if (len != ioq->iocount)
        {
            emsgp = strerror(errno);
            fprintf(err_status, "Error writing unix file %s:\n\t%s\n", unix_name, emsgp);
            break;
        }
    }
    fclose(ufp);
    qiow_close(ioq);
    qio_freeioq(ioq);
    ut.modtime = spc.mtime ? spc.mtime : time(0);
    ut.actime = spc.ctime ? spc.ctime : time(0);
    utime(unix_name, &ut);
    return sts;
}

static int verify_game_to_unix(const char *unix_name, const char *game_name, int dowhat)
{
    FILE *ufp;
    int len, sts;
    QioIOQ *ioq;
    FsysOpenT spc;
    char emsg[132];
    char *emsgp;
    struct stat st;
    U32 g_cs = 0, u_cs = 0, *src;

    ioq = qio_getioq();
    memset(&spc, 0, sizeof(spc));
    spc.spc.path = game_name;
    spc.spc.mode = O_RDONLY;
    sts = qiow_openspc(ioq, &spc.spc);
    if (QIO_ERR_CODE(sts))
    {
        snprintf(emsg, sizeof(emsg), "Error opening game file %s\n", game_name);
        sho_qio_err(sts, emsg);
        qio_freeioq(ioq);
        return 3;
    }
    sts = stat(unix_name, &st);
    if (sts < 0)
    {
        emsgp = strerror(errno);
        fprintf(err_status, "Could not stat unix file: %s\n\t%s\n", unix_name, emsgp);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 1;
    }
    if (st.st_size != spc.eof)
    {
        fprintf(err_status, "Sizes don't match on Unix=%s, game=%s\n", unix_name, game_name);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 1;
    }
    ufp = fopen(unix_name, "rb");
    if (!ufp)
    {
        emsgp = strerror(errno);
        fprintf(err_status, "Could not open unix file: %s\n\t%s\n", unix_name, emsgp);
        qiow_close(ioq);
        qio_freeioq(ioq);
        return 1;
    }
    if (!(dowhat&DO_QUIETLY))
    {
        print_mkb("Verifying ", spc.alloc, ", ");
        printf("FID=%08X, %s\n", spc.fid, game_name);
        fflush(stdout);
    }
    while (1)
    {
        sts = qiow_read(ioq, disk_buff, sizeof(disk_buff));
        if (QIO_ERR_CODE(sts))
        {
            if (sts == QIO_EOF)
            {
                sts = 0;
                break;
            }
            snprintf(emsg, sizeof(emsg), "Error reading game file %s\n", game_name);
            sho_qio_err(sts, emsg);
            break;
        }
        if ((ioq->iocount&3))
        {
            memset((U8*)disk_buff + ioq->iocount, 0, 4-(ioq->iocount&3));
        }
        src = disk_buff;
        for (len=0; len < ioq->iocount; len += 4)
        {
            g_cs += *src++;
        }
        len = fread(disk_buff, 1, ioq->iocount, ufp);
        if (len != ioq->iocount)
        {
            emsgp = strerror(errno);
            fprintf(err_status, "Error writing unix file %s:\n\t%s\n", unix_name, emsgp);
            break;
        }
        src = disk_buff;
        for (len=0; len < ioq->iocount; len += 4)
        {
            u_cs += *src++;
        }
    }
    if (u_cs != g_cs)
    {
        fprintf(err_status, "Checksum mismatch. Unix file %s\n\tUnix=%08X, Game=%08X\n", unix_name, u_cs, g_cs);
    }
    fclose(ufp);
    qiow_close(ioq);
    qio_freeioq(ioq);
    return sts;
}

static int do_it(int dowhat, ParseUnion *pf, FILE *ofp)
{
    QioIOQ *ioq=0;
    int sts=1;
    char emsg[132], *aname, *aname_pool=0;
    const char *phys, *virt;

    ioq = qio_getioq();
    do
    {
        if ( debug_level )
            printf("do_it(): dowhat=0x%X, ptype=%d\n", dowhat, pf?pf->type:0);
        if ( (dowhat & DO_INIT) && pf && pf->type == PARSE_TYPE_VOL )
        {
            FsysInitVol ivol;
            struct stat st;

            sts = qiow_open(ioq, pf->v.phys, O_RDONLY);
            if (QIO_ERR_CODE(sts))
            {
                snprintf(emsg, sizeof(emsg), "Unable to open %s for input", pf->v.phys);
                sho_qio_err(sts, emsg);
                break;
            }
            sts = qiow_fstat(ioq, &st);     /* get size of disk */
            if (QIO_ERR_CODE(sts))
            {
                snprintf(emsg, sizeof(emsg), "Unable to stat %s", pf->v.phys);
                sho_qio_err(sts, emsg);
                break;
            }
            qiow_close(ioq);
            memset(&ivol, 0, sizeof(ivol)); /* in case I add more fields later */
            ivol.cluster = 1;
            ivol.index_sectors = pf->v.index_sectors ? pf->v.index_sectors : 100;
            ivol.free_sectors = pf->v.free_sectors ? pf->v.free_sectors : 110;
            ivol.root_sectors = pf->v.root_sectors ? pf->v.root_sectors : 120;
            ivol.def_extend = pf->v.def_extend ? pf->v.def_extend : 130;
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
            ivol.journal_sectors = pf->v.jou_sectors ? pf->v.jou_sectors : 1*1024*1024/512;
#endif
            ivol.max_lba = pf->v.max_lba;
            ivol.hb_range = pf->v.hb_range;
            sts = fsys_initfs(pf->v.phys, &ivol);
            if (QIO_ERR_CODE(sts))
            {
                snprintf(emsg, sizeof(emsg), "Unable to init filesystem on %s", pf->v.phys);
                sho_qio_err(sts, emsg);
            }
            break;
        }
        phys = "/rd0";
        virt = "/d0";
        if (puv)
        {
            if (puv->v.phys) phys = puv->v.phys;
            if (puv->v.virt) virt = puv->v.virt;
        }
        sts = fsys_mountw(phys, virt);
        if (QIO_ERR_CODE(sts) && sts != FSYS_MOUNT_MOUNTED)
        {
            snprintf(emsg, sizeof(emsg), "Unable to mount %s on %s", virt, phys);
            sho_qio_err(sts, emsg);
            break;
        }
        if (!dowhat || !pf) break;      /* nothing left to do */
        if ((dowhat&DO_DIR))
        {
            int fmt=0;
            if ((dowhat&DO_DIR_Q)) fmt |= AFSYS_DIR_Q;
            if ((dowhat&DO_DIR_F)) fmt |= AFSYS_DIR_F;
            if ((dowhat&DO_DIR_L)) fmt |= AFSYS_DIR_L;
            sts = afsys_dir(fmt, ofp);
            break;
        }
        aname = pf->f.game_name;
        if (strncmp(aname, "/d0/", 4) != 0)
        {
            int len;
            len = strlen(aname) + 5;
            aname_pool = (char *)malloc(len);
            if (!aname_pool)
            {
                fprintf(err_status, "Error malloc'ing %d bytes for %s\n", len, aname);
                sts = 8;
                break;
            }
            strcpy(aname_pool, "/d0/");
            strcpy(aname_pool+4, aname);
            aname = aname_pool;
        }
        if (pf->type == PARSE_TYPE_DEL)
        {
            if (cksums)
            {
                FsysOpenT spc;
                spc.spc.path = aname;
                spc.spc.mode = O_RDONLY;
                spc.fid = 0;
                sts = qiow_openspc(ioq, &spc.spc);
                if (!QIO_ERR_CODE(sts))
                {
                    del_checksum(spc.fid, 0);   /* zap the checksum if present */
                    qiow_close(ioq);
                }
            }
            sts = qiow_delete(ioq, aname);  /* zap the file */
            if (sts == FSYS_DELETE_DIR)   /* if we tried to delete a directory */
            {
                sts = qiow_rmdir(ioq, aname);   /* then delete with rmdir */
            }
            if (QIO_ERR_CODE(sts) && sts != FSYS_LOOKUP_FNF && sts != FSYS_LOOKUP_NOPATH)
            {
                snprintf(emsg, sizeof(emsg), "Unable to delete %s", aname);
                sho_qio_err(sts, emsg);
            }
            break;
        }
        if ((dowhat & DO_VERIFY))
        {
            sts = verify_game_to_unix(pf->f.unix_name, aname, dowhat);
        }
        else
        {
            if (debug_level) printf("do_it(): dowhat = %08X\n", dowhat);
            if ((dowhat & DO_TOATARI))
            {
                int copies;
                copies = pf->f.copies ? pf->f.copies : 1;   /* assume copies */
                if (pf->type >= PARSE_TYPE_BOOT0 && pf->type <= PARSE_TYPE_BOOT3) /* special case the boot images */
                {
                    copies = -(pf->type - PARSE_TYPE_BOOT0);
                }
                if (pf->type == PARSE_TYPE_CS || pf->f.nocksum)
                {
                    dowhat &= ~DO_CHKSUM;
                }
                else
                {
                    dowhat |= DO_CHKSUM;
                }
                sts = unix_to_game(aname, pf->f.unix_name, copies, pf->f.def_extend, dowhat);
            }
            else
            {
                sts = game_to_unix(pf->f.unix_name, aname, dowhat);
            }
        }
    } while (0);
    if (aname_pool) free(aname_pool);
    if (ioq)
    {
        qiow_close(ioq);
        qio_freeioq(ioq);
        ioq = 0;
    }
    fflush(stdout);
    return sts;
}

static char inp_buf[65536];
static int pipe_fd, inp_idx, out_idx;

#if 0
static int fool_gdb_db;
static int fool_gdb(int arg)
{
    return fool_gdb_db + arg;
}
#else
    #define fool_gdb(x) do { ; } while (0)
#endif

static int get_record(FILE *ifp, const char *pipe_name, char *outp, int osize)
{
    int len, leading=0, oosize = osize;
    char *src, *optr = outp;

    --osize;                    /* leave room for trailing null */
    if (ifp)
    {
        outp[osize] = 0;
        while (1)
        {
            len = 0;
            if (fgets(outp, osize, ifp))
            {
                len = strlen(outp);
                if (len > 0 && outp[len-1] == '\n')
                {
                    outp[len-1] = 0;
                    --len;
                }
                src = outp;
                while ((isspace(*src)))    /* eat leading white space */
                {
                    ++src;
                }
                if (!*src)
                {
                    if (debug_level) printf("get_record: skipped blank line\n");
                    continue;       /* skip blank lines */
                }
            }
            break;
        }
        return len;
    }
    while (1)
    {
        while (1)
        {
            if (out_idx >= inp_idx)       /* if the buffer is empty */
            {
                inp_idx = out_idx = 0;
                if (!pipe_fd)
                {
                    pipe_fd = open(pipe_name, O_RDONLY);
                    if (pipe_fd < 0)
                    {
                        if (errno == EINTR) continue;   /* this is ok */
                        perror("Error opening input pipe");
                        exit (5);
                    }
                }
                while ((len=read(pipe_fd, inp_buf+inp_idx, sizeof(inp_buf)-inp_idx)) > 0)
                {
                    inp_idx += len;
                    if (inp_idx >= sizeof(inp_buf)) break;
                }
                if (len <= 0 && !inp_idx)
                {
                    if (len == -1 && errno == EINTR) continue;  /* this is ok */
                    close(pipe_fd);
                    pipe_fd = 0;
                    return 0;
                }
            }
            break;
        }
        src = inp_buf + out_idx;
        if (!leading)
        {
            while (out_idx < inp_idx)
            {
                if (!isspace(*src))
                {
                    leading = 1;
                    break;
                }
                ++out_idx;
                ++src;
            }
            if (out_idx >= inp_idx)
            {
                fool_gdb(2);
                continue;
            }
        }
        while (osize > 0 && out_idx < inp_idx)
        {
            *outp++ = *src;
            ++out_idx;
            --osize;
            if (*src++ == '\n') break;
        }
        *outp = 0;
        len = strlen(optr);
        if (len && len < oosize)
        {
            if (outp[-1] == '\n')     /* terminate on a newline? */
            {
                *--outp = 0;
                --len;
            }
            else                /* nope */
            {
                fool_gdb(3);
                continue;           /* keep reading until we get a newline */
            }
        }
        break;
    }
    return len;
}

static void help_him(void)
{
    fputs("Usage: afsys [-s name] [-c name] [[-F] [-Dn] [-l[qf]] -t[|-u] [-f] [-Cn] [-q]"
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
          " -U path [-PN]"
#else
          " [-Un]"
#endif
          " a=name u=name]\n", stdout);
    fputs("where  -F       initialize the Atari volume\n", stdout);
    fputs("       -c name  read commands from file 'name'. Can be fifo. ('-' means stdin).\n", stdout);
    fputs("       -s name  write command status to file 'name'.\n", stdout);
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
    fputs("       -Un      'n' is 0 or 1 for unit (drive) 0 or 1 (default=0)\n", stdout);
#else
    fputs("       -U path  path to Atari disk or image file. REQUIRED parameter (i.e. /dev/sdj or /bla-bla/fubar.img)\n", stdout);
    fputs("       -P       Expect partition table on disk or in image file\n", stdout);
    fputs("       -N       Do not put or use partition table on disk or in image file\n", stdout);
#endif
    fputs("       -Dn      'n' is debug level, 0-F. Default is 0\n", stdout);
    fputs("       -d       delete the file spec'd by a=xxx\n", stdout);
    fputs("       a=name   'name' on Atari filesystem\n", stdout);
    fputs("       u=name   'name' on Unix filesystem\n", stdout);
    fputs("                 (A single dash, '-', means use stdin or stdout)\n", stdout);
    fputs("       -t       copy Unix file _TO_ Atari filesystem\n", stdout);
    fputs("       -u       copy Unix file _FROM_ Atari filesystem\n", stdout);
    fputs("                 (There must be either a -t or -u but not both\n", stdout);
    fputs("       -f       Ignore file dates/sizes. Copy them anyway.\n", stdout);
    fputs("       -l       Get directory listing of Atari filesystem\n", stdout);
    fputs("       -ll      Get extended directory listing of Atari filesystem\n", stdout);
    fputs("       -lf      Emit directory listing in command file format\n", stdout);
    fputs("       -lq      Get directory listing of Atari filesystem w/o totals\n", stdout);
    fputs("       -q       Don't output anything to stdout\n", stdout);
}

extern int ide_init(void);

int main(int argc, char *argv[])
{
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
    int cp0;
#else
    int usePartitionTable=FSYS_PARTITION_MAYBE;
    char *device_name=0;
#endif
    int unit=0;
    int dowhat=0, sts;
    char *unix_name=0, *atari_name=0, *pipe_name=0, *status_name=0;
    char *s, c, tphys[16];
    ParseUnion tpu, tvol;

#if 0
    if ( sizeof(long) > 4 )
    {
        long lsz=sizeof(long), isz=sizeof(int), psz=sizeof(char *);
        printf("Sorry, this program, as written, has to be built as a 32 bit app.\n\tsizeof(long)=%ld, sizeof(int)=%ld, sizeof(*)=%ld, need 4, 4, 'don't care' respectively\n",
               lsz, isz, psz);
        return 1;
    }
#endif
    image_name = argv[0];
    --argc;
    ++argv;
    while (argc > 0)
    {
        int plus;
        s = *argv;
        c = *s++;
        if (c != '-' && c != '+') break;
        plus = (c == '+') ? 1 : 0;
        while (s && (c = *s++))
        {
            switch (c)
            {
            case 'c':
            case 's':
                if (!*s)
                {
                    --argc;
                    if (argc < 1)
                    {
                        fprintf(stderr, "Need a name on '%c' option\n", c);
                        help_him();
                        return 11;
                    }
                    ++argv;
                    s = *argv;
                }
                if (c == 'c')
                {
                    pipe_name = s;
                }
                else
                {
                    status_name = s;
                }
                s = 0;
                break;
            case 'd':
                if (plus)
                {
                    dowhat &= ~DO_DELETE;
                }
                else
                {
                    dowhat |= DO_DELETE;
                }
                break;
            case 'D':
                c = *s++;
                if (islower(c))
                    c = toupper(c);
                if (c >= 'A' && c <= 'F')
                {
                    c = c-'A' + 10;
                }
                else
                {
                    c -= '0';
                }
                if (c < 0 || c > 15)
                {
                    fprintf(stderr, "Invalid debug level: %s (%d)\n", s-1, c);
                    return 10;
                }
                debug_level = c;
                break;
            case 'f':
                if (plus)
                {
                    dowhat &= ~DO_FORCE;
                }
                else
                {
                    dowhat |= DO_FORCE;
                }
                break;
            case 'F':
                if (plus)
                {
                    dowhat &= ~DO_INIT;
                }
                else
                {
                    dowhat |= DO_INIT;
                }
                break;
            case 'l':
                if (plus)
                {
                    dowhat &= ~(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L);
                }
                else
                {
                    dowhat |= DO_DIR;
                    if (*s == 'q')
                    {
                        dowhat |= DO_DIR_Q;
                        ++s;
                    }
                    else if (*s == 'f')
                    {
                        dowhat |= DO_DIR_F|DO_DIR_Q;
                        ++s;
                    }
                    else if (*s == 'l')
                    {
                        dowhat |= DO_DIR_L;
                        ++s;
                    }
                }
                break;
            case 'q':
                if (plus)
                {
                    dowhat &= ~DO_QUIETLY;
                }
                else
                {
                    dowhat |= DO_QUIETLY;
                }
                break;
            case 't':
                if (plus)
                {
                    dowhat &= ~DO_TOATARI;
                }
                else
                {
                    dowhat |= DO_TOATARI;
                }
                break;
            case 'u':
                if (plus)
                {
                    dowhat &= ~DO_TOUNIX;
                }
                else
                {
                    dowhat |= DO_TOUNIX;
                }
                break;
            case 'U':
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
                c = *s++;
                if (c != '0' && c != '1' && c != '3')
                {
                    fprintf(stderr, "Invalid unit (drive) number: %s\n", s-1);
                    return 10;
                }
                unit = c - '0';
#else
                if (!*s)
                {
                    --argc;
                    if (argc < 1)
                    {
                        fprintf(stderr, "Need a name on '%c' option\n", c);
                        help_him();
                        return 11;
                    }
                    ++argv;
                    s = *argv;
                }
                device_name = s;
                s = 0;
#endif
                break;
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
            case 'P':
                usePartitionTable = FSYS_PARTITION_ALWAYS;
                break;
            case 'N':
                usePartitionTable = FSYS_PARTITION_NEVER;
                break;
#endif
            case 'v':
                if (plus)
                {
                    dowhat &= ~DO_VERIFY;
                }
                else
                {
                    dowhat |= DO_VERIFY;
                }
                break;
            default:
                fprintf(stderr, "Unrecognised option: %s\n", s-1);
                if (!(dowhat&DO_QUIETLY)) help_him();
                return 1;
            }
        }
        --argc;
        ++argv;
    }       
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
    setlocale(LC_ALL,"");
    if ( !device_name )
    {
        fprintf(stderr, "Error: A -U parameter is required.\n");
        return 2;
    }
    else
    {
        char *list[1];
        list[0] = device_name;
        if ( (dowhat&DO_INIT) && usePartitionTable == FSYS_PARTITION_MAYBE )
        {
            fprintf(stderr,"Error: Cannot init f/s (-F) without also specifying either -N or -P\n");
            return 2;
        }
        fsys_set_hd_names(n_elts(list),list,usePartitionTable);
    }
#endif
    if (!status_name)
    {
        err_status = stderr;
    }
    else
    {
        err_status = fopen(status_name, "w");
        if (!err_status)
        {
            fprintf(stderr, "Unable to open status file %s: %s\n", status_name, strerror(errno));
            return 2;
        }
    }
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
    cp0 = open("/dev/atl_cp0", O_RDONLY);
    if (cp0 < 0 && errno != EBUSY)
    {
        perror("Error opening /dev/atl_cp0");
        return 1;
    }
#endif
    qio_init();
    ide_init();
    fsys_init();
    if ( !(dowhat&DO_INIT) )
    {
        sts = get_home_block(0, "/d0");
        if (QIO_ERR_CODE(sts))
        {
            sho_qio_err(sts, "Unable to get home block");
            return 2;
        }
    }
    if (!pipe_name)
    {
        if (!argc && !dowhat)
        {
            help_him();
            return 1;
        }
        while (argc > 0)
        {
            s = *argv;
            c = *s++;
            if ((c != 'a' && c != 'u') || *s != '=')
            {
                fprintf(stderr, "Unrecognised input option: %s\n", s-1);
                if (!(dowhat&DO_QUIETLY)) help_him();
                return 2;
            }
            ++s;
            if (c == 'a')
            {
                if (atari_name)
                {
                    fprintf(stderr, "Can't have two Atari filenames: %s\n", s);
                    if (!(dowhat&DO_QUIETLY)) help_him();
                    return 3;
                }
                atari_name = s;
            }
            else
            {
                if (unix_name)
                {
                    fprintf(stderr, "Can't have two Unix filenames: %s\n", s);
                    if (!(dowhat&DO_QUIETLY)) help_him();
                    return 3;
                }
                unix_name = s;
            }
            --argc;
            ++argv;
        }   
        if ((dowhat&DO_DELETE))
        {
            if (!atari_name)
            {
                fprintf(stderr, "Need to specifiy an Atari name to delete via an a=xxx\n");
                return 4;
            }
            if ((dowhat&(DO_TOATARI|DO_TOUNIX|DO_INIT)))
            {
                fprintf(stderr, "-d is mutually exclusive with -t, -f and -I\n");
                return 5;
            }
            if (unix_name)
            {
                fprintf(stderr, "Unix name on a 'delete' is ignored\n");
                unix_name = 0;
            }
        }
        if ((dowhat&(DO_TOATARI|DO_TOUNIX)))
        {
            if ((dowhat&(DO_TOATARI|DO_TOUNIX)) == (DO_TOATARI|DO_TOUNIX))
            {
                fprintf(stderr, "Can't have both -t and -f\n");
                if (!(dowhat&DO_QUIETLY)) help_him();
                return 4;
            }
            if ((atari_name && !unix_name))
            {
                unix_name = atari_name;
            }
            else if ((!atari_name && unix_name))
            {
                atari_name = unix_name;
            }
        }
        else if (!(dowhat&DO_DELETE))
        {
            if (atari_name || unix_name)
            {
                fprintf(stderr, "Have to specify a -t or -f to copy a file\n");
                if (!(dowhat&DO_QUIETLY)) help_him();
                return 5;
            }
        }
        if ((dowhat&DO_DIR) && unix_name && unix_name[0] == '-') dowhat &= ~DO_DIR;
        if ( !(dowhat&(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L)) && (dowhat&(DO_TOATARI|DO_TOUNIX)) && !atari_name && !unix_name )
        {
            printf("With a -t or -u, you also have to provide a filename with one or both 'a=xx' and 'u=xx'\n);");
            return 1;
        }
        memset(&tpu, 0, sizeof(tpu));
        memset(&tvol, 0, sizeof(tvol));
        snprintf(tphys, sizeof(tphys), "/rd%d", unit);
        tvol.v.phys = tphys;
        tvol.v.virt = "/d0";
        tvol.type = PARSE_TYPE_VOL;
        puv = &tvol;
        tpu.f.unix_name = unix_name;
        tpu.f.game_name = atari_name;
        if ((dowhat&(DO_TOUNIX|DO_TOATARI)) || (dowhat&DO_DELETE))
        {
            tpu.type = (dowhat&DO_DELETE) ? PARSE_TYPE_DEL : PARSE_TYPE_FILE;
        }
        if ((dowhat&DO_DIR_F))
        {
            tcks_puv.type = PARSE_TYPE_CS;
            tcks_puv.f.game_name = "/d0/diags/checksums";
            if (!cks_puv) cks_puv = &tcks_puv;
            do_it(0, 0, stdout);
            get_checksums();
        }
        sts = do_it(dowhat, &tpu, stdout);
        dowhat &= ~(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L);
        if ((dowhat&DO_TOATARI))
        {
            QioIOQ *fioq;
            put_checksums();        /* update checksums, if appropriate */
            fioq = qio_getioq();
            sts = qiow_fsync(fioq, "/d0");
            qio_freeioq(fioq);
            if (QIO_ERR_CODE(sts))
            {
                sho_qio_err(sts, "FSYS sync task didn't start");
            }
        }
    }
    else
    {
        FILE *pipe_fp=0;
        struct stat pipe_st;
        char rcd[256];
        int pipefd=0;

        if (!strcmp(pipe_name, "-"))  /* stdin is a special */
        {
            pipe_fp = stdin;        /* but easy */
        }
        else
        {
            sts = stat(pipe_name, &pipe_st);
            if (sts < 0)
            {
                perror("Unable to stat input pipe");
                return 14;
            }
            if (!S_ISFIFO(pipe_st.st_mode))   /* if input is not a pipe */
            {
                pipe_fp = fopen(pipe_name, "r"); /* then open it for normal fgets() */
                if (!pipe_fp)
                {
                    perror("Unable to open pipe input");
                    return 15;
                }
            }
        }
        if (pipe_fp)          /* if we're reading a "normal" file */
        {
            ParseUnion *pf;
            ParseFile *file_head = 0, *file_tail=0;
            ParseFile *boot_head = 0, *boot_tail=0;
            ParseFile *del_head = 0, *del_tail=0;
            while ((sts = get_record(pipe_fp, pipe_name, rcd, sizeof(rcd))) > 0)
            {
                pf = 0;
                sts = parse_command(rcd, &pf);
                if (sts)
                {
                    return 16;
                }
                switch (pf->type)
                {
                case PARSE_TYPE_UNK:
                    fprintf(err_status, "Unknown command: %s\n", rcd);
                    free(pf);
                    continue;
                case PARSE_TYPE_VOL:
                    if (!puv)
                    {
                        puv = pf;
                    }
                    else
                    {
                        /* fprintf(err_status, "Only 1 VOLUME command allowed\n"); */
                        free(pf);
                    }
                    continue;
                case PARSE_TYPE_CS:
                    cks_puv = pf;
                    continue;
                case PARSE_TYPE_DEFAULT:
                    continue;
                case PARSE_TYPE_PART:
                    fprintf(err_status, "Parition commands not supported. Ignored command: %s\n", rcd);
                    free(pf);
                    continue;
                case PARSE_TYPE_SYNC:
                case PARSE_TYPE_FILE:
                    if (file_head)
                    {
                        file_tail->next = &pf->f;
                        file_tail = &pf->f;
                    }
                    else
                    {
                        file_head = file_tail = &pf->f;
                    }
                    continue;
                case PARSE_TYPE_BOOT0:
                case PARSE_TYPE_BOOT1:
                case PARSE_TYPE_BOOT2:
                case PARSE_TYPE_BOOT3:
                    if (boot_head)
                    {
                        boot_tail->next = &pf->f;
                        boot_tail = &pf->f;
                    }
                    else
                    {
                        boot_head = boot_tail = &pf->f;
                    }
                    continue;
                case PARSE_TYPE_DEL:
                    if ((dowhat&DO_TOATARI))
                    {
                        if (del_head)
                        {
                            del_tail->next = &pf->f;
                        }
                        else
                        {
                            del_head = del_tail = &pf->f;
                        }
                    }
                    else
                    {
                        if (!(dowhat&DO_QUIETLY)) fprintf(err_status, "Ignored DEL command during -u: %s\n", rcd);
                        free(pf);
                    }
                    continue;
                case PARSE_TYPE_EOF:
                    sts = 0;
                    free(pf);
                    break;
                }
                break;
            }
            if ((dowhat&DO_INIT))
            {
                if (!puv)
                {
                    fprintf(err_status, "Need a VOLUME command to perform an INIT\n");
                    return 3;
                }
                sts = do_it(DO_INIT, puv, stdout);
                dowhat &= ~DO_INIT;
            }
            do_it(dowhat&(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L), 0, stdout); /* mount the filesystem */
            dowhat &= ~(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L);
            if ((dowhat&DO_TOATARI)) get_checksums();   /* preload checksums if appropriate */
            while ((pf = (ParseUnion *)file_head))    /* walk the list of files */
            {
                sts = do_it(dowhat, pf, stdout);
                file_head = pf->f.next;
                if (file_tail == &pf->f) file_tail = 0;
                free(pf);
            }
            while ((pf = (ParseUnion *)boot_head))    /* walk the list of boot files */
            {
                sts = do_it(dowhat, pf, stdout);
                boot_head = pf->f.next;
                if (boot_tail == &pf->f) boot_tail = 0;
                free(pf);
            }
            while ((pf = (ParseUnion *)del_head)) /* walk the list of files to delete */
            {
                sts = do_it(dowhat, pf, stdout);
                del_head = pf->f.next;
                if (del_tail == &pf->f) del_tail = 0;
                free(pf);
            }
        }
        else
        {
            if ((dowhat&(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L)))
            {
                do_it(dowhat&(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L), 0, stdout);
                dowhat &= ~(dowhat&(DO_DIR|DO_DIR_Q|DO_DIR_F|DO_DIR_L));
            }
            while (1)
            {
                ParseUnion *pf;
                QioIOQ *fioq;

                sts = get_record(pipe_fp, pipe_name, rcd, sizeof(rcd));
                if (!sts)
                {
                    usleep(50000);          /* wait a little while */
                    continue;               /* ignore EOF's from pipe */
                }
                pf = 0;
                if (!strlen(rcd) || rcd[0] == '\n') continue;   /* skip blank lines */
                if (debug_level)
                {
                    printf("Processing: %s\n", rcd);
                    fflush(stdout);
                }
                sts = parse_command(rcd, &pf);
                if (sts)
                {
                    if (pf) free(pf);           /* give back any memory it malloc'd */
                    continue;               /* this is not an error */
                }
                if (!puv && pf->type != PARSE_TYPE_VOL)
                {
                    if (pf->type == PARSE_TYPE_DEFAULT)
                    {
                        continue;
                    }
                    if (pf->type == PARSE_TYPE_EOF)
                    {
                        sts = 0;
                        break;
                    }
                    fprintf(err_status, "The very first command must be a VOLUME directive.\n");
                    continue;
                }
                switch (pf->type)
                {
                case PARSE_TYPE_UNK:
                    fprintf(err_status, "Unknown command: %s\n", rcd);
                    free(pf);
                    continue;
                case PARSE_TYPE_VOL:
                    if (!puv)
                    {
                        puv = pf;
                    }
                    else
                    {
                        fprintf(err_status, "Only 1 VOLUME command allowed\n");
                        free(pf);
                    }
                    if ((dowhat&DO_INIT))
                    {
                        sts = do_it(DO_INIT, pf, stdout);
                        dowhat &= ~DO_INIT;
                    }
                    continue;
                case PARSE_TYPE_CS:
                    cks_puv = pf;
                    get_checksums();
                    continue;
                case PARSE_TYPE_DEFAULT:
                    continue;
                case PARSE_TYPE_PART:
                    fprintf(err_status, "Ignored command: %s\n", rcd);
                    free(pf);
                    continue;
                case PARSE_TYPE_FILE:
                case PARSE_TYPE_BOOT0:
                case PARSE_TYPE_BOOT1:
                case PARSE_TYPE_BOOT2:
                case PARSE_TYPE_BOOT3:
                case PARSE_TYPE_DEL:
                    sts = do_it(dowhat, pf, stdout);
                    free(pf);
                    continue;
                case PARSE_TYPE_SYNC:
                    put_checksums();        /* update checksums */
                    fioq = qio_getioq();
                    sts = qiow_fsync(fioq, "/d0");
                    qio_freeioq(fioq);
                    if (QIO_ERR_CODE(sts))
                    {
                        sho_qio_err(sts, "FSYS sync task didn't start");
                    }
                    free(pf);
                    continue;
                case PARSE_TYPE_PIPE: {
                        FILE *ofp;
                        int new_dowhat = 0;
                        if (pf->pipe.command)
                        {
                            if (!strcmp(pf->pipe.command, "LIST"))
                            {
                                new_dowhat = DO_DIR;
                            }
                            else if (!strcmp(pf->pipe.command, "LISTQ"))
                            {
                                new_dowhat = DO_DIR|DO_DIR_Q;
                            }
                            else if (!strcmp(pf->pipe.command, "LISTF"))
                            {
                                new_dowhat = DO_DIR|DO_DIR_Q|DO_DIR_F;
                            }
                            else if (!strcmp(pf->pipe.command, "TOGAME"))
                            {
                                if (debug_level)
                                {
                                    printf("Found PIPE TOGAME. Changing dowhat from %08X to %08X\n",
                                           dowhat, (dowhat&~DO_TOUNIX)|DO_TOATARI);
                                }
                                dowhat &= ~DO_TOUNIX;
                                dowhat |= DO_TOATARI;
                            }
                            else if (!strcmp(pf->pipe.command, "TOUNIX"))
                            {
                                dowhat &= ~DO_TOATARI;
                                dowhat |= DO_TOUNIX;
                            }
                            else if (!strcmp(pf->pipe.command, "FORCE"))
                            {
                                if (debug_level)
                                {
                                    printf("Found PIPE FORCE. Changing dowhat from %08X to %08X\n",
                                           dowhat, dowhat|DO_FORCE);
                                }
                                dowhat |= DO_FORCE;
                            }
                            else if (!strcmp(pf->pipe.command, "NOFORCE"))
                            {
                                dowhat &= ~DO_FORCE;
                            }
                            else if (!strcmp(pf->pipe.command, "MARK"))
                            {
                                if (pf->pipe.output)
                                {
                                    ofp = fopen(pf->pipe.output, "w");
                                    if (ofp)
                                    {
                                        fprintf(ofp, "%08lX\n", (unsigned long)time(0));
                                        fclose(ofp);
                                        ofp = 0;
                                    }
                                }
                                free(pf);
                                continue;
                            }
                            else
                            {
                                fprintf(err_status, "Unrecognised PIPE command: %s\n", pf->pipe.command);
                                free(pf);
                                continue;
                            }
                        }
                        else
                        {
                            fprintf(err_status, "No PIPE command specified\n");
                            free(pf);
                            continue;
                        }
                        if (new_dowhat)
                        {
                            if (pf->pipe.output)
                            {
                                ofp = fopen(pf->pipe.output, "w");
                                if (!ofp)
                                {
                                    char *em = strerror(errno);
                                    fprintf(err_status, "Unable to open PIPE command output file: %s\n\t%s\n",
                                            pf->pipe.output, em);
                                    free(pf);
                                    continue;
                                }
                            }
                            else
                            {
                                ofp = stdout;
                            }
                            sts = do_it(new_dowhat, pf, ofp);
                            fflush(ofp);
                            if (ofp != stdout) fclose(ofp);
                        }
                        free(pf);
                        continue;
                    }
                case PARSE_TYPE_EOF:
                    sts = 0;
                    free(pf);
                    break;
                }
                break;
            }
        }
        if ((dowhat&DO_TOATARI))
        {
            put_checksums();        /* update checksums, if appropriate */
            if (pipe_fp)
            {
                QioIOQ *fioq;
                fioq = qio_getioq();
                sts = qiow_fsync(fioq, "/d0");
                qio_freeioq(fioq);
                if (QIO_ERR_CODE(sts))
                {
                    sho_qio_err(sts, "FSYS sync task didn't start");
                }
            }
        }
        if (pipe_fp) fclose(pipe_fp);
        if (pipefd) close(pipefd);
    }
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
    close(cp0);
#endif
    fflush(err_status);
    fclose(err_status);
    return sts;
}
