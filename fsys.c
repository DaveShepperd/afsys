/* See LICENSE.txt for license details */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#include "os_proto.h"
#include "any_proto.h"
#include "nsprintf.h"
#include "st_proto.h"
#define QIO_LOCAL_DEFINES 1
#include "qio.h"
#include "fsys.h"
#if FSYS_USE_BUFF_POOLS || !FSYS_USE_MALLOC
    #include "mallocr.h"
#endif
#define BYTPSECT BYTES_PER_SECTOR
#if !__linux__
    #include "eer_defs.h"
#endif

#ifndef QIO_FIN_SHIM
    #define QIO_FIN_SHIM() do { ; } while (0)
#endif

#ifndef TEST_DISK_TIMEOUT
    #define TEST_DISK_TIMEOUT	0
#endif

#ifndef ST_GAME_CODE
    #define ST_GAME_CODE 1
#endif

#if defined(WDOG) && !NO_WDOG && ST_GAME_CODE 
    #define KICK_THE_DOG	1
#else
    #define KICK_THE_DOG	0
#endif

#ifndef FSYS_NO_AUTOSYNC
    #define FSYS_NO_AUTOSYNC 0
#endif

#if !defined(FSYS_SQUAWKING_STDIO)
    #define FSYS_SQUAWKING_STDIO 0
#endif

#ifndef FSYS_ASYNC_CLOSE
    #if defined(_LINUX_) && _LINUX_
        #define FSYS_ASYNC_CLOSE 0
    #else
        #define FSYS_ASYNC_CLOSE 1
    #endif
#endif

#ifndef FSYS_HAS_TQ_INS
    #if defined(_LINUX_) && _LINUX_
        #define FSYS_HAS_TQ_INS 0
    #else
        #define FSYS_HAS_TQ_INS 1
    #endif
#endif

#ifndef FSYS_HAS_DIRENT
    #define FSYS_HAS_DIRENT 1
#endif

#if (   FSYS_SQUAWKING || FSYS_FREE_SQUAWKING || FSYS_SYNC_SQUAWKING || FSYS_JOU_SQUAWKING \
     || FSYS_FH_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING || FSYS_WRITE_SQUAWKING ) 
    #if !FSYS_SQUAWKING_STDIO
        #include <iio_proto.h>
IcelessIO *fsys_iop;
        #define OUTWHERE fsys_iop,
    #else
        #define OUTWHERE stdout,
    #endif
#endif

#if FSYS_SQUAWKING 
    #if !FSYS_SQUAWKING_STDIO
        #define FSYS_SQK(x) iio_printf x
    #else
        #define FSYS_SQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define FSYS_SQK(x) do { ; } while (0)
#endif

#ifndef FSYS_DMP_FREE
    #define FSYS_DMP_FREE (0)
#endif

#ifndef FSYS_SYNC_FREE_SQUAWKING
    #define FSYS_SYNC_FREE_SQUAWKING (0)
#endif

#if FSYS_WRITE_SQUAWKING 
    #if !FSYS_SQUAWKING_STDIO
        #define FSYS_WR_SQK(x) iio_printf x
    #else
        #define FSYS_WR_SQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define FSYS_WR_SQK(x) do { ; } while (0)
#endif

#if FSYS_FREE_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define FSYS_FREE_SQK(x) iio_printf x
    #else
        #define FSYS_FREE_SQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define FSYS_FREE_SQK(x) do { ; } while (0)
#endif

#if FSYS_SYNC_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define SYNSQK(x) iio_printf x
    #else
        #define SYNSQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define SYNSQK(x) do { ; } while (0)
#endif

#ifndef FSYS_JOU_SQUAWKING
    #define FSYS_JOU_SQUAWKING (0)
#endif

#if FSYS_JOU_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define JOUSQK(x) iio_printf x
    #else
        #define JOUSQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define JOUSQK(x) do { ; } while (0)
#endif

#if FSYS_FH_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define FHSQK(x) iio_printf x
    #else
        #define FHSQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define FHSQK(x) do { ; } while (0)
#endif

#if FSYS_RD_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define RDSQK(x) iio_printf x
    #else
        #define RDSQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define RDSQK(x) do { ; } while (0)
#endif

#if FSYS_WR_SQUAWKING
    #if !FSYS_SQUAWKING_STDIO
        #define WRSQK(x) iio_printf x
    #else
        #define WRSQK(x) do { fprintf x; fflush(stdout); } while (0)
    #endif
#else
    #define WRSQK(x) do { ; } while (0)
#endif

/* set TEST_DISK_ERRORS non-zero to enable fake disk errors */
/* set TEST_DISK_TIMEOUT non-zero to enable fake disk timeouts */

#if defined(HAVE_TIME) && HAVE_TIME
    #include <time.h>
#endif
#if !HOST_BOARD
    #include <assert.h>
#endif		/* HOST_BOARD */

#if !defined(FSYS_NO_SEARCH_HBLOCK)
    #define FSYS_NO_SEARCH_HBLOCK 1		/* default to no homeblock search */
#endif
#if _LINUX_
extern void sync(void);
#endif

typedef struct opnfile_t
{
    FsysOpenT *details; /* point to user's details */
    FsysVolume *vol;    /* volume on which to create file */
    QioIOQ *ioq;    /* completion function, etc. */
    const char *path;   /* pointer to path/filename */
    int mode;       /* mode bits as defined in fcntl.h */
} FsysOpenFileT;

static U32 inx_mask;

FsysVolume volumes[FSYS_MAX_VOLUMES];
#ifndef FSYS_QIO_BATCH
    #define FSYS_QIO_BATCH	16
#endif

static FsysQio *fsysqio_pool_head;
static int32_t fsysqio_pool_batch;

#if FSYS_UMOUNT
    #if MALLOC_DEBUG
        #define QMOUNT_ALLOC(vol, amt) qmount_alloc(vol, amt, __FILE__, __LINE__)
        #define QMOUNT_REALLOC(vol, old, amt) qmount_realloc(vol, old, amt, __FILE__, __LINE__)
        #define QMOUNT_FREE(vol, head) qmount_free(vol, head, __FILE__, __LINE__)
    #else
        #define QMOUNT_ALLOC(vol, amt) qmount_alloc(vol, amt)
        #define QMOUNT_REALLOC(vol, old, amt) qmount_realloc(vol, old, amt)
        #define QMOUNT_FREE(vol, head) qmount_free(vol, head)
    #endif
#else
    #define QMOUNT_ALLOC(vol, amt) QIOcalloc(1, amt)
    #define QMOUNT_REALLOC(vol, old, amt) QIOrealloc(old, amt)
    #define QMOUNT_FREE(vol, head) QIOfree(head)
#endif

#ifdef EER_FSYS_USEALT
static void inc_bram(int arg)
{
    int t;
    t = eer_gets(arg)+1;
    if (t < 256) eer_puts(arg, t);
}

    #define USED_ALT() inc_bram(EER_FSYS_USEALT);
#else
    #define USED_ALT() do { ; } while (0)
#endif

#ifndef FSYS_FQIO_BATCH
    #define FSYS_FQIO_BATCH (32)
#endif

static int fsys_qio_gets, fsys_qio_frees;

/************************************************************
 * fsys_getqio - Get a FsysQio from the system's pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
FsysQio *fsys_getqio(void)
{
    FsysQio *fq;
    if (!fsysqio_pool_head)
    {
        const struct st_envar *st;
        if (!fsysqio_pool_batch)
        {
            st = st_getenv("FSYS_FQIO_BATCH", 0);
            if (!st || !st->value)
            {
                fsysqio_pool_batch =  FSYS_FQIO_BATCH;
            }
            else
            {
                fsysqio_pool_batch = qio_cvtFromPtr(st->value);
            }
        }
        else
        {
            st = st_getenv("FSYS_FQIO_BATCH_GROWABLE", 0);
            if (!st || !st->value) fsysqio_pool_batch = -1;
        }
        if (fsysqio_pool_batch <= 0) return 0;
    }
    fq = (FsysQio *)qio_getioq_ptr((QioIOQ **)&fsysqio_pool_head, sizeof(FsysQio), fsysqio_pool_batch);
    if (fq) ++fsys_qio_gets;
    return fq;
}

/************************************************************
 * fsys_freeqio - Free a FsysQio as obtained from a previous
 * call to fsys_getqio().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
int fsys_freeqio(FsysQio *que)
{
    int sts=0;
    if (que)
    {
        if (que->our_ioq.next) return 2;
        ++fsys_qio_frees;
        sts = qio_freeioq_ptr((QioIOQ *)que, (QioIOQ **)&fsysqio_pool_head);
    }
    return sts;
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
/************************************************************
 * fsys_get_volume - get pointer to volume struct
 *
 * At entry:
 *	which - file descriptor of device with volume
 *
 * At exit:
 *	pointer to FsysVolume structure or 0 if error.
 */

static FsysVolume *fsys_get_volume(int which)
{
    QioFile *file;
    const QioDevice *dvc;
    FsysVolume *vol;

    file = qio_fd2file(which);
    if (!file) return 0;
    dvc = file->dvc;
    if (!dvc) return 0;
    vol = (FsysVolume *)dvc->private;
    if (!vol || vol->id != FSYS_ID_VOLUME) return 0;
    return vol;
}
#endif

typedef struct fsys_ltop
{
    uint32_t phys;         /* physical LBA */
    uint32_t cnt;          /* max number of sectors */
} FsysLtop;

/************************************************************
 * fsys_ltop - convert a logical sector number/count to a starting
 * physical sector number/count pair. This is an internal support
 * function for the file reader.
 *
 * At entry:
 *	ptrs - pointer to array of retrieval pointers
 *	sector - user's logical sector number
 *	count - user's sector count
 * At exit:
 *	returns a struct (_not_ a pointer to struct) containing
 *	the physical sector number and the maximum number of
 *	sectors that can be read or 0 if the logical sector is
 *	out of bounds.
 */

static FsysLtop fsys_ltop(FsysRamRP *ramp, uint32_t sector, int count)
{
    FsysLtop ans;
    FsysRetPtr *ptrs;
    uint32_t lba, lbacnt;
    int nptrs;

    ans.phys = 0;       /* assume no conversion is possible */
    ans.cnt = 0;
    lba = 0;            /* look from the beginning */
#if 0 && FSYS_SQUAWKING
    {
        FsysRamRP *op;
        int ii, jj;
        FSYS_SQK((OUTWHERE "ltop: ramp = %08lX, sector = %08lX, count = %d\n", ramp, sector, count));
        op = ramp;
        ii = 0;
        while (op)
        {
            ptrs = op->rptrs;
            nptrs = op->num_rptrs;
            FSYS_SQK((OUTWHERE "    %d: ptrs = %08lX, nptrs = %d\n", ii++, ptrs, nptrs));
            for (jj=0; jj < nptrs; ++jj, ++ptrs)
            {
                FSYS_SQK((OUTWHERE "      %d: start = %08lX, nblocks=%d\n", jj, ptrs->start, ptrs->nblocks));
            }
#if !FSYS_READ_ONLY
            op = op->next;
#else
            op = 0;
#endif
        }
    }
#endif
    while (ramp)
    {
        ptrs = ramp->rptrs;
        nptrs = ramp->num_rptrs;
        for (;nptrs; --nptrs, ++ptrs)
        {
            uint32_t diff;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
            /* save doing an integer multiply if not required */
            if (ptrs->repeat == 1 || ptrs->nblock == 1)
            {
                if (ptrs->repeat == 1)
                {
                    lbacnt = ptrs->nblocks; 
                }
                else
                {
                    lbacnt = ptrs->repeat;
                }
            }
            else
            {
                lbacnt = ptrs->repeat*ptrs->nblocks;
            }
            if (!lbacnt) continue;
#else
            if (!(lbacnt = ptrs->nblocks)) continue;
#endif
            if (sector >= lba && sector < lba+lbacnt)
            {
                diff = sector-lba;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
                if (!ptrs->skip)          /* if no skip field */
                {
                    ans.phys = ptrs->start + diff;  /* then phys is just start + offset */
                    ans.cnt = lbacnt - diff;        /* and count is amount left in ptr block */
                }
                else
                {
                    uint32_t t;
                    t = diff/ptrs->nblocks;     /* otherwise, it gets very complicated */
                    t *= ptrs->nblocks+ptrs->skip;
                    ans.phys = ptrs->start + t;
                    t = diff%ptrs->nblocks;
                    ans.phys += t;
                    ans.cnt = ptrs->nblocks-t;
                }
#else
                ans.phys = ptrs->start + diff;
                ans.cnt = lbacnt - diff;
#endif
                if (ans.cnt > count) ans.cnt = count;   /* maximize the count to the caller's value */
#if 0 && FSYS_SQUAWKING
                FSYS_SQK((OUTWHERE "ltop: returning phys = %08lX, cnt = %d\n", ans.phys, ans.cnt));
#endif
                return ans;
            }
            lba += lbacnt;  
        }
#if !FSYS_READ_ONLY
        ramp = ramp->next;
#else
        ramp = 0;
#endif
    }
#if 0 && FSYS_SQUAWKING
    FSYS_SQK((OUTWHERE "ltop: returning phys = %08lX, cnt = %d\n", ans.phys, ans.cnt));
#endif
    return ans;
}

FsysRamFH *fsys_find_ramfh(FsysVolume *vol, uint32_t id)
{
    FsysRamFH *rfh = 0;
    FsysFilesLink *fl;
    int idx;

    fl = vol->filesp;
    idx = 0;
    while (fl && id >= idx + fl->items)
    {
        idx += fl->items;
        fl = fl->next;
    }
    if (fl)
    {
        rfh = (FsysRamFH *)(fl+1) + id - idx;
    }
    return rfh;
}

int fsys_find_id(FsysVolume *vol, FsysRamFH *rfh)
{
    FsysFilesLink *fl;
    int idx;
    FsysRamFH *cp;

    fl = vol->filesp;
    idx = 0;
    while (fl)
    {
        cp = (FsysRamFH *)(fl+1);
        if (rfh >= cp && rfh < cp + fl->items)
        {
            return rfh-cp + idx;
        }
        idx += fl->items;
        fl = fl->next;
    }
    return -1;
}

#if !FSYS_READ_ONLY
/********************************************************************
 * compute_total_free - walks the free list and computes how many free
 * sectors remain on the disk.
 *
 * At entry:
 *	vol - pointer to volume set to which the file belongs
 *
 * At exit:
 *	returns the number of computed free clusters.
 */
static int compute_total_free(FsysVolume *vol)
{
    FsysRetPtr *rp;
    FsysRamFH *rfh;
    int ii, tot, prev;

    rp = vol->free;
    prev = 0;
    for (tot=ii=0; rp->start && ii < vol->free_elems; ++ii, ++rp)
    {
#if 1				/* for compatibility with old filesystems */
        if (rp->start < prev) break;
        prev = rp->start + rp->nblocks;
#endif
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
        tot += rp->nblocks*rp->repeat;
#else
        tot += rp->nblocks;
#endif
    }

    rfh = fsys_find_ramfh(vol, FSYS_INDEX_FREE); /* point to freelist header */
    rfh->size = (ii*sizeof(FsysRetPtr)+(BYTPSECT-1))&-BYTPSECT;
    vol->free_ffree = ii;
    return tot;
}

static int collapse_free(FsysVolume *vol, int low, int sqk);

    #ifndef _FSYS_FINDFREE_T_
        #define _FSYS_FINDFREE_T_
typedef struct fsys_findfree_t
{
    FsysRetPtr *freelist; /* ptr to freelist */
    FsysRetPtr *reply;  /* ptr to place to deposit answer */
    U32 request;    /* number of sectors to get */
    U32 skip;       /* number of blocks to skip */
    U32 lo_limit;   /* allocate above this sector */
    U32 hint;       /* connect to this sector if possible */
    int exact;      /* .ne. if size must be exact */
    int actelts;    /* number of active elements in freelist */
    int totelts;    /* total number of elements in freelist */
    int which_elem; /* gets which freelist elem modified */
} FsysFindFreeT;
    #endif

/************************************************************
 * fsys_findfree - find the next free n sectors. This function
 * walks the freelist and tries to find a block of sectors that
 * matches the request'ed amount or an amount nearest. This
 * function is used internally by the filesystem and not expected
 * to be used by the casual user.
 * 
 * At entry:
 *	freet - pointer to FsysFindFreeT struct.
 *	hint - find space starting here if possible (0=don't care)
 *
 * At exit:
 *	returns 0 if success, 1 if nothing available.
 *	updates freelist accordingly.
 */
static int fsys_findfree( FsysFindFreeT *ft )
{
    int ii;
    int nearest_over, nearest_under;
    int32_t diff_over, diff_under;
    FsysRetPtr *fp, *free;

#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
#error You need to rewrite fsys_findfree and add repeat/skip support.
#endif

    if (!ft)
    {
#if FSYS_FREE_SQUAWKING
        FSYS_SQK((OUTWHERE "fsys_findfree: ft is 0\n"));
#endif
        return 0;
    }

    ft->which_elem = INT_MAX;
    fp = ft->freelist;
    free = ft->reply;

    if (!ft->request || !fp || !free)
    {
#if FSYS_FREE_SQUAWKING
        if (!ft->request) FSYS_SQK((OUTWHERE "fsys_findfree: ft->request is 0\n"));
        if (!fp) FSYS_SQK((OUTWHERE "fsys_findfree: ft->freelist is 0\n"));
        if (!free) FSYS_SQK((OUTWHERE "fsys_findfree: ft->reply is 0\n"));
#endif
        return 0;
    }
#if FSYS_FREE_SQUAWKING
    {
        int ii;
        FsysRetPtr *dst;
        dst = ft->freelist;
        FSYS_SQK((OUTWHERE "%6ld: findfree before. rqst=%ld, hint=%ld, lo_lim=%ld, actelts=%d, totelts=%d.\n",
                  eer_rtc, ft->request, ft->hint, ft->lo_limit, ft->actelts, ft->totelts));
        for (ii=0; ii < ft->actelts; ++ii, ++dst)
        {
            FSYS_SQK((OUTWHERE "\t    %3d: start=%7ld, nblocks=%7ld\n",
                      ii, dst->start, dst->nblocks));
        }
    }
#endif

    nearest_over = nearest_under = -1;
    diff_over = S32_MAX;
    diff_under = S32_MIN;
    for (ii=0; ii < ft->actelts; ++ii, ++fp) /* look through the list for one that fits */
    {
        uint32_t d;
        int size;
        size = fp->nblocks;
        if (!size) continue;        /* empty entry */
        if (!fp->start) continue;   /* empty entry */
        if (ft->hint && fp->start == ft->hint) /* entry abuts the request */
        {
            nearest_over = ii;      /* give it to 'em */
            break;
        }
        if (fp->start+fp->nblocks <= ft->lo_limit) continue; /* keep looking */
        if (fp->start < ft->lo_limit) /* may need to split a section in two */
        {
            if (ft->actelts < ft->totelts-1)  /* if room for another entry */
            {
                d = ft->lo_limit - fp->start; /* compute how much to skip */
                if (ft->request < fp->nblocks-d) /* if there's enough left over to split */
                {
                    FsysRetPtr *dst;
                    dst = ft->freelist + ft->actelts + 1;
                    memset(dst, 0, sizeof(FsysRetPtr));
                    --dst;
                    for (; dst > fp; --dst)
                    {
                        memcpy(dst, dst-1, sizeof(FsysRetPtr));
                    }
                    ++dst;
                    dst->start = fp->start + d;
                    dst->nblocks = fp->nblocks - d;
                    fp->nblocks = d;
                    nearest_over = ii+1;    /* give 'em the new one */
                    ++ft->actelts;      /* tell 'em we changed it */
                    break;
                }
            }
        }
        d = fp->nblocks - ft->request;
        if (d == 0)           /* if there's one that exactly fits */
        {
            nearest_over = ii;
            break;          /* use it directly */
        }
        if (d > 0)            /* if the size is over.. */
        {
            if (fp->start > ft->hint)
            {
                nearest_over = ii;  /* issue out of the nearest largest space next to hint */
                break;
            }
            if (d < diff_over)    /* ..but difference is closer */
            {
                nearest_over = ii;  /* remember this entry */
                diff_over = d;      /* and the difference */
            }
        }
        else
        {
            if (d > diff_under)   /* size of under but difference is closer */
            {
                nearest_under = ii; /* remember this entry */
                diff_under = d;     /* and the difference */
            }
        }       
    }
    if (nearest_over >= 0)        /* there's a nearest over */
    {
        int act;
        fp = ft->freelist + nearest_over;
        act = fp->nblocks;      /* size of free region */
        if (act > ft->request) act = ft->request; /* maximize to requested amount */
        free->start = fp->start;    /* his region starts at the free start */
        free->nblocks = act;        /* he gets what he asked for or the size of the region */
        fp->nblocks -= act;     /* take from the free region size what we gave away */
        if (fp->nblocks)      /* if there is any left */
        {
            fp->start += act;       /* advance the starting point of free region */
        }
        else
        {
            fp->start = 0;      /* gave the whole region away, zap it */
        }
        ft->which_elem = nearest_over;  /* tell 'em which one we modified */
    }
    else if (!ft->exact && nearest_under >= 0)
    {
        fp = ft->freelist + nearest_under;
        free->start = fp->start;
        free->nblocks = fp->nblocks;
        fp->start = 0;
        fp->nblocks = 0;
        ft->which_elem = nearest_under;
    }
    else
    {
        free->start = 0;
        free->nblocks = 0;
    }
#if FSYS_FREE_SQUAWKING
    {
        int ii;
        FsysRetPtr *dst;
        dst = ft->freelist;
        if (ft->which_elem < INT_MAX)
        {
            FSYS_SQK((OUTWHERE "        after. %d actelts. Patched entry %d\n",
                      ft->actelts, ft->which_elem));
        }
        else
        {
            FSYS_SQK((OUTWHERE "        after. %d actelts. Patched no entries\n", ft->actelts));
        }
        for (ii=0; ii < ft->actelts; ++ii, ++dst)
        {
            FSYS_SQK((OUTWHERE "\t   %c%3d: start=%7ld, nblocks=%7ld\n",
                      ii == ft->which_elem ? '*' : ' ', ii, dst->start, dst->nblocks));
        }
        FSYS_SQK((OUTWHERE "        Returned to caller: start=%ld, nblocks=%ld\n",
                  free->start, free->nblocks));
    }
#else
    FSYS_SQK(( OUTWHERE "%6ld: find_free returns %d. rqst=%ld, start=%ld, nblocks=%ld.\n",
               eer_rtc, free->start == 0, ft->request, free->start, free->nblocks));
#endif
    return(free->start == 0);
}

/********************************************************************
 * extend_file - extends the retrieval pointer set on a file the
 * specified number of sectors.
 *
 * At entry:
 *	vol - pointer to volume set to which the file belongs
 *	fh - pointer to FsysRamFH of file to be extended
 *	rqst - number of sectors to add to the file
 *	rp - pointer to array of ram retrieval pointers
 *	where - area of disk on which to extend file
 *
 * At exit:
 *	returns 0 on success, and one of FSYS_EXTEND_xxx on error.
 *	Retrieval pointer set in the RamFH may have been updated.
 */

static int extend_file(FsysVolume *vol, int rqst, FsysRamRP *rp, int where)
{
    int cnt, low;
    FsysRetPtr *arp, ans;
    FsysFindFreeT freet;

/* Check to see if there is room on the disk for the requested
 * extension.
 */
    if (vol->total_free_clusters < rqst)
    {
        return FSYS_EXTEND_FULL;    /* filesystem is full */
    }

/* Walk through the entire retrieval pointer array and count how many
 * members there are. At the same time, find the last entry in the list.
 */
    cnt = 0;
    while (1)
    {
        cnt += rp->num_rptrs;
        if (!rp->next) break;
        rp = rp->next;
    }

    freet.skip = 0;
    freet.exact = 0;
    freet.freelist = vol->free;
    freet.actelts = vol->free_ffree;
    freet.totelts = vol->free_elems;
    freet.lo_limit = FSYS_COPY_ALG(where, vol->maxlba);
    low = INT_MAX;

    FSYS_SQK(( OUTWHERE "%6ld: extend_file: asked for %d. cnt=%d\n",
               eer_rtc, rqst, cnt));

/* Keep trying to extend until either the disk fills up, or the request
 * is satisfied.
 */
    while (rqst > 0)
    {
        if (cnt >= FSYS_MAX_FHPTRS)
        {
            return FSYS_EXTEND_2MNYRP;  /* too many retrieval pointers */
        }

        if (rp->num_rptrs > 0)
        {
            arp = rp->rptrs + rp->num_rptrs - 1; /* point to last active RP element */
            freet.hint = arp->start + arp->nblocks; /* try to find a connecting region */
        }
        else
        {
            arp = 0;
            freet.hint = 0;         /* choose no particular place */
        }
        freet.reply = &ans;
        freet.request = rqst;
        if (fsys_findfree( &freet ))
        {
            if (!freet.lo_limit)
            {
                FSYS_SQK(( OUTWHERE "        No room left on disk.\n"));
                return FSYS_EXTEND_FULL;    /* disk has become full */
            }
            FSYS_SQK(( OUTWHERE "        No room in area %ld. Flopping back to 0\n", freet.lo_limit));
            freet.lo_limit = 0;         /* else try again from begininig */
            continue;
        }
        vol->free_ffree = freet.actelts;    /* update if appropriate */
        if (arp && ans.start == freet.hint)   /* did we get an ajoining region? */
        {
            arp->nblocks += ans.nblocks;    /* just increase the size of the last one */
        }
        else
        {
            arp = 0;
        }
        if (!arp)
        {
/* Check to see if there is room in the current retrieval pointer array.
 */
            if (rp->num_rptrs >= rp->rptrs_size)
            {

/* There isn't, so create another chunk of retrieval pointers.
 */
                if (!cnt)             /* at the top level, we don't need a RamRP */
                {
#if FSYS_TIGHT_MEM
#define NUM_RET_PTR_TOP	1		/* one retrevial pointer at top level */
#define NUM_RET_PTR_CHAIN	8		/* overflow into 8 */
#else
#define NUM_RET_PTR_TOP	1		/* one retrevial pointer at top level */
#define NUM_RET_PTR_CHAIN	8		/* overflow into 8 */
#endif
                    FSYS_SQK(( OUTWHERE "        extending top level rptrs by %d\n", NUM_RET_PTR_TOP));
                    rp->rptrs = (FsysRetPtr *)QIOcalloc(NUM_RET_PTR_TOP*sizeof(FsysRetPtr),1);
                    if (!rp->rptrs) return FSYS_EXTEND_NOMEM;
                    rp->rptrs_size = NUM_RET_PTR_TOP;   /* this is how many are in there */
                    rp->mallocd = 1;        /* show rptrs is mallocd */
                }
                else
                {
                    FsysRamRP *trp;
                    FSYS_SQK(( OUTWHERE "       extending rptrs chain by %d\n", NUM_RET_PTR_CHAIN));
                    trp = (FsysRamRP *)QIOcalloc(sizeof(FsysRamRP)+NUM_RET_PTR_CHAIN*sizeof(FsysRetPtr),1);
                    if (!trp)
                    {
                        return FSYS_EXTEND_NOMEM;   /* oops, ran out of memory */
                    }
                    rp->next = trp;
                    rp = trp;           /* tell last guy about new one */
                    rp->rptrs = (FsysRetPtr *)(trp+1); /* point to an array of FsysRetPtr's */
                    rp->rptrs_size = NUM_RET_PTR_CHAIN; /* this is how many are in there */
                    rp->mallocd = 0;        /* not individually mallocd */
                }
                rp->next = 0;           /* new guy is last on the list */
                rp->num_rptrs = 0;      /* no pointers yet. */
            }
            arp = rp->rptrs + rp->num_rptrs;    /* point to RP element */
            *arp = ans;             /* give 'em the new allocation */
            ++rp->num_rptrs;            /* count the RP */
            ++cnt;              /* count the RP */
        }
        rqst -= ans.nblocks;            /* take the amount we obtained from request */
        vol->total_free_clusters -= ans.nblocks; /* take from volume total */
        vol->total_alloc_clusters += ans.nblocks; /* count allocated clusters */
        if (low > freet.which_elem) low = freet.which_elem;
    }
/*
 * Successfully extended the file(s) 
 */
    return collapse_free(vol, INT_MAX, FSYS_FREE_SQUAWKING);    /* adjust the freelist */
}
#endif

static int set_flags(QioFile *file, int new)
{
    int ops = prc_set_ipl(INTS_OFF);
    int old = file->flags;
    if (new < 0)
    {
        file->flags &= new;
    }
    else
    {
        file->flags |= new;
    }
    prc_set_ipl(ops);
    return old;
}

/************************************************************
 * fsys_qread - a file read primitive. This function will read
 * n sectors from a file as specified by the list of retrieval
 * pointers, the starting relative sector number and the sector
 * count. It queue's a read to the I/O subsystem and uses itself
 * as a completion function. It will continue to queue input until
 * the required bytes have been transferred or an error occurs.
 * The I/O subsystem will call this function as a completion
 * routine therefore it will (may) finish asychronously from the
 * calling program. This function is used internally by the
 * filesystem and not expected to be used by the casual user.
 * 
 * At entry:
 *	arg - pointer to read argument list. The list must be in
 *		static memory (i.e., not on the stack)
 * At exit:
 *	returns nothing. The function queue's an input which will
 *	complete asynchronously from the calling program.
 */

static void fsys_qread(QioIOQ *ioq)
{
    FsysQio *arg;
    QioFile *hdfile;
    int ok=1;

    arg = (FsysQio *)ioq;
    hdfile = qio_fd2file(ioq->file);
    if (!hdfile)
    {
        ioq->iostatus = QIO_NOTOPEN;    /* somehow file got closed on us */
        ok = 0;
    }
    else if ((hdfile->flags & QIO_FFLAG_CANCEL))
    {
        ioq->iostatus = QIO_CANCELLED;
        ok = 0;
    }
    RDSQK((OUTWHERE "fsys_qread: iostatus=%08lX, ok=%d, sect=%ld, u_buff=%08lX, u_len=%ld, state=%d\n",
           ioq->iostatus, ok, arg->sector, (U32)arg->buff, arg->u_len, arg->state));
    while (ok)
    {
        switch (arg->state)
        {
        default:
            ioq->iostatus = FSYS_IO_FATAL;  /* fatal internal error */
            break;

        case 0: {
                ioq->complete = fsys_qread;
                arg->total = 0;         /* no bytes xferred */
                if (!arg->u_len) break;     /* a 0 length read simply exits */
                arg->state = 1;
#if 0
                {
                    QioFile *fpq;
                    set_flags(hdfile, ~(HDIO_RW_MODE_SEL|HDIO_RW_MODE_PIO));
                    if ((fpq=arg->fsys_fp))
                    {
                        set_flags(hdfile, fpq->flags&(HDIO_RW_MODE_SEL|HDIO_RW_MODE_PIO));
                    }
                }
#endif
                continue;
            }
        case 1: {
                FsysLtop ltop;
                FsysVolume *v;
                U32 offset;

                int len, sts;
                ltop = fsys_ltop(arg->ramrp, arg->sector, (arg->u_len+(BYTPSECT-1))/BYTPSECT);
                if (!ltop.phys)
                {
                    if (!ioq->iocount) ioq->iostatus = QIO_EOF;
                    break;
                }
                arg->state = 2;
                arg->count = ltop.cnt;
                len = arg->count*BYTPSECT;
                if (len > arg->u_len) len = arg->u_len;
                if ((len&(BYTPSECT-1)) && (len&-BYTPSECT)) len = (len&-BYTPSECT);   /* get multiple of sectors first */
                if (arg->callers_ioq)
                {
                    ioq->timeout = arg->callers_ioq->timeout;
                }
#if FSYS_DEFAULT_TIMEOUT
                if (!ioq->timeout) ioq->timeout = FSYS_DEFAULT_TIMEOUT + len;
#endif
                v = arg->vol;
                offset = v ? v->hd_offset : 0;
                sts = qio_readwpos(ioq, ltop.phys + offset, arg->buff, len);
                if (sts) break;
                return;
            }

        case 2: {
                int t;
                if (QIO_ERR_CODE(ioq->iostatus) && arg->fsys_fp && (arg->fsys_fp->mode&O_RPTNOERR))
                {
                    if (ioq->iostatus != HDIO_FATAL && ioq->iostatus != QIO_EOF)
                    {
                        ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;    /* fake success */
                        if (!ioq->iocount) ioq->iocount += BYTPSECT;    /* force some movement */
                    }
                }
                if (!QIO_ERR_CODE(ioq->iostatus))
                {
                    t = ioq->iocount;       
                    arg->total += t;
                    arg->u_len -= t;
                    arg->buff += t;         /* bump buffer by byte count */
                    t = (t+(BYTPSECT-1))/BYTPSECT;          /* round up to sector size */
                    arg->sector += t;           /* advance logical sector number */
                    if (arg->u_len <= 0) break;
                    arg->state = 1;
                    continue;
                }
                break;
            }
        }
        break;
    }
    arg->state = 0;             /* clean up after ourselves */
    ioq->iocount = arg->total;
    ioq->complete = arg->complt;        /* restore completion rtn address */
#if 0
    if (hdfile)
    {
        set_flags(hdfile, ~(HDIO_RW_MODE_SEL|HDIO_RW_MODE_PIO));
        qio_freemutex(&hdfile->mutex, ioq);
    }
#endif
    qio_complete(ioq);              /* call completion */
    return;
}

#if !FSYS_READ_ONLY
static int wr_ext_file(FsysQio *arg, QioFile *fp, FsysVolume *v)
{
    int ii, jj, def;
	uint32_t fid;
    QioIOQ *ioq;
    FsysRamFH *rfh;
    FsysRamRP *rp;

    ioq = (QioIOQ *)arg;
    if (v && fp && fp->dvc)
    {
        if ((v->flags&FSYS_VOL_FLG_RO))   /* if volume as been marked read-only */
        {
            FSYS_SQK((OUTWHERE "wr_ext_file: error 2 extending. Volume marked read-only. Flags=%08X\n",
                      v->flags));
            return 2;
        }
        fid = qio_cvtFromPtr(fp->private);
        rfh = fsys_find_ramfh(v, (fid&FSYS_DIR_FIDMASK));
        if (rfh)
        {
            def = arg->sector - rfh->clusters;
            if (def < 0) def = 0;
            def += rfh->def_extend ? rfh->def_extend : FSYS_DEFAULT_EXTEND;
            if (def*FSYS_MAX_ALTS >= v->total_free_clusters)  /* maybe there's no room */
            {
                for (ii=jj=0; ii < FSYS_MAX_ALTS; ++ii)   /* compute how many copies */
                {
                    rp = rfh->ramrp + ii;
                    if (rp->num_rptrs) ++jj;
                }
                if (def*jj >= v->total_free_clusters) /* check again for room */
                {
                    ioq->iostatus = FSYS_EXTEND_FULL;   /* no room for extension */
                    FSYS_SQK((OUTWHERE "wr_ext_file: error 1 extending. def=%d, jj=%d, total_free=%ld\n",
                              def, jj, v->total_free_clusters));
                    return 1;
                }
            }
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
            {
                rp = rfh->ramrp + jj;
                if (!rp->num_rptrs) continue;       /* no copy here */
                ii = extend_file(v, def, rp, jj);
                if (ii)
                {
                    ioq->iostatus = ii;
                    break;              /* ran out of room */
                }
            }
            rfh->clusters += def;           /* add to total allocation */
            if (QIO_ERR_CODE(ioq->iostatus))
            {
#if FSYS_SQUAWKING
                char emsg[132];
                qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
                FSYS_SQK((OUTWHERE "wr_ext_file: error 1 extending. def=%d, jj=%d, total_free=%d. sts:\n%s\n",
                          def, jj, v->total_free_clusters, emsg));
#endif
                return 1;
            }
            return 0;               /* recompute retrieval pointer set */
        }
    }
    FSYS_SQK((OUTWHERE "wr_ext_file: error 2 extending. v=%08lX, fp=%08lX, fp->dvc=%08lX\n",
              (U32)v, (U32)fp, fp ? (U32)fp->dvc : 0));
    return 2;
}
#endif

/************************************************************
 * fsys_qwrite - a file write primitive. This function will write
 * n sectors to a file as specified by the list of retrieval
 * pointers, the starting relative sector number and the sector
 * count. It queue's a write to the I/O subsystem and uses itself
 * as a completion function. It will continue to queue output until
 * the required bytes have been transferred or an error occurs.
 * The I/O subsystem will call this function as a completion
 * routine therefore it will (may) finish asychronously from the
 * calling program. This function is used internally by the
 * filesystem and not expected to be used by the casual user.
 * 
 * At entry:
 *	arg - pointer to write argument list. The list must be in
 *		static memory (i.e., not on the stack)
 * At exit:
 *	returns nothing. The function queue's an input which will
 *	complete asynchronously from the calling program.
 */

static void fsys_qwrite(QioIOQ *ioq)
{
    FsysQio *arg;
    QioFile *file;
    int ok=1;

    arg = (FsysQio *)ioq;
    file = qio_fd2file(ioq->file);
    if (!file)
    {
        ioq->iostatus = QIO_NOTOPEN;
        ok = 0;
    }
    else if ((file->flags & QIO_FFLAG_CANCEL))
    {
        ioq->iostatus = QIO_CANCELLED;
        ok = 0;
    }
    else if (arg->bws)
    {
        ioq->iostatus = FSYS_NOTSECTALGN;
        ok = 0;
    }
    WRSQK((OUTWHERE "fsys_qwrite: sect=%ld, u_buff=%08lX, u_len=%ld, state=%d, cancel=%s\n",
           arg->sector, (U32)arg->buff, arg->u_len, arg->state, file ? ((file->flags&QIO_FFLAG_CANCEL) ? "Yes" : "No") : "Closed"));
    while (ok)
    {
        switch (arg->state)
        {
        default:
            ioq->iostatus = FSYS_IO_FATAL;  /* fatal internal error */
            break;

        case 0:
            ioq->complete = fsys_qwrite;
            if (!arg->u_len) break;     /* a 0 length write simply exits */
            arg->state = 1;
            continue;

        case 1: {
                FsysLtop ltop;
                U32 len, offset;
                FsysVolume *v;

                v = arg->vol;
                ltop = fsys_ltop(arg->ramrp, arg->sector, (arg->u_len+(BYTPSECT-1))/BYTPSECT);
                if (!ltop.cnt)
                {
#if !FSYS_READ_ONLY
                    if (!(v->flags&FSYS_VOL_FLG_RO))
                    {
                        QioFile *fp;
                        int sts;

                        fp = arg->fsys_fp;
                        if (fp && fp->dvc && v)
                        {
                            arg->state = 3;
                            sts = qio_getmutex(&v->mutex, fsys_qwrite, ioq);
                            if (!sts) return;
                            if (sts == QIO_MUTEX_NESTED)
                            {
                                WRSQK(( OUTWHERE "%6ld: fsys_qwrite, extending. Volumue already locked\n",
                                        eer_rtc));
                                arg->state = 1;
                                sts = wr_ext_file(arg, fp, v);
                                if (sts == 0) continue; /* recompute lsn */
                                if (sts == 1) break;
                            }
                            else
                            {
                                ioq->iostatus = sts;
                                break;
                            }
                        }
                    }
#endif
                    ioq->iostatus = QIO_EOF; /* always return an EOF if tried to write too much */
                    break;
                }
                arg->state = 2;
                arg->count = ltop.cnt;
                len = arg->count*BYTPSECT;
                if (len > arg->u_len) len = arg->u_len;
                if ((len&(BYTPSECT-1)) && (len&-BYTPSECT)) len = (len&-BYTPSECT);
                WRSQK((OUTWHERE "Queing write. LBA=%ld, buff=%08lX, len=%ld\n", ltop.phys, (U32)arg->buff, len));
                if (arg->callers_ioq)
                {
                    ioq->timeout = arg->callers_ioq->timeout;
                }
#if FSYS_DEFAULT_TIMEOUT
                if (!ioq->timeout) ioq->timeout = FSYS_DEFAULT_TIMEOUT + len;
#endif
                offset = v ? v->hd_offset : 0;
                if (qio_writewpos(ioq, ltop.phys + offset, arg->buff, len)) break;
                return;
            }

        case 2: {
                int t;
                if (QIO_ERR_CODE(ioq->iostatus) && arg->fsys_fp && (arg->fsys_fp->mode&O_RPTNOERR))
                {
                    if (ioq->iostatus != HDIO_FATAL && ioq->iostatus != QIO_EOF)
                    {
                        ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;    /* fake success */
                        if (!ioq->iocount) ioq->iocount += BYTPSECT;    /* force some movement */
                    }
                }
                if (!QIO_ERR_CODE(ioq->iostatus))
                {
                    t = ioq->iocount;       
                    arg->total += t;
                    if ( t >= arg->u_len )
                    {
                        arg->u_len = 0;
                    }
                    else
                    {
                        arg->u_len -= t;
                    }
                    arg->buff += t;         /* advance buffer pointer by 'word' count */
                    arg->sector += (t+(BYTPSECT-1))/BYTPSECT;   /* advance logical sector number */
                    if (arg->u_len <= 0) break;
                    arg->state = 1;
                    continue;
                }
                break;
            }

#if !FSYS_READ_ONLY
        case 3: {
                int sts;
                FsysVolume *v;
                QioFile *fp;

                fp = arg->fsys_fp;
                v = arg->vol;
                if (!(v->flags&FSYS_VOL_FLG_RO))
                {
                    arg->state = 1;             
                    sts = wr_ext_file(arg, fp, v);
                    qio_freemutex(&v->mutex, ioq);      /* free the volume */
                    if (sts == 0) continue;         /* recompute lsn */
                    if (sts == 1) break;            /* die */
                }
                ioq->iostatus = QIO_EOF; /* always return an EOF if tried to write too much */
                break;
            }
#endif
        }
        break;
    }
#if FSYS_WR_SQUAWKING
    {
        char emsg[132];
        qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
        WRSQK((OUTWHERE "fsys_qwrite: done. buff=%08lX, tot=%ld, u_len=%ld, status:\n\t%s\n",
               (U32)arg->buff, arg->total, arg->u_len, emsg));
    }
#endif
    arg->state = 0;     /* clean up after ourselves */
    ioq->complete = arg->complt;
    qio_complete(ioq);
    return;
}

#if MALLOC_DEBUG
    #define ALLOC_EXTRAS ,const char *file, int lineno
    #define QM_ALLOC_CALLOC(x,y) guts_calloc_r( (void *)qio_reent, x, y, file, lineno)
    #define QM_ALLOC_REALLOC(x,y) guts_realloc_r( (void *)qio_reent, x, y, file, lineno)
    #define QM_ALLOC_FREE(x) guts_free_r((void *)qio_reent, x, file, lineno)
#else
    #define ALLOC_EXTRAS 
    #define QM_ALLOC_CALLOC(x,y) QIOcalloc(x,y)
    #define QM_ALLOC_REALLOC(x,y) QIOrealloc(x,y)
    #define QM_ALLOC_FREE(x) QIOfree(x)
#endif

#if FSYS_UMOUNT
static void qmount_free(FsysVolume *vol, void *old ALLOC_EXTRAS )
{
    int ii;
    for (ii=0; ii < vol->freem_indx; ++ii)
    {
        if (vol->freemem[ii] == old)
        {
            QM_ALLOC_FREE(old);
            vol->freemem[ii] = 0;
            break;
        }
    }
    return;
}

static void *qmount_alloc(FsysVolume *vol, int amt ALLOC_EXTRAS )
{
    void *ans, *nmem;
    ans = QM_ALLOC_CALLOC(amt, 1);
    if (!ans)
    {
        return ans;
    }
    if (vol->freem_indx >= vol->freem_elems)
    {
        vol->freem_elems += 10;
        nmem = QIOrealloc(vol->freemem, vol->freem_elems*sizeof(void *));
        if (!nmem)
        {
            QIOfree(ans);
            return 0;
        }
        vol->freemem = nmem;
    }
    vol->freemem[vol->freem_indx++] = ans;
    return ans;
}

static void *qmount_realloc(FsysVolume *vol, void *old, int amt ALLOC_EXTRAS )
{
    void *ans;
    int ii;
    ans = QM_ALLOC_REALLOC(old, amt);
    for (ii=0; ii < vol->freem_indx; ++ii)
    {
        if (vol->freemem[ii] == old)
        {
            vol->freemem[ii] = ans;
            break;
        }
    }
    if (ans && ii >= vol->freem_indx)
    {
        if (vol->freem_indx >= vol->freem_elems)
        {
            vol->freem_elems += 20;
            vol->freemem = QIOrealloc(vol->freemem, vol->freem_elems*sizeof(void *));
            if (!vol->freemem)
            {
                QIOfree(ans);
                return 0;
            }
        }
        vol->freemem[vol->freem_indx++] = ans;
    }
    return ans;
}
#endif

#if FSYS_UMOUNT
static void qmount_freeall(FsysVolume *vol)
{
    int ii;
    for (ii=0; ii < vol->freem_indx; ++ii)
    {
        if (vol->freemem[ii])
        {
            QIOfree(vol->freemem[ii]);
            vol->freemem[ii] = 0;
        }
    }
    if (vol->freemem)
    {
        QIOfree(vol->freemem);
        vol->freemem = 0;
    }
    vol->freem_indx = vol->freem_elems = 0;
    return;
}
#endif

#if FSYS_DIR_HASH_SIZE > 1
static int hashit(const char *string)
{
    int hashv=0;
    unsigned char c;
    while ((c= *string++))
    {
        hashv = (hashv<<3) + (hashv<<1) + hashv + c;  /* hashv = hashv*11 + c */
    }
    return(hashv %= FSYS_DIR_HASH_SIZE) >= 0 ? hashv : -hashv;
}
#endif

static void insert_ent(FsysDirEnt **hash, FsysDirEnt *new) /* insert into hash table */
{
    FsysDirEnt *cur, **prev;
#if FSYS_DIR_HASH_SIZE > 1
    int hashv;
    hashv = hashit(new->name);      /* compute a hash value */
    prev = hash + hashv;        /* record insertion point */
#else
    prev = hash;
#endif
    while ((cur = *prev))     /* walk the chain */
    {
        if (strcmp(cur->name, new->name) > 0) break;
        prev = &cur->next;
    }
    *prev = new;            /* old guy points to new one */
    new->next = cur;            /* new one points to next guy */
    return;             /* and that is all there is to it */
}

static FsysDirEnt *find_ent(FsysDirEnt **hash, const char *name) /* remove from hash table */
{
    FsysDirEnt *cur, **prev;
#if FSYS_DIR_HASH_SIZE > 1
    int hashv;
    hashv = hashit(name);       /* compute a hash value */
    prev = hash + hashv;        /* record insertion point */
#else
    prev = hash;
#endif
    while ((cur = *prev))     /* walk the chain */
    {
        if (strcmp(cur->name, name) == 0) return cur;   /* found it, give it to 'em */
        prev = &cur->next;
    }
    return 0;               /* not in the list */
}

#if !FSYS_READ_ONLY
static FsysDirEnt *remove_ent(FsysDirEnt **hash, const char *name) /* remove from hash table */
{
    FsysDirEnt *cur, **prev;
#if FSYS_DIR_HASH_SIZE > 1
    int hashv;
    hashv = hashit(name);       /* compute a hash value */
    prev = hash + hashv;        /* record insertion point */
#else
    prev = hash;
#endif
    while ((cur = *prev))     /* walk the chain */
    {
        if (strcmp(cur->name, name) == 0)
        {
            *prev = cur->next;      /* pluck this from the list */
            cur->next = 0;      /* break the link */
            return cur;
        }
        prev = &cur->next;
    }
    return 0;               /* not in the list */
}
#endif

static int lookup_filename(FsysLookUpFileT *lu)
{
    char name[256];             /* room for temp copy of path */
    const char *fname, *path;
    FsysDirEnt *dir;
    FsysRamFH *top;
    U32 fid;
    int len;

    if (!lu || !lu->vol || !lu->path)
    {
        return FSYS_LOOKUP_INVARG;      /* it can't be there */
    }
    lu->owner = 0;              /* assume no owner directory */
    lu->file = 0;               /* assume no file either */
    top = lu->top ? lu->top : fsys_find_ramfh(lu->vol, FSYS_INDEX_ROOT);
    fname = lu->path;
    if (*fname == QIO_FNAME_SEPARATOR) ++fname; /* eat leading '/' */
    lu->depth = 0;
    while ((path=strchr(fname, QIO_FNAME_SEPARATOR))) /* see if we have to traverse a directory */
    {
        if (!top->directory)
        {
            return FSYS_LOOKUP_NOPATH;      /* not a directory, so file not found */
        }
        len = path - fname;
        strncpy(name, fname, len);
        name[len] = 0;
        dir = find_ent(top->directory, name);   /* find the directory entry */
        if (!dir || !(fid=(dir->gen_fid&FSYS_DIR_FIDMASK)))
        {
            return FSYS_LOOKUP_NOPATH;      /* directory not found */
        }
        top = fsys_find_ramfh(lu->vol, fid);    /* get pointer to new top */
        if (top->generation != (dir->gen_fid>>FSYS_DIR_GENSHF))
        {
            return FSYS_LOOKUP_NOPATH;
        }
        fname = path + 1;           /* skip to next filename part */
        if (++lu->depth > 31)
        {
            return FSYS_LOOKUP_TOODEEP;
        }
    }
    if (!top->directory)
    {
        return FSYS_LOOKUP_NOPATH;      /* not looking in a directory */
    }
    lu->owner = top;                /* pass back pointer to found directory */
    lu->fname = fname;              /* record name of file we looked for */
    dir = find_ent(top->directory, fname);  /* look up the filename */
    lu->dir = dir;              /* record pointer to directory entry */
    if (dir && (fid=(dir->gen_fid&FSYS_DIR_FIDMASK)))
    {
        lu->file = fsys_find_ramfh(lu->vol, fid);   /* pass back the file */ 
        if (lu->file->generation == (dir->gen_fid>>FSYS_DIR_GENSHF))
        {
            return FSYS_LOOKUP_SUCC|SEVERITY_INFO; /* it worked */
        }
    }
    return FSYS_LOOKUP_FNF;         /* file not found */
}

#ifndef FSYS_FIX_FREELIST
    #define SPC_STATIC static
#else
    #define SPC_STATIC
#endif

#if !FSYS_READ_ONLY
static int add_to_unused(FsysVolume *vol, int fid)
{
    int ii;
    U32 *ulp;

    ulp = vol->unused;
    for (ii=0; ii < vol->unused_ffree; ++ii)
    {
        if (fid == *ulp++) return 0;        /* already in the unused list */
    }
    if (vol->unused_ffree >= vol->unused_elems)
    {
#ifndef FSYS_UNUSED_INCREMENT
#define FSYS_UNUSED_INCREMENT (128)
#endif
        vol->unused_elems += FSYS_UNUSED_INCREMENT; /* room for n updates */
        ulp = (U32*)QMOUNT_REALLOC(vol, vol->unused, vol->unused_elems*sizeof(uint32_t));
        if (!ulp)
        {
            return FSYS_CREATE_NOMEM;
        }
        vol->unused = ulp;
    }
    vol->unused[vol->unused_ffree++] = fid;
    FHSQK((OUTWHERE "Added FID %d to unused list\n", fid));
    return 0;
}
#endif

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static int fsys_sync(FsysSyncT *f, int how);
static int fatal_dirty_error;

SPC_STATIC int add_to_dirty(FsysVolume *vol, U32 fid, int end)
{
    int ii;
    uint32_t *ulp;

    FSYS_SQK(( OUTWHERE "%6ld: Adding fid %08lX to dirty list\n", eer_rtc, fid));
    if (fid >= vol->files_ffree || (vol->flags&FSYS_VOL_FLG_RO))
    {
        FSYS_SQK(( OUTWHERE "%6ld: add_to_dirty Rejected fid %08lX. files_ffree=%d, flags=%08X\n",
                   eer_rtc, fid, vol->files_ffree, vol->flags));
        ++fatal_dirty_error;
        return 0;
    }
    ulp = vol->dirty;
    for (ii=0; ii < vol->dirty_ffree; ++ii)
    {
        if (fid == *ulp++)
        {
            FSYS_SQK(( OUTWHERE "%6ld: add_to_dirty rejected fid %08lX cuz it's already there\n",
                       eer_rtc, fid));
            if (end && ii < vol->dirty_ffree-1)
            {
                memcpy(ulp-1, ulp, (vol->dirty_ffree-ii-1)*sizeof(uint32_t)); /* scoot everybody down one */
                vol->dirty[vol->dirty_ffree-1] = fid;   /* put this one at the end */
            }
            return 0;       /* already in the dirty list or already at the end */
        }
    }
    if (vol->dirty_ffree >= vol->dirty_elems)
    {
        int new;
        new = vol->dirty_elems + 32;
        ulp = (U32*)QMOUNT_REALLOC(vol, vol->dirty, new*sizeof(int32_t));
        if (!ulp)
        {
            return FSYS_CREATE_NOMEM;
        }
        vol->dirty = ulp;
        memset((char *)(vol->dirty+vol->dirty_elems), 0, 32*sizeof(int32_t));
        vol->dirty_elems = new;         /* room for 32 updates */
        FSYS_SQK(( OUTWHERE "%6ld: Increased size of dirty area\n", eer_rtc));
    }
    FSYS_SQK(( OUTWHERE "%6ld: fid %08lx at dirty index %d\n", eer_rtc, fid, vol->dirty_ffree));
    vol->dirty[vol->dirty_ffree++] = fid;
#if !FSYS_NO_AUTOSYNC
    if (vol->dirty_ffree >= 16)       /* Getting full, don't wait for timeout */
    {
        FSYS_SQK(( OUTWHERE "%6ld: kick started sync task\n", eer_rtc));
        fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER); /* startup a sync task */
    }
#endif
    return 0;
}
#endif

#if !FSYS_READ_ONLY
static int mkroom_free(FsysVolume *vol)
{
    FsysRamFH *rfh;
    int ii, sts;
    U32 size;
    FsysRetPtr *ulp;

    if (vol->free_ffree < vol->free_elems) return 0;    /* there's room */
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
    FSYS_SQK((OUTWHERE "%6ld: Increasing size of free list. free_ffree=%d, free_elems = %d\n",
              eer_rtc, vol->free_ffree, vol->free_elems));
#endif
    rfh = fsys_find_ramfh(vol, FSYS_INDEX_FREE); /* point to freelist file */
    vol->free_elems += BYTPSECT/sizeof(FsysRetPtr); /* add another sector's worth of elements */
    size = (vol->free_elems*sizeof(FsysRetPtr)+(BYTPSECT-1))&-BYTPSECT; /* multiple of sector size */
    ulp = (FsysRetPtr *)QMOUNT_REALLOC(vol, vol->free, size);
    if (!ulp) return FSYS_EXTEND_NOMEM;     /* ran out of memory */
    vol->free = ulp;
    size /= BYTPSECT;               /* get size in sectors */
    if (size > rfh->clusters)         /* need to add some more sectors */
    {
        int def;
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
        FSYS_SQK(( OUTWHERE "%6ld: Increasing size of free list file. size=%ld, clust=%ld\n",
                   eer_rtc, size, rfh->clusters));
#endif
        def = rfh->def_extend ? rfh->def_extend : FSYS_DEFAULT_DIR_EXTEND;  /* add more sectors */    
        rfh->clusters += def;
        for (ii=0; ii < FSYS_MAX_ALTS; ++ii)  /* need to extend the freelist file */
        {
            sts = extend_file(vol, def, rfh->ramrp + ii, ii);
            if (sts) return sts;            /* couldn't extend file */
        }
    }
    add_to_dirty(vol, FSYS_INDEX_FREE, 1);  /* make sure the freelist gets updated */
    return 0;
}

static int upd_index_bits(FsysVolume *vol, U32 fid)
{
    U32 lw, le, bits;
    U32 *ulp;

    bits = ((vol->files_ffree+1)*FSYS_MAX_ALTS*sizeof(int32_t)+(BYTPSECT-1))/BYTPSECT; /* number of sectors required to hold index file */
    bits = (bits+31)/32;            /* number of int32_t rqd to hold bit map */
    FSYS_SQK(( OUTWHERE "%6ld: upd_index_bits: fid=%08lX, Bits=%ld, elems=%d\n",
               eer_rtc, fid, bits, vol->index_bits_elems));
    if (bits >= vol->index_bits_elems)
    {
        int newsize, len;
        newsize = vol->index_bits_elems + bits + 8;
        FSYS_SQK(( OUTWHERE "%6ld: upd_index_bits: Increasing size. newsize=%d\n", eer_rtc, newsize));
        ulp = (U32*)QMOUNT_REALLOC(vol, vol->index_bits, newsize*sizeof(int32_t));
        if (!ulp)
        {
            return FSYS_FREE_NOMEM;
        }
        len = newsize - vol->index_bits_elems; /* size of additional area */
        memset((char *)(ulp + vol->index_bits_elems), 0, len*sizeof(U32));
        vol->index_bits = ulp;
        vol->index_bits_elems = newsize;
    }
    lw = fid*FSYS_MAX_ALTS*sizeof(int32_t); /* byte position in index file of first byte */
    le = lw + FSYS_MAX_ALTS*sizeof(int32_t)-1; /* byte position of last byte in array */
    lw /= BYTPSECT;             /* sector position in index file of first byte */
    fid = lw&31;            /* bit position in bitmap element of first byte */
    lw /= 32;               /* longword element in bitmap of first byte */
    FSYS_SQK(( OUTWHERE "%6ld: upd_index_bits: lw=%ld, new=%08lX, old=%08lX\n",
               eer_rtc, lw, ((U32)1)<<fid, vol->index_bits[lw]));
    vol->index_bits[lw] |= 1<<fid;  /* set bit in bitmap */
    le /= BYTPSECT;             /* sector position in index file of last byte */
    fid = le&31;            /* bit position in bitmap element of last byte */
    le /= 32;               /* longword element in bitmap of last byte */
    vol->index_bits[le] |= 1<<fid;  /* set bit in bitmap of last byte */
    return 0;
}

    #if FSYS_DMP_FREE && (FSYS_FREE_SQUAWKING || FSYS_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING /* || FSYS_JOU_SQUAWKING */)
        #if !FSYS_SYNC_FREE_SQUAWKING
            #define DMPFREE FSYS_SQK
        #else
            #define DMPFREE FSYS_FREE_SQK
        #endif
static void dump_freelist(FsysVolume *vol, const char *title)
{
    int ii;
    FsysRetPtr *rp;
    rp = vol->free;
    DMPFREE((OUTWHERE "%6ld: %s: %d entries\n", eer_rtc, title, vol->free_ffree));
    for (ii=0; ii < vol->free_ffree; ++ii, ++rp)
    {
        DMPFREE((OUTWHERE "        %3d: start=%7ld, nblocks=%6ld\n",
                 ii, rp->start, rp->nblocks));
    }
}
        #define DUMP_FREELIST(x,y) dump_freelist(x,y)
    #else
        #define DUMP_FREELIST(x,y) do { ; } while (0)
    #endif

static int collapse_free(FsysVolume *vol, int low, int sqk)   /* squeeze all the empty space out of free list */
{
    FsysRetPtr *dst, *src, *lim;
#if FSYS_SQUAWKING
    int old_start = vol->free_start, old_ffree=vol->free_ffree;
#endif

#if FSYS_FREE_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING
    if (sqk)
    {
        DUMP_FREELIST(vol, "collapse_free before.");
    }
#endif
    dst = vol->free;
    src = dst+1;            /* point to free list */
    lim = dst + vol->free_ffree;    /* point to end of list */
    while (src < lim)         /* walk the whole list */
    {
        uint32_t s, ss, ds, de;
        s = src->start;
        ss = src->nblocks;
        ds = dst->start;
        de = ds + dst->nblocks;
        if (ss)           /* the source has to have a size */
        {
            if (!de)          /* if the destination has no size */
            {
                *dst = *src;        /* just copy the source to it */
            }
            else
            {
                if (s <= de)      /* if the regions touch or overlap */
                {
                    dst->nblocks += ss - (de - s);
                    src->start = src->nblocks = 0;
                    if (dst - vol->free < low) low = dst - vol->free;
                }
                else
                {
                    ++dst;          /* advance destination */
                    if (dst != src) *dst = *src; /* copy source if in different locations */
                }
            }
        }
        ++src;              /* advance the source */
    }
    ++dst;
    vol->free_ffree = dst - vol->free; /* compute new length */
    if (dst - vol->free < vol->free_elems)
    {
        dst->start = dst->nblocks = 0;  /* follow with a null entry if there's room */
    }
    if (low < vol->free_start) vol->free_start = low;
#if FSYS_SQUAWKING
    if (old_start != vol->free_start || old_ffree != vol->free_ffree)
    {
        FSYS_SQK(( OUTWHERE "%6ld: collapse_free entry: start=%d, ffree=%d; exit: start=%d, ffree=%d\n",
                   eer_rtc, old_start, old_ffree, vol->free_start, vol->free_ffree));
    }
    else
    {
        FSYS_SQK(( OUTWHERE "%6ld: collapse_free, start=%d, ffree=%d. No changes.\n",
                   eer_rtc, vol->free_start, vol->free_ffree));
    }
#endif
#if FSYS_FREE_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING
    if (sqk)
    {
        DUMP_FREELIST(vol, "collapse_free after.");
    }
#endif
    return 0;
}

SPC_STATIC int back_to_free(FsysVolume *vol, FsysRetPtr *rp)
{
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
#error You need to re-write back_to_free to use the skip repeat feature
#else
    int ii, sts, amt, reuse;
    int old_start, old_end, new_start, new_end;
    FsysRetPtr *vrp;

    vrp = vol->free;
    new_start = rp->start;
    new_end = rp->start + rp->nblocks;
#if FSYS_FREE_SQUAWKING
    {
        int ii;
        FsysRetPtr *dst;
        dst = vol->free;
        FSYS_SQK((OUTWHERE "%6ld: back_to_free before. %d items. Adding start=%d, nblocks=%ld\n",
                  eer_rtc, vol->free_ffree, new_start, rp->nblocks));
        for (ii=0; ii < vol->free_ffree; ++ii, ++dst)
        {
            FSYS_SQK((OUTWHERE "\t   %3d: start=%7ld, nblocks=%7ld\n",
                      ii, dst->start, dst->nblocks));
        }
    }
#endif
    for (amt=ii=0; ii < vol->free_ffree; ++ii, ++vrp)
    {
        old_start = vrp->start;
        old_end = vrp->start + vrp->nblocks;
        if (new_start > old_end) continue;  /* insertions are maintained in a sorted list */
        if (new_end < old_start) break;     /* need to insert a new entry */
        sts = ((new_start == old_end)<<1) | (new_end == old_start);
        if (sts)
        {
            amt = rp->nblocks;
            vrp->nblocks += amt;
            if ((sts&1)) vrp->start = new_start;
            vol->total_free_clusters += amt;
            vol->total_alloc_clusters -= amt;
            return collapse_free(vol, ii, FSYS_FREE_SQUAWKING);
        }
        if (new_start >= old_start)   /* overlaps are actually errors, but ... */
        {
            if (new_end >= old_end)   /* we ignore them as errors, and handle them */
            {
                amt = new_end - old_end;
                vrp->nblocks += amt;    /* just move end pointer in free list */
                vol->total_free_clusters += amt;
                vol->total_alloc_clusters -= amt;
            }
            return collapse_free(vol, ii, FSYS_FREE_SQUAWKING);
        }
        amt = old_start - new_start;
        if (new_end > old_end) amt += new_end - old_end;
        vrp->nblocks += amt;        /* move the end pointer */
        vol->total_free_clusters += amt;
        vol->total_alloc_clusters -= amt;
        vrp->start = new_start;     /* move the start pointer */
        return collapse_free(vol, ii, FSYS_FREE_SQUAWKING);
    }
    reuse = 0;
    if (vrp > vol->free)
    {
        --vrp;              /* look back one slot */
        if (!vrp->nblocks)        /* is it empty? */
        {
            while (vrp > vol->free)   /* yes, then keep looking back */
            {
                --vrp;
                if (vrp->nblocks) break;    /* until a non-empty one is found */
            }
            if (vrp->nblocks) ++vrp;    /* if we stopped because of a non-empty, advance over it */
            reuse = 1;
        }
        else
        {
            ++vrp;          /* nevermind */
        }
    }
    if (!reuse)
    {
        ++vol->free_ffree;      /* advance pointer */
        sts = mkroom_free(vol);     /* make sure there is room for insertion */
        if (sts) return sts;        /* there isn't, can't update */
        vrp = vol->free + ii;       /* vol->free might have moved */
        if (ii < vol->free_ffree-1)   /* if in the middle */
        {
            memmove((char *)(vrp+1), (char *)vrp, (vol->free_ffree-1-ii)*sizeof(FsysRetPtr));
        }
        if (vol->free_ffree < vol->free_elems)
        {
            FsysRetPtr *ep;
            ep = vol->free + vol->free_ffree;
            ep->start = ep->nblocks = 0; /* follow with a null if there's room */
        }
    }
    vrp->start = rp->start;
    vrp->nblocks = rp->nblocks;
    vol->total_free_clusters += rp->nblocks;
    vol->total_alloc_clusters -= rp->nblocks;
    return collapse_free(vol, vrp - vol->free, FSYS_FREE_SQUAWKING);
#endif	
}

static int free_rps(FsysVolume *vol, FsysRamRP *rps)
{
    FsysRamRP *cur, *nxt, *prev;
    FsysRetPtr *retp;
    int ii, jj, sts;

#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
#error You need to rewrite free_rps to get skip/repeat feature
#endif
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        nxt = cur = rps + ii;
        while (nxt)
        {
            retp = nxt->rptrs;
            for (jj=0; retp && jj < nxt->num_rptrs; ++jj, ++retp)
            {
                sts = back_to_free(vol, retp);
                if (sts) return sts;
            }
            nxt = nxt->next;
        }
        while (1)
        {
            nxt = cur->next;
            if (!nxt) break;
            prev = cur;
            while (nxt->next)
            {
                prev = nxt;
                nxt = nxt->next;
            }
            QIOfree(nxt);               /* done with this */
            prev->next = 0;             /* break the link */
        }
        if (cur->rptrs && cur->mallocd) QIOfree(cur->rptrs);
        cur->rptrs = 0;
        cur->mallocd = 0;
        cur->num_rptrs = 0;
        cur->rptrs_size = 0;
    }
    return 0;
}

    #if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
static FsysFilesLink *rps_to_limbo(FsysVolume *vol, FsysRamRP *rps, int xtra)
{
    FsysRamRP *cur, *nxt, *prev;
    FsysRetPtr *retp;
    FsysFilesLink *fl;
    int ii, sts;

#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
#error You need to rewrite free_to_limbo to get skip/repeat feature
#endif
    sts = 0;
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        nxt = rps + ii;
        while (nxt)
        {
            sts += nxt->num_rptrs;
            nxt = nxt->next;
        }
    }
    fl = (FsysFilesLink *)QIOmalloc(sizeof(FsysFilesLink)+(sts+xtra)*sizeof(FsysRetPtr));
    if (!fl) return 0;          /* sorry, ran out of memory */
    fl->next = 0;
    fl->items = sts;            /* record number of items we have */
    if (sts)              /* if there's something to do */
    {
        retp = (FsysRetPtr *)(fl+1);    /* skip to our RP list */
        for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
        {
            nxt = cur = rps + ii;
            while (nxt)
            {
                memcpy(retp, nxt->rptrs, nxt->num_rptrs*sizeof(FsysRetPtr));
                retp += nxt->num_rptrs;
                nxt = nxt->next;
            }
        }
    }
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        nxt = cur = rps + ii;
        while (1)
        {
            nxt = cur->next;
            if (!nxt) break;
            prev = cur;
            while (nxt->next)
            {
                prev = nxt;
                nxt = nxt->next;
            }
            QIOfree(nxt);               /* done with this */
            prev->next = 0;             /* break the link */
        }
        if (cur->rptrs && cur->mallocd) QIOfree(cur->rptrs);
        cur->rptrs = 0;
        cur->mallocd = 0;
        cur->num_rptrs = 0;
        cur->rptrs_size = 0;
    }
    return fl;
}
    #endif

U32 *fsys_entry_in_index(FsysVolume *vol, uint32_t fid)
{
    FsysIndexLink *ip = vol->indexp;
    int accum=0;
    U32 *ans=0;

    while (ip && fid >= accum+ip->items)
    {
        accum += ip->items;
        ip = ip->next;
    }
    if (ip)
    {
        ans = (U32 *)(ip+1) + (fid-accum)*FSYS_MAX_ALTS;
    }
    return ans;
}

static int get_fh(FsysVolume *vol)
{
    int fid, gen, ii;
    uint32_t *ulp;
    FsysRamFH *rfh;

    fid = 0;                    /* assume failure */
    gen = 1;                    /* assume generation of 1 */

    while (vol->unused_ffree)         /* if something on the unused list */
    {
        --vol->unused_ffree;            /* use it */
        fid = vol->unused[vol->unused_ffree];
        ulp = ENTRY_IN_INDEX(vol, fid);
        if (!(*ulp&FSYS_EMPTYLBA_BIT))    /* error, not really a free fid */
        {
            fid = 0;                /* keep looking */
            continue;
        }
        gen = (*ulp & 0xff)+1;          /* get the new generation number */
        if (gen > 255) gen = 1;         /* wrap it back to 1 */
        break;
    }
    if (!fid)                 /* nothing on the unused list */
    {
        FsysRamFH *nf;
        if (vol->files_ffree >= vol->files_elems) /* grab a new one from the end of the allocated list */
        {
            FsysIndexLink *ilp;
            FsysFilesLink *flp;

/* Set FSYS_INDEX_EXTEND_ITEMS such that it will make a block of index data a multiple of a sector size */
#if FSYS_MAX_ALTS == 1
#define FSYS_INDEX_EXTEND_ITEMS 512		/* (1*512) bytes == 128 LBA's */
#define FSYS_FILES_EXTEND_ITEMS 128
#elif FSYS_MAX_ALTS == 2
#define FSYS_INDEX_EXTEND_ITEMS 512		/* (1*512) bytes == 64 pairs of LBA's */
#define FSYS_FILES_EXTEND_ITEMS 64
#elif FSYS_MAX_ALTS == 3
#define FSYS_INDEX_EXTEND_ITEMS (512*3)		/* (3*512) bytes == 128 triplets of LBA's */
#define FSYS_FILES_EXTEND_ITEMS 128
#else
#error You need to figure out what this value should be set to
#endif
            ilp = vol->indexp;          /* find end of list */
            while (ilp->next)
            {
                ilp = ilp->next;
            }
            ilp->next = (FsysIndexLink *)QMOUNT_ALLOC(vol, sizeof(FsysIndexLink) + FSYS_INDEX_EXTEND_ITEMS);
            if (!ilp->next)
            {
                return -FSYS_CREATE_NOMEM;  /* ran out of room bumping index */
            }
            ilp = ilp->next;
            ilp->items = FSYS_INDEX_EXTEND_ITEMS/(FSYS_MAX_ALTS*sizeof(U32));
            vol->files_elems += ilp->items; /* add more to the total */

            flp = vol->filesp;
            while (flp->next)
            {
                flp = flp->next;
            }
            flp->next = (FsysFilesLink *)QMOUNT_ALLOC(vol, sizeof(FsysFilesLink) + FSYS_FILES_EXTEND_ITEMS*sizeof(FsysRamFH));
            if (!flp->next)
            {
                return -FSYS_CREATE_NOMEM;  /* ran out of room bumping files */
            }
            flp = flp->next;
            flp->items = FSYS_FILES_EXTEND_ITEMS;
            flp->next = 0;
        }
        fid = vol->files_ffree++;
        nf = fsys_find_ramfh(vol, FSYS_INDEX_INDEX);    /* point to index file header */
        nf->size += sizeof(int32_t)*FSYS_MAX_ALTS; /* increment index file size */
        if (nf->size > nf->clusters*BYTPSECT) /* if file has grown out of its britches */
        {
            FsysRamRP *rp;
            if (!nf->def_extend) nf->def_extend = FSYS_DEFAULT_DIR_EXTEND;
            if (nf->def_extend*FSYS_MAX_ALTS >= vol->total_free_clusters)
            {
                return -FSYS_EXTEND_FULL;   /* no room to extend index file */
            }
            for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
            {
                int ans;
                rp = nf->ramrp + ii;
                ans = extend_file(vol, nf->def_extend, rp, ii);
                if (ans) return -ans;       /* some kind of error */
            }
            nf->clusters += nf->def_extend;
        }
        add_to_dirty(vol, FSYS_INDEX_INDEX, 0); /* index file header needs updating */
    }
    else
    {
        FHSQK((OUTWHERE "Found fid %d on unused list\n", fid));
    }
    ulp = ENTRY_IN_INDEX(vol, fid);     /* point to index */
    FHSQK((OUTWHERE "get_fh: Assigned new fid %08lX. ulp=%08lX\n", (U32)fid, (U32)ulp));
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        *ulp++ = FSYS_EMPTYLBA_BIT | gen;   /* make pointers to FH's invalid */
    }
    rfh = fsys_find_ramfh(vol, fid);
    memset((char *)rfh, 0, sizeof(FsysRamFH));  /* zap the entire file header */
    rfh->generation = gen;          /* set the file's generation number */
    return fid;
}

static int blast_fid_from_index(FsysVolume *vol, FsysRamFH *rfh)
{
    uint32_t *ndx;
    int ii, sts;
    U32 fid;

    fid = fsys_find_id(vol, rfh);
    ndx = ENTRY_IN_INDEX(vol, fid);
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    if (vol->journ_rfh)
    {
        FsysFilesLink *fl;
        FsysRetPtr *rp;
        fl = rps_to_limbo(vol, rfh->ramrp, FSYS_MAX_ALTS);
        if (!fl)
        {
            sts = QIO_NOMEM;
        }
        else
        {
            fl->next = vol->limbo;      /* hold all sectors in limbo until 'sync' */
            vol->limbo = fl;
            rp = (FsysRetPtr *)(fl+1) + fl->items; /* point to place to put FH rp's */
            for (ii=0; ii < FSYS_MAX_ALTS; ++ii, ++rp)
            {
                rp->start = *ndx & FSYS_LBA_MASK;
                rp->nblocks = 1;
                *ndx++ = rfh->generation | FSYS_EMPTYLBA_BIT;   /* remember the old generation number */
            }
            fl->items += FSYS_MAX_ALTS;     /* count the FH rp's too */
            memset((char *)rfh, 0, sizeof(FsysRamFH)); /* zap the entire file header */
            sts = 0;
        }
    }
    else
    {
#endif
        sts = free_rps(vol, rfh->ramrp);        /* free all the retrieval pointers */
        if (!sts)
        {
            for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
            {
                FsysRetPtr t;
                t.start = *ndx;
                t.nblocks = 1;
                back_to_free(vol, &t);      /* free the FH */
                *ndx++ = rfh->generation | FSYS_EMPTYLBA_BIT;
            }
            memset((char *)rfh, 0, sizeof(FsysRamFH)); /* zap the entire file header */
        }
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    }
#endif
    if (!sts)
    {
        sts = add_to_unused(vol, fid);      /* mark FH as unused */
        if (!sts)
        {
            sts = upd_index_bits(vol, fid); /* need to update index file */
        }
    }
    return sts ? sts : (FSYS_DELETE_SUCC|SEVERITY_INFO);
}

static int zap_file( FsysLookUpFileT *luf )
{
    FsysVolume *vol;
    int sts;

    vol = luf->vol;
    remove_ent(luf->owner->directory, luf->fname);  /* delete the name from the directory */
    add_to_dirty(vol, fsys_find_id(vol, luf->owner), 0); /* put owner directory on the dirty list */
    sts = blast_fid_from_index(vol, luf->file);
#if 0 && (FSYS_FREE_SQUAWKING || FSYS_SYNC_FREE_SQUAWKING && (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL))
    {
        FsysFilesLink *fl;
        int itms;
        FSYS_FREE_SQK((OUTWHERE "%6ld: zap_file. Zapped %s\n", eer_rtc, luf->fname));
        fl = vol->limbo;
        if (fl)
        {
            FsysRetPtr *rp;
            rp = (FsysRetPtr *)(fl+1);
            for (itms=0; itms < fl->items; ++itms, ++rp)
            {
                FSYS_FREE_SQK((OUTWHERE "\t%2d: %8ld-%08ld (%8ld)\n",
                               itms, rp->start, rp->start+rp->nblocks-1, rp->nblocks));
            }
        }
        else
        {
            FSYS_FREE_SQK((OUTWHERE "\tNo RP's\n"));
        }
    }
#endif
    return sts;
}

static int zap_stub_files(FsysVolume *vol, FsysRamFH *parent, const char *path)
{
    FsysRamFH *rfh;
    FsysDirEnt **hash, **prev, *dir;
    int dire;

    for (dire=0; dire < FSYS_DIR_HASH_SIZE; ++dire)
    {
        hash = parent->directory + dire;
        prev = hash;
        while ((dir = *prev))
        {
            rfh = fsys_find_ramfh(vol, (dir->gen_fid & FSYS_DIR_FIDMASK));
            if (rfh && rfh->valid && rfh->fh_new)
            {
                FSYS_SQK(( OUTWHERE "zap_stub_files: zapped file %08lX:.../?/%s/%s\n",
                           dir->gen_fid, path, dir->name));
                add_to_dirty(vol, fsys_find_id(vol, parent), 0);    /* put owner directory on the dirty list */
                blast_fid_from_index(vol, rfh);
                *prev = dir->next;                  /* pluck ourself from the list */
                continue;                       /* don't advance prev ptr */
            }
            if (rfh->fh_upd)
            {
                FSYS_SQK(( OUTWHERE "zap_stub_files: File not previously closed %08lX:.../?/%s/%s\n",
                           dir->gen_fid, path, dir->name));
                rfh->fh_upd = 0;
            }
            if (rfh->directory && 
                (strcmp(dir->name, ".") != 0 &&         /* don't walk the . and .. directories */
                 strcmp(dir->name, "..") != 0))
            {
                zap_stub_files(vol, rfh, dir->name);            /* walk the new directory */
            }
            prev = &dir->next;
        }
    }
    return 0;   
}

static void delete_q(QioIOQ *ioq)
{
    FsysLookUpFileT luf;
    QioFile *file;

    luf.path = (char *)ioq->pparam0;
    luf.vol = (FsysVolume *)ioq->pparam1;
    luf.top = 0;
    ioq->iostatus = lookup_filename(&luf);
    if (ioq->iostatus == (FSYS_LOOKUP_SUCC|SEVERITY_INFO))
    {
        if (luf.file->directory)
        {
            ioq->iostatus = FSYS_DELETE_DIR;        /* cannot delete a directory this way */
        }
        else
        {
            if (luf.file->not_deleteable)
            {
                ioq->iostatus = FSYS_DELETE_FNF;    /* fake 'em out and say no such file */
            }
            else
            {
                ioq->iostatus = zap_file(&luf);
            }
        }
    }
    file = qio_fd2file(ioq->file);
    file->dvc = 0;
    file->private = 0;
    qio_freefile(file);
    ioq->file = -1;
    qio_freemutex(&luf.vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_delete( QioIOQ *ioq, const char *name )
{
    FsysVolume *vol;
    QioFile *file;

    if ( !name || *name == 0 ) return(ioq->iostatus = FSYS_DELETE_INVARG);
    file = qio_fd2file(ioq->file);      /* get pointer to file */
    vol = (FsysVolume *)file->dvc->private; /* point to our mounted volume */
    if (!vol) return(ioq->iostatus = FSYS_DELETE_FNF);
    if (!vol->filesp) (ioq->iostatus = FSYS_DELETE_FNF);
    ioq->pparam0 = (void *)name;
    ioq->pparam1 = (void *)vol;
    ioq->iostatus = 0;
    return qio_getmutex(&vol->mutex, delete_q, ioq);
}

static void rmdir_q( QioIOQ *ioq )
{
    FsysLookUpFileT luf;
    QioFile *file;

    luf.path = (char *)ioq->pparam0;
    luf.vol = (FsysVolume *)ioq->pparam1;
    luf.top = 0;
    ioq->iostatus = lookup_filename(&luf);
    if (ioq->iostatus == (FSYS_LOOKUP_SUCC|SEVERITY_INFO))
    {
        FsysDirEnt **dir;
        dir = luf.file->directory; 
        if (!dir)
        {
            ioq->iostatus = FSYS_LOOKUP_NOTDIR;
        }
        else
        {
            int ii;
            for (ii=0; ii < FSYS_DIR_HASH_SIZE; ++ii)
            {
                FsysDirEnt *cur;
                cur = dir[ii];
                if (cur)
                {
                    if ( cur->next || 
                         (strcmp(cur->name, "..") != 0 && strcmp(cur->name, ".") != 0) )
                    {
                        break;
                    }
                }
            }
            if (ii < FSYS_DIR_HASH_SIZE)
            {
                ioq->iostatus = FSYS_DELETE_NOTEMPTY;   /* directory not empty */
            }
            else
            {
                luf.file->directory = 0;    /* not a directory anymore */
                QIOfree(dir);           /* release the memory */
                ioq->iostatus = zap_file(&luf); /* delete the file */
            }
        }
    }
    file = qio_fd2file(ioq->file);
    file->dvc = 0;
    file->private = 0;
    qio_freefile(file);
    ioq->file = -1;
    qio_freemutex(&luf.vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_rmdir( QioIOQ *ioq, const char *name )
{
    FsysVolume *vol;
    QioFile *file;

    if ( !name || *name == 0 ) return(ioq->iostatus = FSYS_DELETE_INVARG);
    file = qio_fd2file(ioq->file);      /* get pointer to file */
    vol = (FsysVolume *)file->dvc->private; /* point to our mounted volume */
    if (!vol) return(ioq->iostatus = FSYS_DELETE_FNF);
    if (!vol->filesp) (ioq->iostatus = FSYS_DELETE_FNF);
    ioq->pparam0 = (void *)name;
    ioq->pparam1 = (void *)vol;
    ioq->iostatus = 0;
    return qio_getmutex(&vol->mutex, rmdir_q, ioq);
}

static void fcreate_q(QioIOQ *ioq)
{
    FsysOpenFileT *t;
    QioFile *file;
    FsysVolume *vol;
    FsysOpenT *dtls;
    FsysRamFH *rfh=0;
    FsysLookUpFileT luf;
    FsysDirEnt *dp;
    const char *fname;
    U32 fid;
    int sts, ii, new, sectors, delete_after=0, defext;
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    FsysFilesLink *fl = 0;
#endif

    t = (FsysOpenFileT *)ioq->private;
    if (!t)
    {
        ioq->iostatus = FSYS_CREATE_FATALNOPRIV;
        qio_complete(ioq);      /* this is certain death since the mutex is being held */
        return;
    }
    ioq = t->ioq;
    vol = t->vol;
    dtls = t->details;
    FSYS_SQK((OUTWHERE "fcreate_q(before): %s\n", t->path));
    FSYS_SQK((OUTWHERE "\talloc=%ld, eof=%ld, place=%d, copies=%d\n",
              dtls->alloc, dtls->eof, dtls->placement, dtls->copies));
    FSYS_SQK((OUTWHERE "\tdef_extend=%d, mkdir=%d, ctime=%08lX, mtime=%08lX, btime=%08lX, atime=%08lX\n",
              dtls->def_extend, dtls->mkdir, dtls->ctime, dtls->mtime, dtls->btime, dtls->atime));
    file = qio_fd2file(ioq->file);  /* get pointer to file */
    vol = luf.vol = t->vol;
    luf.top = 0;            /* If we supported 'cwd', this is where it would go */
    luf.path = t->path;

    if (!(defext=dtls->def_extend))
    {
        defext = dtls->mkdir ? FSYS_DEFAULT_DIR_EXTEND : FSYS_DEFAULT_EXTEND;
    }
    if (dtls->alloc)
    {
        sectors = ((dtls->alloc+(BYTPSECT-1))/BYTPSECT);
    }
    else
    {
        sectors = defext;
    }
    do
    {
        sts = lookup_filename(&luf);
        if (sts != (FSYS_LOOKUP_SUCC|SEVERITY_INFO) && sts != FSYS_LOOKUP_FNF)
        {
            ioq->iostatus = sts;
            break;
        }
        fname = strrchr(t->path, QIO_FNAME_SEPARATOR); /* isolate the name from the path */
        if (!fname) fname = t->path;        /* no path, use name as is */
        if ((rfh=luf.file))           /* there's an old file */
        {
            int gen;
            if (rfh->directory)       /* cannot create a new file on top of a directory */
            {
                ioq->iostatus = FSYS_CREATE_NAMEINUSE;
                break;
            }
            if (vol->total_free_clusters < sectors*dtls->copies) /* room for file? */
            {
                FSYS_SQK(( OUTWHERE "%6ld: fcreate_q sets FSYS_CREATE_FULL. tot_free=%ld, wants=%d.\n",
                           eer_rtc, vol->total_free_clusters, sectors*dtls->copies));
                ioq->iostatus = FSYS_CREATE_FULL; /* file system is full, stop here */
                break;
            }
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
            if (vol->journ_rfh)
            {
                fl = rps_to_limbo(vol, rfh->ramrp, 0); /* prepare to put all the file's retrieval ptrs into limbo */
                if (!fl)
                {
                    ioq->iostatus = QIO_NOMEM;  /* sorry, ran out of memory */
                    break;
                }
            }
            else
#endif
                if (QIO_ERR_CODE((ioq->iostatus=free_rps(vol, rfh->ramrp)))) /* free all the retrieval pointers */
            {
                break;
            }
            gen = rfh->generation;          /* save old FH generation number */
            memset((char *)rfh, 0, sizeof(FsysRamFH));  /* zap the entire file header */
            luf.dir->gen_fid = (luf.dir->gen_fid&FSYS_DIR_FIDMASK)|(gen<<FSYS_DIR_GENSHF); /* restore generation number in directory */
            rfh->generation = gen;      /* restore generation number in FH */
            fid = fsys_find_id(vol, rfh);
            dp = luf.dir;           /* remember where the directory is */
            new = 0;                /* signal not to update directory */
        }
        else
        {
            U32 *ndx;
            int bcnt;
            char *fname;
            FsysFindFreeT freet;
            FsysRetPtr freep;
            int oowner;             /* need to save this */

            if (vol->total_free_clusters < FSYS_MAX_ALTS+1) /* room for some additional FH's? */
            {
                ioq->iostatus = FSYS_CREATE_NOFH;   /* nope, file system is full */
                break;
            }
            if (vol->total_free_clusters < sectors*dtls->copies) /* room for file? */
            {
                FSYS_SQK(( OUTWHERE "%6ld: fcreate_q sets FSYS_CREATE_FULL. tot_free=%ld, wants=%d.\n",
                           eer_rtc, vol->total_free_clusters, sectors*dtls->copies));
                ioq->iostatus = FSYS_CREATE_FULL;   /* file system is full */
                break;
            }
            oowner = fsys_find_id(vol, luf.owner);
            fid = get_fh(vol);              /* get the next available fileheader */
            if (fid <= 0)             /* no more file headers */
            {
                ioq->iostatus = fid ? -fid : FSYS_CREATE_NOFH;
                break;
            }
            luf.owner = fsys_find_ramfh(vol, oowner);
            ndx = ENTRY_IN_INDEX(vol, fid);
            rfh = fsys_find_ramfh(vol, fid);
            freet.skip = 0;
            freet.exact = 0;
            freet.actelts = vol->free_ffree;
            freet.totelts = vol->free_elems;
            freet.freelist = vol->free;
            freet.reply = &freep;
            freet.request = 1;
            freet.hint = 0;
            for (sts=bcnt=0; bcnt < FSYS_MAX_ALTS; ++bcnt)
            {
                freet.lo_limit = FSYS_COPY_ALG(bcnt, vol->maxlba);
                if ( fsys_findfree( &freet ) )    /* get a free block */
                {
                    freet.lo_limit = 0;         /* try again with no limits */
                    if ( fsys_findfree( &freet ) )
                    {
                        FSYS_SQK(( OUTWHERE "%6ld: fcreate_q sets FSYS_CREATE_FULL.\n", eer_rtc));
                        ioq->iostatus = FSYS_CREATE_FULL; /* no room for file header */
                        break;
                    }
                }
                if (vol->free_start > freet.which_elem) vol->free_start = freet.which_elem;
                if (*ndx && !(*ndx&FSYS_EMPTYLBA_BIT)) /* just to make sure we didn't screw up */
                {
                    FSYS_SQK(( OUTWHERE "%6ld: fcreate_q sets FSYS_CREATE_FATAL.\n", eer_rtc));
                    ioq->iostatus = FSYS_CREATE_FATAL;  /* oops, must have double mapped a file header */
                    break;
                }
                vol->free_ffree = freet.actelts;    /* update if appropriate */
                *ndx = freep.start;         /* load index file with new LBA */
#if (FSYS_FEATURES&FSYS_FEATURES_DIRLBA)
                if ((vol->features&FSYS_FEATURES_DIRLBA) && /* If the volume was authored with this feature */
                    dtls->mkdir)          /* and the file is to be a directory */
                {
                    *ndx |= FSYS_DIRLBA_BIT;        /* set the DIRBLA bit in the index file */
                }
#endif
                ++ndx;
            }   
            if (QIO_ERR_CODE(ioq->iostatus)) break;
            vol->total_free_clusters -= FSYS_MAX_ALTS;
            vol->total_alloc_clusters += FSYS_MAX_ALTS;
            dp = (FsysDirEnt *)QMOUNT_ALLOC(vol, sizeof(FsysDirEnt)+strlen(luf.fname)+1);
            fname = (char *)(dp+1);
            strcpy(fname, luf.fname);
            dp->name = fname;
            dp->gen_fid = (rfh->generation<<FSYS_DIR_GENSHF) | fid;
            dp->next = 0;
            upd_index_bits(vol, fid);
            new = 1;
        }
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
#if defined(HAVE_TIME) && HAVE_TIME
        rfh->ctime = dtls->ctime ? dtls->ctime : (U32)time(0);
#else
        rfh->ctime = dtls->ctime;
#endif
        rfh->mtime = dtls->mtime ? dtls->mtime : rfh->ctime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
        rfh->atime = dtls->atime;
        rfh->btime = dtls->btime;
#endif
        rfh->def_extend = defext;
        rfh->fh_upd = 1;                /* put FH on dirty list at close too */
        rfh->fh_new = 1;                /* file is new, so blow it away if found not closed during mount */
        add_to_dirty(t->vol, fid, 0);           /* put FH on the dirty list so free list stays correct */
        luf.file = rfh;                 /* in case of errors */
        rfh->clusters = sectors;
        for (ii=0; ii < FSYS_MAX_ALTS && ii < dtls->copies; ++ii) /* add retrieval pointers to the file */
        {
            FsysRamRP *rp;
            int where;

            rp = rfh->ramrp + ii;

            where = dtls->placement + ii;
            if (where >= FSYS_MAX_ALTS) where = FSYS_MAX_ALTS-1;
            sts = extend_file(vol, rfh->clusters, rp, where);
            if (sts)
            {
                FSYS_SQK(( OUTWHERE "%6ld: fcreate_q sets FSYS_CREATE_FULL. tot_free=%ld, wants=%d.\n",
                           eer_rtc, vol->total_free_clusters, sectors*dtls->copies));
                zap_file( &luf );           /* remove all traces of this file */
                ioq->iostatus = sts;
                break;
            }
        }
        if (QIO_ERR_CODE(ioq->iostatus)) break;
        if (new)
        {
            insert_ent(luf.owner->directory, dp);   /* insert new file into parent directory */
            add_to_dirty(t->vol, fsys_find_id(vol, luf.owner), 0); /* put owner directory on the dirty list too */
        }
        rfh->valid = 1;             /* mark entry as valid and useable */
        ioq->iostatus = FSYS_CREATE_SUCC|SEVERITY_INFO; /* assume success */
        dtls->fid = (rfh->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, rfh);
        file->private = qio_cvtToPtr(dtls->fid);
        dtls->parent = (luf.owner->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, luf.owner);
        while (dtls->mkdir)           /* are we to make it into a directory? */
        {
            FsysDirEnt *inddir;         /* pointer to list of directory ents */
            char *strings;          /* pointer to place to put strings */

            delete_after = 1;
            rfh->directory = (FsysDirEnt **)QMOUNT_ALLOC(vol, FSYS_DIR_HASH_SIZE*sizeof(FsysDirEnt*) +
                                                         3+2+    /* + room for strings (. and ..) */
                                                         2*sizeof(FsysDirEnt)); /* + room for directory items */
            if (!rfh->directory)
            {
                ioq->iostatus = FSYS_CREATE_NOMEM;
                break;
            }

            inddir = (FsysDirEnt *)(rfh->directory + FSYS_DIR_HASH_SIZE);
            strings = (char *)(inddir + 2);
            inddir->name = strings;
            *strings++ = '.';
            *strings++ = '.';
            *strings++ = 0;
            inddir->gen_fid = (luf.owner->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, luf.owner);
            insert_ent(rfh->directory, inddir);
            ++inddir;
            inddir->name = strings;
            *strings++ = '.';
            *strings = 0;
            inddir->gen_fid = (rfh->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, rfh);
            insert_ent(rfh->directory, inddir);
            rfh->fh_upd = 0;        /* directories are not normally 'opened', so don't mark it copy on close */
            rfh->fh_new = 0;        /* new directories cannot be stub files either */
            break;
        }
        break;
    } while (0);
    file->sect = file->bws = 0;     /* !FIXME!!! start at beginning of file */
    FSYS_SQK((OUTWHERE "fcreate_q(after): fh clust=%ld, size=%ld, dir=%08lX, active_rp=%d\n",
              rfh->clusters, rfh->size, (U32)rfh->directory, rfh->active_rp));
    FSYS_SQK((OUTWHERE "\tgen=%d, valid=%d, def_extend=%d\n",
              rfh->generation, rfh->valid, rfh->def_extend));
    qio_freemutex(&vol->mutex, ioq);    /* done with volume mutex */
    if (delete_after || QIO_ERR_CODE(ioq->iostatus))  /* if there were open errors */
    {
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
        if (fl)
        {
            QIOfree(fl);        /* nevermind about these */
            fl = 0;
        }
#endif
        qio_freefile(file);     /* done with the file descriptor */
        ioq->file = -1;         /* file didn't open */
        dtls->fid = -1;
        dtls->parent = -1;
    }
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    if (fl)
    {
        fl->next = vol->limbo;      /* put old sectors into limbo for now */
        vol->limbo = fl;
    }
#endif
    QIOfree(ioq->private);      /* done with this memory */
    ioq->private = 0;           /* burn our bridges */
    qio_complete(ioq);          /* call user's completion routine if any */
    return;
}
#endif				/* !FSYS_READ_ONLY */

static void fopen_q(QioIOQ *ioq)
{
    FsysOpenFileT *t;
    QioFile *file;
    FsysVolume *vol;
    FsysOpenT *dtls;
    FsysRamFH *rfh;
    FsysLookUpFileT luf;
    U32 fid, gen;
#if !FSYS_READ_ONLY
    int sts;
#endif

    t = (FsysOpenFileT *)ioq->private;
    vol = t->vol;
    dtls = t->details;

#if !FSYS_READ_ONLY
    sts = ((t->mode&(_FTRUNC|_FWRITE)) == (_FTRUNC|_FWRITE));
    if (sts || (t->mode&_FCREAT))
    {
        const char *fname;
        int lusts;

        luf.vol = vol;
        luf.top = 0;                /* If we supported 'cwd', this is where it would go */
        luf.path = t->path;
        lusts = lookup_filename(&luf);
        if (lusts != (FSYS_LOOKUP_SUCC|SEVERITY_INFO) && lusts != FSYS_LOOKUP_FNF)
        {
            sts = 1;                /* let fcreate_q() clean up errors */
        }
        else
        {
            fname = strrchr(t->path, QIO_FNAME_SEPARATOR); /* isolate the name from the path */
            if (!fname) fname = t->path;    /* no path, use name as is */
            if (!(rfh=luf.file))      /* there's no old file */
            {
                sts = 1;            /* so call create */
            }
        }
    }
    if (sts)
    {
        if ((vol->flags&FSYS_VOL_FLG_RO)) /* can we create a file? */
        {
            ioq->iostatus = FSYS_IO_NOWRITE;    /* filesystem is read only */
            ioq->private = 0;           /* burn our bridges */
            qio_freemutex(&vol->mutex, ioq);    /* done with volume mutex */
            QIOfree(t);             /* done with this memory */
            qio_complete(ioq);          /* call user's completion routine if any */
        }
        else
        {
            fcreate_q(ioq);         /* create a new file or truncate an old one */
        }
        return;
    }
#endif

    file = qio_fd2file(ioq->file);      /* get pointer to file */
    if (!file)
    {
        ioq->iostatus = FSYS_OPEN_NOTOPEN;
    }
    else do
        {
            if ((fid=dtls->fid))          /* if to open by FID */
            {
                gen = (fid >> FSYS_DIR_GENSHF)&0xFF;
                fid &= FSYS_DIR_FIDMASK;
                FSYS_SQK((OUTWHERE "open_q (READ) by fid: gen=%ld, fid=%ld\n", gen, fid));
                if (fid >= vol->files_ffree)
                {
                    ioq->iostatus = FSYS_OPEN_NOFID; /* fid out of range */
                    break;
                }
                rfh = fsys_find_ramfh(vol, fid);
                if (rfh->generation != gen)
                {
                    ioq->iostatus = FSYS_OPEN_NOGEN; /* generation numbers don't match */
                    break;
                }
                dtls->parent = 0;           /* don't know a parent if open by FID */
            }
            else                /* else we're to open by name */
            {
                FsysRamFH *own;
                luf.vol = vol;
                luf.top = 0;            /* start looking at root directory */
                luf.path = t->path;         /* point to adjusted name */
#if !FSYS_WRITE_SQUAWKING
                FSYS_SQK((OUTWHERE "open_q (READ) by name: %s\n", t->path));
#else
                if ( (t->mode&_FWRITE) )
                {
                    FSYS_WR_SQK((OUTWHERE "open_q (WRITE) by name: %s\n", t->path));
                }
#endif
                ioq->iostatus = lookup_filename(&luf);
                if (QIO_ERR_CODE(ioq->iostatus))  /* if didn't open */
                {
                    break;              /* just die */
                }
                rfh = luf.file;
                own = luf.owner;
                dtls->fid = (rfh->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, rfh); /* pass back FID of opened file */
                dtls->parent = (own->generation<<FSYS_DIR_GENSHF) | fsys_find_id(vol, own); /* pass back FID of parent */
            }       
#if !FSYS_READ_ONLY
            dtls->alloc = rfh->clusters * BYTPSECT; /* number of bytes allocated to file */
#else
            dtls->alloc = rfh->size;        /* number of bytes allocated to file */
#endif
            dtls->eof = rfh->size;          /* size of file in bytes */
            if (!(file->mode&_FAPPEND))
            {
                file->sect = file->bws = 0;     /* assume start at position 0 */
                if ((file->mode&(_FWRITE|_FREAD)) == _FWRITE) /* if open only for write ... */
                {
                    rfh->size = 0;          /* ... move the file's eof mark too */
                }
            }
            else
            {
                file->sect = dtls->eof/BYTPSECT;
                file->bws = dtls->eof&(BYTPSECT-1);
            }
            file->size = rfh->size;         /* note the size of the file */
            dtls->placement = 0;            /* cannot determine placement */
            for (gen=0; gen < FSYS_MAX_ALTS; ++gen)
            {
                if (!rfh->ramrp[gen].rptrs) break;
            }
            if ((file->mode&O_OPNCPY))
            {
                file->mode &= ~FSYS_OPNCPY_M;   /* zap all the bits */
                if (dtls->copies+1 > gen)
                {
                    ioq->iostatus = FSYS_LOOKUP_FNF; /* cannot open that version */
                    break;
                }
                file->mode |= (dtls->copies+1) << FSYS_OPNCPY_V;
            }
            dtls->copies = gen;         /* give 'em copies */
            dtls->mkdir = rfh->directory ? 1 : 0;   /* tell 'em if file is directory */
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
#if HAVE_TIME
            if ((t->mode&_FWRITE))        /* if we're opening for write */
            {
                rfh->mtime = time(0);       /* update the modification time */
            }
#endif	
            dtls->ctime = rfh->ctime;
            dtls->mtime = rfh->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
            dtls->atime = rfh->atime;
            dtls->btime = rfh->btime;
#endif
            file->private = qio_cvtToPtr(dtls->fid);   /* remember FID of open file for ourself */
            ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
            break;
        } while (0);
    if (QIO_ERR_CODE(ioq->iostatus))  /* if there were open errors */
    {
        qio_freefile(file);     /* done with the file descriptor */
        ioq->file = -1;         /* file didn't open */
        dtls->fid = -1;
        dtls->parent = -1;
    }
    dtls->hd_offset = vol->hd_offset;   /* return these just for kicks and giggles */
    dtls->hd_maxlba = vol->hd_maxlba;
    ioq->private = 0;           /* burn our bridges */
    qio_freemutex(&vol->mutex, ioq);    /* done with volume mutex */
    qio_complete(ioq);          /* call user's completion routine if any */
    QIOfree(t);             /* done with this memory */
    return;
}

/*********************************************************************
 * fsys_open - open a file for input or output. Expected to be called
 * from qio_open.
 *
 * At entry:
 *	ioq - pointer to QioIOQ struct.
 *	name - pointer to null terminated string with path/filename.
 *
 * NOTE: If O_QIOSPC is set in 'mode', then ioq->spc points to a
 * FsysOpenT struct with additional details about the open:
 *	path - pointer to null terminated string of whole name including
 *		volume name.
 *	fid - if .ne. and mode is O_RDONLY, will open the file by FID
 *		ignoring the 'name' parameter. Regardless of mode bits, open
 *		will set this field to the FID of the opened file at completion
 *		(or -1 if open failed).
 *	parent - Regardless of mode bits, open will set this field to the FID
 *		of the parent directory (or -1 if open failed).
 *	alloc - if mode set to O_WRONLY, specifies the amount of disk to
 *		pre-allocate to the file. Regardless of mode bits, open will
 *		set this field to the amount of space allocated to the file.
 *	eof - if mode set to O_WRONLY, specifies where the EOF marker is
 *		to be set. Regardless of mode bits, open will set this field
 *		to the position of the EOF marker at completion.
 *	placement - number from 0 to FSYS_MAX_ALTS-1 indicating the preferred
 *		placement of the file. Only relevant during file creation.
 *	copies - if mode contained O_WRONLY, specifies the number of copies
 *		of the file that are to be maintained (value must be from 1
 *		to FSYS_MAX_ALTS). Regardless of mode bits, open will set this
 *		field to the copies of files that are present in the file system
 *		at completion.
 *	mkdir - if mode contains O_CREAT and this field is not 0, the created
 *		file will made into a directory.

 * At exit:
 *	returns one of FSYS_CREATE_xxx signalling success or failure
 *	*file gets pointer to newly created FsysRamFH struct.
 */

static int fsys_open( QioIOQ *ioq, const char *name )
{
    FsysVolume *vol;
    FsysOpenFileT *ours=0;
    FsysOpenT *his;
    QioFile *file;
    int sts = 0;

    if (!ioq) return FSYS_OPEN_INVARG;
    do
    {
        file = qio_fd2file(ioq->file);      /* get pointer to file */
        his = (FsysOpenT *)file->private;
        vol = (FsysVolume *)file->dvc->private; /* point to our mounted volume */
        if (!vol)
        {
            ioq->iostatus = FSYS_OPEN_NOTMNT;
            break;
        }
        if ( vol->status != (FSYS_MOUNT_SUCC|SEVERITY_INFO) )
        {
            ioq->iostatus = vol->status ? vol->status : FSYS_OPEN_NOTMNT;
            break;
        }
        if (!vol->filesp)
        {
#if 0
            if (!his || his->fid != -1 || file->mode || (!his->hd_offset && !his->hd_maxlba))
            {
                ioq->iostatus = FSYS_OPEN_NOTMNT;
                break;
            }
            vol->hd_offset = his->hd_offset;
            vol->hd_maxlba = his->hd_maxlba;
            ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
            ioq->file = -1;
            ioq->private = 0;
            qio_freefile(file);         /* done with this */
            qio_complete(ioq);
            return 0;
#else
            ioq->iostatus = FSYS_OPEN_NOTMNT;
            break;
#endif
        }
        if ( (!his || !his->fid) && (!name || *name == 0) )
        {
            ioq->iostatus = FSYS_OPEN_INVARG;
            break;
        }
        if ((file->mode&_FCREAT))
        {
#if FSYS_READ_ONLY
            ioq->iostatus = FSYS_CREATE_NOSUPP;
            break;
#else
            if ((vol->flags&FSYS_VOL_FLG_RO))
            {
                ioq->iostatus = FSYS_IO_NOWRITE;
                break;
            }
            if (his && ((his->copies < 0 || his->copies > FSYS_MAX_ALTS) ||
                        (his->placement < 0 || his->placement >= FSYS_MAX_ALTS)) )
            {
                ioq->iostatus = FSYS_OPEN_INVARG;
                break;
            }
            if (his && !his->copies) his->copies = 1;
#endif
        }
        ours = QIOcalloc(sizeof(FsysOpenFileT)+sizeof(FsysOpenT),1);
        if (!ours)
        {
            ioq->iostatus = FSYS_OPEN_NOMEM;
            break;
        }
        if (!his)
        {
            his = (FsysOpenT *)(ours+1);
            his->spc.path = name;
            his->spc.mode = file->mode;
            his->copies = 1;
        }
        ours->details = his;
        ours->vol = vol;
        ours->ioq = ioq;
        ours->path = name;
        ours->mode = file->mode;
        ioq->private = (void *)ours;
        sts = qio_getmutex(&vol->mutex, fopen_q, ioq);
        if (sts)
        {
            ioq->iostatus = sts;
            break;
        }
        return 0;
    } while (0);
    if (ours) QIOfree(ours);
    qio_freefile(file);
    ioq->file = -1;
    ioq->private = 0;
    return ioq->iostatus;
}

#if !FSYS_READ_ONLY
/*********************************************************************
 * fsys_mkdir - create a directory. This function creates a directory on the
 * specified volume.
 *
 * At entry:
 *	ioq - pointer to QioIOQ struct.
 *	name - pointer to null terminated string with path/filename
 *	mode - mode to apply to directory (not used)
 *
 * At exit:
 *	returns one of FSYS_CREATE_xxx signalling success or failure.
 */

static int fsys_mkdir( QioIOQ *ioq, const char *name, int mode )
{
    FsysVolume *vol;
    FsysOpenFileT *ours=0;
    FsysOpenT *his;
    QioFile *file;
    int sts = 0;

    if (!ioq) return FSYS_OPEN_INVARG;
    do
    {
        file = qio_fd2file(ioq->file);      /* get pointer to file */
        vol = (FsysVolume *)file->dvc->private; /* point to our mounted volume */
        if (!vol)
        {
            ioq->iostatus = FSYS_OPEN_NOTMNT;
            break;
        }
        if ((vol->flags&FSYS_VOL_FLG_RO))
        {
            ioq->iostatus = FSYS_IO_NOWRITE;
            break;
        }
        ours = QIOcalloc(sizeof(FsysOpenFileT)+sizeof(FsysOpenT),1);
        if (!ours)
        {
            ioq->iostatus = FSYS_OPEN_NOMEM;
            break;
        }
        his = (FsysOpenT *)(ours+1);
        his->spc.path = name;
        his->spc.mode = O_CREAT|O_TRUNC;
        his->copies = FSYS_MAX_ALTS;    /* there are always max copies of directories */
        his->mkdir = 1;
        ours->details = his;
        ours->vol = vol;
        ours->ioq = ioq;
        ours->path = name;
        ours->mode = _FCREAT;
        ioq->private = (void *)ours;
        sts = qio_getmutex(&vol->mutex, fopen_q, ioq);
        if (sts)
        {
            ioq->iostatus = sts;
            break;
        }
        return 0;
    } while (0);
    if (ours) QIOfree(ours);
    qio_freefile(file);
    ioq->file = -1;
    return ioq->iostatus;
}
#endif

static int mk_ramdir(FsysVolume *vol)
{
    uint8_t *s, *lim, gen;
    uint32_t fid;
    int qty, len, totstr;
    FsysDirEnt *inddir, **dir;      /* pointer to list of directory ents */
    uint8_t *strings;     /* pointer to place to put strings */
    FsysRamFH *rfh;

    rfh = fsys_find_ramfh(vol, vol->files_indx);
    dir = rfh->directory;       /* pointer to our hash table */
    if (!dir)
    {
        vol->status = FSYS_MOUNT_FATAL;
        return 1;
    }
    s = (unsigned char *)vol->buff;
#if 1
    lim = s + vol->rw_amt;
    qty = totstr = 0;           /* start with nothing */            
    while (s < lim)           /* walk the list to get lengths and counts */
    {
        fid = (s[2]<<16) | (s[1]<<8) | *s;
        if (!fid) break;        /* fid of 0 is end of list */
        s += 4;             /* skip fid and generation number */
        len = *s++;
        if (!len) len = 256;
        totstr += len;
        ++qty;
        s += len;
    }
    inddir = (FsysDirEnt *)QMOUNT_ALLOC(vol, totstr+    /* room for strings */
                                        qty*sizeof(FsysDirEnt)); /* room for directory items */
    if (!inddir)
    {
        vol->status = FSYS_MOUNT_NOMEM;
        return 1;
    }
    strings = (unsigned char *)(inddir + qty);

    s = (unsigned char *)vol->buff;
    while (s < lim)
    {
        fid = (s[2]<<16) | (s[1]<<8) | *s;
        if (!fid) break;        /* fid of 0 is end of list */
        s += 3;
        gen = *s++;
        len = *s++;
        if (!len) len = 256;
        if (gen)
        {
            strcpy((char *)strings, (char *)s);
            inddir->name = (char *)strings;
            inddir->gen_fid = 0;        /* assume invalid entry */
            if (fid < vol->files_ffree)   /* fid is in bounds */
            {
                rfh = fsys_find_ramfh(vol, fid);    /* point to file header */
                inddir->gen_fid = (gen<<FSYS_DIR_GENSHF) | fid;
            }
            insert_ent(dir, inddir);    /* put filename into directory */
            ++inddir;
            strings += len;
        }
        s += len;
    }
#else
    lim = s + vol->rw_amt;
    while (s < lim)           /* walk the list to get lengths and counts */
    {
        totstr = 0;             /* start with nothing */            
        fid = (s[2]<<16) | (s[1]<<8) | *s;
        if (!fid) break;        /* fid of 0 is end of list */
        s += 3;
        gen = *s++;
        len = *s++;
        if (!len) len = 256;
        inddir = (FsysDirEnt *)QMOUNT_ALLOC(vol, len+   /* room for string */
                                            sizeof(FsysDirEnt)); /* room for directory item */
        if (!inddir)
        {
            vol->status = FSYS_MOUNT_NOMEM;
            return 1;
        }
        strings = (unsigned char *)(inddir + 1);
        strcpy((char *)strings, (char *)s);
        inddir->name = (char *)strings;
        inddir->fid = 0;        /* assume invalid entry */
        if (fid < vol->files_ffree)   /* fid is in bounds */
        {
            inddir->gen_fid = (gen<<FSYS_DIR_GENSHF) | fid;
        }
        insert_ent(dir, inddir);    /* put filename into directory */
        s += len;
    }
#endif
    return 0;
}

static int mk_ramfh(FsysVolume *vol, FsysRamFH *rfh)
{
    FsysHeader *hdr;
    FsysRamRP *rrp;
    FsysRetPtr *dst, *src;
    int ii, jj, kk, amt, nblk=FSYS_MAX_ALTS;

    hdr = (FsysHeader *)vol->buff;
    memset((char *)rfh, 0, sizeof(FsysRamFH));
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
    if (hdr->extension)
    {
        vol->status = FSYS_MOUNT_NOSUPP;    /* extension headers not supported */
        return 1;
    }
#endif
#if !FSYS_READ_ONLY
    rfh->clusters = hdr->clusters;
#endif
    rfh->size = hdr->size;
    rfh->generation = hdr->generation;      /* keep a copy of generation number */
    if (hdr->type == FSYS_TYPE_DIR)       /* if file is a directory */
    {
        rfh->directory = (FsysDirEnt **)QMOUNT_ALLOC(vol, FSYS_DIR_HASH_SIZE*sizeof(FsysDirEnt*));
        if (!rfh->directory)
        {
            vol->status = FSYS_MOUNT_NOMEM;
            return 1;
        }
        rfh->def_extend = FSYS_DEFAULT_DIR_EXTEND;
    }
    else
    {
        rfh->def_extend = FSYS_DEFAULT_EXTEND;
    }
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    rfh->mtime = hdr->mtime;
    rfh->ctime = hdr->ctime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_PERMS)
    rfh->perms = hdr->perms;
    rfh->owner = hdr->owner;
#endif
    rrp = rfh->ramrp;
    for (amt=kk=jj=0; jj < FSYS_MAX_ALTS; ++jj, ++rrp)
    {
        src = hdr->pointers[jj];
        for (ii=0; ii < FSYS_MAX_FHPTRS; ++ii, ++src)
        {
            if (!src->start) break;
            nblk += src->nblocks;
        }
        if (ii) ++kk;
        if (ii > amt) amt = ii;
    }
    if (kk)
    {
        jj = kk*amt;
        rrp = rfh->ramrp;
#if FSYS_TIGHT_MEM
        if (vol->rp_pool_size >= jj)
        {
            dst = vol->rp_pool;
            vol->rp_pool += jj;
            vol->rp_pool_size -= jj;
        }
        else
#endif
            dst = (FsysRetPtr *)QMOUNT_ALLOC(vol, jj*sizeof(FsysRetPtr));
        if (!dst)     /* ooops, ran out of memory */
        {
            vol->status = FSYS_MOUNT_NOMEM ;
            return 1;
        }
        for (jj=0; jj < kk; ++jj, ++rrp, dst += amt)
        {
            FsysRetPtr *lcl_dst;
            src = hdr->pointers[jj];
            if (src->start == 0) break; /* nothing left to do */
            rrp->rptrs = dst;
#if !FSYS_READ_ONLY
            rrp->rptrs_size = amt;
#endif
            lcl_dst = dst;
            for (ii=0; ii < amt; ++ii, ++lcl_dst, ++src)
            {
                if (!src->start) break;
                lcl_dst->start = src->start;
                lcl_dst->nblocks = src->nblocks;
            }
            rrp->num_rptrs = ii;
        }
    }
    vol->total_alloc_clusters += nblk;
    rfh->valid = 1;
#if !FSYS_READ_ONLY
    rfh->fh_upd = (FSYS_FH_FLAGS_OPEN&hdr->flags) ? 1 : 0;
    rfh->fh_new = (FSYS_FH_FLAGS_NEW&hdr->flags) ? 1 : 0;
#endif
    return 0;
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
    #if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
        #define JSYNC_TYPE_FH	1
        #define JSYNC_TYPE_IDX	2
        #define JSYNC_TYPE_FREE	3
        #define JSYNC_TYPE_EOF	4

enum jour_wstate
{
    JSYNC_BEGIN,
    JSYNC_WR_COMPL,
    JSYNC_FH_TO_JOU,
    JSYNC_DIR_TO_JOU,
    JSYNC_INDEX_TO_JOU,
    JSYNC_FREE_FH_TO_JOU,
    JSYNC_FREE_TO_JOU,
    JSYNC_MARK_BUSY,
    JSYNC_MARK_BUSY_COMPL,
    JSYNC_DONE
};

enum sync_dir_state
{
    JSYNC_DIR_BEGIN,
    JSYNC_DIR_WALK_HASH,
    JSYNC_DIR_DMP_ENT
};
    #endif		/* (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) */

enum syn_state
{
    SYNC_BEGIN,
    SYNC_WALK_DIRTY,
    SYNC_WRITE_FH,
    SYNC_WRITE_FH_COMPL,
#if !FSYS_READ_ONLY
    SYNC_UPD_INDEX,
    SYNC_INDEX_WRCOMPL,
    SYNC_UPD_FREE,
    SYNC_FREE_WRCOMPL,
    SYNC_UPD_DIRECTORY,
    SYNC_UPD_DIRECTORY_COMPL,
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    SYNC_MARK_JCLEAN,
#endif
#endif
    SYNC_DONE
};
#endif

#if !FSYS_READ_ONLY || FSYS_UPD_FH
    #if FSYS_NO_AUTOSYNC
static void sort_by_lba(FsysVolume *vol, FsysSyncT *syn)
{
    U32 *src, *begin, *end;
    int free;
#if FSYS_SYNC_SQUAWKING
    SYNSQK((OUTWHERE "%6ld: sort_by_lba: before sort, dirty_ffree = %d, alts = %d\n",
            eer_rtc, vol->dirty_ffree, syn->alts));
#if FSYS_SORT_SQUAWKING
    {
        int ii, jj;
        U32 *lba;
        for (ii=0; ii < vol->dirty_ffree; ++ii)
        {
            U32 fid;
            fid = vol->dirty[ii];
            SYNSQK((OUTWHERE "\t%4d: FID %08lX", ii, fid));
            lba = ENTRY_IN_INDEX(vol, fid);
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
            {
                SYNSQK((OUTWHERE ", LBA[%d]:%08lX", jj, *lba++ & FSYS_LBA_MASK));
            }
            SYNSQK((OUTWHERE "\n"));
        }
    }
#endif
#endif
    if (vol->dirty_ffree < 2)
    {
        SYNSQK((OUTWHERE "\tNo sort done. List already sorted\n"));
        return;         /* less than 2 items, already sorted */
    }
    begin = vol->dirty;
    end = begin + vol->dirty_ffree - 1;
    free = 0;
    if (!syn->alts)       /* on the first pass */
    {
        while (begin <= end)  /* one pass through to remove ptr to freelist */
        {
            if (*begin == FSYS_INDEX_FREE) /* if pointer to free list */
            {
                ++free;         /* remember it */
                memcpy(begin, begin+1, (end-begin)*sizeof(U32 *)); /* move everybody down one */
                --end;          /* and whack one off the end */
                break;
            }
            ++begin;
        }
        begin = vol->dirty;     /* start over */
    }
    while (begin < end)
    {
        src = begin;
        while (src < end)
        {
            int lba1, lba2;
            U32 t, *ulp;
            t = src[0];
            ulp = ENTRY_IN_INDEX(vol, t);
            lba1 = ulp[syn->alts] & FSYS_LBA_MASK;
            ulp = ENTRY_IN_INDEX(vol, src[1]);
            lba2 = ulp[syn->alts] & FSYS_LBA_MASK;
            if (lba1 > lba2)
            {
                src[0] = src[1];
                src[1] = t;
            }
            ++src;
        }
        --end;
    }
    if (free)
    {
        end = begin + vol->dirty_ffree - 1;
        end[1] = FSYS_INDEX_FREE;   /* add freelist pointer to end of list */
    }
#if FSYS_SYNC_SQUAWKING
    SYNSQK((OUTWHERE "%6ld: sort_by_lba: after sort, dirty_ffree = %d\n", eer_rtc, vol->dirty_ffree));
#if FSYS_SORT_SQUAWKING
    {
        int ii, jj;
        U32 *lba;
        for (ii=0; ii < vol->dirty_ffree; ++ii)
        {
            U32 fid;
            fid = vol->dirty[ii];
            SYNSQK((OUTWHERE "\t%4d: FID %08lX", ii, fid));
            lba = ENTRY_IN_INDEX(vol, fid);
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
            {
                SYNSQK((OUTWHERE ", LBA[%d]:%08lX", jj, *lba++ & FSYS_LBA_MASK));
            }
            SYNSQK((OUTWHERE "\n"));
        }
    }
#endif
#endif
}
    #endif

static int get_sync_buffers(FsysVolume *vol, FsysSyncT *syn)
{
    int old_size;
    unsigned char *old_ptr, *old_head;
    old_size = syn->buffer_size;
    old_ptr = (U8*)syn->output;
    old_head = (U8*)syn->buffers;
    syn->buffer_size = (syn->buffer_size + FSYS_SYNC_BUFFSIZE + (BYTPSECT-1)) & -BYTPSECT;
    SYNSQK((OUTWHERE "%6ld: get_sync_buffers: Increasing buffer size from %d to %d\n",
            eer_rtc, old_size, syn->buffer_size));
    syn->buffers = (uint32_t *)QMOUNT_ALLOC(vol, syn->buffer_size+QIO_CACHE_LINE_SIZE);
    if (!syn->buffers)
    {
        SYNSQK((OUTWHERE "        Ran out of memory\n"));
        return(syn->status = FSYS_SYNC_NOMEM);
    }
    syn->output = (uint32_t *)QIO_ALIGN(syn->buffers, QIO_CACHE_LINE_SIZE);
    memcpy(syn->output, old_ptr, old_size);
    QMOUNT_FREE(vol, old_head);
    syn->our_ioq.vol = vol;
    return 0;
}

static void unpack_rfh(FsysHeader *fh, FsysRamFH *rfh, U32 fid)
{
    FsysRetPtr *dst;
    FsysRamRP *src;
    int alts, ii;

    fh->id = (fid == FSYS_INDEX_INDEX ? FSYS_ID_INDEX : FSYS_ID_HEADER);
    fh->type = rfh->directory ? FSYS_TYPE_DIR : FSYS_TYPE_FILE;
    ii = 0;                 /* assume no flags */
    if (rfh->fh_new) ii |= FSYS_FH_FLAGS_NEW;   /* signal this file was new */
    if (rfh->fh_upd) ii |= FSYS_FH_FLAGS_OPEN;  /* signal this file was opened for write but had not been closed */
    fh->flags = ii;
#if (FSYS_FEATURES&FSYS_FEATURES_CMTIME)
#if (FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    fh->ctime = rfh->ctime;
    fh->mtime = rfh->mtime;
#else
    fh->ctime = fh->mtime = 0;  /* times not supported in this version */
#endif
#endif
#if (FSYS_FEATURES&FSYS_FEATURES_ABTIME)
#if (FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    fh->atime = rfh->atime;
    fh->btime = rfh->btime;
#else
    fh->atime = fh->btime = 0;
#endif
#endif
    fh->generation = rfh->generation;
    fh->size = rfh->size;
    fh->clusters = rfh->clusters;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_PERMS)
    fh->perms = rfh->perms;
    fh->owner = rfh->owner;
#endif
    for (alts=0; alts < FSYS_MAX_ALTS; ++alts)
    {
        src = rfh->ramrp + alts;
        dst = fh->pointers[alts];
        for (ii=0; ii < FSYS_MAX_FHPTRS;)
        {
            int lim;
            lim = FSYS_MAX_FHPTRS - ii;
            if (lim > src->num_rptrs) lim = src->num_rptrs;
            memcpy((char *)dst, (char *)src->rptrs, lim*sizeof(FsysRetPtr));
#if !FSYS_READ_ONLY
            ii += lim;
            dst += lim;         
            if (ii < FSYS_MAX_FHPTRS && !src->next)
            {
                lim = FSYS_MAX_FHPTRS - ii;
                memset((char *)dst, 0, lim*sizeof(FsysRetPtr));
                break;
            }
            src = src->next;
#else
            break;
#endif
        }           /* -- for FSYS_MAX_FHPTRS */
    }               /* -- for FSYS_MAX_ALTS */
}

    #if !FSYS_READ_ONLY
static int size_directory(FsysVolume *vol, FsysDirEnt **hash)
{
    FsysDirEnt *dir;
    int ii, tot=4;

    for (ii=0; ii < FSYS_DIR_HASH_SIZE; ++ii, ++hash)
    {
        dir = *hash;            /* point to directory entry */
        while (dir)
        {
            U32 fid;
            fid = (dir->gen_fid&FSYS_DIR_FIDMASK);
            if (fid  < vol->files_ffree)
            {
                FsysRamFH *nfh;
                nfh = fsys_find_ramfh(vol, fid);
                if (nfh->valid)
                {
                    tot += strlen(dir->name)+6;
                }
            }
            dir = dir->next;
        }
    }
    return tot;
}

static U32 adj_dir_size(FsysVolume *vol, FsysRamFH *rfh)
{
    int jj, sts, len, ovr, lim;
    len = size_directory(vol, rfh->directory);      /* compute size of directory */
    if (len > rfh->size)              /* if file grew */
    {
        if (len >= (lim=rfh->clusters*BYTPSECT))  /* grew past its britches */
        {
            int def;
            def = rfh->def_extend ? rfh->def_extend : FSYS_DEFAULT_DIR_EXTEND;
            ovr = ((len - lim) + BYTPSECT-1);       /* how much more we need */
            if (ovr/BYTPSECT > def) def = ovr;      /* get how much we actually need */
            if (def*FSYS_MAX_ALTS >= vol->total_free_clusters) /* Hey, there's no room */
            {
                SYNSQK((OUTWHERE "%6ld: adj_dir_size: No room to extend directory. len=%d, ovr=%d, def=%d, total_free=%ld\n",
                        eer_rtc, len, ovr, def, vol->total_free_clusters));
                return FSYS_EXTEND_FULL;        /* no room for extension */
            }
            sts = 0;
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
            {
                FsysRamRP *rp;
                rp = rfh->ramrp + jj;
                if (!rp->num_rptrs) continue;       /* no copy here */
                sts = extend_file(vol, def, rp, jj);
                if (sts)
                {
                    return sts;             /* ran out of room */
                }
                rfh->clusters += def;           /* up total allocation */
            }
        }
    }
    rfh->size = len;                    /* record file size (may have gotten smaller) */
    return 0;
}

static int sync_directory(FsysSyncT *syn, FsysRamRP *ramrp, U32 sector)
{
    char *dst;
    FsysDirEnt **hash, *dir;
    FsysVolume *vol;
    int ii, sects;
    FsysQio *qio;
    QioIOQ *ioq;
    FsysRamFH *rfh;

    qio = (FsysQio *)syn;
    ioq = (QioIOQ *)syn;
    vol = syn->vol;
    rfh = syn->dirfh;

    if ((syn->status = adj_dir_size(vol, rfh)))
    {
        return syn->status;
    }

    if (rfh->size > syn->buffer_size)
    {
        int news;
        news = (rfh->size + (BYTPSECT-1)) & -BYTPSECT;
        if (syn->buffers) QMOUNT_FREE(vol, syn->buffers);
        syn->buffers = QMOUNT_ALLOC(vol, news+QIO_CACHE_LINE_SIZE);
        if (!syn->buffers)
        {
            return syn->status = FSYS_SYNC_NOMEM;
        }
        syn->output = (uint32_t *)QIO_ALIGN(syn->buffers, QIO_CACHE_LINE_SIZE);
        syn->buffer_size = news;
    }

    dst = (char *)syn->output;

    hash = rfh->directory;      /* point to hash table */
    SYNSQK(( OUTWHERE "%6ld: Writing directory\n", eer_rtc));
    for (ii=0; ii < FSYS_DIR_HASH_SIZE; ++ii, ++hash)
    {
        dir = *hash;            /* point to directory entry */
        while (dir)
        {
            U32 fid;
            fid = (dir->gen_fid&FSYS_DIR_FIDMASK);
            if (fid  < vol->files_ffree)
            {
                FsysRamFH *nfh;
                nfh = fsys_find_ramfh(vol, fid);
                fid = dir->gen_fid;
                if (nfh->valid)
                {
                    int len;
                    len = strlen(dir->name)+1;
                    if (nfh->generation != (fid>>FSYS_DIR_GENSHF))
                    {
                        SYNSQK(( OUTWHERE "\tFile %s has bad generation #. Was %ld, changed to %d\n",
                                 dir->name, fid>>FSYS_DIR_GENSHF, nfh->generation));
                        fid = (nfh->generation<<FSYS_DIR_GENSHF) | (fid&FSYS_DIR_FIDMASK);
                    }
                    *dst++ = fid;
                    *dst++ = fid>>8;
                    *dst++ = fid>>16;
                    *dst++ = fid>>24;
                    *dst++ = len;
                    strcpy(dst, dir->name);
                    dst += len;
                }
            }
            else
            {
                SYNSQK(( OUTWHERE "\tFile %s has invalid FID of %08lX. Discarded entry\n",
                         dir->name, dir->gen_fid));
            }
            dir = dir->next;
        }
        if (syn->status) break;
    }
    *dst++ = 0;         /* need a null fid to end the list */
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = 0;
#if FSYS_SQUAWKING && !FSYS_SQUAWKING_STDIO
    if (!syn->alts && fsys_find_id(vol, rfh) == FSYS_INDEX_ROOT)
    {
        extern void fsys_dump_bytes(void *iop, U8 *s, int siz);
        SYNSQK(( OUTWHERE "       Dump of root directory (%d bytes)\n", rfh->size));
        fsys_dump_bytes(OUTWHERE (U8*)syn->output, dst - (char *)syn->output);
    }
#endif
    sects = dst - (char *)syn->output; /* compute size of data */
    if (sects&(BYTPSECT-1))
    {
        memset(dst, 0, BYTPSECT - (sects&(BYTPSECT-1))); /* clear the rest of the buffer to sector boundary */
    }
    if (syn->status) return syn->status;
    syn->state = SYNC_UPD_DIRECTORY_COMPL;
    qio->ramrp = ramrp;         /* point to retrieval pointer set */
    qio->u_len = rfh->size;
    qio->buff = (U8*)syn->output;   /* point to output buffer */
    qio->state = 0;
    qio->sector = sector;       /* starting sector */
    SYNSQK(( OUTWHERE "%6ld: sync, directory upd: Queuing write %d. u_len=%ld, sector=0\n",
             eer_rtc, syn->alts, qio->u_len));
    fsys_qwrite(ioq);
    return 0;
}

typedef struct wp_index
{
    U32 start;
    int sects;
    int sects_bytes;
    U8  *where;
    int max;
} WPIndex;

static int what_part_index(FsysSyncT *syn, WPIndex *wpi)
{
    U32 bit, *bp, *bits;
    U32 sects, sects_bytes, start, start_bytes;
    char *bufp;
    int elem, nelems, len, accum, end;
    FsysIndexLink *ilp;
    FsysVolume *vol;

    vol = syn->vol;
    nelems = vol->index_bits_elems;
    bits   = vol->index_bits;
#if FSYS_SYNC_SQUAWKING
    if (!syn->substate && !syn->alts)
    {
        int ii;
        SYNSQK(( OUTWHERE "%6ld: what_part_index: nelms=%d, bits = %08lX\n",
                 eer_rtc, nelems, (U32)bits));
        for (ii=0; ii < nelems; ++ii)
        {
            SYNSQK(( OUTWHERE "             Bits[%2d]: %08lX\n", ii, bits[ii]));
        }
    }
#endif
    if (!nelems || !bits)
    {
        return 0;           /* naught to do */
    }
    do
    {
        elem = syn->substate/32;
        bit = syn->substate&31;
        bp = bits + elem;
        while (elem < nelems && !*bp)
        {
            ++bp;
            ++elem;
            bit = 0;
            syn->substate = (syn->substate & ~31) + 32;
        }
        if (elem >= nelems)
        {
            return 0;           /* naught to do */
        }
        while (bit < 32 && !(*bp & (1<<bit)))
        {
            ++bit;
        }
        if (bit >= 32)
        {
            syn->substate = (syn->substate & ~31) + 32;
            continue;           /* loop to next batch */
        }
        break;
    } while (1);
    start = elem*32 + bit;      /* relative starting sector */
    elem = (start+1)/32;        /* next element */
    bit = (start+1)&31;         /* next bit */
    bp = bits + elem;
    sects = 0;
    do
    {
        if (elem >= nelems)
        {
            sects = elem*32 - start;
            break;
        }
        while (bit < 32 && (*bp & (1<<bit)))
        {
            ++bit;
        }
        if (bit < 30 && ( (*bp & (1<<(bit+1))) || (*bp & (1<<(bit+2))) ) )
        {
            bit += 3;
            continue;
        }
        else if (bit < 31 && (*bp & (1<<(bit+1))) )
        {
            bit += 2;
            continue;
        }
        if (bit >= 32)
        {
            ++elem;
            ++bp;
            bit = 0;
            continue;
        }
        sects = elem*32 + bit - start;
    } while (!sects);
    start_bytes = start*BYTPSECT;
    len = wpi->max;
    SYNSQK(( OUTWHERE "%6ld: what_part_index: found start=%ld, sects=%ld, max=%d\n",
             eer_rtc, start, sects, len));
    if (sects > len) sects = len;
    ilp = vol->indexp;
    end = accum = 0;
    while (ilp && start >= accum + (end=ilp->items*FSYS_MAX_ALTS*sizeof(U32)/BYTPSECT))
    {
        accum += end;       /* advance accumulator by sectors */
        ilp = ilp->next;
    }
    if (!ilp)
    {
        prc_panic("Fatal SYNC error");
    }
    len = accum+end-start;
    SYNSQK(( OUTWHERE "%6ld: what_part_index: after walking index list, sects=%ld, max=%d\n",
             eer_rtc, sects, len));
    if (sects > len) sects = len;
    sects_bytes = sects*BYTPSECT;
    syn->substate = start + sects;  /* advance substate for next time */
    if (wpi->where)
    {
        bufp = (char *)(ilp+1) + (start-accum)*BYTPSECT;
        memcpy(wpi->where, bufp, sects_bytes);
    }
    len = vol->files_ffree*FSYS_MAX_ALTS*sizeof(U32); /* length of index file */
    if (start_bytes+sects_bytes > len)
    {
        len -= start_bytes;
        if (len > 0 && wpi->where)
        {
            memset(wpi->where+len, 0, sects_bytes-len);
        }
    }
    wpi->sects = sects;
    wpi->sects_bytes = sects_bytes;
    wpi->start = start;
#if FSYS_JOU_SQUAWKING
    if (wpi->where)
    {
        int row, col, max;
        U32 *lba = (U32*)wpi->where;
        sects_bytes /= sizeof(U32);
        row = start*BYTPSECT/sizeof(U32);
        max = 9;
        for (; sects_bytes > 0; row += max)
        {
            printf("%08X: ", row);
            if (sects_bytes < max) max = sects_bytes;
            for (col=0; col < max; ++col)
            {
                printf("%08lX ", *lba++);
            }
            sects_bytes -= max;
            printf("\n");
        }       
    }
#endif
    return 1;           /* write something */
}
    #endif

    #if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
static int is_free_dirty(FsysVolume *vol)
{
    int ii, free_dirty = 0;
    for (ii=0; ii < vol->dirty_ffree; ++ii)
    {
        if (vol->dirty[ii] == FSYS_INDEX_FREE)    /* if free list header is dirty */
        {
            free_dirty = 1;
            break;
        }
    }
    return free_dirty;
}

static void fsys_sync_q(QioIOQ *ioq);

static int size_journal(FsysVolume *vol, FsysSyncT *syn)
{
    U32 fid, fh_need, dir_need, idx_need, free_need;
    int ii, ibits, sects, def, jj;
    WPIndex wpi;
    FsysRamFH *rfh;
    FsysFilesLink *fl;

    fh_need = (vol->dirty_ffree+2)*BYTPSECT;    /* one sector for each fileheader + free + jou itself */
    fh_need += (1+FSYS_MAX_ALTS)*sizeof(U32);   /* + tag and LBA's for each FH */
    dir_need = 0;
    for (ii=0; ii < vol->dirty_ffree; ++ii)   /* figure out how big the journal file needs to be */
    {
        fid = vol->dirty[ii];
        rfh = fsys_find_ramfh(vol, fid);    /* get fileheader */
        if (rfh->directory)           /* if it's a directory */
        {
            syn->status = adj_dir_size(vol, rfh); /* adjust the size of the directory and record it */
            if (syn->status) return syn->status;
            dir_need += rfh->size;
        }
    }
    JOUSQK((OUTWHERE "%6ld: size_journal: Need %ld bytes for fileheaders\n", eer_rtc, fh_need));
    JOUSQK((OUTWHERE "%6ld: size_journal: Need %ld bytes for directorues\n", eer_rtc, dir_need));
    idx_need = 0;
    syn->substate = 0;          /* start this at 0 */
    wpi.where = 0;
    wpi.max = (syn->buffer_size - 3*sizeof(U32))/BYTPSECT;
    while ((ibits = what_part_index(syn, &wpi)))
    {
        idx_need += 3*sizeof(U32);  /* accumulate tag + starting LBA's + num sects */
        idx_need += wpi.sects*BYTPSECT; /* accumulate count of sectors */
    }
    JOUSQK((OUTWHERE "%6ld: size_journal: Need %ld bytes for index file\n", eer_rtc, idx_need));
    free_need = 0;
    fl = vol->limbo;
    while (fl)
    {
        free_need += fl->items*sizeof(FsysRetPtr);  /* assume worst case */
        fl = fl->next;
    }
    rfh = fsys_find_ramfh(vol, FSYS_INDEX_FREE);    /* point to freelist file */
    free_need += 2*sizeof(U32);             /* tag + size */
    free_need += vol->free_ffree*sizeof(FsysRetPtr);    /* include current size */
    JOUSQK((OUTWHERE "%6ld: size_journal: Need %ld bytes for freelist file\n", eer_rtc, free_need));
    sects = (free_need+BYTPSECT-1)/BYTPSECT;
    if (sects > rfh->clusters)
    {
        def = rfh->def_extend;
        if (rfh->clusters+def < sects)
        {
            def = sects - rfh->clusters;
        }
        JOUSQK(( OUTWHERE "%6ld: jsync: extending freelist file. Was %ld sects, needs %d sects, bumping to %d bytes\n",
                 eer_rtc, rfh->clusters, sects, def));
        if (def*FSYS_MAX_ALTS >= vol->total_free_clusters)
        {
            JOUSQK(( OUTWHERE "%6ld: jsync: filesystem too full to extend freelist file. Has %ld, wants %d\n",
                     eer_rtc, vol->total_free_clusters, def*FSYS_MAX_ALTS));
            syn->status = FSYS_EXTEND_FULL; /* sorry, no more room */
            return syn->status;
        }
        for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
        {
            FsysRamRP *rp;
            rp = rfh->ramrp + jj;
            if (!rp->num_rptrs) continue;
            if ((syn->status = extend_file(vol, def, rp, jj)))
            {
                return syn->status;
            }
        }
        rfh->clusters += def;
        add_to_dirty(vol, FSYS_INDEX_FREE, 1);      /* make sure the freelist header gets updated too */
    }
    rfh = vol->journ_rfh;
    sects = (fh_need+dir_need+idx_need+free_need+BYTPSECT-1)/BYTPSECT;
    JOUSQK((OUTWHERE "%6ld: jsync: need %ld bytes for journal (%d sectors; has %ld sectors)\n",
            eer_rtc, fh_need+dir_need+idx_need+free_need, sects, rfh->clusters));
    if (sects > rfh->clusters)
    {
        int def;
        sects += 3;                 /* we have to add ourself, so make sure we have room for it */
        def = rfh->def_extend;
        if (rfh->clusters+def < sects)
        {
            def = sects - rfh->clusters;
        }
        JOUSQK((OUTWHERE "%6ld: jsync: Extending journal by %d sectors.\n", eer_rtc, def));
        if (def >= vol->total_free_clusters)
        {
            JOUSQK(( OUTWHERE "%6ld: jsync: filesystem too full to extend journal file. Has %ld, wants %d\n",
                     eer_rtc, vol->total_free_clusters, def));
            return syn->status = FSYS_EXTEND_FULL;  /* sorry, no more room */
        }
        if (extend_file(vol, def, rfh->ramrp, 0))
        {
            return syn->status = FSYS_EXTEND_FULL;  /* sorry, no more room */
        }
        rfh->clusters += def;               /* up total allocation */
        add_to_dirty(vol, vol->journ_id, 0);        /* record for posterity */
    }
    else
    {
        JOUSQK(( OUTWHERE "%6ld: jsync: journal has room. alloc=%ld sects, size=%ld bytes.\n",
                 eer_rtc, rfh->clusters, rfh->size));
    }
    return 0;
}

static int jsyn_wr_buff(FsysVolume *vol, FsysSyncT *syn, int nxtstate)
{
    QioIOQ *ioq = (QioIOQ *)syn;
    FsysQio *qio = (FsysQio *)syn;
    qio->ramrp = vol->journ_rfh->ramrp + 0; /* point to journal's retreival ptr set */
    qio->u_len = syn->buff_p & -BYTPSECT;
    qio->buff = (U8*)syn->output;       /* point to output buffer */
    qio->state = 0;
    qio->sector = vol->journ_sect;      /* start writing here */
    syn->nxt_state = nxtstate;          /* where we really go next */
    syn->state = JSYNC_WR_COMPL;        /* generic completion routine */
    syn->nxt_state = nxtstate;          /* where to go next */
    JOUSQK(( OUTWHERE "%6ld: sync, jsyn_wr_buff: Queuing write. u_len=%ld, sector=%ld\n",
             eer_rtc, qio->u_len, qio->sector));
    fsys_qwrite(ioq);
    return 1;
}

static int jsyn_wr_dir(FsysVolume *vol, FsysSyncT *syn)
{
    int ii;
    FsysRamFH *rfh;
    FsysDirEnt **dirp, *dir;
    U32 fid;
    char *dst;

    rfh = syn->dirfh;           /* directory we're currently processing */
    while (1)
    {
        switch (syn->dir_state)
        {
        case JSYNC_DIR_BEGIN:
            JOUSQK(( OUTWHERE "%6ld: jsyn_wr_dir: Writing %ld byte directory. Offset=%d\n",
                     eer_rtc, rfh->size, syn->buff_p));
            syn->htbl_indx = 0;     /* first entry in hash table */
            syn->dir_size = 0;      /* for error checking */
            syn->dir_state = JSYNC_DIR_WALK_HASH;
#if 0
            continue;           /* !!!FALL THROUGH TO NEXT CASE!!! */
#endif
        case JSYNC_DIR_WALK_HASH:
            dirp = rfh->directory + syn->htbl_indx;
            dir = 0;
            for (ii=syn->htbl_indx; ii < FSYS_DIR_HASH_SIZE; ++ii, ++dirp)
            {
                if ((dir = *dirp))
                {
                    break;
                }
            }
            if (!dir)
            {
                if (syn->buffer_size - syn->buff_p < sizeof(U32))
                {
                    return jsyn_wr_buff(vol, syn, JSYNC_DIR_TO_JOU); /* make room for EOD tag */
                }
                memset((char *)syn->output + syn->buff_p, 0, sizeof(U32));
                syn->buff_p += sizeof(U32);
                syn->dir_size += sizeof(U32);                   
                if (syn->dir_size != rfh->size)   /* if size didn't match */
                {
                    JOUSQK((OUTWHERE "%6ld: ERROR!!! Wrote %d bytes of directory. s/b %ld bytes\n",
                            eer_rtc, syn->dir_size, rfh->size));
                    syn->status = FSYS_SYNC_FATAL;  /* this is unrecoverable */
                    return 2;
                }
                syn->buff_p = (syn->buff_p+sizeof(U32)-1)&-sizeof(U32); /* round it up to longword boundary */
                return 0;
            }
            syn->htbl_indx = ii;
            syn->dir = dir;
            syn->dir_state = JSYNC_DIR_DMP_ENT;
#if 0
            continue;               /* !!!FALL THROUGH TO NEXT STATE!!! */
#endif
        case JSYNC_DIR_DMP_ENT: {
                FsysRamFH *nfh;
                dir = syn->dir;
                JOUSQK(( OUTWHERE "%6ld: jsyn_wr_dir: Dumping directory tree. Offset=%d\n",
                         eer_rtc, syn->buff_p));
                while (dir)
                {
                    if (syn->buffer_size - syn->buff_p < 256+sizeof(U32)+1)
                    {
                        syn->dir = dir;
                        JOUSQK(( OUTWHERE "%6ld: jsyn_wr_dir: Need room in buffer for directory. Offset=%d, last_name=%s\n",
                                 eer_rtc, syn->buff_p, last_name ? last_name : "<none>"));
                        return jsyn_wr_buff(vol, syn, JSYNC_DIR_TO_JOU);
                    }
                    nfh = fsys_find_ramfh(vol, dir->gen_fid&FSYS_DIR_FIDMASK);
                    fid = dir->gen_fid;
                    if (nfh->valid)
                    {
                        int len;

                        len = strlen(dir->name)+1;
                        if (nfh->generation != (fid>>FSYS_DIR_GENSHF))
                        {
                            JOUSQK(( OUTWHERE "\tFile %s has bad generation #. Was %ld, changed to %d\n",
                                     dir->name, fid>>FSYS_DIR_GENSHF, nfh->generation));
                            fid = (nfh->generation<<FSYS_DIR_GENSHF) | (fid&FSYS_DIR_FIDMASK);
                        }
                        dst = (char *)((char *)syn->output + syn->buff_p);
                        *dst++ = fid;
                        *dst++ = fid>>8;
                        *dst++ = fid>>16;
                        *dst++ = fid>>24;
                        *dst++ = len;
                        strcpy(dst, dir->name);
                        syn->buff_p += 5+len;
                        syn->dir_size += 5+len;
                    }
                    dir = dir->next;
                }
                syn->dir_state = JSYNC_DIR_WALK_HASH; /* look in hash table for more */
                ++syn->htbl_indx;       /* Next hash entry */
                continue;
            }
        }
    }
}

static int jsyn_wr_fh(FsysVolume *vol, FsysSyncT *syn)
{
    FsysRamFH *rfh;
    FsysHeader *fh;
    U32 fid, *begin, *end, *ndx;

    if (syn->buffer_size - syn->buff_p < BYTPSECT+(1+FSYS_MAX_ALTS)*sizeof(U32))
    {
        return jsyn_wr_buff(vol, syn, JSYNC_FH_TO_JOU);
    }
    if (!syn->substate)       /* on first pass */
    {
        int jou;
        begin = vol->dirty;
        end = begin + vol->dirty_ffree - 1;
        jou = 0;
        while (begin <= end)      /* one pass through to remove ptr to journal file */
        {
            if (*begin == vol->journ_id)  /* if pointer to journal */
            {
                ++jou;          /* remember we found it */
                memmove(begin, begin+1, (end-begin)*sizeof(U32 *)); /* move everybody down one */
                --end;          /* and whack one off the end */
                break;
            }
            ++begin;
        }
        if (jou)              /* if the journal is on the list */
        {
            JOUSQK((OUTWHERE "%ld jsync: Moving journal FH to head of dirty list.\n", eer_rtc));
            memmove(vol->dirty+1, vol->dirty, (vol->dirty_ffree-1)*sizeof(U32)); /* move everybody up one */
            vol->dirty[0] = vol->journ_id;  /* put the journal FH first */
        }
    }
    fid = vol->dirty[syn->substate];    /* get fid of file to process next */
    if (fid == FSYS_INDEX_FREE)
    {
        JOUSQK(( OUTWHERE "%6ld: jsync: Skipping FID %08lX until later\n",
                 eer_rtc, fid));
        return 0;           /* continue */
    }
    rfh = fsys_find_ramfh(vol, fid);    /* point to FH */
    JOUSQK(( OUTWHERE "%6ld: jsync: processing dirty fid: %08lX, valid=%d\n",
             eer_rtc, fid, 
             (fid >= FSYS_INDEX_ROOT) && (fid < vol->files_ffree) ? rfh->valid : 0));
    if ((fid > FSYS_INDEX_ROOT && !rfh->valid) || fid >= vol->files_ffree) /* it isn't valid anymore */
    {
        JOUSQK(( OUTWHERE "%6ld: jsync: fid %08lX invalid. ffree=%d, valid=%d\n",
                 eer_rtc, fid, vol->files_ffree, fid <= FSYS_INDEX_ROOT ? rfh->valid : 0));
        ndx = ENTRY_IN_INDEX(vol, fid);
        if (!syn->alts && fid < vol->files_ffree && !(*ndx&FSYS_EMPTYLBA_BIT)) /* check that the index file is ok with an invalid RFH */
        {
            int jj;
            for (jj=0; jj < FSYS_MAX_ALTS; ++jj) ndx[jj] = FSYS_EMPTYLBA_BIT+1;
            upd_index_bits(vol, fid); /* signal we have to write the index file */
        }
        return 0;           /* next file */
    }
    ndx = ENTRY_IN_INDEX(vol, fid);
    JOUSQK(( OUTWHERE "%6ld: jsync: FH offset=%d, fid %08lX, LBAs=%08lX, %08lX, %08lX\n",
             eer_rtc, syn->buff_p, fid, ndx[0], ndx[1], ndx[2]));
    begin = (U32 *)((U8*)syn->output + syn->buff_p);
    *begin++ = JSYNC_TYPE_FH;       /* what follows is a file header */
    memcpy(begin, ndx, sizeof(U32)*FSYS_MAX_ALTS);  /* first the 3 LBA's */
    begin += FSYS_MAX_ALTS;
    fh = (FsysHeader *)begin;
    unpack_rfh(fh, rfh, fid);
    syn->buff_p += BYTPSECT+(1+FSYS_MAX_ALTS)*sizeof(U32);
    if (rfh->directory)       /* if a directory follows */
    {
        syn->dirfh = rfh;       /* which file we're writing */
        syn->dir_state = JSYNC_DIR_BEGIN;
        return jsyn_wr_dir(vol, syn);
    }
    return 0;               /* next file */
}

static void fsys_jou_q(QioIOQ *ioq)
{
    FsysSyncT *syn;
    FsysQio *qio;
    FsysVolume *vol;
    FsysRamFH *rfh=0;
    FsysFilesLink *fl, *nxt;
    int ii;

    syn = (FsysSyncT *)ioq;
    qio = (FsysQio *)ioq;
    vol = syn->vol;
    syn->status = 0;            /* assume success */

    while (1)
    {
        switch ((enum jour_wstate)(syn->state))
        {
        default:
            JOUSQK((OUTWHERE "%6ld: fsys_jou_q: Illegal state of %d\n", eer_rtc, syn->state));
            syn->status = FSYS_SYNC_FATAL;      /* something terrible happened */
            break;
        case JSYNC_BEGIN: {
                if (!syn->buffers)
                {
                    JOUSQK(( OUTWHERE "%6ld: jsync: Begin, with getting buffers.\n", eer_rtc));
                    if (get_sync_buffers(vol, syn))   /* get buffers */
                    {
                        JOUSQK(( OUTWHERE "%6ld: jsync: failed to get sync buffers.\n", eer_rtc));
                        break;              /* gaack. Skip this */
                    }
                    ioq->file = vol->iofd;
                    syn->ramrp.rptrs = &syn->rptr;
                }
                if (size_journal(vol, syn))       /* figure out how much journal we need */
                {
                    break;              /* if we got errors, nevermind */
                }
                syn->ramrp.rptrs_size = 1;
                syn->ramrp.next = 0;
                syn->ramrp.num_rptrs = 1;
                syn->rptr.start = 0;            /* start at block 0 on device */
                syn->rptr.nblocks = vol->maxlba;    /* the whole disk is one file */
                qio->complt = fsys_jou_q;       /* completion routine is us */
                vol->journ_sect = 1;            /* start at sector 1 of journal file */
                syn->buff_p = 0;            /* start writing at byte 0 in 'output' */
                syn->substate = 0;          /* start this at 0 */
                syn->alts = 0;              /* start with alt 0 */ 
                syn->state = JSYNC_FH_TO_JOU;       /* walk the dirty list */
                continue;
            }
        case JSYNC_FH_TO_JOU:
            if (syn->substate >= vol->dirty_ffree) /* if we've reached the limit */
            {
                syn->state = JSYNC_INDEX_TO_JOU;
                syn->substate = 0;
                continue;
            }
            ii = jsyn_wr_fh(vol, syn);
            if (ii == 0)
            {
                ++syn->substate;            /* room for more, or we're to skip it */
                continue;
            }
            if (ii == 1)
            {
                return;             /* off writing the buffer */
            }
            break;                  /* got an error, have to give it up */
        case JSYNC_WR_COMPL:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                JOUSQK(( OUTWHERE "%6ld: jsync: Got write error (%08lX).\n",
                         eer_rtc, ioq->iostatus));
                syn->status = ioq->iostatus;
                break;
            }
            vol->journ_sect = qio->sector;
            if (vol->journ_sect > vol->journ_hiwater) vol->journ_hiwater = vol->journ_sect;
            memmove(syn->output, (U8*)syn->output + ioq->iocount, syn->buff_p - ioq->iocount);
            syn->buff_p &= (BYTPSECT-1);
            syn->state = syn->nxt_state;
            continue;
        case JSYNC_DIR_TO_JOU:
            ii = jsyn_wr_dir(vol, syn);
            if (ii == 0)              /* room for more */
            {
                syn->state = JSYNC_FH_TO_JOU;
                ++syn->substate;            /* next file */
                continue;
            }
            if (ii == 1)
            {
                return;             /* off writing the buffer */
            }
            break;                  /* got an error, have to give it up */
        case JSYNC_INDEX_TO_JOU: {
                int sts;
                WPIndex wpi;
                U32 *dst;

                if (syn->buffer_size - syn->buff_p < 3*sizeof(U32)+FSYS_INDEX_EXTEND_ITEMS)   /* has to be room for index file */
                {
                    jsyn_wr_buff(vol, syn, JSYNC_INDEX_TO_JOU); /* flush the buffer */
                    return;             /* wait for complete */
                }
                wpi.where = (U8*)syn->output + syn->buff_p + 3*sizeof(U32);
                wpi.max = (syn->buffer_size - syn->buff_p - 3*sizeof(U32))/BYTPSECT; /* max number of sectors */
                sts = what_part_index(syn, &wpi);
                if (!sts)
                {
                    if (!syn->substate)
                    {
                        JOUSQK(( OUTWHERE "%6ld: jsync: INDEX file not dirty.\n", eer_rtc));
                    }
                    else
                    {
                        JOUSQK(( OUTWHERE "%6ld: jsync: Done writing INDEX file.\n", eer_rtc));
                    }
                    syn->substate = 0;
                    syn->state = JSYNC_FREE_FH_TO_JOU;
                    continue;
                }
                dst = (U32 *)((U8*)syn->output + syn->buff_p);
                *dst++ = JSYNC_TYPE_IDX;        /* index type */
                *dst++ = wpi.start;         /* starting LBA */
                *dst++ = wpi.sects;         /* number of sectors */
                syn->buff_p += 3*sizeof(U32) + wpi.sects*BYTPSECT;
                JOUSQK(( OUTWHERE "%6ld: jsync: Wrote INDEX file. rel lba=%08lX, sects=%d.\n",
                         eer_rtc, wpi.start, wpi.sects));
                continue;
            }
        case JSYNC_FREE_FH_TO_JOU: {
                U32 *dst, *ndx;
                if (syn->buff_p+(3+FSYS_MAX_ALTS)*sizeof(U32)+BYTPSECT > syn->buffer_size) /* room for type and FH? */
                {
                    JOUSQK(( OUTWHERE "%6ld: jsync: Need %d bytes in buffer for freelist FH. Have %d.\n",
                             eer_rtc, (1+FSYS_MAX_ALTS)*sizeof(U32)+BYTPSECT,
                             syn->buffer_size-syn->buff_p));
                    jsyn_wr_buff(vol, syn, JSYNC_FREE_FH_TO_JOU);   /* nope, make room */
                    return;
                }
                if ((fl = vol->limbo))            /* pop the limbo list */
                {
#if FSYS_SYNC_FREE_SQUAWKING
#define JOU_FREE_SQK FSYS_FREE_SQK
#else
#define JOU_FREE_SQK JOUSQK
#endif
                    JOU_FREE_SQK((OUTWHERE "%6ld: jsync: Put limbo sectors back on free list.\n", eer_rtc));
                    while (fl)
                    {
                        FsysRetPtr *rp;
                        int itms;
                        rp = (FsysRetPtr *)(fl+1);
                        JOUSQK(( OUTWHERE "%6ld: jsync: tossing %d limbo RP's\n", eer_rtc, fl->items));
                        for (itms=0; itms < fl->items; ++itms, ++rp)
                        {
                            JOUSQK(( OUTWHERE "\t%3d: start=%08lX, numsect=%ld\n", itms, rp->start, rp->nblocks));
                            back_to_free(vol, rp);
                        }
                        nxt = fl->next;
                        QIOfree(fl);                /* done with this */
                        fl = nxt;
                    }
                    collapse_free(vol, INT_MAX, FSYS_JOU_SQUAWKING);    /* exposure to fsys corruption from here on */
                    vol->limbo = 0;
                }
                rfh = fsys_find_ramfh(vol, FSYS_INDEX_FREE);
                ii = (vol->free_ffree*sizeof(FsysRetPtr) + BYTPSECT - 1) & -BYTPSECT;
                if (is_free_dirty(vol) || ii != rfh->size)        /* need to update freelist FH */
                {
                    FsysHeader *fh;

                    JOU_FREE_SQK(( OUTWHERE "%6ld: jsync: free file changed size. Was %ld, now is %d.\n",
                                   eer_rtc, rfh->size, ii));
                    rfh->size = ii;             /* record high water mark */
                    dst = (U32*)((U8*)syn->output + syn->buff_p);
                    *dst++ = JSYNC_TYPE_FH;         /* file header follows */
                    ndx = ENTRY_IN_INDEX(vol, FSYS_INDEX_FREE);
                    memcpy(dst, ndx, sizeof(U32)*FSYS_MAX_ALTS);
                    dst += FSYS_MAX_ALTS;
                    fh = (FsysHeader *)dst;
                    unpack_rfh(fh, rfh, FSYS_INDEX_FREE);
                    syn->buff_p += BYTPSECT + (1+FSYS_MAX_ALTS)*sizeof(U32);
                    JOUSQK(( OUTWHERE "%6ld: jsync: FH (freelist) fid %08X, LBAs=%08lX, %08lX, %08lX\n",
                             eer_rtc, FSYS_INDEX_FREE, ndx[0], ndx[1], ndx[2]));
                    add_to_dirty(vol, FSYS_INDEX_FREE, 1);  /* make sure the freelist header gets updated too */
                }
                else
                {
                    JOU_FREE_SQK(( OUTWHERE "%6ld: jsync: Freelist header not dirty. Just writing freefile.\n", eer_rtc));
                }
                ii = vol->free_ffree*sizeof(FsysRetPtr);
                dst = (U32*)((U8*)syn->output + syn->buff_p);
                if (ii)
                {
                    *dst++ = JSYNC_TYPE_FREE;
                    *dst++ = ii;
                    syn->buff_p += 2*sizeof(U32);
                }
                syn->state = JSYNC_FREE_TO_JOU;
                syn->substate = ii;
                syn->findx = 0;         /* start writing at relative 0 */
                continue;
            }
        case JSYNC_FREE_TO_JOU: {
                ii = syn->buffer_size - syn->buff_p;    /* how much room left in the output buffer */
                JOUSQK(( OUTWHERE "%6ld: jsync: Writing freelist data. Bytes remaining: %d, room=%d\n",
                         eer_rtc, syn->substate, ii));
                if (syn->substate > 0)
                {
                    if (syn->substate < ii)
                    {
                        ii = syn->substate;
                    }
                    memcpy((U8*)syn->output + syn->buff_p, (U8*)vol->free + syn->findx, ii);
                    syn->buff_p += ii;
                    syn->substate -= ii;
                    syn->findx += ii;
                }
                if (syn->substate > 0)
                {
                    jsyn_wr_buff(vol, syn, JSYNC_FREE_TO_JOU);
                    return;
                }
                else
                {
                    syn->buff_p = (syn->buff_p+sizeof(U32)-1) & -sizeof(U32);
                    syn->state = JSYNC_MARK_BUSY;
                    syn->substate = 0;
                    continue;
                }
            }
        case JSYNC_MARK_BUSY: {
                U32 *dst;
                if (syn->buffer_size - syn->buff_p < sizeof(U32))
                {
                    jsyn_wr_buff(vol, syn, JSYNC_MARK_BUSY);
                    return;
                }
                dst = (U32 *)((U8*)syn->output + syn->buff_p);
                *dst = JSYNC_TYPE_EOF;
                syn->buff_p = (syn->buff_p + sizeof(U32) + BYTPSECT -1) & -BYTPSECT;
                jsyn_wr_buff(vol, syn, JSYNC_MARK_BUSY_COMPL);
                return;
            }
        case JSYNC_MARK_BUSY_COMPL: {
                U32 siz;
                siz = vol->journ_sect;  /* last sector */
                memset(syn->output, 0, BYTPSECT);
                syn->output[0] = siz;   /* record how big the file is */
                syn->buff_p = BYTPSECT;
                vol->journ_sect = 0;    /* sector 0 */
                JOUSQK(( OUTWHERE "%6ld: jsync: Marked journal busy containing %ld sectors.\n", eer_rtc, siz));
                jsyn_wr_buff(vol, syn, JSYNC_DONE);
                return;
            }
        case JSYNC_DONE:
#if _LINUX_
			sync(); /* make sure all data is written to disk */
#endif
            break;
        }       /* -- switch sync->state */
        break;
    }           /* -- while forever */
#ifndef FSYS_TEST_NO_SYNC
#define FSYS_TEST_NO_SYNC (0)
#endif
    if ( FSYS_TEST_NO_SYNC || QIO_ERR_CODE(syn->status))
    {
        if ( !FSYS_TEST_NO_SYNC )
        {
            syn->errlog[syn->err_in++] = syn->status;
            if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
            ++syn->errcnt;
        }
        syn->busy = 0;              /* not busy anymore */
        qio_freemutex(&vol->mutex, ioq);    /* done with the mutex */
    }
    else
    {
        syn->state = SYNC_BEGIN;
        fsys_sync_q(ioq);           /* now start the real sync task */
    }
    return;
}
    #endif			/* FSYS_OPTIONS&FSYS_FEATURES_JOURNAL && !FSYS_READ_ONLY */

static void fsys_sync_q(QioIOQ *ioq)
{
    FsysVolume *vol;
    FsysSyncT *syn;
    int sts;
    FsysRamFH *rfh=0;
    FsysQio *qio;
    int ii;
    U32 fid, *ndx;

    syn = (FsysSyncT *)ioq;
    qio = (FsysQio *)ioq;
    vol = syn->vol;
    syn->status = 0;            /* assume success */

    while (1)
    {
        switch (syn->state)
        {
        default:
            SYNSQK((OUTWHERE "%6ld: fsys_sync_q: Illegal state of %d\n", eer_rtc, syn->state));
            syn->status = FSYS_SYNC_FATAL;  /* something terrible happened */
            break;
        case SYNC_BEGIN: {
                if (!vol->dirty_ffree)
                {
                    break;          /* nothing in the dirty list, we're done */
                }
                if (!syn->output)     /* need to get a buffer */
                {
                    SYNSQK(( OUTWHERE "%6ld: sync: Begin, getting buffers.\n", eer_rtc));
                    if (get_sync_buffers(vol, syn)) break;
                    ioq->file = vol->iofd;
                    syn->ramrp.rptrs = &syn->rptr;
#if !FSYS_READ_ONLY
                    syn->ramrp.rptrs_size = 1;
                    syn->ramrp.next = 0;
#endif
                    syn->ramrp.num_rptrs = 1;
                    syn->rptr.start = 0;    /* start at block 0 on device */
                    syn->rptr.nblocks = vol->maxlba; /* the whole disk is one file */
                }
                qio->complt = fsys_sync_q;  /* completion routine is us */
                syn->substate = 0;
                syn->state = SYNC_WALK_DIRTY;   /* walk the dirty list */
                syn->alts = 0;          /* start with the first copy */
                SYNSQK(( OUTWHERE "%6ld: sync: Begin alt 0.\n", eer_rtc));
                continue;
            }

        case SYNC_WALK_DIRTY: {
#if FSYS_NO_AUTOSYNC
                if (!syn->substate) sort_by_lba(vol, syn);  /* arrange updates by LBA, low to high */
#endif
                if (syn->substate >= vol->dirty_ffree)
                {
#if !FSYS_READ_ONLY
                    if (!syn->alts)           /* check this on the first pass */
                    {
                        rfh = fsys_find_ramfh(vol, FSYS_INDEX_FREE);
                        collapse_free(vol, INT_MAX, FSYS_SYNC_FREE_SQUAWKING);
                        ii = (vol->free_ffree * sizeof(FsysRetPtr) + (BYTPSECT-1)) & -BYTPSECT;
                        if (ii != rfh->size)
                        {
                            FSYS_FREE_SQK(( OUTWHERE "%6ld: sync: updated FREE FH. Old size=%ld, new size=%d\n",
                                            eer_rtc, rfh->size, ii));
                            rfh->size = ii;         /* record high water mark */
                            add_to_dirty(vol, FSYS_INDEX_FREE, 1); /* add to end of dirty list */
                            continue;               /* reiterate cuz could have bumped dirty_ffree */
                        }
                        else
                        {
                            FSYS_FREE_SQK(( OUTWHERE "%6ld: sync: didn't update FREE FH (oldsize=%ld, newsize=%d)\n",
                                            eer_rtc, rfh->size, ii));
                        }
                    }
#endif
                    SYNSQK(( OUTWHERE "%6ld: sync: dirty list %d processing complete. substate=%d\n",
                             eer_rtc, syn->alts, syn->substate));

                    if (++syn->alts < FSYS_MAX_ALTS)
                    {
                        syn->substate = 0;      /* restart the whole procedure again */
                        continue;
                    }

#if !FSYS_READ_ONLY
                    syn->state = SYNC_UPD_INDEX; /* start updating index file */
                    syn->alts = 0;
#else
                    syn->state = SYNC_DONE;
#endif
                    syn->substate = 0;
                    continue;
                }
                fid = vol->dirty[syn->substate]; /* get fid of file to process next */
                rfh = fsys_find_ramfh(vol, fid);    /* point to FH */
                SYNSQK(( OUTWHERE "%6ld: sync: processing dirty fid: %08lX.%d. valid=%d\n",
                         eer_rtc, fid, syn->alts,
                         (fid >= FSYS_INDEX_ROOT) && (fid < vol->files_ffree) ? rfh->valid : 0));
#if FSYS_SYNC_FREE_SQUAWKING
                if (fid == FSYS_INDEX_FREE)
                {
                    FSYS_FREE_SQK((OUTWHERE "%6ld: sync, WALK_DIRTY, Updating free FH. clust=%ld, size=%ld\n",
                                   eer_rtc, rfh->clusters, rfh->size));
                }
#endif
                if ((fid > FSYS_INDEX_ROOT && !rfh->valid) || fid >= vol->files_ffree) /* it isn't valid anymore */
                {
                    SYNSQK(( OUTWHERE "%6ld: sync: fid %08lX invalid. ffree=%d, valid=%d\n",
                             eer_rtc, fid, vol->files_ffree, fid <= FSYS_INDEX_ROOT ? rfh->valid : 0));
#if !FSYS_READ_ONLY
                    ndx = ENTRY_IN_INDEX(vol, fid);
                    if (!syn->alts && fid < vol->files_ffree && !(*ndx&FSYS_EMPTYLBA_BIT)) /* check that the index file is ok with an invalid RFH */
                    {
                        int jj;
                        for (jj=0; jj < FSYS_MAX_ALTS; ++jj) ndx[jj] = FSYS_EMPTYLBA_BIT+1;
                        upd_index_bits(vol, fid); /* signal we have to write the index file */
                    }
#endif
                    ++syn->substate;    /* next file */
                    continue;
                }
#if !FSYS_READ_ONLY
                if (rfh->directory)
                {
                    syn->state = SYNC_UPD_DIRECTORY;
                    syn->dirfh = rfh;   /* point to the file to unpack */
                    syn->dir_state = 0; /* what dir creation state */
                    syn->htbl_indx = 0; /* which hash table entry to start working on */
                }
                else
                {
                    syn->state = SYNC_WRITE_FH;
                }
#else
                syn->state = SYNC_WRITE_FH;
#endif
                continue;
            }               /* -- case WALK_DIRTY */

#if !FSYS_READ_ONLY
        case SYNC_UPD_DIRECTORY:
            syn->dirfh = rfh;
            if (!sync_directory(syn, rfh->ramrp+syn->alts, 0))
            {
                return;
            }
            syn->errlog[syn->err_in++] = syn->status;   /* just log the error */
            if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
            ++syn->errcnt;
            syn->state = SYNC_WRITE_FH; /* go try to write the FH anyway */
            continue;
        case SYNC_UPD_DIRECTORY_COMPL:
            if (QIO_ERR_CODE(ioq->iostatus)) /* All we can do is log write errors */
            {
                syn->errlog[syn->err_in++] = ioq->iostatus;
                if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
                ++syn->errcnt;
            }
            syn->state = SYNC_WRITE_FH; /* go update the directory's file header */
            continue;
#endif
        case SYNC_WRITE_FH: {
                FsysHeader *fh;
                fid = vol->dirty[syn->substate]; /* get fid of file to process */
                fh = (FsysHeader *)syn->output;
                rfh = fsys_find_ramfh(vol, fid);
                unpack_rfh(fh, rfh, fid);
                ndx = ENTRY_IN_INDEX(vol, fid) + syn->alts;
                syn->state = SYNC_WRITE_FH_COMPL;
                qio->ramrp = &syn->ramrp;   /* point to fake retrieval pointer */
                qio->u_len = BYTPSECT;
                qio->buff = (U8*)syn->output;   /* point to output buffer */
                qio->state = 0;
                qio->sector = *ndx & FSYS_LBA_MASK;
                SYNSQK(( OUTWHERE "%6ld: sync, FH upd: Queuing write %d. u_len=%ld, sector=%ld\n",
                         eer_rtc, syn->alts, qio->u_len, qio->sector));
                fsys_qwrite(ioq);
                return;
            }           /* -- case SYNC_WRITE_FH */
        case SYNC_WRITE_FH_COMPL: {
                if (QIO_ERR_CODE(ioq->iostatus)) /* All we can do is log write errors */
                {
                    syn->errlog[syn->err_in++] = ioq->iostatus;
                    if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
                    ++syn->errcnt;
                }
                ++syn->substate;        /* next fid */
                syn->state = SYNC_WALK_DIRTY;   /* back to top of loop */
                continue;
            }           /* -- case SYNC_WRITE_FH_COMPL */

#if !FSYS_READ_ONLY
        case SYNC_UPD_INDEX: {
                int sts;
                WPIndex wpi;
                wpi.where = (U8*)syn->output;
                wpi.max = syn->buffer_size/BYTPSECT;
                sts = what_part_index(syn, &wpi);
                if (!sts)
                {
                    syn->substate = 0;
                    syn->state = SYNC_UPD_FREE;
                    continue;
                }
                syn->state = SYNC_INDEX_WRCOMPL;
                qio->ramrp = (fsys_find_ramfh(vol, FSYS_INDEX_INDEX))->ramrp + syn->alts; /* point to first retrieval pointer set */
                syn->sects = wpi.sects;     /* number of sectors to write */
                qio->u_len = wpi.sects_bytes;
                qio->buff = (U8*)syn->output;   /* point to output buffer */
                qio->state = 0;
                qio->sector = syn->start = wpi.start;
                SYNSQK(( OUTWHERE "%6ld: sync, INDEX upd: Queuing write %d. u_len=%ld, sector=%ld\n",
                         eer_rtc, syn->alts, qio->u_len, wpi.start));
                fsys_qwrite(ioq);
                return;
            }

        case SYNC_INDEX_WRCOMPL:
            if (QIO_ERR_CODE(ioq->iostatus)) /* All we can do is log write errors */
            {
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
#if !defined(AN_VIS_COL_MAX)
#define AN_VIS_COL_MAX 132
#endif
                char emsg[AN_VIS_COL_MAX];
                qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
                FSYS_SQK((OUTWHERE "%6ld: sync error writing INDEX %d. u_len=%ld, sector=%d\n",
                          eer_rtc, syn->alts, qio->u_len, syn->start));
                FSYS_SQK((OUTWHERE "        Reason: \"%s\"\n", emsg));
#endif
                syn->errlog[syn->err_in++] = ioq->iostatus;
                if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
                ++syn->errcnt;
            }
            syn->state = SYNC_UPD_INDEX;
            continue;

        case SYNC_UPD_FREE:
#if 0
            if (vol->free_start >= vol->free_ffree)
            {
                syn->state = SYNC_DONE;
                continue;
            }
#endif
            DUMP_FREELIST(vol, "SYNC_UPD_FREE");
            syn->state = SYNC_FREE_WRCOMPL;
            syn->start = 0; 
            syn->sects = (vol->free_ffree*sizeof(FsysRetPtr) + (BYTPSECT-1))/BYTPSECT; 
            qio->ramrp = (fsys_find_ramfh(vol, FSYS_INDEX_FREE))->ramrp+syn->alts; /* point to first retrieval pointer set */
            qio->u_len = syn->sects*BYTPSECT;
            qio->buff = (U8*)vol->free;
            qio->state = 0;
            qio->sector = syn->start;
#if FSYS_SYNC_FREE_SQUAWKING
            FSYS_FREE_SQK(( OUTWHERE "%6ld: sync, FREE upd: Queuing write %d. u_len=%ld, sector=%d\n",
                            eer_rtc, syn->alts, qio->u_len, syn->start));
#else
            SYNSQK(( OUTWHERE "%6ld: sync, FREE upd: Queuing write %d. u_len=%ld, sector=%d\n",
                     eer_rtc, syn->alts, qio->u_len, syn->start));
#endif
            fsys_qwrite(ioq);
            return;

        case SYNC_FREE_WRCOMPL:
            if (QIO_ERR_CODE(ioq->iostatus)) /* All we can do is log write errors */
            {
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
                char emsg[AN_VIS_COL_MAX];
                qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
                FSYS_SQK((OUTWHERE "%6ld: sync error writing FREE %d. u_len=%ld, sector=%d\n",
                          eer_rtc, syn->alts, qio->u_len, syn->start));
                FSYS_SQK((OUTWHERE "        Reason: \"%s\"\n", emsg));
#endif
                syn->errlog[syn->err_in++] = ioq->iostatus;
                if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
                ++syn->errcnt;
            }
            if (++syn->alts < FSYS_MAX_ALTS)
            {
                syn->state = SYNC_UPD_INDEX;
            }
            else
            {
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
                if (vol->journ_rfh)
                {
                    syn->alts = 0;
                    syn->state = SYNC_MARK_JCLEAN;
                    memset(syn->output, 0, BYTPSECT);
                    continue;
                }
#endif
                syn->state = SYNC_DONE;
            }
            continue;

#endif
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
        case SYNC_MARK_JCLEAN:
#if !FSYS_TEST_NO_JCLEAN
            SYNSQK(( OUTWHERE "%6ld: sync: Marking journal clean.\n", eer_rtc));
            qio->ramrp = vol->journ_rfh->ramrp;
            qio->u_len = BYTPSECT;
            qio->buff = (U8*)syn->output;
            qio->state = 0;
            qio->sector = 0;
            syn->state = SYNC_DONE;
            fsys_qwrite(ioq);
            return;
#else
            SYNSQK(( OUTWHERE "%6ld: sync: Not marking journal clean for testing...\n", eer_rtc));
            syn->state = SYNC_DONE;
            continue;
#endif
#endif
        case SYNC_DONE:
#if !FSYS_READ_ONLY
            if (vol->index_bits) memset((char *)vol->index_bits, 0, vol->index_bits_elems*sizeof(int32_t));
            vol->free_start = INT_MAX;
#endif
            vol->dirty_ffree = 0;
            SYNSQK(( OUTWHERE "%6ld: Sync done\n", eer_rtc));
#if _LINUX_
            sync(); /* make sure all data is written to disk */
#endif
            break;
        }       /* -- switch sync->state */
        break;
    }           /* -- while forever */

    ii = syn->busy;             /* save this for later */
    syn->busy = 0;              /* not busy anymore */
    syn->state = SYNC_BEGIN;            /* reset ourself */
    if (!syn->status) syn->status = FSYS_SYNC_SUCC|SEVERITY_INFO;
    sts = qio_freemutex(&vol->mutex, ioq);  /* done with the mutex */
    if (sts || syn->status != (FSYS_SYNC_SUCC|SEVERITY_INFO))
    {
        syn->errlog[syn->err_in++] = sts ? sts : syn->status;
        if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
        ++syn->errcnt;
    }
#if 0 && defined(IO_MAIN_LED_ON) && defined(IO_MAIN_CTL_T)
    IO_MAIN_CTL_T &= ~IO_MAIN_LED_ON;
#endif
    return;
}

static int fsys_sync(FsysSyncT *f, int how)
{
    FsysVolume *vol;
    int sts = 0, oldipl;
    void (*sync_p)(QioIOQ *ioq);

    vol = f->vol;
    oldipl = prc_set_ipl(INTS_OFF);
    if (!f->busy && vol->dirty_ffree && vol->status == (FSYS_MOUNT_SUCC|SEVERITY_INFO) )  /* if it not already running and there is something to do */
    {
        f->busy = how;          /* say it is busy */
        prc_set_ipl(oldipl);
        SYNSQK(( OUTWHERE "%6ld: sync: started. getting volume mutex\n", eer_rtc));
#if 0 && defined(IO_MAIN_LED_ON) && defined(IO_MAIN_CTL_T)
        IO_MAIN_CTL_T |= IO_MAIN_LED_ON;
#endif
        sync_p = fsys_sync_q;
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
        if ((vol->features&FSYS_FEATURES_JOURNAL) && vol->journ_rfh)  /* if we have journalling */
        {
            sync_p = fsys_jou_q;            /* Then start the journal routine first */
        }
#endif
        sts = qio_getmutex(&vol->mutex, sync_p, (QioIOQ *)f);
        if (sts)
        {
            f->errlog[f->err_in++] = sts;
            ++f->errcnt;
            if (f->err_in >= n_elts(f->errlog)) f->err_in = 0;
        }
        return sts;
    }
    prc_set_ipl(oldipl);
    SYNSQK(( OUTWHERE "%6ld: sync: busy=%d, dirty_ffree=%d. Back to waiting\n",
             eer_rtc, f->busy, vol->dirty_ffree));
    return -1;
}

    #if !FSYS_NO_AUTOSYNC
static int fsys_sync_time;

static void fsys_sync_t(QioIOQ *ioq)
{
    FsysVolume *vol;
    FsysSyncT *f;

    f = (FsysSyncT *)ioq;
    vol = f->vol;
    fsys_sync(f, FSYS_SYNC_BUSY_TIMER);
    f->sync_t.delta = fsys_sync_time ? fsys_sync_time : FSYS_SYNC_TIMER; /* requeue the sync timer */
    tq_ins(&f->sync_t);
    return;
}

int fsys_sync_delay(int new)
{
    int old, ii, oldps;
    FsysVolume *vol;

    oldps = prc_set_ipl(INTS_OFF);
    old = fsys_sync_time ? fsys_sync_time : FSYS_SYNC_TIMER;
    fsys_sync_time = new ? new : FSYS_SYNC_TIMER;
    vol = volumes;
    for (ii=0; ii < FSYS_MAX_VOLUMES; ++ii, ++vol)
    {
        struct tq *tq;
        tq = &vol->sync_work.sync_t;
        if (tq->que)
        {
            tq_del(tq);
            tq->delta = fsys_sync_time;
            tq_ins(tq);
            prc_set_ipl(oldps);
            fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER); /* force a sync */
            prc_set_ipl(INTS_OFF);
        }
    }
    prc_set_ipl(oldps);
    return old;
}
    #endif

static int fsys_fsync( QioIOQ *ioq )
{
    FsysVolume *vol;

    if (!ioq) return QIO_INVARG;
    vol = fsys_get_volume(ioq->file);
    qio_freefile(qio_fd2file(ioq->file)); /* done with the file descriptor */
    ioq->file = -1;
    if (!vol) return(ioq->iostatus = FSYS_SYNC_NOMNT);
    fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER);
    ioq->iostatus = FSYS_SYNC_SUCC|SEVERITY_INFO;   /* never report a sync error */
    return 0;
}
#endif

#if FSYS_USE_BUFF_POOLS
static void *fsys_getenv(const char *name, void *def)
{
    const struct st_envar *st;
    st = st_getenv(name, 0);
    if (!st || !st->value) return def;
    return(void *)st->value;
}
#endif

#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
enum jou_state
{
    JOU_RD_JOU_FH,
    JOU_RD_PROC_JOU_FH,
    JOU_RD_JOU_0,
    JOU_RD_JOU,
    JOU_RD_PROC_JOU,
    JOU_RD_IDX_FH,
    JOU_RD_PROC_IDX_FH,
    JOU_RD_IDX_0,
    JOU_RD_PROC_IDX_0,
    JOU_RD_FRE_FH,
    JOU_RD_PROC_FRE_FH,
    JOU_RD,
    JOU_RD_PROC,
    JOU_RD_PROC_FH,
    JOU_RD_PROC_IDX,
    JOU_RD_PROC_FRE,
    JOU_WR_FH,
    JOU_WR_DIR,
    JOU_WR_IDX,
    JOU_WR_FRE,
    JOU_WR_ZAP,
    JOU_RD_DONE
};
#endif

enum mount_e
{
    MOUNT_BEGIN,
    MOUNT_RD_HOME,
    MOUNT_CK_HOME,
    MOUNT_RD_HOME2,
#if !FSYS_NO_SEARCH_HBLOCK
    MOUNT_SEARCH_HOME,
#endif
    MOUNT_NOHBLK,
    MOUNT_RD_FH,
    MOUNT_PROC_FH,
#if !FSYS_READ_ONLY && (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    MOUNT_PROC_JOU,
#endif
    MOUNT_RD_FILE,
    MOUNT_PROC_FILE,
    MOUNT_RD_DIR,
    MOUNT_PROC_DIR,
    MOUNT_DEL_STUB
};

#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
static void fill_jou_rps(FsysHeader *hdr, FsysRamRP *rp, const char *txt)
{
    FsysRetPtr *rtp;
    int ii, jj;

    for (ii=0; ii < FSYS_MAX_ALTS; ++ii, ++rp)
    {
        rtp = rp->rptrs = hdr->pointers[ii];    /* point to the list of RP's */
        for (jj=0; jj < FSYS_MAX_FHPTRS; ++jj, ++rtp)
        {
            if (!rtp->start) break;
        }
        JOUSQK((OUTWHERE "%s file's header, set %d has %d RP's%s", txt, ii, jj, jj ? ":\n\t" : ".\n"));
        if (!jj)
        {
            rp->rptrs = 0;
        }
        else
        {
            rp->rptrs_size = FSYS_MAX_FHPTRS;   /* max size region */
            rp->num_rptrs = jj;     /* number of items in list */
#if FSYS_JOU_SQUAWKING
            rtp = rp->rptrs;
            if (jj > 4) jj = 4;
            for (;jj > 0; --jj, ++rtp)
            {
                JOUSQK((OUTWHERE "%08lX/%08lX ", rtp->start, rtp->nblocks));
            }
            if (rp->num_rptrs > 4)
            {
                JOUSQK((OUTWHERE "..."));
            }
            JOUSQK((OUTWHERE "\n"));
#endif		
        }
    }
}

typedef struct journ_data
{
    enum jou_state state;   /* process state */
    U32 *jou_buff;      /* ptr to journal buffer */
    U32 jou_offset;     /* (U32) index into buffer for next item */
#if !FSYS_USE_MALLOC
    struct _reent *jou_buff_re; /* where we got the buffer */
#endif
    FsysHeader jou_hdr;     /* journal file header goes here */
    FsysHeader idx_hdr;     /* index file header goes here */
    FsysHeader fre_hdr;     /* free file header goes here */
    U32 fre_lbas[FSYS_MAX_ALTS]; /* ptrs to freelist FH */
    U32 fh_lbas[FSYS_MAX_ALTS]; /* where file header goes */
    FsysRamRP jou_rp[FSYS_MAX_ALTS]; /* Journal file RP's */
    FsysRamRP idx_rp[FSYS_MAX_ALTS]; /* Index file RP's */
    FsysRamRP fre_rp[FSYS_MAX_ALTS]; /* Free file RP's */
    FsysRamRP dir_rp[FSYS_MAX_ALTS]; /* directory file RP's */
    int dir_size;       /* size of directory */
    int sects;          /* number of sectors in journal */
} FsysJouData;

static void jou_clean_up(FsysVolume *vol, FsysJouData *jd, int pass)
{
/* !FIXME! actually the journal potentially has crap, so we ought not to allow a mount at all */
    vol->state = MOUNT_RD_FH;       /* Start with normal mount */
#if 0
    memset(vol->journ_lbas, 0, FSYS_MAX_ALTS*sizeof(U32)); /* zap references to journal file */
#endif
    if (jd)
    {
#if !FSYS_USE_MALLOC
        if (jd->jou_buff)
        {
            _free_r(jd->jou_buff_re, jd->jou_buff);
        }
        _free_r(vol->journ_data_re, jd);
        vol->journ_data_re = 0;
#else
        if (jd->jou_buff) free(jd->jou_buff);
        free(jd);
#endif
    }
    vol->journ_data = 0;
    vol->substate = 0;          /* this needs to start out 0 */
    if (!pass) vol->flags |= FSYS_VOL_FLG_RO;   /* mark the volume as read only from here on */
}

    #if FSYS_JOU_SQUAWKING
static int jou_dump_bytes(U32 *src)
{
    int lin, col;
    for (lin=0; lin < 8; ++lin)
    {
        for (col=0; col < 8; ++col)
        {
            JOUSQK((OUTWHERE "%08lX ", src[lin*8+col]));
        }
        JOUSQK((OUTWHERE "\n"));
    }
    return 0;
}
        #define JOU_DUMP_BYTES jou_dump_bytes
    #else
        #define JOU_DUMP_BYTES(x) do { ; } while (0)
    #endif

static int proc_jou_file(FsysVolume *vol)
{
    QioIOQ *ioq;
    FsysQio *qio;
    FsysJouData *jd;
    FsysHeader *hdr;
    int ii;

    jd = (FsysJouData *)vol->journ_data;
    JOUSQK((OUTWHERE "Into proc_jou_file. vol->substate = %d, jd=%08lX, journ_state = %d\n",
            vol->substate, (U32)jd, jd ? jd->state : 0));
    if (!jd)                  /* first time through */
    {
#if !FSYS_USE_MALLOC
        vol->journ_data_re = malloc_from_any_pool((void **)&jd, sizeof(FsysJouData));
#else
        jd = malloc(sizeof(FsysJouData));
#endif
        if (!jd)              /* no memory for journal processing */
        {
            JOUSQK((OUTWHERE "Not enough memory to hold %d bytes.\n", sizeof(FsysJouData)));
            jou_clean_up(vol, jd, 0);
            return 0;               /* 0 = continue */
        }
        JOUSQK((OUTWHERE "Malloc'd %d bytes for journal data.\n", sizeof(FsysJouData)));
        memset(jd, 0, sizeof(FsysJouData));
        vol->journ_data = jd;
        vol->substate = 0;          /* this starts at 0 too */
    }

    ioq = (QioIOQ *)vol;
    qio = (FsysQio *)ioq;

    switch (jd->state)
    {
    case JOU_RD_JOU_FH:
        if (vol->substate < FSYS_MAX_ALTS)
        {
            JOUSQK((OUTWHERE "Trying for copy %d of journal file header.\n", vol->substate));
            qio->sector = vol->journ_lbas[vol->substate++] & FSYS_LBA_MASK;
            qio->ramrp = &vol->tmpramrp;        /* Use the fake RP */
            qio->u_len = BYTPSECT;          /* one sector worth */
            qio->buff = (U8*)&jd->jou_hdr;      /* into our journal header buffer */
            jd->state = JOU_RD_PROC_JOU_FH;     /* process the header */
            fsys_qread(ioq);
            return 1;               /* 1 = return */
        }
        JOUSQK((OUTWHERE "Error (%08lX) reading journal file's header. Chucking it...\n", ioq->iostatus));
        jou_clean_up(vol, jd, 0);
        return 0;
    case JOU_RD_PROC_JOU_FH:
        hdr = &jd->jou_hdr;
        if (QIO_ERR_CODE(ioq->iostatus) ||      /* can't have any read errors */
            hdr->id != FSYS_ID_HEADER)        /* and the id needs to be the correct type */
        {
            JOUSQK((OUTWHERE "Journal file's header is not a header. id=%08lX, iostatus=%08lX.\n",
                    hdr->id, ioq->iostatus));
            jd->state = JOU_RD_JOU_FH;      /* try to read the next one */
            return 0;
        }
        if (vol->substate > 1) USED_ALT();      /* log the fact that we got an error */
        fill_jou_rps(hdr, jd->jou_rp, "Journal");
        qio->sector = 0;                /* sector 0 of journal */
        qio->ramrp = jd->jou_rp + 0;        /* point to journal retrieval pointer set */
        qio->u_len = BYTPSECT;          /* read 1 sector */
        qio->buff = vol->file_buff;         /* buffer */
        vol->substate = 0;              /* start at copy 0 of journal file */
        jd->state = JOU_RD_JOU_0;           /* process journal sector 0 */
        JOUSQK((OUTWHERE "Queueing read of sector 0 of journal.\n"));
        fsys_qread(ioq);                /* queue the read */
        return 1;
    case JOU_RD_JOU_0: {
            U32 *ptr;
            int sects;
            if (QIO_ERR_CODE(ioq->iostatus))      /* can never have any input errors */
            {
                JOUSQK((OUTWHERE "Error (%08lX) reading journal file sector 0.\n", ioq->iostatus));
                jou_clean_up(vol, jd, 0);
                return 0;
            }
            ptr = (U32 *)vol->file_buff;        /* pointer to sector buffer */
            sects = *ptr++;             /* pickup the total sector count */
            for (ii=2; ii < BYTPSECT/sizeof(U32); ++ii)
            {
                if (*ptr++) break;          /* check that the rest of the sector is 0 */
            }
            if (ii != BYTPSECT/sizeof(U32))   /* first sector is not valid */
            {
                JOUSQK((OUTWHERE "Journal file sector 0 has crap in it at offset %d.\n",
                        ii*sizeof(U32)));
                jou_clean_up(vol, jd, 0);
                return 0;
            }
            JOUSQK((OUTWHERE "Journal file has %d sectors. CS=%08lX\n", sects, cs));
            if (!sects)               /* if journal is empty, we're done */
            {
                JOUSQK((OUTWHERE "Journal file is empty. Nothing to do.\n"));
                jou_clean_up(vol, jd, 1);
                return 0;
            }
            jd->sects = sects;          /* remember how many sectors the journal has */
            sects *= BYTPSECT;          /* convert file size to bytes */
#if !FSYS_USE_MALLOC
            jd->jou_buff = 0;
            jd->jou_buff_re = malloc_from_any_pool((void **)&jd->jou_buff, sects);
#else
            jd->jou_buff = malloc(sects);
#endif
            if (!jd->jou_buff)            /* no memory for journal */
            {
                JOUSQK((OUTWHERE "Not enough memory to hold %d bytes.\n", sects));
                jou_clean_up(vol, jd, 0);
                return 0;
            }
            jd->state = JOU_RD_IDX_FH;      /* read the index FH */
#if 0
            return 0;               /* !!!FALL THROUGH TO NEXT STATE!!! */
#endif
        }
    case JOU_RD_IDX_FH:
        if (vol->substate < FSYS_MAX_ALTS)
        {
            JOUSQK((OUTWHERE "Journal restore: Trying for copy %d of Index file header.\n", vol->substate));
            qio->sector = vol->index_lbas[vol->substate++] & FSYS_LBA_MASK;
            qio->ramrp = &vol->tmpramrp;        /* Use the fake RP */
            qio->u_len = BYTPSECT;          /* one sector worth */
            qio->buff = (U8*)&jd->idx_hdr;      /* into our index header buffer */
            jd->state = JOU_RD_PROC_IDX_FH;     /* process the header */
            fsys_qread(ioq);
            return 1;               /* wait for read to complete */
        }
        jou_clean_up(vol, jd, 0);
        return 0;
    case JOU_RD_PROC_IDX_FH:
        hdr = &jd->idx_hdr;
        if (QIO_ERR_CODE(ioq->iostatus) ||      /* can't have any read errors */
            hdr->id != FSYS_ID_INDEX)     /* and the id needs to be the correct type */
        {
            JOUSQK((OUTWHERE "Journal recover: Index file's header ID wrong. id=%08lX, iostatus=%08lX.\n",
                    hdr->id, ioq->iostatus));
            jd->state = JOU_RD_IDX_FH;      /* try to read the next one */
            return 0;
        }
        if (vol->substate > 1) USED_ALT();      /* log the fact that we got an error */
        fill_jou_rps(hdr, jd->idx_rp, "Index");
        jd->state = JOU_RD_IDX_0;           /* set next state */
        vol->substate = 0;              /* start at copy 0 */
#if 0
        return 0;
#endif
    case JOU_RD_IDX_0:
        if (vol->substate < FSYS_MAX_ALTS)    /* if we can start a read */
        {
            qio->sector = 0;            /* sector 0 of index file */
            qio->ramrp = jd->idx_rp + vol->substate++;  /* point to next set of Index file retrieval pointers */
            qio->u_len = BYTPSECT;          /* read 1 sector */
            qio->buff = vol->file_buff;     /* buffer */
            jd->state = JOU_RD_PROC_IDX_0;      /* process index file sector 0 */
            JOUSQK((OUTWHERE "Queueing read of sector 0 of index file.\n"));
            fsys_qread(ioq);                /* queue the read */
            return 1;                   /* wait for reply */
        }
        JOUSQK((OUTWHERE "No more options for reading Index file.\n"));
        jou_clean_up(vol, jd, 0);
        return 0;
    case JOU_RD_PROC_IDX_0:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a read error */
        {
            JOUSQK((OUTWHERE "Error (%08lX) reading sector 0 of index file during %dth try.\n",
                    ioq->iostatus, vol->substate-1));
            jd->state = JOU_RD_IDX_0;       /* try it again */
            return 0;
        }
        memcpy(jd->fre_lbas, vol->file_buff + FSYS_INDEX_FREE*FSYS_MAX_ALTS*sizeof(U32), FSYS_MAX_ALTS*sizeof(U32));
        JOUSQK((OUTWHERE "Found freelist FH LBA's at %08lX, %08lX, %08lX\n",
                jd->fre_lbas[0], jd->fre_lbas[1], jd->fre_lbas[2]));
        vol->substate = 0;
        jd->state = JOU_RD_FRE_FH;          /* Now go get the freelist FH */
#if 0
        return 0;               /* !!!FALL THROUGH TO NEXT STATE!!! */
#endif
    case JOU_RD_FRE_FH:
        if (vol->substate < FSYS_MAX_ALTS)
        {
            JOUSQK((OUTWHERE "Trying for copy %d of Freelist file header.\n", vol->substate));
            qio->sector = jd->fre_lbas[vol->substate++] & FSYS_LBA_MASK;
            qio->ramrp = &vol->tmpramrp;        /* Use the fake RP */
            qio->u_len = BYTPSECT;          /* one sector worth */
            qio->buff = (U8*)&jd->fre_hdr;      /* into our freelist header buffer */
            jd->state = JOU_RD_PROC_FRE_FH;     /* process the header */
            fsys_qread(ioq);
            return 1;               /* 1 = return */
        }
        JOUSQK((OUTWHERE "No more options for reading freelist header.\n"));
        jou_clean_up(vol, jd, 0);
        return 0;
    case JOU_RD_PROC_FRE_FH:
        hdr = &jd->fre_hdr;
        if (QIO_ERR_CODE(ioq->iostatus) ||      /* can't have any read errors */
            hdr->id != FSYS_ID_HEADER)        /* and the id needs to be the correct type */
        {
            JOUSQK((OUTWHERE "Journal file's header is not a header. id=%08lX, iostatus=%08lX.\n",
                    hdr->id, ioq->iostatus));
            jd->state = JOU_RD_FRE_FH;      /* try to read the next one */
            return 0;
        }
        if (vol->substate > 1) USED_ALT();      /* log the fact that we got an error */
        fill_jou_rps(hdr, jd->fre_rp, "Freelist");
        qio->sector = 1;            /* start with sector 1 of journal */
        qio->ramrp = jd->jou_rp;        /* point to rp set */
        qio->u_len = jd->sects*BYTPSECT;    /* read the whole damn journal into memory */
        qio->buff = (U8*)jd->jou_buff;  /* buffer */
        jd->state = JOU_RD_JOU;     /* read the journal */
        JOUSQK((OUTWHERE "Queing read of journal for %ld bytes.\n", qio->u_len));
        fsys_qread(ioq);            /* read the whole journal file */
        return 1;
    case JOU_RD_JOU:
        if (QIO_ERR_CODE(ioq->iostatus))  /* can never have any read errors */
        {
            JOUSQK((OUTWHERE "Error (%08lX) reading journal file. Failing sector probably %ld.\n",
                    ioq->iostatus, qio->sector));
            jou_clean_up(vol, jd, 0);
            return 0;
        }
        vol->substate = 0;
        jd->state = JOU_RD_PROC_JOU;    /* start processing journal */
        jd->jou_offset = 0;
#if 0
        return 0;               /* !!!FALL THROUGH TO NEXT STATE!!! */
#endif
    case JOU_RD_PROC_JOU: {
            U32 *src;
            src = jd->jou_buff + jd->jou_offset;
            vol->substate = 0;              /* which copy of FH we're writing */
            switch (*src)
            {
            case JSYNC_TYPE_FH:
                JOUSQK((OUTWHERE "Journal: Found FH tag. LBAs=%08lX, %08lX %08lX\n",
                        src[1], src[2], src[3]));
                memcpy(jd->fh_lbas, src+1, FSYS_MAX_ALTS*sizeof(U32)); /* record the LBAs */
                jd->jou_offset += 1+FSYS_MAX_ALTS;  /* point to header */
                jd->state = JOU_WR_FH;      /* write fileheader */
                return 0;               /* loop */
            case JSYNC_TYPE_IDX:
                ++jd->jou_offset;           /* skip tag */
                JOUSQK((OUTWHERE "Journal: Found Index tag. Starting sector=%08lX, num_sects=%ld\n",
                        src[1], src[2]));
                jd->state = JOU_WR_IDX;     /* write index sectors */
                return 0;               /* loop */
            case JSYNC_TYPE_FREE:
                jd->fre_hdr.size = *++src;      /* record size */
                jd->jou_offset += 2;        /* skip tag and length */
                JOUSQK((OUTWHERE "Journal: Found Freelist tag. Freelist size=%ld\n",
                        jd->fre_hdr.size));
                jd->state = JOU_WR_FRE;     /* write index sectors */
                return 0;               /* loop */
            case JSYNC_TYPE_EOF:
                JOUSQK((OUTWHERE "Journal: Found EOF.\n"));
                jd->state = JOU_WR_ZAP;
                return 0;               /* loop */
            default:
                JOUSQK((OUTWHERE "Error decoding journal entry at offset %ld:\n",
                        jd->jou_offset));
                JOU_DUMP_BYTES(src);
                jou_clean_up(vol, jd, 0);
                return 0;
            }
            break;                  /* better never get here */
        }
    case JOU_WR_FH:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a write error */
        {
            JOUSQK((OUTWHERE "Journal: Got a write error (%08lX) while writing copy %d of FH at %08lX\n",
                    ioq->iostatus, vol->substate-1, qio->sector));
/* !!!FIXME!!! probably fatal error */
        }
        hdr = (FsysHeader *)(jd->jou_buff + jd->jou_offset);
        if (vol->substate < FSYS_MAX_ALTS)        /* still something to write */
        {
            qio->sector = jd->fh_lbas[vol->substate++] & FSYS_LBA_MASK;
            if (vol->substate == 1)
            {
                if (hdr->id == FSYS_ID_HEADER && qio->sector == jd->fre_lbas[0])  /* if this is a new freelist header */
                {
                    JOUSQK((OUTWHERE "Found a new Freelist file header\n"));
                    memcpy(&jd->fre_hdr, hdr, sizeof(FsysHeader));  /* copy the freelist header */
                    fill_jou_rps(&jd->fre_hdr, jd->fre_rp, "New Freelist");
                }
                else if (hdr->id == FSYS_ID_INDEX && qio->sector == vol->index_lbas[0]) /* if this is a new index header */
                {
                    JOUSQK((OUTWHERE "Found a new Index file header\n"));
                    memcpy(&jd->idx_hdr, hdr, sizeof(FsysHeader));
                    fill_jou_rps(&jd->idx_hdr, jd->idx_rp, "New Index");
                }
                else if (hdr->id == FSYS_ID_HEADER && qio->sector == vol->journ_lbas[0]) /* if this is a new journal header */
                {
                    JOUSQK((OUTWHERE "Found a new Journal file header\n"));
                    if (memcmp(&jd->jou_hdr, hdr, sizeof(FsysHeader)) != 0) /* if different than what we already have */
                    {
                        memcpy(&jd->jou_hdr, hdr, sizeof(FsysHeader));
                        fill_jou_rps(&jd->jou_hdr, jd->jou_rp, "New Journal");
                        qio->sector = 1;            /* start with sector 1 of journal */
                        qio->ramrp = jd->jou_rp;        /* point to rp set */
                        qio->u_len = jd->sects*BYTPSECT;    /* read the whole damn journal into memory */
                        qio->buff = (U8*)jd->jou_buff;  /* buffer */
                        jd->state = JOU_RD_JOU;     /* read the journal */
                        JOUSQK((OUTWHERE "Queing (re)read of journal for %ld bytes.\n", qio->u_len));
                        fsys_qread(ioq);            /* read the whole journal file */
                        return 1;
                    }
                    else
                    {
                        JOUSQK((OUTWHERE "Journal header no different than what we already have.\n"));
                    }
                }
                else if (hdr->id != FSYS_ID_HEADER)
                {
                    JOUSQK((OUTWHERE "Hey, fileheader is not:\n"));
                    JOU_DUMP_BYTES((U32 *)hdr);
                    jou_clean_up(vol, jd, 0);
                    return 0;
                }
            }
            JOUSQK((OUTWHERE "Journal: Writing FH copy %d at %08lX.\n", vol->substate-1, qio->sector));
            qio->ramrp = &vol->tmpramrp;        /* Use the fake RP */
            qio->u_len = BYTPSECT;          /* one sector worth */
            qio->buff = (U8*)hdr;           /* pt to buffer */
            fsys_qwrite(ioq);
            return 1;                   /* wait for complete */
        }
        if (hdr->type == FSYS_TYPE_DIR)       /* if file is a directory */
        {
            jd->dir_size = hdr->size;       /* size of directory in bytes */
            fill_jou_rps(hdr, jd->dir_rp, "Directory");
            jd->state = JOU_WR_DIR;
            vol->substate = 0;
        }
        else
        {
            jd->state = JOU_RD_PROC_JOU;
        }
        jd->jou_offset += BYTPSECT/sizeof(U32);
        return 0;           /* continue */
    case JOU_WR_DIR:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a write error */
        {
            JOUSQK((OUTWHERE "Journal: Got a write error (%08lX) while writing copy %d of directory at %08lX\n",
                    ioq->iostatus, vol->substate-1, qio->sector));
/* !!!FIXME!!! probably fatal error */
        }
        if (vol->substate < FSYS_MAX_ALTS)        /* still something to write */
        {
            qio->sector = 0;                /* write directory starting at location 0 */
            JOUSQK((OUTWHERE "Journal: Writing %d byte directory. Copy %d.\n", jd->dir_size, vol->substate));
            qio->ramrp = jd->dir_rp + vol->substate++;  /* assign the RP's for this file */
            if (!qio->ramrp->rptrs || !qio->ramrp->num_rptrs)
            {
                JOUSQK((OUTWHERE "Journal: HEY!! No RP set %d\n", vol->substate-1));
            }
            else
            {
                qio->u_len = (jd->dir_size+BYTPSECT-1) & -BYTPSECT; /* directory size (rounded to sector size) */
                qio->buff = (U8*)(jd->jou_buff + jd->jou_offset); /* pt to buffer */
                fsys_qwrite(ioq);
                return 1;                   /* wait for complete */
            }
        }
        jd->jou_offset += (jd->dir_size+3)/4;       /* skip over directory */
        jd->state = JOU_RD_PROC_JOU;            /* go back to decoding */
        return 0;
    case JOU_WR_IDX:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a write error */
        {
            JOUSQK((OUTWHERE "Journal: Got a write error (%08lX) while writing copy %d of Index at %08lX\n",
                    ioq->iostatus, vol->substate-1, qio->sector));
/* !!!FIXME!!! probably fatal error */
        }
        if (vol->substate < FSYS_MAX_ALTS)        /* still something to write */
        {
            qio->sector = jd->jou_buff[jd->jou_offset];
            qio->ramrp = jd->idx_rp + vol->substate++;  /* Use the Index file RP */
            qio->u_len = jd->jou_buff[jd->jou_offset+1]*BYTPSECT; /* n sector's worth */
            qio->buff = (U8*)(jd->jou_buff + jd->jou_offset + 2); /* pt to buffer */
            JOUSQK((OUTWHERE "Journal: Writing %ld bytes of Index. copy %d at %08lX.\n",
                    qio->u_len, vol->substate-1, qio->sector));
            fsys_qwrite(ioq);
            return 1;                   /* wait for complete */
        }
        jd->state = JOU_RD_PROC_JOU;
        jd->jou_offset += jd->jou_buff[jd->jou_offset+1]*(BYTPSECT/sizeof(U32))+2;
        return 0;                   /* continue */
    case JOU_WR_FRE:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a write error */
        {
            JOUSQK((OUTWHERE "Journal: Got a write error (%08lX) while writing copy %d of Freelist at %08lX\n",
                    ioq->iostatus, vol->substate-1, qio->sector));
/* !!!FIXME!!! probably fatal error */
        }
        if (vol->substate < FSYS_MAX_ALTS)        /* still something to write */
        {
            qio->sector = 0;                /* we always write the whole freelist */
            qio->ramrp = jd->fre_rp + vol->substate++;  /* Use the Freelist file RP */
            qio->u_len = (jd->fre_hdr.size+BYTPSECT-1) & -BYTPSECT; /* size of freelist rounded up to sector size */
            qio->buff = (U8*)(jd->jou_buff + jd->jou_offset); /* pt to buffer */
            JOUSQK((OUTWHERE "Journal: Writing %ld bytes of Freelist copy %d.\n",
                    qio->u_len, vol->substate-1));
            fsys_qwrite(ioq);
            return 1;                   /* wait for complete */
        }
        jd->state = JOU_RD_PROC_JOU;
        jd->jou_offset += jd->fre_hdr.size/sizeof(U32);
        return 0;                   /* continue */
    case JOU_WR_ZAP:
        JOUSQK((OUTWHERE "Journal: Clearing sector 0 of journal.\n"));
        memset(jd->jou_buff, 0, BYTPSECT);
        qio->sector = 0;                /* we always write the whole freelist */
        qio->ramrp = jd->jou_rp + 0;        /* Use the Journal file RP */
        qio->u_len = BYTPSECT;          /* one sector */
        qio->buff = (U8*)jd->jou_buff;
        jd->state = JOU_RD_DONE;            /* we're done when this completes */
        fsys_qwrite(ioq);
        return 1;                   /* wait for complete */
    case JOU_RD_DONE:
        if (QIO_ERR_CODE(ioq->iostatus))      /* if we got a write error */
        {
            JOUSQK((OUTWHERE "Journal: Got a write error (%08lX) while clearing sector 0\n",
                    ioq->iostatus));
/* !!!FIXME!!! probably fatal error */
        }
        JOUSQK((OUTWHERE "Journal: Successfully applied journal.\n"));
        jou_clean_up(vol, jd, 1);
        return 0;
    default:
        JOUSQK((OUTWHERE "Undecoded journal state: %d\n", jd->state));
        break;
    }
    JOUSQK((OUTWHERE "proc_jou_file: !!ERROR!!! Fell out of switch statement.\n"));
    jou_clean_up(vol, jd, 0);
    return 0;
}
#endif

/*****************************************************************
 * fsys_qmount() - mount a volume. This function performs the steps necessary to
 * read the home blocks, index.sys, freemap.sys and directory files. It
 * places portions of those files into the 'volumes' array described above.
 * This routine is expected to run entirely as an AST function.
 *
 * At entry:
 *	vol - pointer to element in volumes array to which to mount.
 * At exit:
 *	returns nothing. 
 */
static void fsys_qmount(QioIOQ *ioq)
{
    FsysQio *qio;
    FsysHomeBlock *hb;
    FsysHeader *hdr;
    FsysRamFH *rfh;
    FsysRamRP *rrp;
    FsysVolume *vol;
/*    QioFile *hdfile; */
    int ii, sts;
    uint32_t *ulp;

    vol = (FsysVolume *)ioq;
    qio = (FsysQio *)ioq;
/*    hdfile = qio_fd2file(vol->iofd); */
    while (1)
    {
        switch ((enum mount_e)vol->state)         /* next? */
        {
        default:                /* Better never get here */
            ioq = qio->callers_ioq;
            vol->state = MOUNT_BEGIN;
            ioq->iostatus = vol->status = FSYS_MOUNT_FATAL;
            qio_complete(ioq);
            return;
/*****************************************************************************
 * First we init some variables, grab the mutex and switch to AST level.
 */
        case MOUNT_BEGIN: {         /* setup to read home blocks */
                struct act_q *q;
#if (!FSYS_READ_ONLY || FSYS_UPD_FH) 
                FsysSyncT *sw;
                sw = &vol->sync_work;
                sw->vol = vol;
                sw->our_ioq.vol = vol;
#if !FSYS_NO_AUTOSYNC
                {
                    struct tq *qt;
                    qt = &sw->sync_t;
                    qt->func = (void (*)(void *))fsys_sync_t;
                    qt->vars = (void *)sw;
                }
#endif
#endif
                if (!vol->buff)
                {
                    vol->buff_size = BYTPSECT;
#if FSYS_USE_BUFF_POOLS
                    {
                        const char *tn;
                        tn = (const char *)fsys_getenv("FSYS_TEMP_POOL", "JUNK_POOL_SIZE");
                        if (tn)
                        {
                            if ((vol->buff_pool = malloc_reentbyname(tn)))
                            {
                                vol->buff = _malloc_r(vol->buff_pool, vol->buff_size);
                            }
                        }
                        if (!vol->buff)
                        {
                            vol->buff_pool = malloc_from_any_pool((void **)&vol->buff, vol->buff_size);
                        }
                    }
#else
                    vol->buff = (U32*)QMOUNT_ALLOC(vol, vol->buff_size); /* get more memory */
#endif
                    if (!vol->buff)
                    {
                        vol->status = FSYS_MOUNT_NOMEM;
                        goto clean_up;
                    }
                }
                vol->substate = 0;      /* substate */
                vol->status = 0;        /* start with no status */
                vol->files_indx = 0;        /* start at index file */
                /* build a dummy retrieval pointer to get file headers... */
                vol->tmprp.start = 0;       /* "file header"'s are found in a a pseudo file starting at 0 */
                vol->tmprp.nblocks = vol->maxlba; /* ...and comprising the whole disk */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
                vol->tmprp.repeat = 1;
                vol->tmprp.skip = 0;
#endif
                vol->tmpramrp.rptrs = &vol->tmprp;
                vol->tmpramrp.num_rptrs = 1;
#if !FSYS_READ_ONLY
                vol->tmpramrp.next = 0;
                vol->tmpramrp.rptrs_size = 1;
#endif
                qio->ramrp = &vol->tmpramrp;    /* init the reader stuff */
                qio->u_len = BYTPSECT;
                qio->buff = (U8*)vol->buff; /* into our buffer */
                qio->complt = fsys_qmount;  /* come back to us when done */
                q = &vol->tmpq;
                q->action = (void (*)(void *))fsys_qmount;
                q->param = (void *)ioq;
                q->next = q->que = 0;
                vol->state = MOUNT_RD_HOME; /* switch to next state */
                vol->total_free_clusters = 0;
                vol->total_alloc_clusters = 1 + FSYS_MAX_ALTS;  /* add sector 0 + home blocks */
                sts = qio_getmutex(&vol->mutex, fsys_qmount, ioq);
                if (sts)
                {
                    vol->status = sts;
                    goto clean_up;
                }
                return;
            }

/*****************************************************************************
 * Loop however many times it takes to read a home block without error or until
 * all the home blocks have been tried. If it cannot find a home block where one
 * is expected, it reads every 256'th sector (+1, skipping sector 1) looking for
 * a sector that it recognizes as a home block. The read procedure hops back and
 * forth between states MOUNT_RD_HOME, MOUNT_CK_HOME and MOUNT_SEARCH_HOME until
 * a home block is successfully read.
 */
        case MOUNT_RD_HOME:         /* loop reading home blocks until a good one is found */
            qio->sector = FSYS_HB_ALG(vol->substate, vol->hb_range); /* relative sector number */
            qio->u_len = BYTPSECT;
            qio->buff = (U8*)vol->buff; /* into our buffer */
            vol->state = MOUNT_CK_HOME;
            fsys_qread(ioq);
            return;

        case MOUNT_RD_HOME2:        /* loop reading home blocks until a good one is found */
            qio->sector = FSYS_HB_ALG(vol->substate-FSYS_MAX_ALTS, vol->maxlba); /* relative sector number */
            qio->u_len = BYTPSECT;
            qio->buff = (U8*)vol->buff; /* into our buffer */
            vol->state = MOUNT_CK_HOME;
            fsys_qread(ioq);
            return;             /* wait for I/O to complete */

        case MOUNT_CK_HOME: {
                uint32_t cs;
                hb = (FsysHomeBlock *)vol->buff;
                ulp = vol->buff;
                for (cs=0, ii=0; ii < 128; ++ii) cs += *ulp++; /* checksum the home block */
                if (cs ||           /* checksum is expected to be 0 */
                    QIO_ERR_CODE(ioq->iostatus) ||  /* no errors are expected or accepted */
                    hb->id != FSYS_ID_HOME)   /* block is wrong kind */
                {
                    ++vol->substate;
                    if (vol->substate < FSYS_MAX_ALTS)
                    {
                        vol->state = MOUNT_RD_HOME; /* first 3 tries are normal */
                        continue;
                    }
                    if (vol->substate < FSYS_MAX_ALTS*2)
                    {
                        vol->state = MOUNT_RD_HOME2; /* second 3 tries are old method */
                        continue;
                    }
#if !FSYS_NO_SEARCH_HBLOCK
                    vol->state = MOUNT_SEARCH_HOME; /* try alternate home block addrs */
#else
                    vol->state = MOUNT_NOHBLK;      /* just return with error */
#endif
                    continue;
                }
                if (vol->substate > 0) USED_ALT();
                if (hb->hb_major != FSYS_VERSION_HB_MAJOR ||    /* sorry, can't abide it */
#if !FSYS_READ_ONLY && !(FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
                    hb->hb_size > sizeof(FsysHomeBlock) ||  /* if previously journalled, don't allow it */
#endif
                    hb->fh_major != FSYS_VERSION_FH_MAJOR ||
                    hb->fh_size != sizeof(FsysHeader) ||
                    hb->rp_major != FSYS_VERSION_RP_MAJOR ||
                    hb->rp_size != sizeof(FsysRetPtr) ||
                    hb->cluster != FSYS_CLUSTER_SIZE ||
                    hb->maxalts != FSYS_MAX_ALTS)
                {
                    vol->status = FSYS_MOUNT_VERSERR;
                    goto clean_up;
                }
                memcpy(vol->index_lbas, hb->index, sizeof(U32)*FSYS_MAX_ALTS);
#if FSYS_INCLUDE_BOOT_MARKERS
                memcpy(vol->boot_lbas[0], hb->boot, sizeof(U32)*FSYS_MAX_ALTS);
                memcpy(vol->boot_lbas[1], hb->boot1, 3*sizeof(U32)*FSYS_MAX_ALTS);
#endif
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
                memcpy(vol->journ_lbas, hb->journal, sizeof(U32)*FSYS_MAX_ALTS);
#endif
                if (hb->max_lba) vol->maxlba = hb->max_lba; /* adjust our maxlba from value in home block */
                vol->features = hb->features;       /* remember the features with which this fs was created */
                vol->substate = 0;
                vol->state = MOUNT_RD_FH;       /* goto to next state */
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
                if (vol->journ_lbas[0])       /* if there's a journal FH */
                {
                    JOUSQK((OUTWHERE "Found a journal file. Checking journal first.\n"));
                    vol->state = MOUNT_PROC_JOU;    /* Process the journal */
                }
#endif
                continue;
            }
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
        case MOUNT_PROC_JOU:
            sts = proc_jou_file(vol);
            if (!sts)
            {
                continue;
            }
            if (sts == 1)
            {
                return;
            }
            break;
#endif
#if !FSYS_NO_SEARCH_HBLOCK
        case MOUNT_SEARCH_HOME: {
                int nxt;
                nxt = (vol->substate-FSYS_MAX_ALTS*2+1)*256 + 1;
                if (nxt >= vol->maxlba)
                {
                    vol->state = MOUNT_NOHBLK;  /* couldn't find home block */
                    continue;
                }
                qio->sector = nxt;      /* relative sector number */
                qio->u_len = BYTPSECT;
                qio->buff = (U8*)vol->buff; /* into our buffer */
                vol->state = MOUNT_CK_HOME;
                fsys_qread(ioq);
                return;             /* wait for I/O to complete */
            }
#endif

        case MOUNT_NOHBLK: {
                vol->status = FSYS_MOUNT_NOHBLK ;   /* could not read any of the home blocks */
                clean_up:
#if FSYS_UMOUNT
                qmount_freeall(vol);
#else
                if (vol->filesp)
                {
                    FsysFilesLink *flp, *nlp;
                    flp = vol->filesp;
                    while (flp)
                    {
                        nlp = flp->next;
                        QMOUNT_FREE(vol, flp);
                        flp = nlp;
                    }
                }
#if !FSYS_READ_ONLY
                if (vol->indexp)
                {
                    FsysIndexLink *ilp, *nlp;
                    ilp = vol->indexp;
                    while (ilp)
                    {
                        nlp = ilp->next;
                        QMOUNT_FREE(vol, ilp);
                        ilp = nlp;
                    }
                }
#endif
#endif
                vol->filesp = 0;
                vol->files_elems = vol->files_ffree = 0;
                vol->buff = 0;
#if !FSYS_READ_ONLY
                vol->indexp = 0;
                vol->free = 0;
                vol->free_ffree = vol->free_elems = 0;
#else
                vol->index = 0;
#endif
                vol->state = MOUNT_BEGIN;
                qio_freemutex(&vol->mutex, ioq);
                ioq = qio->callers_ioq;
                ioq->iostatus = vol->status;
                qio_complete(ioq);
                return;
            }

/*****************************************************************************
 * The home block has the LBA's to find the file header(s) for the index file.
 *
 * The next step is to read all the file headers on the disk starting with the
 * index file. As file headers are read, their contents are stuffed into a
 * 'malloc'd FsysRamFH struct. The FsysRamFH structs are maintained as a linear
 * array, one element per file, the 'array index' of which matches the file's
 * position in the index file (its FID). This allows one to access both the pointer
 * to the LBA's in the index file and the ram copies of the file header with the
 * file's FID. 
 *
 * The read procedure hops back and forth between states MOUNT_RD_FH and MOUNT_PROC_FH
 * until a file header is read successfully. At this time, an unrecoverable error
 * reading a file header is fatal.
 */
        case MOUNT_RD_FH: {
                int bad;
                uint32_t sect;

                if (vol->files_indx == FSYS_INDEX_INDEX)  /* reading index file header */
                {
                    ulp = vol->index_lbas;      /* lba's are in a special spot while reading index.sys */
                }
                else
                {
                    if (vol->files_indx >= vol->files_ffree)  /* if we've reached the end */
                    {
                        vol->state = MOUNT_RD_DIR;      /* now go back and read all the directories */
                        vol->files_indx = FSYS_INDEX_ROOT;  /* start reading at root directory */
                        vol->substate = 0;
                        continue;
                    }
                    ulp = ENTRY_IN_INDEX(vol, vol->files_indx); /* any other file header */
                }
                if (!vol->substate)           /* on the first pass of a file header read */
                {
                    uint32_t *p;
                    bad = 0;
                    p = ulp;
                    for (ii=0; ii < FSYS_MAX_ALTS; ++ii, ++p) /* check to see if any of them are bad */
                    {
                        sect = *p;
                        if (!(sect&FSYS_EMPTYLBA_BIT))
                        {
                            sect &= FSYS_LBA_MASK;
                            if (!sect || sect >= vol->maxlba) /* if value is illegal */
                            {
                                sect = ii ? FSYS_EMPTYLBA_BIT : FSYS_EMPTYLBA_BIT+1; /* record it as unused */
                            }
                        }
                        if ((sect&FSYS_EMPTYLBA_BIT)) /* if unused */
                        {
#if !FSYS_READ_ONLY
                            if (!ii)          /* and the first one */
                            {
                                add_to_unused(vol, vol->files_indx);
                            }
#endif
                            ++bad;          /* record a bad index */
                        }
                        if (bad)
                        {
                            *p = sect;          /* replace with unused flag */
                        }
                    }
                    if (bad >= FSYS_MAX_ALTS)     /* if all are bad */
                    {
                        if (vol->files_indx <= FSYS_INDEX_ROOT)
                        {
                            if (vol->files_indx == FSYS_INDEX_INDEX)
                            {
                                vol->status = FSYS_MOUNT_NOINDX;
                            }
                            else if (vol->files_indx == FSYS_INDEX_FREE)
                            {
                                vol->status = FSYS_MOUNT_NOFREE;
                            }
                            else
                            {
                                vol->status = FSYS_MOUNT_NOROOT;
                            }
                            goto clean_up;      /* can't continue */
                        }
                        ++vol->files_indx;      /* skip this fileheader */
                        continue;           /* loop */
                    }
                }
                if (vol->substate < FSYS_MAX_ALTS)
                {
                    qio->sector = ulp[vol->substate++] & FSYS_LBA_MASK;
                    qio->ramrp = &vol->tmpramrp;    /* init the reader stuff */
                    qio->u_len = BYTPSECT;
                    qio->buff = (U8*)vol->buff;     /* into our local header buffer */
                    vol->state = MOUNT_PROC_FH;
                    fsys_qread(ioq);
                    return;
                }
                vol->status = FSYS_MOUNT_FHRDERR;   /* for now, an unreadable header is fatal */
                goto clean_up;
            }

        case MOUNT_PROC_FH: {       /* process the file header */
                uint32_t id;
#if !FSYS_READ_ONLY
                int jj;
#endif
                hdr = (FsysHeader *)vol->buff;
                id = (vol->files_indx == FSYS_INDEX_INDEX) ? FSYS_ID_INDEX : FSYS_ID_HEADER;
                if (QIO_ERR_CODE(ioq->iostatus) ||  /* can't have any input errors */
                    hdr->id != id)        /* and the id needs to be the correct type */
                {
                    vol->state = MOUNT_RD_FH;   /* try to read the next one */
                    continue;
                }
                if (vol->substate > 1) USED_ALT();
                if (vol->files_indx == FSYS_INDEX_INDEX) /* index file is done first */
                {
                    ii = hdr->size/(FSYS_MAX_ALTS*sizeof(int32_t));
                    vol->files_ffree = ii;  /* point to end of active list */
#if !FSYS_READ_ONLY
/* Round up the allocation to the appropriate multiple of sectors (the writer needs it this way) */
                    jj = ii % (FSYS_INDEX_EXTEND_ITEMS/(FSYS_MAX_ALTS*sizeof(U32)));
                    if (jj) ii += FSYS_INDEX_EXTEND_ITEMS/(FSYS_MAX_ALTS*sizeof(U32)) - jj;
#endif
                    vol->filesp = (FsysFilesLink *)QMOUNT_ALLOC(vol, sizeof(FsysFilesLink)+ii*sizeof(FsysRamFH));
                    if (!vol->filesp)
                    {
                        vol->status = FSYS_MOUNT_NOMEM; /* ran out of memory */
                        goto clean_up;
                    }
                    vol->filesp->items = ii;
                    vol->files_elems = ii;      /* total available elements */
#if FSYS_READ_ONLY
#if !FSYS_UPD_FH && FSYS_USE_BUFF_POOLS
                    vol->index = 0;
                    {
                        const char *tn;
                        tn = (const char *)fsys_getenv("FSYS_TEMP_POOL", "JUNK_POOL_SIZE");
                        if (tn)
                        {
                            if ((vol->index_pool = malloc_reentbyname(tn)))
                            {
                                vol->index = _malloc_r(vol->index_pool, (ii*FSYS_MAX_ALTS*sizeof(int32_t)+(BYTPSECT-1))&-BYTPSECT);
                            }
                        }
                    }               
                    if (!vol->index)
                    {
                        vol->index_pool = malloc_from_any_pool((void **)&vol->index, (ii*FSYS_MAX_ALTS*sizeof(int32_t)+(BYTPSECT-1))&-BYTPSECT);
                    }
#else
                    vol->index = (U32*)QMOUNT_ALLOC(vol, (ii*FSYS_MAX_ALTS*sizeof(int32_t)+(BYTPSECT-1))&-BYTPSECT);
#endif
                    if (!vol->index)
                    {
                        vol->status = FSYS_MOUNT_NOMEM;
                        goto clean_up;
                    }
                    vol->contents = vol->index;
#else    
                    vol->indexp = (FsysIndexLink *)QMOUNT_ALLOC(vol,
                                                                sizeof(FsysIndexLink) +
                                                                ii*FSYS_MAX_ALTS*sizeof(U32));      /* room to read the whole index file */
                    if (!vol->indexp)
                    {
                        vol->status = FSYS_MOUNT_NOMEM;
                        goto clean_up;
                    }
                    vol->indexp->next = 0;
                    vol->indexp->items = ii;
                    vol->contents = (U32 *)(vol->indexp + 1);
#endif
#if FSYS_TIGHT_MEM
                    vol->rp_pool = (FsysRetPtr *)QMOUNT_ALLOC(vol, FSYS_MAX_ALTS*ii*sizeof(FsysRetPtr));
                    if (vol->rp_pool) vol->rp_pool_size = ii;
#endif
                }
                else if (vol->files_indx == FSYS_INDEX_FREE)
                {
#if !FSYS_READ_ONLY
                    vol->free_elems = ((hdr->size+(2*BYTPSECT-1))&-BYTPSECT)/sizeof(FsysRetPtr);
                    vol->free = (FsysRetPtr *)QMOUNT_ALLOC(vol,
                                                           (vol->free_elems*sizeof(FsysRetPtr)+(BYTPSECT-1))&-BYTPSECT); /* room to read the whole file */
                    if (!vol->free)
                    {
                        vol->status = FSYS_MOUNT_NOMEM;
                        goto clean_up;
                    }
                    vol->contents = (uint32_t *)vol->free;
#else
                    ++vol->files_indx;
                    vol->substate = 0;
                    vol->state = MOUNT_RD_FH;
                    continue;
#endif
                }
                vol->rw_amt = (hdr->size+(BYTPSECT-1))&-BYTPSECT;
                rfh = fsys_find_ramfh(vol, vol->files_indx);
                if (mk_ramfh(vol, rfh)) goto clean_up;
                if (vol->files_indx <= FSYS_INDEX_ROOT)
                {
                    rfh->not_deleteable = 1;        /* the file cannot be deleted */
                }
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
                if (qio->sector-1 == vol->journ_lbas[vol->substate-1])
                {
                    JOUSQK((OUTWHERE "Found journal file at index %d\n", vol->substate));
                    vol->journ_rfh = rfh;
                    vol->journ_id = vol->files_indx;
                    rfh->not_deleteable = 1;        /* the journal file cannot be deleted */
                    rfh->def_extend = FSYS_DEFAULT_JOU_EXTEND;
                }
#endif
                if (vol->files_indx <= FSYS_INDEX_FREE)
                {
                    rfh->def_extend = FSYS_DEFAULT_DIR_EXTEND;
                }
                if (vol->files_indx > FSYS_INDEX_FREE)
                {
                    ++vol->files_indx;
                    vol->substate = 0;
                    vol->state = MOUNT_RD_FH;       /* read the next file header */
                    continue;               /* loop */
                }
                vol->state = MOUNT_RD_FILE; /* next state */
                vol->substate = 0;      /* substate */
                continue;
            }

/*****************************************************************************
 * The file header has been read. If it is a type INDEX, FREEMAP or DIRECTORY,
 * the contents need to be obtained. The read procedure hops between states MOUNT_RD_FILE
 * and MOUNT_PROC_FILE until the contents are successfully obtained. Note that these three
 * types of files are always duplicated FSYS_MAX_ALTS times on the disk.
 *
 * At this time, an unrecoverable error reading the contents of one of these
 * files is fatal.
 */
        case MOUNT_RD_FILE:         /* loop reading files until a good one is found */
            rfh = fsys_find_ramfh(vol, vol->files_indx);
            while (vol->substate < FSYS_MAX_ALTS)
            {
                rrp = rfh->ramrp + vol->substate++;
                if (!rrp->num_rptrs) continue;
                qio->ramrp = rrp;
                qio->sector = 0;        /* read file starting at relative sector 0 */
                qio->u_len = vol->rw_amt;
                qio->buff = (U8*)vol->contents;
                vol->state = MOUNT_PROC_FILE;
                fsys_qread(ioq);
                return;             /* wait for I/O to complete */
            }       
            vol->status = FSYS_MOUNT_RDERR; /* for now, an unreadable file is fatal */
            goto clean_up;

        case MOUNT_PROC_FILE: 
            if (QIO_ERR_CODE(ioq->iostatus))  /* can't have any input errors */
            {
                vol->state = MOUNT_RD_FILE; /* try to read the alternates */
                continue;
            }

            if (vol->files_indx == FSYS_INDEX_INDEX)
            {
                U32 *ulp;
                int jj, bad=0;
                ulp = (U32 *)vol->contents;
                for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
                {
                    if (!jj && vol->index_lbas[jj] != ulp[jj])
                    {
                        ++bad;
                        continue;
                    }
                    if (!(vol->index_lbas[jj]&FSYS_EMPTYLBA_BIT) &&
                        ulp[0] != vol->index_lbas[0])
                    {
                        ++bad;
                        continue;
                    }
                }
                if (bad)
                {
                    vol->state = MOUNT_RD_FILE; /* doesn't match, reject it */
                    continue;
                }
            }

            if (vol->substate > 1) USED_ALT();
#if 0
/*
 * If the file is of type DIRECTORY, a FsysDir struct is created for it and its
 * contents copied into that struct.
 */
            if (vol->files_indx > FSYS_INDEX_FREE)    /* Not index or free, so has to be dir */
            {
                if (mk_ramdir(vol)) goto clean_up;  /* create a directory for it */
            }
#endif
            ++vol->files_indx;
            vol->state = MOUNT_RD_FH;   /* read the next file */
            vol->substate = 0;
            continue;
        case MOUNT_RD_DIR: {
                int need;
                if (vol->files_indx >= vol->files_ffree)
                {
#if !FSYS_READ_ONLY
                    vol->files_indx = 0;    /* now sweep through the list again and delete any file stubs */
                    vol->state = MOUNT_DEL_STUB;
                    vol->substate = 0;
                    continue;
#else
                    break;          /* else, done */
#endif		    
                }
                rfh = fsys_find_ramfh(vol, vol->files_indx);
                if (!rfh->directory)
                {
                    ++vol->files_indx;      /* file is not a directory, skip it */
                    continue;
                }
                need = (rfh->size+(BYTPSECT-1))&-BYTPSECT;
                if (!need)            /* empty directory!!!! This is a bug */
                {
                    ++vol->files_indx;      /* so skip it. */
                    continue;           /* loop */
                }
                if (vol->buff_size < need)
                {
                    vol->buff_size = need;
#if FSYS_USE_BUFF_POOLS
                    if (vol->buff)
                    {
                        _free_r(vol->buff_pool, vol->buff);
                    }
                    vol->buff = 0;
                    {
                        const char *tn;
                        tn = (const char *)fsys_getenv("FSYS_TEMP_POOL", "JUNK_POOL_SIZE");
                        if (tn)
                        {
                            if ((vol->buff_pool = malloc_reentbyname(tn)))
                            {
                                vol->buff = _malloc_r(vol->buff_pool, vol->buff_size);
                            }
                        }
                    }               
                    if (!vol->buff)
                    {
                        vol->buff_pool = malloc_from_any_pool((void **)&vol->buff, vol->buff_size);
                    }
#else
                    if (vol->buff) QMOUNT_FREE(vol, vol->buff);
                    vol->buff = (U32*)QMOUNT_ALLOC(vol, vol->buff_size); /* get more memory */
#endif
                    if (!vol->buff)
                    {
                        vol->status = FSYS_MOUNT_NOMEM;
                        goto clean_up;
                    }
                }
                vol->contents = vol->buff;
                while (vol->substate < FSYS_MAX_ALTS)
                {
                    rrp = rfh->ramrp + vol->substate++;
                    if (!rrp->num_rptrs) continue;
                    qio->ramrp = rrp;
                    qio->sector = 0;        /* read file starting at relative sector 0 */
                    qio->u_len = vol->rw_amt = need;
                    qio->buff = (U8*)vol->contents;
                    vol->state = MOUNT_PROC_DIR;
                    fsys_qread(ioq);
                    return;             /* wait for I/O to complete */
                }       
                vol->status = FSYS_MOUNT_RDERR; /* for now, an unreadable directory is fatal */
                goto clean_up;
            }
        case MOUNT_PROC_DIR:
            if (QIO_ERR_CODE(ioq->iostatus))  /* can't have any input errors */
            {
                vol->state = MOUNT_RD_DIR;      /* try to read the alternates */
                continue;
            }
            if (vol->substate > 1) USED_ALT();
            if (mk_ramdir(vol)) goto clean_up;  /* create a directory for it */
            ++vol->files_indx;
            vol->state = MOUNT_RD_DIR;      /* read the next file */
            vol->substate = 0;
            continue;
#if !FSYS_READ_ONLY
        case MOUNT_DEL_STUB: {
                rfh = fsys_find_ramfh(vol, FSYS_INDEX_ROOT);
                FSYS_SQK(( OUTWHERE "fmount: calling zap_stub_files...\n"));
                zap_stub_files(vol, rfh, "./");     /* walk the entire directory tree */
                break;                  /* we're completely done now */
            }
#endif
        }               /* -- switch state */
        break;
    }                   /* -- while forever */
#if (!FSYS_READ_ONLY || FSYS_UPD_FH) && !FSYS_NO_AUTOSYNC
    if (!(vol->flags&FSYS_VOL_FLG_RO))
    {
        vol->sync_work.sync_t.delta = FSYS_SYNC_TIMER;
        tq_ins(&vol->sync_work.sync_t); /* start a sync timer */
    }
#endif
    if (vol->buff)
    {
#if FSYS_USE_BUFF_POOLS
        _free_r(vol->buff_pool, vol->buff); /* done with this */
#else
        QIOfree(vol->buff);         /* done with this buffer */
#endif
        vol->buff = 0;
        vol->buff_size = 0;
    }
#if FSYS_READ_ONLY && !FSYS_UPD_FH
    if (vol->index)
    {
#if FSYS_USE_BUFF_POOLS
        _free_r(vol->index_pool, vol->index);   /* done with this */
#else
        QIOfree(vol->index);            /* done with this buffer */
#endif
        vol->index = 0;
    }
#endif
    vol->state = MOUNT_BEGIN;       /* done */
    vol->status = FSYS_MOUNT_SUCC|SEVERITY_INFO; /* mounted with success */
#if !FSYS_READ_ONLY
    vol->total_free_clusters = compute_total_free(vol);
    vol->free_start = INT_MAX;      /* start it at max */
    DUMP_FREELIST(vol, "AFTER MOUNT" );
#endif
    sts = qio_freemutex(&vol->mutex, ioq);
    ioq = qio->callers_ioq;
    ioq->iostatus = vol->status;
    qio_complete(ioq);
    return;
}

extern void ide_squawk(int row, int col);
extern void ide_unsquawk(void);

/****************************************************************
 * General purpose async mount procedure.
 *
 * The following routines offer support for the asynch mount procedure.
 * mcom1_done() - called as completion routine after the temp file is
 *		closed. Cleans up any leftover stuff.
 * mcom1_clean() - called at completion of mount either due to error or
 *		success. Launches a close on the temp file.
 * mcom1_updpcnt() - called at 5Ms intervals while the mount is taking
 *		place. Updates the iocount field in the caller's QioIOQ
 *		from 0 to 100 representing the percent completion.
 * mcom1() - main completion routine called as various substeps are
 *		performed during the init of the mount sequence.
 * fsys_mount() - function to be called by the user.
 */

static void mcom1_done(QioIOQ *ioq)
{
    QioIOQ *uioq = (QioIOQ *)ioq->user;
    uioq->iostatus = qio_cvtFromPtr(ioq->user2);
    qio_freeioq(ioq);
    qio_complete(uioq);
}

#define MCOM1_START		(0)
#define MCOM1_DO_FSTAT		(1)
#define MCOM1_PARSE_PARTTBL	(2)
#define MCOM1_FSTAT_DONE	(3)
#define MCOM1_START_MOUNT	(4)
#define MCOM1_MOUNT_DONE	(5)

static void mcom1_clean(int32_t sts, QioIOQ *fioq)
{
    int32_t ostate;

#if FSYS_HAS_TQ_INS
    tq_del(&fioq->timer);       /* stop the pcnt timer */
#endif
    ostate = qio_cvtFromPtr(fioq->user2);      /* remember what state our mcom1 procedure was in */
    fioq->user2 = qio_cvtToPtr(sts);      /* save final exit status */
    if (QIO_ERR_CODE(sts) || ostate != MCOM1_MOUNT_DONE) /* if we got an error during qmount */
    {
        fioq->complete = mcom1_done;    /* place to go when close is complete */
        if (qio_close(fioq))      /* start a close procedure */
        {
            mcom1_done(fioq);       /* didn't queue, so just clean up and exit */
        }
    }
    else
    {
        mcom1_done(fioq);       /* Success. Don't close file. Just clean up and exit */
    }
}

#if FSYS_HAS_TQ_INS
static void mcom1_updpcnt(void *arg)
{
    QioIOQ *mioq = (QioIOQ *)arg;
    QioIOQ *uioq;
    FsysVolume *vol;
    int pcnt;

    uioq = (QioIOQ *)mioq->user;
    vol = (FsysVolume *)uioq->private2;
    if (vol->files_ffree)
    {
        if (vol->state >= MOUNT_RD_DIR)
        {
            pcnt = 95 + (vol->files_indx*5)/vol->files_ffree;
        }
        else
        {
            pcnt = (vol->files_indx*95)/vol->files_ffree;
        }
        if (pcnt > 99) pcnt = 99;
    }
    else
    {
        pcnt = 0;
    }
    uioq->iocount = pcnt;
    mioq->timer.delta = 5000;       /* re-launch ourself */
    tq_ins(&mioq->timer);
}
#endif

extern void _memcpy(void *dst, void *src, int len);

#if FSYS_USE_PARTITION_TABLE && FSYS_INCLUDE_HB_READER

    #define PPART_START		(0)
    #define PPART_PARSE_PARTTBL	(1)
    #define PPART_DONE		(2)

static void parse_part(QioIOQ *mioq)
{
    FsysVolume *vol;
    int sts;
    QioIOQ *hioq, *uioq;

    uioq = (QioIOQ *)mioq->user;
    hioq = (QioIOQ *)uioq->user;
    vol = (FsysVolume *)hioq->pparam1;
    sts = mioq->iostatus;
    mioq->iostatus = 0;
    if (QIO_ERR_CODE(sts))
    {
        if (!uioq->iostatus) uioq->iostatus = sts;
        mioq->user2 = (void *)PPART_DONE;
    }
    while (1)
    {
        switch ((int)mioq->user2)
        {
        case PPART_START:
            mioq->user2 = (void *)PPART_PARSE_PARTTBL;
            sts = (int)vol->file_buff;
            vol->filep = (U8*)((sts+31)&-32);
            sts = qio_readwpos(mioq, 0, vol->filep, BYTPSECT);  /* read the boot sector */
            if (sts)
            {
                break;
            }
            return;
        case PPART_PARSE_PARTTBL: {
                DOSBootSect *bp;
                DOSPartition *ppart;

                bp = (DOSBootSect *)vol->filep;
                if (bp->end_sig != 0xAA55)
                {
                    sts = 4;                /* force a failure */
                }
                else
                {
                    ppart = ((FsysGSetHB *)hioq->pparam0)->parts;
                    for (sts=0; sts < 4; ++sts, ++ppart)
                    {
                        _memcpy(ppart, bp->parts + sts, sizeof(DOSPartition)); /* clone and align table entry (not with builtin memcpy) */
                    }
                    ppart = ((FsysGSetHB *)hioq->pparam0)->parts;
                    for (sts=0; sts < 4; ++sts, ++ppart)
                    {
                        if (ppart->type == FSYS_USE_PARTITION_TABLE)
                        {
                            vol->hd_offset = ppart->abs_sect;
                            vol->hd_maxlba = ppart->abs_sect + ppart->num_sects;
                            vol->maxlba = ppart->num_sects - (ppart->num_sects%FSYS_MAX_ALTS); 
                            break;
                        }
                    }
                }
                if (sts >= 4)
                {
                    sts = FSYS_MOUNT_NOPARTS;       /* Didn't find a partition */
                }
                else
                {
                    sts = 0;                /* no errors */
                }
                break;                  /* we're done */
            }
        case PPART_DONE:
        default:
            break;
        }
        break;
    }
    if (sts && !uioq->iostatus) uioq->iostatus = sts;
    qio_freeioq(mioq);
    qio_complete(uioq);
    return;
}
#endif		/* FSYS_USE_PARTITION_TABLE */

#if FSYS_INCLUDE_HB_READER
    #define HBRDR_BEGIN		(0)
    #define HBRDR_READ		(1)
    #define HBRDR_PARSE		(2)
    #define HBRDR_CLOSE		(3)
    #define HBRDR_DONE		(4)

static void hb_rdr(QioIOQ *mioq)
{
    FsysVolume *vol;
    FsysGSetHB *hb;
    QioIOQ *uioq;
    int32_t sts, max;

    uioq = (QioIOQ *)mioq->user;
    hb = (FsysGSetHB *)uioq->pparam0;
    vol = (FsysVolume *)uioq->pparam1;
    sts = mioq->iostatus;
    mioq->iostatus = 0;
    while (1)
    {
        switch (qio_cvtFromPtr(mioq->user2))
        {
        case HBRDR_BEGIN: {
                if (QIO_ERR_CODE(sts) && sts != FSYS_MOUNT_NOPARTS)
                {
                    uioq->iostatus = sts;
                    mioq->user2 = qio_cvtToPtr(HBRDR_CLOSE);
                }
                else
                {
                    max = vol->maxlba;
                    if (max > FSYS_HB_RANGE) max = FSYS_HB_RANGE;
                    for (sts=0; sts < FSYS_MAX_ALTS; ++sts)
                    {
                        U32 sector;
                        sector = FSYS_HB_ALG(sts, max);
                        hb->rel_lba[sts] = sector;
                        hb->abs_lba[sts] = sector + vol->hd_offset;
                    }
                    mioq->user2 = qio_cvtToPtr(HBRDR_READ);
                }
                continue;
            }
        case HBRDR_READ: {
                mioq->user2 = (void *)HBRDR_PARSE;
                sts = qio_readwpos(mioq, hb->abs_lba[hb->copy], &hb->hbu.homeblk, BYTPSECT);
                if (sts)
                {
                    uioq->iostatus = sts;
                    mioq->user2 = qio_cvtToPtr(HBRDR_CLOSE);
                    continue;
                }
                return;
            }
        case HBRDR_PARSE: {
                U32 *ulp, cs;
                int32_t next = HBRDR_CLOSE;     /* assume to close next */
                ulp = (U32 *)&hb->hbu.homeblk;
                cs = 0;
                for (max=0; max < BYTPSECT/4; ++max) cs += *ulp++;
                if (cs || (hb->hbu.homeblk.id != FSYS_ID_HOME) || QIO_ERR_CODE(sts))
                {
                    if (++hb->copy >= FSYS_MAX_ALTS)
                    {
                        uioq->iostatus = QIO_ERR_CODE(sts) ? sts : FSYS_MOUNT_NOHBLK;
                    }
                    else
                    {
                        next = HBRDR_READ;
                    }
                }
                mioq->user2 = qio_cvtToPtr(next);
                continue;
            }
        case HBRDR_CLOSE:
            mioq->user2 = qio_cvtToPtr(HBRDR_DONE);
        case HBRDR_DONE:
#if FSYS_USE_PARTITION_TABLE
            hb->hd_offset = vol->hd_offset;
            hb->hd_maxlba = vol->hd_maxlba;
#else
            hb->hd_maxlba = vol->maxlba;
#endif
        default:
            break;
        }
        break;
    }
    if (!uioq->iostatus) uioq->iostatus = sts;
    qio_freeioq(mioq);
    qio_complete(uioq);
    return;
}

int fsys_gethomeblk(QioIOQ *ioq, const char *virt, FsysGSetHB *home)
{
    FsysVolume *vol;
    const QioDevice *d;
    QioIOQ *fioq;
    QioFile *fp;

    if (!ioq) return QIO_INVARG;    /* invalid parameter */
    ioq->iocount = 0;           /* always starts out 0 */
    if ( !virt || !home || home->copy >= FSYS_MAX_ALTS)
    {
        return(ioq->iostatus = QIO_INVARG);    /* invalid parameter */
    }
    fp = qio_fd2file(ioq->file);
    if (!fp)
    {
        return(ioq->iostatus = QIO_NOTOPEN);
    }
    if (prc_get_astlvl() < 0) ide_init();   /* make sure drive is init'd */
    d = qio_lookupdvc(virt);            /* see if our mount point is installed */
    if (!d) return(ioq->iostatus = FSYS_MOUNT_NSV);    /* no such volume */
    vol = (FsysVolume *)d->private;     /* get ptr to our volume */
    if (!vol) return(ioq->iostatus = FSYS_MOUNT_FATAL);
    fioq = qio_getioq();
    if (!fioq) return(ioq->iostatus = QIO_NOIOQ);
    ioq->pparam0 = home;
    ioq->pparam1 = vol;
    ioq->iostatus = 0;              /* so far so good */
    fioq->file = ioq->file;
    fioq->user = ioq;
    fioq->user2 = (void *)HBRDR_BEGIN;
    fioq->complete = hb_rdr;
    if (!vol->maxlba) vol->maxlba = fp->size;   /* record max lba in case no partition table */
#if FSYS_USE_PARTITION_TABLE
    if (!vol->hd_offset || !vol->hd_maxlba)       /* if we don't already have this */
    {
        QioIOQ *newioq;

        newioq = qio_getioq();
        if (!newioq)
        {
            qio_freeioq(fioq);
            return(ioq->iostatus = QIO_NOIOQ);
        }
        newioq->user = fioq;
        newioq->user2 = 0;
        newioq->file = ioq->file;
        newioq->complete = parse_part;
        newioq->aq.action = parse_part;
        newioq->aq.param = newioq;
        prc_q_ast(QIO_ASTLVL, &newioq->aq);     /* go read the partition table */
        return 0;
    }
#endif			/* FSYS_USE_PARTITION_TABLE */
    fioq->aq.action = hb_rdr;
    fioq->aq.param = fioq;
    prc_q_ast(QIO_ASTLVL, &fioq->aq);       /* go read the partition table */
    return 0;
}
#endif			/* FSYS_INCLUDE_HB_READER */

static void mcom1(QioIOQ *mioq)
{
    FsysVolume *vol;
    int sts;
    QioIOQ *uioq;

    uioq = (QioIOQ *)mioq->user;
    vol = (FsysVolume *)uioq->private2;
    sts = mioq->iostatus;
    mioq->iostatus = 0;
    if (QIO_ERR_CODE(sts))
    {
        mcom1_clean(sts, mioq);
        return;
    }
    while (1)
    {
        switch (qio_cvtFromPtr(mioq->user2))
        {
        case MCOM1_START:
            vol->iofd = mioq->file;     /* remember the FD we're to use */
            if (vol->hd_offset || vol->hd_maxlba) /* if we've already set this up */
            {
                int maxlba;
                maxlba = vol->hd_maxlba - vol->hd_offset;
                vol->maxlba = maxlba - (maxlba%FSYS_MAX_ALTS); 
                mioq->user2 = (void *)MCOM1_START_MOUNT;
                continue;
            }
#if FSYS_USE_PARTITION_TABLE
            mioq->user2 = (void *)MCOM1_PARSE_PARTTBL;
            sts = qio_readwpos(mioq, 0, vol->filep, BYTPSECT);  /* read the partition table */
            if (!sts) return;
#endif
            /* Fall through to MCOM1_DO_FSTAT */
        case MCOM1_DO_FSTAT:
            mioq->user2 = (void *)MCOM1_FSTAT_DONE;
            sts = qio_fstat(mioq, (struct stat *)mioq->private);
            if (sts)
            {
                mcom1_clean(sts, mioq);
            }
            return;
#if FSYS_USE_PARTITION_TABLE
        case MCOM1_PARSE_PARTTBL: {
/*
 * This is a bit hokey, but the paradigm is if there is a partition table, then
 * the first table entry that matches is mounted unless already setup.
 */
                DOSBootSect *bp = (DOSBootSect *)vol->filep;
                DOSPartition lclpart, *lp = &lclpart;
                int pt = vol-volumes;

                mioq->user2 = (void *)MCOM1_DO_FSTAT;       /* assume this won't work */
                if (bp->end_sig == 0xAA55)            /* if legit partition table */
                {
                    for (sts=0; sts < 4; ++sts)
                    {
                        _memcpy(lp, bp->parts + sts, sizeof(DOSPartition)); /* clone and align table entry (not with builtin memcpy) */
                        if (lp->type == FSYS_USE_PARTITION_TABLE) /* partition one of ours? */
                        {
                            if (--pt > 0)             /* Yep. Is it the one we wanted? */
                            {
                                continue;               /* Nope, keep looking */
                            }
                            vol->hd_offset = lp->abs_sect;
                            vol->hd_maxlba = lp->abs_sect + lp->num_sects;
                            vol->maxlba = lp->num_sects - (lp->num_sects%FSYS_MAX_ALTS); 
                            mioq->user2 = (void *)MCOM1_START_MOUNT;
                            break;
                        }
                    }
                }
                continue;                   /* back to switch() to see what to do next */
            }
#endif
        case MCOM1_FSTAT_DONE: {
                U32 lim;
                struct stat *fs;
                fs = (struct stat *)mioq->private;
                lim = fs->st_size;
#if defined(FSYS_MAX_LBA) && FSYS_MAX_LBA
                if (FSYS_MAX_LBA < lim) lim = FSYS_MAX_LBA;
#endif
                lim = lim - (lim%FSYS_MAX_ALTS); /* make total a multiple of MAX_ALTS */
                vol->maxlba = lim;
                /*  Fall through to MCOM1_START_MOUNT */
            }
        case MCOM1_START_MOUNT: {
                QioIOQ *fioq;
                mioq->user2 = (void *)MCOM1_MOUNT_DONE;
                vol->reader.callers_ioq = mioq;
                fioq = &vol->reader.our_ioq;
                fioq->file = mioq->file;        /* fd to use for reading */
#if FSYS_HAS_TQ_INS
                mioq->timer.func = mcom1_updpcnt;   /* update the percentage in uioq->iocount */
                mioq->timer.vars = mioq;        /* point to us */
                mioq->timer.delta = 5000;       /* every 5 milliseconds */
                tq_ins(&mioq->timer);       /* launch the timer */
#endif			/* FSYS_HAS_TQ_INS */
                vol->hb_range = (vol->maxlba > FSYS_HB_RANGE) ? FSYS_HB_RANGE : (vol->maxlba&-256);
                fsys_qmount(fioq);          /* queue the mount */
                return;
            }
        case MCOM1_MOUNT_DONE:
            mcom1_clean(sts, mioq);     /* we're done */
            return;
        default:
            break;
        }
        break;
    }
    mcom1_clean(QIO_FATAL, mioq);       /* should never get here */
    return;
}

/*****************************************************************
 * fsys_mount() - mount a volume and wait for completion. 
 *
 * At entry:
 *	ioq - pointer to QioIOQ to use for async I/O. Only the 'complete'
 *		member need be set, all others remain unused.
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *	tmp - pointer to temporary memory (scratch work space)
 *	tmpsize - size of temp area in chars. Must be at least
 *		sizeof(struct stat).
 *
 * At exit:
 *	returns  0 if volume is successfully mounted.
 *	returns  FSYS_MOUNT_xxx if other errors.
 */

int fsys_mount(QioIOQ *ioq, const char *where, const char *what, U32 *tmp, int tmpsize)
{
    FsysVolume *vol;
    const QioDevice *d;
    QioIOQ *fioq;
    int32_t sts;

    if (!ioq) return QIO_INVARG;    /* invalid parameter */
    if ( !where || !what || !tmp || tmpsize < sizeof(struct stat) )
    {
        return(ioq->iostatus = QIO_INVARG);    /* invalid parameter */
    }
    if (prc_get_astlvl() < 0) ide_init(); /* make sure drive is init'd */
    d = qio_lookupdvc(what);        /* see if our mount point is installed */
    if (!d) return(ioq->iostatus = FSYS_MOUNT_NSV);    /* no such volume */
    vol = (FsysVolume *)d->private; /* get ptr to our volume */
    if (!vol) return(ioq->iostatus = FSYS_MOUNT_FATAL);
    if (vol->state) return(ioq->iostatus = FSYS_MOUNT_BUSY); /* volume is already mounting */
    if (vol->filesp) return(ioq->iostatus = FSYS_MOUNT_MOUNTED); /* volume is already mounted */
#if FSYS_READ_ONLY
    vol->flags |= FSYS_VOL_FLG_RO;  /* force this */
#endif
    fioq = qio_getioq();
    if (!fioq) return(ioq->iostatus = QIO_NOIOQ);
    vol->filep = QIO_ALIGN(vol->file_buff,QIO_CACHE_LINE_SIZE);	/* Align to cache line */
    fioq->private = (void *)tmp;
    ioq->private2 = vol;
    fioq->complete = mcom1;
    fioq->user = ioq;
    fioq->user2 = (void *)MCOM1_START;
    ioq->iocount = 0;           /* reset these */
    ioq->iostatus = 0;
    sts = qio_open(fioq, where, O_RDWR);
    if (sts)
    {
        ioq->iostatus = sts;
        qio_freeioq(fioq);
    }
    return sts;
}

static int mountwcb(const char *where, const char *what, void (*wait)(int))
{
    QioIOQ *ioq;
    int sts;
    struct stat lst;

    ioq = qio_getioq();
    sts = fsys_mount(ioq, where, what, (U32 *)&lst, sizeof(lst));
#if KICK_THE_DOG
    {
        int pcnt;
        pcnt = 0;
        while (!sts)
        {
            if (wait) wait(ioq->iocount);
            sts = ioq->iostatus;
            if (pcnt != ioq->iocount)
            {
                WDOG = 0;
                pcnt = ioq->iocount;
            }
        }
    }
#else
    while (!sts)
    {
        if (wait) wait(ioq->iocount);
        sts = ioq->iostatus;
    }
#endif
    qio_freeioq(ioq);           /* done with this */
    return sts;             /* return with whatever status was reported */
}

/*****************************************************************
 * fsys_mountwcb() - mount a volume and wait for completion. 
 *
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *	wait - pointer to function which will be called while waiting
 *		for mount to complete. The function will be passed
 *		a single parameter which will be a number from 0 to 99
 *		representing the percent of the mount that is complete.
 *
 * At exit:
 *	returns  0 if volume is successfully mounted.
 *	returns  FSYS_MOUNT_xxx if other errors.
 */

int fsys_mountwcb(const char *where, const char *what, void (*wait)(int))
{
#if defined(FSYS_SEARCH_FOR_DISK) && FSYS_SEARCH_FOR_DISK
    char tmp_where[64], *cp;
    int ii, sts;
    strncpy(tmp_where, where, sizeof(tmp_where-1));
    tmp_where[sizeof(tmp_where)-1] = 0;
    cp = tmp_where;
    if (*cp == QIO_FNAME_SEPARATOR) ++cp;
    while (*cp && *cp != QIO_FNAME_SEPARATOR) ++cp;
    --cp;
    ii = *cp - '0';
    if (ii >= 0 && ii < NUM_HDRIVES)
    {
        for (; ii < NUM_HDRIVES; ++ii)
        {
            *cp = '0' + ii;
            sts = mountwcb(tmp_where, what, wait);
            if (!QIO_ERR_CODE(sts) || sts == FSYS_MOUNT_MOUNTED) return sts;
        }
        return sts;
    }
#endif
    return mountwcb(where, what, wait);
}

/*****************************************************************
 * fsys_mountw() - mount a volume and wait for completion. 
 *
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *
 * At exit:
 *	returns  0 if volume is successfully mounted.
 *	returns  FSYS_MOUNT_xxx if other errors.
 */

int fsys_mountw(const char *where, const char *what)
{
    return mountwcb(where, what, 0);
}

#if !FSYS_READ_ONLY 
static void mk_freelist(FsysRetPtr *ilist, int nelts, int size, uint32_t range)
{
    int ii;
    U32 prev;
    FsysRetPtr *list = ilist;

    memset((char *)list, 0, nelts*sizeof(FsysRetPtr));
    prev = 1;               /* start allocating free blocks at 1 */
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        U32 amt, hb;

        hb = FSYS_HB_ALG(ii, range);
        amt = hb - prev;
        FSYS_SQK(( OUTWHERE "mk_freelist: hb=%ld, amt=%ld, prev=%ld, range=%ld.\n",
                   hb, amt, prev, range));
        if (amt)
        {
            list->start = prev;
            list->nblocks = amt;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
            list->repeat = 1;
            list->skip = 0;
#endif
            ++list;
        }
        prev = hb+1;
    }
    if (prev < size)
    {
        list->start = prev;
        list->nblocks = size-prev;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
        list->repeat = 1;
        list->skip = 0;
#endif
        ++list;
    }
#if FSYS_SQUAWKING
    FSYS_SQK(( OUTWHERE "Dump of freelist:\n"));
    for (; ilist < list; ++ilist)
    {
        FSYS_SQK(( OUTWHERE "\tstart=%ld, nblock=%ld\n", ilist->start, ilist->nblocks));
    }
#endif
    return;
}

static int fsys_mkalloc( FsysFindFreeT *freet )
{
    int ii, alloc;

    alloc = freet->request;
    for (ii=0; ii < FSYS_MAX_FHPTRS; ++ii)
    {
        if (fsys_findfree( freet )) break; /* no more free space */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
#error You need to rewrite fsys_mkalloc and add repeat/skip support.
#endif
        alloc -= freet->reply->nblocks;
        if (alloc <= 0) break;
        freet->reply += 1;
    }
    return alloc;
}

static void fsys_mkheader(FsysHeader *hdr, int gen)
{
    memset((char *)hdr, 0, sizeof(FsysHeader));
    hdr->id = FSYS_ID_HEADER;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME) && defined(HAVE_TIME) && HAVE_TIME
    hdr->ctime = hdr->mtime = (uint32_t)time(0);
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    hdr->atime = hdr->ctime;
    hdr->btime = 0;
#endif
    hdr->size = 0;          /* file size in bytes */
    hdr->type = FSYS_TYPE_FILE;     /* assume plain file */
    hdr->flags = 0;         /* not used, start with 0 */
    hdr->generation = gen;
}

enum init_e
{
    INIT_BEGIN,
    INIT_MKFREE,
    INIT_WRITE_INDEX_FH,
    INIT_WRITE_INDEX,
    INIT_WRITE_FREE_FH,
    INIT_WRITE_FREE,
    INIT_WRITE_ROOT_FH,
    INIT_WRITE_ROOT,
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
    INIT_WRITE_JOURNAL_FH,
    INIT_WRITE_JOURNAL,
#endif
    INIT_WRITE_HOME,
    INIT_NEXT_ALT
};

typedef struct
{
    FsysQio q;          /* !!! this absolutely needs to be the first member !!! */
    FsysInitVol iv;
    struct act_q act;
    enum init_e state;
    int substate;
    int status;
    int base_lba;       /* lba offset in case of partition table */
    int num_lba;        /* num of lba's if partition table present */
#if FSYS_USE_PARTITION_TABLE
    DOSBootSect bootsect;   /* place to deposit boot sector */
#endif    
    FsysHomeBlock *hb;
    FsysHeader ihdr, fhdr, rhdr;
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
    FsysHeader jhdr;
    U8 blanks[BYTPSECT];
    uint32_t jlbas[FSYS_MAX_ALTS];
#endif
    FsysRetPtr *fake_free;
    FsysRetPtr freep, tmprp;
    FsysRamRP ramrp;
    uint32_t *dir;
    uint32_t *index;
    uint32_t ilbas[FSYS_MAX_ALTS];
    int free_nelts;
} FsysFinit;

static void compute_home_blocks(FsysFinit *fin)
{
    FsysHomeBlock *hb;
    FsysInitVol *iv;
    int ii;
    uint32_t sa, *tmp;

    hb = fin->hb;
    iv = &fin->iv;
    hb->id = FSYS_ID_HOME;
    hb->hb_minor = FSYS_VERSION_HB_MINOR;
    hb->hb_major = FSYS_VERSION_HB_MAJOR;
    hb->hb_size = sizeof(FsysHomeBlock);
    hb->fh_minor = FSYS_VERSION_FH_MINOR;
    hb->fh_major = FSYS_VERSION_FH_MAJOR;
    hb->fh_size = sizeof(FsysHeader);
    hb->fh_ptrs = FSYS_MAX_FHPTRS;
#if (FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
    hb->efh_minor = FSYS_VERSION_EFH_MINOR;
    hb->efh_major = FSYS_VERSION_EFH_MAJOR;
    hb->efh_size = sizeof(FsysEHeader);
    hb->efh_ptrs = FSYS_MAX_EFHPTRS;
#else
    hb->efh_minor = 0;
    hb->efh_major = 0;
    hb->efh_size = 0;
    hb->efh_ptrs = 0;
#endif
    hb->rp_minor = FSYS_VERSION_RP_MINOR;
    hb->rp_major = FSYS_VERSION_RP_MAJOR;
    hb->rp_size = sizeof(FsysRetPtr);
    hb->cluster = FSYS_CLUSTER_SIZE;
    hb->maxalts = FSYS_MAX_ALTS;
#if defined(HAVE_TIME) && HAVE_TIME
    hb->atime = hb->ctime = hb->mtime = (uint32_t)time(0);
#endif
    hb->btime = 0;
    hb->options = FSYS_OPTIONS;
    hb->features = FSYS_FEATURES;
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii)
    {
        hb->index[ii] = fin->ilbas[ii];
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
        hb->journal[ii] = fin->jlbas[ii];
#endif
    }
    hb->max_lba = iv->max_lba;
    hb->hb_range = iv->hb_range;
    hb->def_extend = iv->def_extend;
    for (tmp=(uint32_t *)hb, sa=0, ii=0; ii < BYTPSECT/4; ++ii) sa += *tmp++;
    hb->chksum = 0-sa;
    return;
}

static void init_file_system(QioIOQ *ioq)
{
    int ii, bcnt, sts=0;
    FsysFinit *fin;
    FsysInitVol *iv;
    uint32_t *indxp;

    fin = (FsysFinit *)ioq;
    iv = &fin->iv;
    while (1)
    {
        switch (fin->state)
        {
        case INIT_BEGIN: {
                QioIOQ *eioq;
                eioq = fin->q.callers_ioq;
                fin->fake_free = (FsysRetPtr *)QIOmalloc(iv->free_sectors*BYTPSECT);
                if (!fin->fake_free)
                {
                    sts = FSYS_INITFS_NOMEM;
                    initfs_error:
                    eioq->iostatus = fin->status = sts;
                    return;
                }
                fin->free_nelts = (iv->free_sectors*BYTPSECT)/sizeof(FsysRetPtr);
                fin->index = (uint32_t *)QIOcalloc(iv->index_sectors*BYTPSECT, 1);     
                if (!fin->index)
                {
                    QIOfree(fin->fake_free);
                    sts = FSYS_INITFS_NOMEM;
                    goto initfs_error;
                }
                fin->dir = (uint32_t *)QIOcalloc(iv->root_sectors*BYTPSECT, 1);
                if (!fin->dir)
                {
                    QIOfree(fin->fake_free);
                    QIOfree(fin->index);
                    sts = FSYS_INITFS_NOMEM;
                    goto initfs_error;
                }
                fin->hb = (FsysHomeBlock *)QIOcalloc(BYTPSECT, 1);
                if (!fin->hb)
                {
                    sts = FSYS_INITFS_NOMEM;
                    QIOfree(fin->fake_free);
                    QIOfree(fin->index);
                    QIOfree(fin->dir);
                    goto initfs_error;
                }
                fin->state = INIT_MKFREE;
                fin->act.action = (void (*)(void *))init_file_system;
                fin->act.param = (void *)ioq;
                prc_q_ast(QIO_ASTLVL, &fin->act);   /* jump to AST level */
                QIO_FIN_SHIM();
                return;
            }

        case INIT_MKFREE: {
                FsysFindFreeT freet;
                unsigned char *s;

                /* First create a free list that accounts for the holes caused by home blocks */
                mk_freelist(fin->fake_free, fin->free_nelts, iv->max_lba, iv->hb_range);    /* assume a n block filesystem */

                /* start an index.sys file */
                fsys_mkheader(&fin->ihdr, 1);   /* make an index file header */
                ii = 3;
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
                ii += (iv->journal_sectors ? 1 : 0);
#endif
                fin->ihdr.size = ii*FSYS_MAX_ALTS*sizeof(int32_t);
                fin->ihdr.id = FSYS_ID_INDEX;
                fin->ihdr.type = FSYS_TYPE_INDEX;
                fsys_mkheader(&fin->fhdr, 1);   /* make a freemap file header */
                fsys_mkheader(&fin->rhdr, 1);   /* make a root file header */
                fin->rhdr.type = FSYS_TYPE_DIR;
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
                if (iv->journal_sectors)
                {
                    fsys_mkheader(&fin->jhdr, 1);   /* make a journal file header */
                }
#endif
                freet.skip = 0;
                freet.exact = 0;
                freet.actelts = FSYS_MAX_ALTS;
                freet.totelts = fin->free_nelts;                
                freet.freelist = fin->fake_free;
                freet.hint = 0;

                for (sts=bcnt=0; bcnt < FSYS_MAX_ALTS; ++bcnt)
                {
                    /* get a free block for the index.sys file header */

                    freet.lo_limit = FSYS_COPY_ALG(bcnt, iv->max_lba);
                    freet.request = 1;
                    freet.reply = &fin->freep;
                    if (fsys_findfree( &freet )) /* get a free block */
                    {
                        no_free:
                        sts = FSYS_INITFS_NOFREE;
                        break;
                    }
                    /* point index to itself */
                    indxp = fin->index+(FSYS_INDEX_INDEX*FSYS_MAX_ALTS);
                    indxp[bcnt] = fin->freep.start;

                    /* tell home block where index.sys's file header is */
                    fin->ilbas[bcnt] = fin->freep.start;

                    /* allocate some blocks for the index file itself */
                    fin->ihdr.clusters = freet.request = iv->index_sectors;
                    freet.reply = fin->ihdr.pointers[bcnt];
                    if (fsys_mkalloc( &freet )) goto no_free;

                    freet.request = 1;
                    freet.reply = &fin->freep;
                    /* get a free block for the freelist.sys file header */
                    if (fsys_findfree( &freet )) goto no_free;

                    /* point index file to freelist.sys's file header */
                    indxp = fin->index+(FSYS_INDEX_FREE*FSYS_MAX_ALTS);
                    indxp[bcnt] = fin->freep.start;

                    /* allocate some blocks for the freelist file itself */
                    fin->fhdr.clusters = freet.request = iv->free_sectors;
                    freet.reply = fin->fhdr.pointers[bcnt];
                    if (fsys_mkalloc( &freet )) goto no_free;

                    freet.request = 1;
                    freet.reply = &fin->freep;
                    /* get a free block for the root directory file header */
                    if (fsys_findfree( &freet )) goto no_free;

                    /* point index file to root directory file */
                    indxp = fin->index+(FSYS_INDEX_ROOT*FSYS_MAX_ALTS);
                    indxp[bcnt] = fin->freep.start;
#if (FSYS_FEATURES&FSYS_FEATURES_DIRLBA)
                    indxp[bcnt] |= FSYS_DIRLBA_BIT;
#endif

                    /* allocate some blocks for the root file itself */
                    fin->rhdr.clusters = freet.request = iv->root_sectors;
                    freet.reply = fin->rhdr.pointers[bcnt];
                    if (fsys_mkalloc( &freet )) goto no_free;

#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
                    if (iv->journal_sectors)
                    {
                        freet.request = 1;
                        freet.reply = &fin->freep;
                        /* get a free block for the journal file header */
                        if (fsys_findfree( &freet )) goto no_free;

                        /* point index file to journal file header */
                        indxp = fin->index+(FSYS_INDEX_JOURNAL*FSYS_MAX_ALTS);
                        indxp[bcnt] = fin->freep.start;

                        /* tell home block where journal file header is */
                        fin->jlbas[bcnt] = fin->freep.start;

                        if (!bcnt)            /* only 1 copy of journal file necessary */
                        {
                            /* allocate some blocks for the journal file itself */
                            fin->jhdr.clusters = freet.request = iv->journal_sectors;
                            fin->jhdr.size = iv->journal_sectors*BYTPSECT;
                            freet.reply = fin->jhdr.pointers[bcnt];
                            if (fsys_mkalloc( &freet )) goto no_free;
                        }
                    }
#endif
                }

                if (sts) break;

                compute_home_blocks(fin);

                s = (unsigned char *)fin->dir;
                *s++ = FSYS_INDEX_ROOT;     /* ".." points to parent */
                *s++ = FSYS_INDEX_ROOT>>8;
                *s++ = FSYS_INDEX_ROOT>>16;
                *s++ = 1;           /* parent's generation number starts a 1 */
                *s++ = 3;           /* string length (3) */
                *s++ = '.';         /* filename (..) */
                *s++ = '.';
                *s++ = 0;           /* null terminate the string */
                *s++ = FSYS_INDEX_ROOT;     /* "." points to ourself */
                *s++ = FSYS_INDEX_ROOT>>8;
                *s++ = FSYS_INDEX_ROOT>>16;
                *s++ = 1;           /* owner's generation number starts at 1 */
                *s++ = 2;           /* string length (2) */
                *s++ = '.';         /* filename */
                *s++ = 0;           /* null terminate the string */
                *s++ = 0;           /* index of 0 means end of list */
                *s++ = 0;
                *s++ = 0;
                fin->rhdr.size = s - (U8*)fin->dir;

                for (ii=0; ii < fin->free_nelts; ++ii)
                {
                    FSYS_SQK((OUTWHERE "Fake free %d: start=%08lX, nblocks=%08lX\n",
                              ii, fin->fake_free[ii].start, fin->fake_free[ii].nblocks));
                    if (fin->fake_free[ii].start == 0 && fin->fake_free[ii].nblocks == 0) break;
                }
                fin->fhdr.size = (ii*sizeof(FsysRetPtr)+(BYTPSECT-1))&-BYTPSECT;
                fin->substate = 0;
                fin->tmprp.start = fin->base_lba;   /* whole disk starts at sector 0 */
                fin->tmprp.nblocks = fin->num_lba ? fin->num_lba : iv->max_lba; /* the whole damn disk is one file */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
                fin->tmprp.repeat = 1;
                fin->tmprp.skip = 0;
#endif
                fin->ramrp.rptrs = &fin->tmprp;
                fin->ramrp.num_rptrs = 1;
                fin->ramrp.next = 0;
                fin->ramrp.rptrs_size = 0;
                fin->q.ramrp = &fin->ramrp;
                fin->q.complt = init_file_system;
                fin->state = INIT_WRITE_HOME;
                continue;
            }

        case INIT_WRITE_HOME:
            /* write a home block */
            fin->ramrp.rptrs = &fin->tmprp;
            fin->q.buff = (U8*)fin->hb;
            fin->q.sector = FSYS_HB_ALG(fin->substate, iv->hb_range);
            fin->q.u_len = BYTPSECT;
            fin->state = INIT_WRITE_INDEX_FH;
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_INDEX_FH:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADHB;
                break;
            }
            /* write the file header for index.sys */
            indxp = fin->index+(FSYS_INDEX_INDEX*FSYS_MAX_ALTS);
            fin->q.buff = (U8*)&fin->ihdr;
            fin->q.sector = indxp[fin->substate] & FSYS_LBA_MASK;
            fin->q.u_len = BYTPSECT;
            fin->state = INIT_WRITE_INDEX;
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_INDEX:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADINDX;
                break;
            }
            /* write the index.sys file */
            fin->q.buff = (U8*)fin->index;
            fin->q.u_len = iv->index_sectors*BYTPSECT;
            fin->state = INIT_WRITE_FREE_FH;
#if 0
            fin->ramrp.rptrs = fin->ihdr.pointers[fin->substate];
            fin->q.sector = 0;
#else
            fin->q.sector = fin->ihdr.pointers[fin->substate][0].start;
            if (fin->q.u_len > (fin->ihdr.pointers[fin->substate][0].nblocks<<9))
            {
                fin->status = FSYS_INITFS_FATAL;
                break;
            }
#endif
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_FREE_FH:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADINDXF;
                break;
            }
            /* write the file header for freelist.sys */
            fin->ramrp.rptrs = &fin->tmprp;
            indxp = fin->index+(FSYS_INDEX_FREE*FSYS_MAX_ALTS);
            fin->q.buff = (U8*)&fin->fhdr;
            fin->q.sector = indxp[fin->substate] & FSYS_LBA_MASK;
            fin->q.u_len = BYTPSECT;
            fin->state = INIT_WRITE_FREE;
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_FREE:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADFREE;
                break;
            }
            /* write the freelist.sys file */
            fin->q.buff = (U8*)fin->fake_free;
            fin->q.u_len = iv->free_sectors*BYTPSECT;
            fin->state = INIT_WRITE_ROOT_FH;
#if 0
            fin->ramrp.rptrs = fin->fhdr.pointers[fin->substate];
            fin->q.sector = 0;
#else
            fin->q.sector = fin->fhdr.pointers[fin->substate][0].start;
            if (fin->q.u_len > (fin->fhdr.pointers[fin->substate][0].nblocks<<9))
            {
                fin->status = FSYS_INITFS_FATAL;
                break;
            }
#endif
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_ROOT_FH:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADFREEF;
                break;
            }
            /* write the file header for root directory */
            fin->ramrp.rptrs = &fin->tmprp;
            indxp = fin->index+(FSYS_INDEX_ROOT*FSYS_MAX_ALTS);
            fin->q.buff = (U8*)&fin->rhdr;
            fin->q.sector = indxp[fin->substate] & FSYS_LBA_MASK;
            fin->q.u_len = BYTPSECT;
            fin->state = INIT_WRITE_ROOT;
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_ROOT:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADROOT;
                break;
            }
            /* write the root directory file */
            fin->q.buff = (U8*)fin->dir;
            fin->q.u_len = iv->root_sectors*BYTPSECT;
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
            fin->state = INIT_WRITE_JOURNAL_FH;
#else
            fin->state = INIT_NEXT_ALT;
#endif
            fin->q.sector = fin->rhdr.pointers[fin->substate][0].start;
            if (fin->q.u_len > (fin->rhdr.pointers[fin->substate][0].nblocks<<9))
            {
                fin->status = FSYS_INITFS_FATAL;
                break;
            }
            fsys_qwrite(ioq);
            return;

#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
        case INIT_WRITE_JOURNAL_FH:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADINDXF;
                break;
            }
            /* write the file header for journal file */
            fin->ramrp.rptrs = &fin->tmprp;
            indxp = fin->index+(FSYS_INDEX_JOURNAL*FSYS_MAX_ALTS);
            fin->q.buff = (U8*)&fin->jhdr;
            fin->q.sector = indxp[fin->substate] & FSYS_LBA_MASK;
            fin->q.u_len = BYTPSECT;
            fin->state = INIT_WRITE_JOURNAL;
            fsys_qwrite(ioq);
            return;

        case INIT_WRITE_JOURNAL:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADFREE;
                break;
            }
            fin->state = INIT_NEXT_ALT;
            if ((fin->q.sector = fin->jhdr.pointers[fin->substate][0].start))
            {
                /* write a blank journal file */
                fin->q.buff = fin->blanks;
                memset(fin->blanks, 0, sizeof(fin->blanks));
                fin->q.u_len = sizeof(fin->blanks);
                if (fin->q.u_len > (fin->jhdr.pointers[fin->substate][0].nblocks<<9))
                {
                    fin->status = FSYS_INITFS_FATAL;
                    break;
                }
                fsys_qwrite(ioq);
                return;
            }
            continue;
#endif

        case INIT_NEXT_ALT:
            if (QIO_ERR_CODE(ioq->iostatus))
            {
                fin->status = FSYS_INITFS_BADROOTF;
                break;
            }
            fin->state = INIT_WRITE_HOME;
            ++fin->substate;
            if (fin->substate < FSYS_MAX_ALTS) continue;
            sts = FSYS_INITFS_SUCC|SEVERITY_INFO;   /* normal success */
            break;

        default:
            sts = FSYS_INITFS_FATAL;
            break;
        }           /* -- switch state */
        break;
    }               /* -- while (1) */
    QIOfree(fin->fake_free);
    QIOfree(fin->index);
    QIOfree(fin->dir);
    QIOfree(fin->hb);
    fin->status = sts;
    fin->state = 0;
    ioq = fin->q.callers_ioq;
    ioq->iostatus = sts;
    qio_complete(ioq);
    return;
}
#endif					/* !FSYS_READ_ONLY && !FSYS_USE_PARTITION_TABLE */

#if !FSYS_READ_ONLY
/* rename file named 'name1' to 'name2' */

static void rename_q(QioIOQ *ioq)
{
    FsysLookUpFileT src, dst;
    FsysVolume *vol;
    int sts, flen;
    const char *fname;

    vol = (FsysVolume *)ioq->private;
    dst.vol = src.vol = vol;        /* point to volume */
    dst.top = src.top = 0;      /* start at the beginning */
    src.path = ioq->pparam0;        /* source filename */
    dst.path = ioq->pparam1;        /* destination filename */
    do
    {
        FsysDirEnt *dp;
        char *s;

        sts = lookup_filename(&src);    /* lookup source file */
        if ( sts != (FSYS_LOOKUP_SUCC|SEVERITY_INFO))
        {
            ioq->iostatus = sts;    /* no source */
            break;
        }
        sts = lookup_filename(&dst);    /* lookup destination file */
        if (sts == (FSYS_LOOKUP_SUCC|SEVERITY_INFO))
        {
            ioq->iostatus = FSYS_CREATE_NAMEINUSE;  /* output name is already in use */
            break;          /* and die */
        }
        if (sts != FSYS_LOOKUP_FNF)   /* the only legal value is FNF */
        {
            ioq->iostatus = sts;
            break;
        }
        fname = dst.fname;      /* get destination filename */
        flen = strlen(fname);       /* compute its length */
        dp = (FsysDirEnt *)QMOUNT_ALLOC(vol, sizeof(FsysDirEnt)+flen+1); /* get mem for directory entry + name */
        if (!dp)
        {
            ioq->iostatus = QIO_NOMEM;  /* sorry, no more memory */
            break;
        }
        s = (char *)(dp+1);     /* point to place to put string */
        strcpy(s, fname);       /* copy filename string */
        dp->name = s;           /* assign output filename to directory entry */
        dp->gen_fid = src.dir->gen_fid; /* dst gets FID and generation number of source */
        dp->next = 0;
/* !!Memory leak here!!! Old directory entries are not reclaimed */
        remove_ent(src.owner->directory, src.fname);    /* zap the source name */
        insert_ent(dst.owner->directory, dp);       /* install the destination name */
        add_to_dirty(vol, fsys_find_id(vol, src.owner), 0); /* put old directory on the dirty list */
        add_to_dirty(vol, fsys_find_id(vol, dst.owner), 0); /* put new directory on the dirty list */
    } while (0);
    ioq->private = 0;
    qio_freefile(qio_fd2file(ioq->file));
    ioq->file = -1;
    if (!ioq->iostatus) ioq->iostatus = FSYS_CREATE_SUCC|SEVERITY_INFO;
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_rename( QioIOQ *ioq, const char *source, const char *dest )
{
    FsysVolume *vol;
    QioFile *file;

    if ( !source || *source == 0 ) return(ioq->iostatus = QIO_INVARG);
    if ( !dest || *dest == 0 ) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);      /* get pointer to file */
    vol = (FsysVolume *)file->dvc->private; /* point to our mounted volume */
    if (!vol) return(ioq->iostatus = FSYS_LOOKUP_FNF);
    if (!vol->filesp) (ioq->iostatus = FSYS_LOOKUP_FNF);
    ioq->pparam0 = (void *)source;
    ioq->pparam1 = (void *)dest;
    ioq->private = (void *)vol;
    ioq->iostatus = 0;
    return qio_getmutex(&vol->mutex, rename_q, ioq);
}
#endif			/* !FSYS_READ_ONLY */

static void lseek_q(QioIOQ *ioq)
{
    QioFile *fp;
    off_t where, new=0;
    int whence;

    fp = qio_fd2file(ioq->file);
    if ((fp->flags&(QIO_FFLAG_CANCEL|QIO_FFLAG_CLOSING)))
    {
        ioq->iostatus = QIO_CANCELLED;
        goto seek_err;
    }
    where = ioq->iparam0;
    whence = ioq->iparam1;
    switch (whence)
    {
    case SEEK_SET:
        new = where;
        break;

    case SEEK_END:
        new = fp->size + where;
        break;

    case SEEK_CUR:
        new = (fp->sect*BYTPSECT) + fp->bws + where;
        break;
    default:
        ioq->iostatus = QIO_INVARG;
        goto seek_err;
    }
    if ((new < fp->size && (new & (FSYS_CLUSTER_SIZE*BYTPSECT-1))) || (new > fp->size))
    {
        ioq->iostatus = FSYS_IO_INVSEEK;    /* invalid seek */
        if ( (fp->mode&_FWRITE) )
        {
            FSYS_WR_SQK((OUTWHERE "fsys:lseek_q(). Invalid seek. size=0x%lX, new=0x%lX.\n", fp->size, new ));
        }
    }
    else
    {
        if ( (fp->mode&_FWRITE) )
        {
            FSYS_WR_SQK((OUTWHERE "fsys:lseek_q(). size=0x%lX, old: sect=0x%lX, bws=0x%X. new:sect=0x%lX, bws=0x%lX\n",
                         fp->size, fp->sect, fp->bws, new/BYTPSECT, new&(BYTPSECT-1) ));
        }
        fp->sect = new/BYTPSECT;            /* set the new position */
        fp->bws = new&(BYTPSECT-1);
        ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    }
    ioq->iocount = (fp->sect*BYTPSECT) + fp->bws;
    seek_err:
    qio_freemutex(&fp->mutex, ioq);
    qio_complete(ioq);
    return;
}

/* fsys_lseek - is called directly as a result of the user calling qio_lseek()
 * To protect the sect/bws fields of the file, this function has to be serialized
 * with other calls to lseek and read/write. This is done by loading the 
 * parameters into the IOQ, waiting on the mutex for the file then calling
 * lseek_q().
 */

static int fsys_lseek( QioIOQ *ioq, off_t where, int whence )
{
    QioFile *fp;
    int sts;

    if (!ioq) return QIO_INVARG;

    ioq->iparam0 = where;
    ioq->iparam1 = whence;
    fp = qio_fd2file(ioq->file);
    sts = qio_getmutex(&fp->mutex, lseek_q, ioq);
    return sts;
}

static void fsys_read_done( QioIOQ *ioq )
{
    FsysQio *q;
    QioIOQ *hisioq;
    QioFile *fp;
    FsysVolume *v;
    FsysRamFH *rfh;
    uint32_t fid;
    U32 npos;

    q = (FsysQio *)ioq;
    hisioq = q->callers_ioq;
    hisioq->iocount += q->total;        /* record total bytes xferred so far */
    fp = qio_fd2file(hisioq->file);
    if (!(fp->flags & QIO_FFLAG_CANCEL))
    {
        v = (FsysVolume *)fp->dvc->private;
        fid = qio_cvtFromPtr(fp->private);
        rfh = fsys_find_ramfh(v, (fid&FSYS_DIR_FIDMASK));
        if (QIO_ERR_CODE(ioq->iostatus) && ioq->iostatus != QIO_EOF && !(fp->mode&FSYS_OPNCPY_M))
        {
            ++q->o_which;
            if (q->o_which < FSYS_MAX_ALTS)   /* if we haven't tried all reads */
            {
                FsysRamRP *rp;
                rp = rfh->ramrp + q->o_which;   /* get next retrieval pointer set */
                if (rp->num_rptrs)        /* if there are alternate retrevial pointers */
                {
                    U32 nxt;
                    USED_ALT();
                    rfh->active_rp = q->o_which;
                    nxt = q->o_where*BYTPSECT + q->o_bws + q->total;
                    q->sector = q->o_where = nxt/BYTPSECT;
                    q->bws = q->o_bws = nxt&(BYTPSECT-1);
                    q->o_len = q->u_len = q->o_len - q->total;
                    q->o_buff = q->buff = (q->o_buff + q->total);
                    q->ramrp = rp;          /* point to new retreival pointers */
                    q->state = 0;           /* make sure we start at beginning */
                    q->total = 0;           /* start the read over at 0 */
                    fsys_qread(ioq);
                    return;
                }
            }
        }
        if (QIO_ERR_CODE(ioq->iostatus) && (fp->flags&HDIO_RW_RPT_NOERR)) /* if not to report errors */
        {
            if (ioq->iostatus != HDIO_FATAL)
            {
                ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;    /* fake success */
                if (!hisioq->iocount)
                {
                    hisioq->iocount = BYTPSECT; /* force read of 1 sector */
                    q->total = BYTPSECT;        /* assume 1 sector read */
                }
            }
        }
        hisioq->iostatus = ioq->iostatus;   /* record last status */
    }
    else
    {
        hisioq->iostatus = QIO_CANCELLED;
    }
    npos = (q->o_where*BYTPSECT) + q->total + q->o_bws;
    fp->sect = npos/BYTPSECT;
    fp->bws = npos&(BYTPSECT-1);
    fsys_freeqio(q);
    if (!(fp->flags&FSYS_HOLD_MUTEX)) qio_freemutex(&fp->mutex, hisioq);
    qio_complete(hisioq);
    return;
}

static void readwpos_q(QioIOQ *ioq)
{
    FsysQio *q;
    QioFile *fp;
    FsysRamFH *rfh;
    off_t where;
/*    FsysVolume *v; */

    q = (FsysQio *)ioq->pparam0;
    fp = q->fsys_fp;
    if ((fp->flags&QIO_FFLAG_CANCEL))
    {
        ioq->iostatus = QIO_CANCELLED;      /* signal been cancelled */
        fsys_freeqio(q);            /* done with this */
        qio_freemutex(&fp->mutex, ioq);     /* done with this too */
        qio_complete(ioq);          /* done */
        return;
    }
    rfh = (FsysRamFH *)ioq->pparam1;
/*    v = (FsysVolume *)fp->dvc->private; */
    if (!(fp->mode&O_CREAT) && (fp->mode&FSYS_OPNCPY_M)) /* if reading a specific file */
    {
        int t;
        t = (((U32)fp->mode&FSYS_OPNCPY_M)>>FSYS_OPNCPY_V) - 1;
        q->ramrp = rfh->ramrp + t;
    }
    else
    {
        q->ramrp = rfh->ramrp + rfh->active_rp;
    }
    q->o_which = rfh->active_rp;
    set_flags(fp, ~HDIO_RW_RPT_NOERR);      /* assume to record errors */
#if defined(GUTS_OPT_RPT_DISK_ERRS)
    if (!(fp->mode&FSYS_OPNCPY_M) && !(debug_mode&GUTS_OPT_RPT_DISK_ERRS))
    {
        if (q->o_which+1 < FSYS_MAX_ALTS)
        {
            FsysRamRP *rp;
            rp = rfh->ramrp + q->o_which + 1;   /* see if there's an alternate */
            if (!rp->num_rptrs) set_flags(fp, HDIO_RW_RPT_NOERR);
        }
        else
        {
            set_flags(fp, HDIO_RW_RPT_NOERR);
        }
    }
#endif
    where = ioq->iparam0;
    if (where + q->o_len >= rfh->size)
    {
        if (where >= rfh->size)
        {
            fp->sect = rfh->size/BYTPSECT;      /* set position to end of file */
            fp->bws = rfh->size&(BYTPSECT-1);
            ioq->iostatus = QIO_EOF;        /* signal end of file */
            q->o_len = 0;
        }
        else
        {
            q->o_len = q->u_len = rfh->size - where; /* max out the length */
        }
    }
    if (!q->o_len)                /* if nothing to copy */
    {
        if (!ioq->iostatus) ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO; /* Normal success */
        ioq->iocount = 0;           /* no data xferred */
        fsys_freeqio(q);            /* done with this */
        qio_freemutex(&fp->mutex, ioq);     /* done with the file mutex */
        qio_complete(ioq);          /* done */
        return;
    }
    fsys_qread(&q->our_ioq);
    return;
}

static int validate_read( QioIOQ *ioq, void *buf, int32_t len)
{
    FsysQio *q;
    QioFile *fp;
    FsysVolume *v;
    uint32_t fid;
    FsysRamFH *rfh;

    if (!ioq || len < 0) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    v = (FsysVolume *)fp->dvc->private;
    fid = qio_cvtFromPtr(fp->private);
    if ((fid&FSYS_DIR_FIDMASK) >= v->files_ffree) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = fsys_find_ramfh(v, (fid&FSYS_DIR_FIDMASK));
    if (!rfh->valid) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>FSYS_DIR_GENSHF)) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    q = fsys_getqio();
    if (!q) return(ioq->iostatus = FSYS_IO_NOQIO);
    q->vol = v;
    q->our_ioq.file = v->iofd;          /* physical I/O to this channel */
    q->callers_ioq = ioq;
    q->o_buff = q->buff = buf;
    q->o_len = q->u_len = len;
    q->complt = fsys_read_done;
    q->fsys_fp = fp;
    ioq->pparam0 = (void *)q;
    ioq->pparam1 = (void *)rfh;
    return 0;
}

/*
 * This function is entered after the file mutex is obtained. The IOQ ptr is
 * pointing to a QioIOQ obtained by the dispatcher specifically for this purpose.
 * The file's mutex is owned by this IOQ ptr. This routine also grabs the mutex
 * on the sector buffer (filep in the FsysVolume struct) making all files share
 * a single sector buffer. A sector sized 'read' is issued to the appropriate
 * sector into the filep buffer after which the appropriate number of bytes are
 * copied to the callers buffer. The pointers and counters are adjusted accordingly
 * and the remaining bytes are read using the 'normal' read routines. The file
 * mutex ownership is snapped to the caller's IOQ in the process.
 */

static void readbws_q( QioIOQ *ioq)
{
    QioIOQ *hisioq;
    FsysQio *mine, *his;
    FsysVolume *vol;
    QioFile *fp;
    int sts;

    sts = ioq->iostatus;
    hisioq = (QioIOQ *)ioq->user;       /* get ptr to caller's IOQ */
    his = (FsysQio *)hisioq->pparam0;
    fp = his->fsys_fp;
    vol = (FsysVolume *)fp->dvc->private;
    mine = (FsysQio *)ioq->pparam0;     /* either 0 or ptr to our FsysQio */
    if ((fp->flags&QIO_FFLAG_CANCEL))
    {
        ioq->iostatus = QIO_CANCELLED;      /* signal been cancelled */
    }
    else
    {
        if (!QIO_ERR_CODE(sts))
        {
            switch (qio_cvtFromPtr(ioq->user2))
            {
            case 0: {           /* now have file mutex */
                    U32 hwhere = hisioq->iparam0; /* get his requested where */
                    if (hwhere >= fp->size)   /* if to read at or beyond EOF */
                    {
                        fp->sect = fp->size/BYTPSECT;   /* smack the file pointer to EOF */
                        fp->bws = fp->size&(BYTPSECT-1);
                        sts = QIO_EOF;      /* signal end of file */
                        break;          /* done */
                    }
                    hwhere = fp->size - hwhere; /* compute number of bytes to EOF */
                    if (his->o_len > hwhere) his->u_len = his->o_len = hwhere;  /* max out to size to EOF */
                    ioq->user2 = (void *)1; /* next state is 1 */
                    sts = qio_getmutex(&vol->filep_mutex, readbws_q, ioq ); /* get buffer mutex */
                    if (sts) break;
                    return;
                }
            case 1:             /* now have both file and buffer mutex */
                ioq->user2 = (void *)2; /* next state is 2 */
                sts = validate_read( ioq, vol->filep, BYTPSECT );
                if (sts) break;
                mine = (FsysQio *)ioq->pparam0;
                mine->o_bws = mine->bws = 0;
                mine->o_where = mine->sector = his->o_where;
                set_flags(fp, FSYS_HOLD_MUTEX); /* tell fsys_read_done to not let go of mutex */
                ioq->aq.action = readwpos_q;
                ioq->aq.param = ioq;
                prc_q_ast(QIO_ASTLVL, &ioq->aq);
                return;
            case 2: {               /* finished reading the filler sector */
                    int amt;
                    amt = BYTPSECT - his->o_bws;
                    if (amt > his->o_len) amt = his->o_len;
                    memcpy(his->o_buff, vol->filep+his->o_bws, amt);
                    hisioq->iocount = amt;      /* preload the count */
                    his->o_buff = his->buff += amt;
                    if ((his->o_len = his->u_len -= amt) <= 0)
                    {
                        U32 pos = hisioq->iparam0 + amt;
                        fp->sect = pos/BYTPSECT;
                        fp->bws = pos&(BYTPSECT-1);
                        break;
                    }
                    his->o_bws = his->bws = 0;
                    his->o_where = his->sector = fp->sect;
                    hisioq->iparam0 = fp->sect*BYTPSECT + fp->bws;
                    set_flags(fp, ~FSYS_HOLD_MUTEX);
                    hisioq->aq.action = readwpos_q;
                    hisioq->aq.param = hisioq;
                    fp->mutex.current = &hisioq->aq;        /* change ownership of mutex to caller's IOQ */
                    qio_freemutex(&vol->filep_mutex, ioq);  /* done with buffer */
                    qio_freeioq(ioq);               /* done with this too */
                    prc_q_ast(QIO_ASTLVL, &hisioq->aq);
                    return;
                }
            default:
                sts = FSYS_IO_FATAL;
                break;
            }
        }
    }
    set_flags(fp, ~FSYS_HOLD_MUTEX);        /* let go of file mutex */
    qio_freemutex(&vol->filep_mutex, ioq);  /* done with file buffer */
    fp->mutex.current = &hisioq->aq;        /* change ownership of mutex to caller's IOQ */
    qio_freemutex(&fp->mutex, hisioq);      /* we're done with file mutex */
    if (!hisioq->iostatus) hisioq->iostatus = sts;
    if (!hisioq->iocount) hisioq->iocount = ioq->iocount;
    qio_freeioq(ioq);
    if (mine) fsys_freeqio(mine);
    fsys_freeqio(his);
    qio_complete(hisioq);
}

/* fsys_readwpos - is called directly as a result of the user calling qio_readwpos()
 * To protect the sect/bws fields of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * file struct. This is done by loading the caller's parameters into the IOQ and
 * waiting on the mutex for the file then calling readwpos_q().
 */

static int fsys_readwpos( QioIOQ *ioq, off_t where, void *buf, int32_t len )
{
    FsysQio *q;
    QioFile *fp;
    int sts, bws;

    sts = validate_read( ioq, buf, len );
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    fp = q->fsys_fp;
    bws = q->o_bws = q->bws = where&(BYTPSECT-1);
    q->o_where = q->sector = where/BYTPSECT;
    ioq->iparam0 = where;
    if (bws)
    {
/*
 * Special case where we need to break up the read into a small sub-sector
 * size followed by the 'rest'. This is done by grabbing a new ioq and issuing
 * a smaller read with the 'len' set to read to the next sector boundary or the
 * user's count whichever is less. If there is more to read after that, the
 * user's I/O is left to complete.
 */
        QioIOQ *mioq;
        mioq = qio_getioq();
        if (!mioq)
        {
            fsys_freeqio(q);
            return ioq->iostatus = QIO_NOIOQ;
        }
        mioq->complete = readbws_q;
        mioq->file = ioq->file;
        mioq->user = ioq;
        mioq->user2 = 0;        /* state 0 */
        sts = qio_getmutex(&fp->mutex, readbws_q, mioq);
        if (sts)
        {
            fsys_freeqio(q);
            qio_freeioq(mioq);
            ioq->iostatus = sts;
        }
        return sts;
    }
    sts = qio_getmutex(&fp->mutex, readwpos_q, ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void read_q( QioIOQ *ioq )
{
    FsysQio *q;
    QioFile *fp;

    q = (FsysQio *)ioq->pparam0;
    fp = q->fsys_fp;
    q->o_where = q->sector = fp->sect;
    ioq->iparam0 = fp->sect*BYTPSECT + fp->bws;
    if ((q->o_bws = fp->bws))
    {
        QioIOQ *mioq;
/*
 * Special case where we need to break up the read into a small sub-sector
 * size followed by the 'rest'. This is done by grabbing a new ioq and issuing
 * a smaller read with the 'len' set to read to the next sector boundary or the
 * user's count whichever is less. If there is more to read after that, the
 * user's I/O is left to complete.
 */
        mioq = qio_getioq();
        if (!mioq)
        {
            ioq->iostatus = QIO_NOIOQ;
            fsys_freeqio(q);
            qio_freemutex(&fp->mutex, ioq);
            qio_complete(ioq);
            return;
        }
        mioq->complete = readbws_q;
        mioq->file = ioq->file;
        mioq->user = ioq;
        mioq->user2 = 0;        /* state 0 */
        fp->mutex.current = &mioq->aq;  /* switch owners of file mutex */
        readbws_q(mioq);
    }
    else
    {
        readwpos_q(ioq);
    }
    return;
}

/* fsys_read - is called directly as a result of the user calling qio_read()
 * To protect the sect/bws fields of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * QioFile struct. This is done by loading the caller's parameters into the IOQ and
 * waiting on the mutex for the file then calling read_q().
 */

static int fsys_read( QioIOQ *ioq, void *buf, int32_t len )
{
    FsysQio *q;
    int sts;

    sts = validate_read( ioq, buf, len );
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    sts = qio_getmutex(&q->fsys_fp->mutex, read_q, ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void fsys_write_done( QioIOQ *ioq )
{
    FsysQio *q;
    QioIOQ *hisioq;
    QioFile *fp;
    FsysVolume *v;
    uint32_t fid;
    FsysRamFH *rfh;
    int lim;
    U32 npos;

    q = (FsysQio *)ioq;
    if (QIO_ERR_CODE(ioq->iostatus)) q->o_iostatus = ioq->iostatus;
    fp = q->fsys_fp;
    v = (FsysVolume *)fp->dvc->private;
    hisioq = q->callers_ioq;
    fp = qio_fd2file(hisioq->file);
    fid = qio_cvtFromPtr(fp->private);
    rfh = fsys_find_ramfh(v, (fid&FSYS_DIR_FIDMASK));
    if (!(fp->flags&QIO_FFLAG_CANCEL) && !(fp->mode&FSYS_OPNCPY_M))
    {
        ++q->o_which;
        if (q->o_which < FSYS_MAX_ALTS)   /* if we haven't tried all writes */
        {
            FsysRamRP *rp;
            rp = rfh->ramrp + q->o_which;   /* get next retrieval pointer set */
            if (rp->num_rptrs)        /* if there are alternate retrevial pointers */
            {
                q->ramrp = rp;          /* point to new batch */
                q->sector = q->o_where;     /* rewind the significant pointers */
                q->u_len = q->o_len;
                q->buff = (void *)q->o_buff;
                q->total = 0;           /* reset the total */
                q->state = 0;           /* make sure we start at beginning */
                fsys_qwrite(ioq);       /* and re start the write */
                return;
            }
        }
    }
    lim = q->total;         /* total number of bytes xferred */
    if (lim > q->o_len) lim = q->o_len; /* limit it to the number the user asked */
    hisioq->iocount = lim;      /* record total bytes xferred */
    if (q->o_iostatus)
    {
        hisioq->iostatus = q->o_iostatus;   /* record last error status */
    }
    else
    {
        hisioq->iostatus = ioq->iostatus;
    }
    npos = q->o_where*BYTPSECT + q->o_bws + lim; /* advance the files' sector pointer */
    if (rfh->size < npos)
    {
        rfh->size = npos;
        rfh->fh_upd = 1;        /* update FH at close */
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) && !FSYS_READ_ONLY
        if (v->journ_rfh)
        {
            add_to_dirty(v, (fid&FSYS_DIR_FIDMASK), 0); /* keep this current so we don't loose sectors */
        }
#endif
    }
    fp->sect = npos/BYTPSECT;
    fp->bws = npos&(BYTPSECT-1);
    fp->size = rfh->size;       /* keep a running total in here too */
    fsys_freeqio(q);
    qio_freemutex(&fp->mutex, &q->our_ioq);
    qio_complete(hisioq);
    return;
}

static int validate_write( QioIOQ *ioq, const void *buf, int32_t len )
{
    FsysQio *q;
    QioFile *fp;
    FsysVolume *v;
    uint32_t fid;
    FsysRamFH *rfh;

    if (!ioq) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    v = (FsysVolume *)fp->dvc->private;
    fid = qio_cvtFromPtr(fp->private);
    if ((fid&FSYS_DIR_FIDMASK) >= v->files_ffree) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = fsys_find_ramfh(v, (fid&FSYS_DIR_FIDMASK));
    if (!rfh->valid) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>FSYS_DIR_GENSHF)) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    q = fsys_getqio();
    if (!q) return(ioq->iostatus = FSYS_IO_NOQIO);
    q->vol = v;
    q->o_which = 0;
    q->o_len = q->u_len = len;
    q->o_buff = q->buff = (void *)buf;
    q->o_iostatus = 0;              /* assume no errors */
    q->our_ioq.file = v->iofd;          /* physical I/O to this channel */
    q->callers_ioq = ioq;
    q->ramrp = rfh->ramrp;
    q->complt = fsys_write_done;
    q->fsys_fp = fp;
    ioq->pparam0 = (void *)q;
    return 0;
}

/* fsys_writewpos - is called directly as a result of the user calling qio_writewpos()
 * To protect the sect/bws fields of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * file struct. This is done by loading the caller's parameters into the IOQ,
 * waiting on the mutex for the file then calling writewpos_q().
 */

static int fsys_writewpos( QioIOQ *ioq, off_t where, const void *buf, int32_t len )
{
    FsysQio *q;
    int sts;

    sts = validate_write(ioq, buf, len);
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    q->o_where = q->sector = where/BYTPSECT;
    q->o_bws = q->bws = where&(BYTPSECT-1);
#if !FSYS_WRITE_SQUAWKING
    FSYS_SQK((OUTWHERE "fsys_writepos: fd=%04X, where=%ld, buf=%08lX, len=%ld, fp_pos=%ld\n",
              ioq->file, where, (U32)buf, len, (q->fsys_fp->sect*BYTPSECT)+q->fsys_fp->bws));
#else
    FSYS_WR_SQK((OUTWHERE "fsys_writepos: fd=0x%04X, where=0x%lX, buf=0x%08lX, len=0x%lX, fp_pos=0x%lX\n",
                 ioq->file, where, (U32)buf, len, (q->fsys_fp->sect*BYTPSECT)+q->fsys_fp->bws));
#endif
    sts = qio_getmutex(&q->fsys_fp->mutex, fsys_qwrite, &q->our_ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void write_q( QioIOQ *ioq )
{
    FsysQio *q;

    q = (FsysQio *)ioq;
    q->o_where = q->sector = q->fsys_fp->sect;
    q->o_bws = q->fsys_fp->bws;
    fsys_qwrite(&q->our_ioq);
    return;
}

static int fsys_write( QioIOQ *ioq, const void *buf, int32_t len )
{
    int sts;
    FsysQio *q;

    sts = validate_write( ioq, buf, len);
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
#if !FSYS_WRITE_SQUAWKING
    FSYS_SQK((OUTWHERE "fsys_write: fd=%04X, buf=%08lX, len=%ld, fp_pos=%ld\n",
              ioq->file, (U32)buf, len, (q->fsys_fp->sect*BYTPSECT)+q->fsys_fp->bws));
#else
    FSYS_WR_SQK((OUTWHERE "fsys_write: fd=0x%04X, buf=0x%08lX, len=0x%lX, fp_pos=0x%lX\n",
                 ioq->file, (U32)buf, len, (q->fsys_fp->sect*BYTPSECT)+q->fsys_fp->bws));
#endif
    sts = qio_getmutex(&q->fsys_fp->mutex, write_q, &q->our_ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static void ioctl_setfh( QioIOQ *ioq )
{
    QioFile *fp;
    FsysVolume *vol;
    FsysFHIoctl *ctl;
    FsysRamFH *rfh;
    uint32_t fid, afid;
#if !FSYS_READ_ONLY
    FsysRamRP *rp;
    int ii, jj, copies;
#endif

    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
    if (fp->flags&QIO_FFLAG_CANCEL)
    {
        ioq->iostatus = QIO_CANCELLED;
    }
    else
    {
        if ((vol->flags&FSYS_VOL_FLG_RO))
        {
            ioq->iostatus = FSYS_IO_NOWRITE;
        }
        else
        {
            fid = qio_cvtFromPtr(fp->private);
            afid = fid & FSYS_DIR_FIDMASK;
            rfh = fsys_find_ramfh(vol, afid);
            ctl = (FsysFHIoctl *)ioq->pparam0;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
            if ((ctl->fields&FSYS_FHFIELDS_CTIME)) rfh->ctime = ctl->ctime;
            if ((ctl->fields&FSYS_FHFIELDS_MTIME)) rfh->mtime = ctl->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
            if ((ctl->fields&FSYS_FHFIELDS_ATIME)) rfh->atime = ctl->atime;
            if ((ctl->fields&FSYS_FHFIELDS_BTIME)) rfh->btime = ctl->btime;
#endif    
#if !FSYS_READ_ONLY
            for (ii=copies=0; ii < FSYS_MAX_ALTS; ++ii)   /* compute how many copies */
            {
                rp = rfh->ramrp + ii;
                if (rp->num_rptrs) ++copies;
            }
            if ((ctl->fields&FSYS_FHFIELDS_COPIES))
            {
                for (; copies < ctl->copies; ++copies)
                {
                    rp = rfh->ramrp + copies;
                    if (rp->num_rptrs) continue;        /* already have a copy here */
                    ii = extend_file(vol, rfh->clusters, rp, copies);
                    if (ii)
                    {
                        ioq->iostatus = ii;
                        goto ioctl_done;
                    }
                }
            }
            if ((ctl->fields&FSYS_FHFIELDS_ALLOC))
            {
                int def;
                def = (ctl->alloc+(BYTPSECT-1))/BYTPSECT;
                if (def > rfh->clusters)
                {
                    def -= rfh->clusters;
                    if (def*copies >= vol->total_free_clusters)
                    {
                        ioq->iostatus = FSYS_EXTEND_FULL;   /* no room on disk */
                        goto ioctl_done;
                    }
                    for (jj=0; jj < copies; ++jj)
                    {
                        rp = rfh->ramrp + jj;
                        if (!rp->num_rptrs) continue;       /* no copy here */
                        ii = extend_file(vol, def, rp, jj);
                        if (ii)
                        {
                            ioq->iostatus = ii;
                            break;              /* ran out of room */
                        }
                    }
                    rfh->clusters += def;           /* bump allocation record */
                }
            }
            if ((ctl->fields&FSYS_FHFIELDS_DEFEXT))
            {
                int lim;
                lim = (ctl->def_extend+(BYTPSECT-1))/BYTPSECT;
                if (lim > 65535) lim = 65535;
                rfh->def_extend = lim;
            }
#endif
            if ((ctl->fields&FSYS_FHFIELDS_SIZE))
            {
                if (ctl->size > rfh->clusters*BYTPSECT)
                {
                    rfh->size = fp->size = rfh->clusters*BYTPSECT;
                }
                else
                {
                    rfh->size = fp->size = ctl->size;
                }
            }
            add_to_dirty(vol, afid, 0);             /* record changed FH */
            ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
        }
    }
#if !FSYS_READ_ONLY
    ioctl_done:
#endif
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
}
#endif

#if !FSYS_READ_ONLY
static int purge_rp( FsysVolume *vol, FsysRamRP *top, int tofree)
{
    FsysRamRP *rp, **prev;
    int jj;

    rp = top;
    prev = &rp->next;
    while (rp)
    {
        int cnt = rp->num_rptrs;
        if (tofree)
        {
            FsysRetPtr *retptr;
            retptr = rp->rptrs;
            for (jj=0; jj < cnt; ++jj)
            {
                back_to_free(vol, retptr++);
            }
        }
        if (rp->mallocd)  /* if rptrs is malloc'd */
        {
            QIOfree(rp->rptrs); /* free it */
            rp->rptrs = 0;  /* nothing here */
            rp->rptrs_size = 0; /* or here */
            rp->num_rptrs = 0;  /* or here */
            rp->mallocd = 0;    /* or here */
            prev = &rp->next;   /* we stay on the list */
            rp = rp->next;  /* walk the chain */
        }
        else
        {
            *prev = rp->next;   /* then pluck ourself from the list */
            QIOfree(rp);    /* free ourself */
            rp = *prev;     /* point to next guy */
        }
    }
    return 0;
}
#endif

static void ioctl_rpstuff( QioIOQ *ioq )
{
    int ii, jj, rc;
    uint32_t fid, afid;
    QioFile *fp;
    FsysRPIoctl *iocrp;
    FsysVolume *vol;
    FsysRamFH *rfh;
    FsysRamRP *rp;
    FsysRetPtr *retptr;

    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
    if ((fp->flags&(QIO_FFLAG_CANCEL|QIO_FFLAG_CLOSING)))
    {
        ioq->iostatus = QIO_CANCELLED;
    }
    else
    {
        iocrp = (FsysRPIoctl *)ioq->pparam0;
        iocrp->out_rp = iocrp->tot_rp = 0;  /* in case of error */
        retptr = iocrp->rps;        /* point to "his" array of retrieval pointers */
        fid = qio_cvtFromPtr(fp->private);
        afid = fid & FSYS_DIR_FIDMASK;
        rfh = fsys_find_ramfh(vol, afid);
        rc = FSYS_IO_SUCC|SEVERITY_INFO;    /* assume success */
        switch ((int)ioq->iparam0)
        {
#if !FSYS_READ_ONLY
        case FSYS_IOC_PLUCKRP: {
                break;
            }
        case FSYS_IOC_PURGERP: {    /* purge the specified retrieval pointer set from file */
                FsysRamRP *src; 
                if ((vol->flags&FSYS_VOL_FLG_RO))
                {
                    rc = FSYS_IO_NOWRITE;
                    break;
                }
                purge_rp(vol, (rp = rfh->ramrp + iocrp->which), 0); /* toss any retrieval pointers */
                src = rp + 1;
                for (jj=iocrp->which+1; jj < FSYS_MAX_ALTS; ++jj, ++rp, ++src)
                {
                    *rp = *src;
                    memset(src, 0, sizeof(FsysRamRP));
                }
                add_to_dirty(vol, afid, 0);
                break;
            }
        case FSYS_IOC_REALLRP: { /* alloc a new set of RP's */
                purge_rp(vol, (rp = rfh->ramrp + iocrp->which), 1); /* put any RP's on the free list */
                if ((vol->flags&FSYS_VOL_FLG_RO))
                {
                    rc = FSYS_IO_NOWRITE;
                    break;
                }
                ii = extend_file(vol, rfh->clusters, rp, iocrp->which);
                if (ii)
                {
                    rc = ii;
                    purge_rp(vol, rp, 1);       
                }
                break;      /* done */
            }
        case FSYS_IOC_FREERP: { /* add the provided RP's to the free list */
                if ((vol->flags&FSYS_VOL_FLG_RO))
                {
                    rc = FSYS_IO_NOWRITE;
                    break;
                }
                for (ii=0; ii < iocrp->inp_rp; ++ii)
                {
                    back_to_free(vol, retptr++);
                }
                iocrp->out_rp = iocrp->inp_rp;
                break;      /* done */
            }
#endif
        case FSYS_IOC_GETRP: {  /* get the retrevial pointer set for file */
                rp = rfh->ramrp + iocrp->which; /* point to file's rp set */
#if !FSYS_READ_ONLY
                while (rp)    /* count all the RP's in use */
                {
                    iocrp->tot_rp += rp->num_rptrs;
                    rp = rp->next;
                }
#else
                iocrp->tot_rp = rp->num_rptrs;
#endif
                rp = rfh->ramrp + iocrp->which; /* point to file's rp set again */
                ii = iocrp->inp_rp;         /* remember how many we can copy */
                while (rp && ii > 0)      /* while we have a pointer and a count */
                {
                    if (rp->num_rptrs && rp->rptrs)
                    {
                        jj = (ii > rp->num_rptrs) ? rp->num_rptrs : ii;
                        memcpy(retptr, rp->rptrs, jj * sizeof(FsysRetPtr)); /* copy up to 'n' pointers */
                        ii -= jj;       /* count how many we've copied so far */
                        retptr += jj;   /* advance user's pointer */
                    }
#if !FSYS_READ_ONLY
                    rp = rp->next;
#else
                    break;          /* read only filesystem has no link. Cannot continue */
#endif
                }
                iocrp->out_rp = iocrp->inp_rp - ii; /* say how many we copied */
                ioq->iocount = iocrp->out_rp*sizeof(FsysRetPtr);
                break;          /* done */
            }
        }
        ioq->iostatus = rc;
    }
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_ioctl( QioIOQ *ioq, unsigned int cmd, void *arg )
{
    int sts, ii, jj;
    uint32_t fid, afid;
    QioFile *fp;
    FsysFHIoctl *ctl;
    FsysRPIoctl *iocrp;
    FsysVolume *vol;
    FsysRamFH *rfh;
    FsysRamRP *rp;

    if (!ioq) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
    switch (cmd)
    {
    default: sts = FSYS_IO_BADIOCTL;
        break;
    case FSYS_IOC_EXPIRE: {
            vol->status = FSYS_MOUNT_EXPIRED;   /* clobber this filesystem */
            sts = FSYS_IO_SUCC|SEVERITY_INFO;
            break;
        }
    case FSYS_IOC_GETFH: {
            if (!(ctl = (FsysFHIoctl *)arg))
            {
                sts = QIO_INVARG;
                break;
            }
            fid = qio_cvtFromPtr(fp->private);
            afid = fid & FSYS_DIR_FIDMASK;
            rfh = fsys_find_ramfh(vol, afid);
            ctl->fields = FSYS_FHFIELDS_ALLOC
                          |FSYS_FHFIELDS_DEFEXT|FSYS_FHFIELDS_SIZE|FSYS_FHFIELDS_COPIES
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
                          |FSYS_FHFIELDS_CTIME|FSYS_FHFIELDS_MTIME
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
                          |FSYS_FHFIELDS_ATIME|FSYS_FHFIELDS_BTIME
#endif
                          ;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
            ctl->ctime = rfh->ctime;
            ctl->mtime = rfh->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
            ctl->atime = rfh->atime;
            ctl->btime = rfh->btime;
#else
            ctl->atime = ctl->btime = 0;
#endif
#if !FSYS_READ_ONLY
            ctl->alloc = rfh->clusters*BYTPSECT;
#else
            ctl->alloc = fp->size;
#endif
            ctl->def_extend = rfh->def_extend*BYTPSECT;
            ctl->size = fp->size;
            ctl->sect = fp->sect;
            ctl->bws = fp->bws;
            ctl->fid = fid;
            for (ii=jj=0; ii < FSYS_MAX_ALTS; ++ii)   /* compute how many copies */
            {
                rp = rfh->ramrp + ii;
                if (rp->num_rptrs) ++jj;
            }
            ctl->copies = jj;
            ctl->dir = rfh->directory != 0;
            ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
            qio_complete(ioq);
            return 0;
        }
    case FSYS_IOC_GETFHLBA: {
#if !FSYS_READ_ONLY || FSYS_UPD_FH
            U32 *his_lbas = (U32 *)arg;
            U32 *our_lbas;

            fid = qio_cvtFromPtr(fp->private);
            afid = fid & FSYS_DIR_FIDMASK;
            our_lbas = ENTRY_IN_INDEX(vol, afid);
            for (fid=0; fid < FSYS_MAX_ALTS; ++fid) *his_lbas++ = *our_lbas++ & FSYS_LBA_MASK;
            ioq->iocount = FSYS_MAX_ALTS*sizeof(U32);
            ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
#else
            ioq->iostatus = FSYS_IO_NOSUPP;
#endif
            qio_complete(ioq);
            return 0;
        }
    case FSYS_IOC_SETFH:
        if (!(ctl = (FsysFHIoctl *)arg))
        {
            sts = QIO_INVARG;
            break;
        }
#if !FSYS_READ_ONLY || FSYS_UPD_FH
#if FSYS_READ_ONLY && FSYS_UPD_FH
        if ((ctl->fields&(FSYS_FHFIELDS_ALLOC|FSYS_FHFIELDS_COPIES|FSYS_FHFIELDS_DEFEXT)))
        {
            sts = FSYS_IO_NOSUPP;
            break;
        }
#else
        if ((vol->flags&FSYS_VOL_FLG_RO))
        {
            sts = FSYS_IO_NOWRITE;
            break;
        }
        if ((ctl->fields&FSYS_FHFIELDS_COPIES) && ctl->copies > FSYS_MAX_ALTS)
        {
            sts = QIO_INVARG;
            break;
        }
#endif
        ioq->iparam0 = cmd;
        ioq->pparam0 = arg;
        sts = qio_getmutex(&vol->mutex, ioctl_setfh, ioq);
        if (sts) break;
        return 0;
#else
        sts = FSYS_IO_NOSUPP;
        break;
#endif
    case FSYS_IOC_PLUCKRP:      /* Pluck the set of RP's from the freelist if possible */
    case FSYS_IOC_REALLRP:      /* alloc a new set of RP's without 'free'ing the current set */
    case FSYS_IOC_FREERP:       /* add the provided RP's to the free list */
    case FSYS_IOC_PURGERP:      /* purge the specified RP set from file */
#if FSYS_READ_ONLY
        sts = FSYS_IO_NOWRITE;  /* not legal on read only system */
        break;
#else
        if ((vol->flags&FSYS_VOL_FLG_RO))
        {
            sts = FSYS_IO_NOWRITE;
            break;
        }
#endif
    case FSYS_IOC_GETRP: {      /* get the specified retrevial pointer set for file */
            if (!(iocrp = (FsysRPIoctl *)arg) || !iocrp->rps || iocrp->which >= FSYS_MAX_ALTS)
            {
                sts = QIO_INVARG;
                break;
            }
            ioq->iparam0 = cmd;
            ioq->pparam0 = arg;
            sts = qio_getmutex(&vol->mutex, ioctl_rpstuff, ioq); /* lock the volume for these */
            if (sts) break;
            return 0;
        }
    }
    return(ioq->iostatus = sts);
}

#if FSYS_ASYNC_CLOSE && (!FSYS_READ_ONLY || FSYS_UPD_FH)
static void fsys_close_upd(QioIOQ *ioq)
{
    QioFile *file;
    uint32_t fid, afid;
    FsysRamFH *rfh;
    FsysVolume *vol;

    file = qio_fd2file(ioq->file);
    vol = (FsysVolume *)file->dvc->private;
    fid = qio_cvtFromPtr(file->private);
    afid = fid & FSYS_DIR_FIDMASK;
    rfh = fsys_find_ramfh(vol, afid);
    if ((rfh->fh_upd))
    {
        add_to_dirty(vol, afid, 0);
        rfh->fh_upd = 0;    /* not dirty anymore */
        rfh->fh_new = 0;    /* not new anymore either */
    }
    file->private = 0;
    file->dvc = 0;
    qio_freefile(file);     /* put file back on free list */
    ioq->file = -1;     /* not open anymore */
    qio_freemutex(&vol->mutex, ioq);
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return;
}
#endif

#if FSYS_ASYNC_CLOSE
static void fsys_closeq(QioIOQ *ioq)
{
    QioIOQ *his;
    QioFile *file;
/*    int sts; */

    file = qio_fd2file(ioq->file);
    his = (QioIOQ *)ioq->user;
    his->iocount = ioq->iocount;
/*    sts = ioq->iostatus; */
    qio_freeioq(ioq);
    ioq = his;
#if !FSYS_READ_ONLY || FSYS_UPD_FH
    {
        FsysVolume *vol;
        vol = (FsysVolume *)file->dvc->private;
        if (!(vol->flags&FSYS_VOL_FLG_RO))
        {
            if ((ioq->iostatus = qio_getmutex(&vol->mutex, fsys_close_upd, ioq)))
            {
                qio_complete(ioq);
            }
            return;
        }
    }
#else
    file->private = 0;
    file->dvc = 0;
    qio_freefile(file);     /* put file back on free list */
    ioq->file = -1;     /* not open anymore */
    ioq->iostatus = QIO_ERR_CODE(sts) ? sts : FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return;
#endif
}
#endif

#if FSYS_ASYNC_CLOSE
static void cancel_done(QioIOQ *ioq)
{
    QioFile *file;

    file = qio_fd2file(ioq->file);
    if (file)
    {
        set_flags(file, ~QIO_FFLAG_CANCEL); /* signal done with cancel */
        ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    }
    qio_freemutex(&file->mutex, ioq);       /* done with this mutex */
    qio_complete(ioq);
}
#endif

static int fsys_cancel(QioIOQ *ioq)
{
#if FSYS_ASYNC_CLOSE
    QioFile *file;
    struct act_q *m;
    int oldps, cnt;

    file = qio_fd2file(ioq->file);
    oldps = prc_set_ipl(INTS_OFF);
    file->flags |= QIO_FFLAG_CANCEL;        /* signal we're to cancel all pending I/O */
    m = file->mutex.waiting;
    cnt = 0;
    while (m)
    {
        ++cnt;
        m = m->next;
    }
    if (file->mutex.current) ++cnt;
    prc_set_ipl(oldps);
    ioq->iocount = cnt;
    return qio_getmutex(&file->mutex, cancel_done, ioq);    /* put ourself at the end of the list */
#else
    return 0;
#endif
}

static int fsys_close( QioIOQ *ioq)
{
    QioFile *file;
    FsysVolume *vol;
    uint32_t fid, afid;
    FsysRamFH *rfh;

    if (!ioq) return QIO_INVARG;
    file = qio_fd2file(ioq->file);
    if (!file) return(ioq->iostatus = QIO_NOTOPEN);

    vol = (FsysVolume *)file->dvc->private;
    if (!vol) return(ioq->iostatus = FSYS_OPEN_NOTOPEN);
    fid = qio_cvtFromPtr(file->private);
    afid = fid & FSYS_DIR_FIDMASK;
    if (afid >= vol->files_ffree) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = fsys_find_ramfh(vol, afid);
    if (!rfh->valid) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>FSYS_DIR_GENSHF)) return(ioq->iostatus = FSYS_IO_NOTOPEN);
#if FSYS_ASYNC_CLOSE
    {
        QioIOQ *mine;
        mine = qio_getioq();
        if (!mine)
        {
            return(ioq->iostatus = QIO_NOIOQ);
        }
        set_flags(file, QIO_FFLAG_CLOSING);
        mine->file = ioq->file;
        mine->complete = fsys_closeq;
        mine->user = ioq;
        return fsys_cancel(mine);
    }
#else
#if !FSYS_READ_ONLY
    if (rfh->fh_upd)
    {
        if (!(vol->flags&FSYS_VOL_FLG_RO)) add_to_dirty(vol, (fid>>FSYS_DIR_GENSHF), 0);
        rfh->fh_upd = 0;    /* not dirty anymore */
        rfh->fh_new = 0;    /* not new anymore either */
    }
#endif
    file->private = 0;
    file->dvc = 0;
    qio_freefile(file);     /* put file back on free list */
    ioq->file = -1;     /* not open anymore */
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
#endif
}

static int fsys_trunc( QioIOQ *ioq )
{
    if (!ioq) return QIO_INVARG;
    return(ioq->iostatus = FSYS_IO_NOSUPP);
}

static void statfs_q( QioIOQ *ioq )
{
    struct qio_statfs *sfs;
    FsysVolume *vol;
#if !FSYS_READ_ONLY || FSYS_UPD_FH
    U32 *ip;
    int ii;
#endif

    vol = (FsysVolume *)ioq->private;
    sfs = (struct qio_statfs *)ioq->pparam0;
    sfs->f_bsize = BYTPSECT;            /* size of sector */
    sfs->f_blocks = vol->maxlba;    /* sectors allocated to this volume */
    sfs->f_files = vol->files_ffree;    /* number of files headers allocated */
    sfs->f_used = vol->total_alloc_clusters; /* number of sectors assigned to files */
    sfs->f_unusedfh = 0;
#if !FSYS_READ_ONLY || FSYS_UPD_FH
    for (ii=0; ii < vol->files_ffree; ++ii)
    {
        ip = ENTRY_IN_INDEX(vol, ii);
        if ((*ip & FSYS_EMPTYLBA_BIT)) ++sfs->f_unusedfh;
    }
#endif
#if !FSYS_READ_ONLY
    {
        FsysRetPtr *rp;
        U32 free_max, free_min, tot=0;
        sfs->f_frags = vol->free_ffree; /* number of free fragments */
        rp = vol->free;
        free_min = free_max = 0;
        for (ii=0; ii < vol->free_ffree; ++ii, ++rp)
        {
            tot += rp->nblocks;
            if (!free_min || rp->nblocks < free_min) free_min = rp->nblocks;
            if (rp->nblocks > free_max) free_max = rp->nblocks;
        }
        sfs->f_lfrag = free_max;
        sfs->f_sfrag = free_min;
        sfs->f_bfree = tot;
        sfs->f_bavail = vol->total_free_clusters;
    }
#endif
    sfs->f_flag = ST_NOSUID|ST_NOTRUNC|ST_LOCAL;
#if FSYS_READ_ONLY
    sfs->f_flag |= ST_RDONLY;
#else
    if ((vol->flags&FSYS_VOL_FLG_RO)) sfs->f_flag |= ST_RDONLY;
#endif
    ioq->iocount = sizeof(struct qio_statfs);
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_statfs( QioIOQ *ioq, const char *name, struct qio_statfs *sfs, int len, int fstyp)
{
    FsysVolume *vol;
    QioDevice *dvc;
    dvc = (QioDevice *)ioq->private;    /* passed to us from qio dispatch */
    vol = (FsysVolume *)dvc->private;
    ioq->private = vol;
    ioq->pparam0 = sfs;
    ioq->iparam0 = len;
    ioq->iparam1 = fstyp;
    return qio_getmutex(&vol->mutex, statfs_q, ioq);
}

static int fsys_fstat( QioIOQ *ioq, struct stat *st )
{
    QioFile *file;
    FsysVolume *vol;
    FsysRamFH *rfh;
    uint32_t fid, afid;

    file = qio_fd2file(ioq->file);      /* get pointer to file */
    if (!file) return(ioq->iostatus = FSYS_OPEN_NOTOPEN);
    vol = (FsysVolume *)file->dvc->private;
    if (!vol) return(ioq->iostatus = FSYS_OPEN_NOTOPEN);
    fid = qio_cvtFromPtr(file->private);
    afid = fid & FSYS_DIR_FIDMASK;
    if (afid >= vol->files_ffree) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = fsys_find_ramfh(vol, afid);
    if (!rfh->valid) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>FSYS_DIR_GENSHF)) return(ioq->iostatus = FSYS_IO_NOTOPEN);
    st->st_mode = rfh->directory ? S_IFDIR : S_IFREG;
    st->st_size = file->size;
#if !WATCOM_LIBC
    st->st_blksize = BYTPSECT;
    st->st_blocks = (file->size+(BYTPSECT-1))/BYTPSECT;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    st->st_ctime = rfh->ctime;
    st->st_mtime = rfh->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    st->st_atime = rfh->atime;
#endif
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_isatty( QioIOQ *ioq )
{
    if (!ioq) return QIO_INVARG;
    return(ioq->iostatus = FSYS_IO_NOSUPP);
}

#if FSYS_HAS_DIRENT
static FsysDIR *dir_pool_head;
static int32_t dir_pool_batch;
static int num_dirs;
    #ifndef FSYS_DIR_BATCH
        #define FSYS_DIR_BATCH (32)
    #endif

/************************************************************
 * fsys_getDIR - Get a DIR from the system's pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
static FsysDIR *fsys_getDIR(void)
{
    int batch;
    if (!dir_pool_head)
    {
        const struct st_envar *st;
        if (!dir_pool_batch)
        {
            st = st_getenv("FSYS_DIR_BATCH", 0);
            if (!st || !st->value)
            {
                dir_pool_batch =  FSYS_DIR_BATCH;
            }
            else
            {
                dir_pool_batch = qio_cvtFromPtr(st->value);
            }
        }
        else
        {
            st = st_getenv("FSYS_DIR_BATCH_GROWABLE", 0);
            if (!st || !st->value) dir_pool_batch = -1;
        }
        if (dir_pool_batch <= 0) return 0;
    }
    ++num_dirs;
    batch = dir_pool_batch > 0 ? dir_pool_batch : 0;
    return(FsysDIR*)qio_getioq_ptr((QioIOQ **)&dir_pool_head, sizeof(FsysDIR), batch);
}

/************************************************************
 * fsys_freeDIR - Free a DIR as obtained from a previous
 * call to fsys_getDIR().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
static int fsys_freeDIR(FsysDIR *que)
{
    --num_dirs;
    return qio_freeioq_ptr((QioIOQ *)que, (QioIOQ **)&dir_pool_head);
}

static int fsys_opendir( QioIOQ *ioq, void **dirp, const char *name )
{
    FsysVolume *vol;
    const QioDevice *dvc;
    FsysDIR *dp;
    int sts;
    FsysLookUpFileT luf;

    dvc = (const QioDevice *)ioq->private;
    vol = (FsysVolume *)dvc->private;       /* point to our mounted volume */
    if (!vol || !vol->filesp) return(ioq->iostatus = FSYS_OPEN_NOTMNT);
    dp = fsys_getDIR();
    if (!dp) return(ioq->iostatus = FSYS_IO_NODIRS);
    luf.vol = vol;
    luf.top = 0;
    luf.path = name;
    sts = lookup_filename(&luf);
    if (QIO_ERR_CODE(sts))
    {
        fsys_freeDIR(dp);
        return(ioq->iostatus = sts);
    }
    if (!(dp->hash=luf.file->directory))
    {
        fsys_freeDIR(dp);
        return(ioq->iostatus = FSYS_LOOKUP_NOTDIR);
    }
    dp->vol = vol;
    *dirp = dp;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_readdir( QioIOQ *ioq, void *dirp, void *directp )
{
    FsysDIR *dp;
    struct fsys_direct *direct;

    dp = (FsysDIR *)dirp;
    direct = (struct fsys_direct *)directp;
    if (dp->current) dp->current = dp->current->next;
    if (!dp->current)
    {
        for (;dp->entry < FSYS_DIR_HASH_SIZE; ++dp->entry)
        {
            if ((dp->current = dp->hash[dp->entry])) break;
        }
        ++dp->entry;            /* this always needs bumping, even if we found one */
    }
    if (dp->current)
    {
        direct->name = dp->current->name;
        direct->fid = (dp->current->gen_fid);
        ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    }
    else
    {
        direct->name = 0;
        direct->fid = 0;
        ioq->iostatus = QIO_EOF;
    }
    qio_complete(ioq);
    return 0;
}

static int fsys_rewdir( QioIOQ *ioq, void *dirp )
{
    FsysDIR *dp;
    dp = (FsysDIR *)dirp;
    dp->current = 0;
    dp->entry = 0;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_closedir( QioIOQ *ioq, void *dirp )
{
    FsysDIR *dp;
    dp = (FsysDIR *)dirp;
    fsys_freeDIR(dp);
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_seekdir( QioIOQ *ioq, void *dirp )
{
    if (!ioq) return QIO_INVARG;
    return(ioq->iostatus = FSYS_IO_NOSUPP);
}

static int fsys_telldir( QioIOQ *ioq, void *dirp )
{
    if (!ioq) return QIO_INVARG;
    return(ioq->iostatus = FSYS_IO_NOSUPP);
}
#endif

static const QioFileOps fsys_fops = {
    fsys_lseek     /* lseek to specific byte in file */
    ,fsys_read      /* Read from a file */
    ,fsys_write     /* Write to a file */
    ,fsys_ioctl     /* I/O control */
    ,fsys_open      /* open file */
    ,fsys_close     /* close previously opened file */
#if !FSYS_READ_ONLY
    ,fsys_delete    /* delete a file */
    ,fsys_fsync     /* sync the file system */
    ,fsys_mkdir     /* make a directory */
    ,fsys_rmdir     /* remove a directory */
    ,fsys_rename    /* rename a directory */
#else
    ,0          /* no delete */
#if FSYS_UPD_FH
    ,fsys_fsync
#else
    ,0          /* no fsync */
#endif
    ,0          /* no mkdir */
    ,0          /* no rmdir */
    ,0          /* no rename */
#endif
    ,fsys_trunc     /* truncate a file */
    ,fsys_statfs    /* stat a file system */
    ,fsys_fstat     /* stat a file */
    ,fsys_cancel    /* cancel I/O */
    ,fsys_isatty    /* maybe a tty */
    ,fsys_readwpos  /* read with combined lseek */
    ,fsys_writewpos /* write with combined lseek */
#if FSYS_HAS_DIRENT
    ,fsys_opendir   /* open directory */
    ,fsys_seekdir   /* seek directory */
    ,fsys_telldir   /* tell directory */
    ,fsys_rewdir    /* rewind directory */
    ,fsys_readdir   /* read directory entry */
    ,fsys_closedir  /* close directory */
#endif
};

#ifndef FSYS_DEV0_NAME
    #define FSYS_DEV0_NAME "d0"
#endif
#ifndef FSYS_DEV1_NAME
    #define FSYS_DEV1_NAME "d1"
#endif
#ifndef FSYS_DEV2_NAME
    #define FSYS_DEV2_NAME "d2"
#endif
#ifndef FSYS_DEV3_NAME
    #define FSYS_DEV3_NAME "d3"
#endif

static const QioDevice vol0_dvc = {
    FSYS_DEV0_NAME,         /* device name */
    2,                  /* length of name */
    &fsys_fops,             /* list of operations allowed on this device */
    0,                  /* no mutex required for null device */
    0,                  /* unit 0 */
    (void *)(volumes+0)         /* volume 0 */
};

#if FSYS_MAX_VOLUMES > 1
static const QioDevice vol1_dvc = {
    FSYS_DEV1_NAME,         /* device name */
    2,                  /* length of name */
    &fsys_fops,             /* list of operations allowed on this device */
    0,                  /* no mutex required for null device */
    1,                  /* unit 1 */
    (void *)(volumes+1)         /* volume 1 */
};
#endif

#if FSYS_MAX_VOLUMES > 2
static const QioDevice vol2_dvc = {
    FSYS_DEV2_NAME,         /* device name */
    2,                  /* length of name */
    &fsys_fops,             /* list of operations allowed on this device */
    0,                  /* no mutex required for null device */
    2,                  /* unit 2 */
    (void *)(volumes+2)         /* volume 2 */
};
#endif

#if FSYS_MAX_VOLUMES > 3
static const QioDevice vol3_dvc = {
    FSYS_DEV3_NAME,         /* device name */
    2,                  /* length of name */
    &fsys_fops,             /* list of operations allowed on this device */
    0,                  /* no mutex required for null device */
    3,                  /* unit 3 */
    (void *)(volumes+3)         /* volume 3 */
};
#endif

#if !FSYS_READ_ONLY 
enum initfs_states
{
    INITFSC_BEGIN,
    INITFSC_GETPART,
    INITFSC_PARSEPART,
    INITFSC_SETLIMS,
    INITFSC_CLOSE,
    INITFSC_DONE
};

static void initfs_compl(QioIOQ *fioq)
{
    QioIOQ *uioq;
#if FSYS_USE_PARTITION_TABLE
    QioIOQ *newioq;
#endif
    FsysFinit *finp;
    struct stat *drvstat;
    int sts;

    uioq = (QioIOQ *)fioq->private;
    finp = (FsysFinit *)fioq->private2;
    drvstat = (struct stat *)(finp+1);
    sts = fioq->iostatus;
    if (!QIO_ERR_CODE(sts))
    {
        switch (qio_cvtFromPtr(fioq->user))
        {
        case INITFSC_BEGIN: {
#if !FSYS_USE_PARTITION_TABLE
                fioq->user = (void *)INITFSC_SETLIMS;
#else
                fioq->user = (void *)INITFSC_GETPART;
#endif
                finp->base_lba = 0;         /* assume no partitions */
                finp->num_lba = 0;
                sts = qio_fstat(fioq, drvstat);
                if (sts) break;
                return;
            }
#if FSYS_USE_PARTITION_TABLE
        case INITFSC_GETPART: {
                newioq = qio_getioq();
                if (!newioq)
                {
                    sts = QIO_NOIOQ;
                    break;
                }
                newioq->file = fioq->file;
                newioq->complete = initfs_compl;
                fioq->user = newioq->user = (void *)INITFSC_PARSEPART;
                newioq->user2 = fioq;
                sts = qio_readwpos(newioq, 0, &finp->bootsect, BYTPSECT);
                if (sts) break;
                return;
            }
        case INITFSC_PARSEPART: {
                DOSPartition *pp, part;
                int loop;
                newioq = (QioIOQ *)fioq->user2;
                qio_freeioq(fioq);
                fioq = newioq;
                uioq = (QioIOQ *)fioq->private;
                finp = (FsysFinit *)fioq->private2;
                drvstat = (struct stat *)(finp+1);
                if (finp->bootsect.end_sig == 0xAA55)
                {
                    pp = finp->bootsect.parts;
                    for (loop=0; loop < 4; ++loop, ++pp)
                    {
                        if (pp->type == FSYS_USE_PARTITION_TABLE) break;
                    }
                    if (loop < 4)
                    {
                        _memcpy(&part, pp, sizeof(DOSPartition)); /* align the structure */
                        finp->base_lba = part.abs_sect;
                        finp->num_lba = part.num_sects;
                        if (part.abs_sect+part.num_sects > drvstat->st_size)
                        {
                            finp->num_lba = drvstat->st_size-finp->base_lba;
                        }
                        drvstat->st_size = finp->num_lba;
                    }
                }
            }
/* Fall through to INITFSC_SETLIMS */
#endif
        case INITFSC_SETLIMS: {
                U32 lim;
                FsysInitVol *mine;

                lim = drvstat->st_size;
                mine = &finp->iv;
                if (mine->max_lba > 0)
                {
                    if (mine->max_lba < lim) lim = mine->max_lba;
#if defined(FSYS_MAX_LBA) && FSYS_MAX_LBA
                }
                else
                {
                    if (FSYS_MAX_LBA < lim) lim = FSYS_MAX_LBA;
#endif
                }
                lim = lim - (lim%FSYS_MAX_ALTS); /* make total a multiple of MAX_ALTS */
                mine->max_lba = lim;
                if (mine->hb_range > lim) mine->hb_range = lim; /* limit HB range too */
                if (mine->index_sectors + mine->free_sectors + mine->root_sectors < lim)
                {
                    finp->q.callers_ioq = fioq;
                    finp->q.our_ioq.file = fioq->file;
                    fioq->user = (void *)INITFSC_CLOSE;
                    fioq->iostatus = 0;
                    fioq->iocount = 0;
                    init_file_system((QioIOQ *)finp);
                    return;
                }
                sts = FSYS_INITFS_TOOMANYCLUST;
                break;
            }
        case INITFSC_CLOSE:
            fioq->user = (void *)INITFSC_DONE;
            sts = qio_close(fioq);
            if (!sts) return;
            break;
        case INITFSC_DONE:
            sts = FSYS_INITFS_SUCC|SEVERITY_INFO;
            break;
        default:
            sts = FSYS_INITFS_FATAL;
            break;
        }
    }
#if FSYS_USE_PARTITION_TABLE
    if (fioq->user == (void  *)INITFSC_PARSEPART)
    {
        newioq = (QioIOQ *)fioq->user2;
        qio_freeioq(fioq);
        fioq = newioq;
        uioq = fioq->private;
        finp = fioq->private2;
    }
#endif
    QIOfree(finp);
    uioq->iostatus = sts;
    qio_freeioq(fioq);
    qio_complete(uioq);
    return;
}

int fsys_qinitfs(QioIOQ *uioq, const char *what, FsysInitVol *his)
{
    QioIOQ *oioq;
    FsysFinit *finp;
    FsysInitVol *mine;
    int sts;

    if (!uioq) return QIO_NOIOQ;
    if (!his || !what) return uioq->iostatus = QIO_INVARG;
    if (his->cluster != 1) return uioq->iostatus = FSYS_INITFS_CLUSTERNOT1;
    oioq = qio_getioq();
    if (!oioq) return uioq->iostatus = QIO_NOIOQ;
    if (!(finp=QIOcalloc(1, sizeof(FsysFinit)+sizeof(struct stat))))
    {
        qio_freeioq(oioq);
        return uioq->iostatus = QIO_NOMEM;
    }
    oioq->private = uioq;
    oioq->private2 = finp;
    mine = &finp->iv;
    *mine = *his;
    if (!mine->cluster) mine->cluster = FSYS_CLUSTER_SIZE;
    if (!mine->free_sectors) mine->free_sectors = 50;
    if (!mine->index_sectors) mine->index_sectors = 55;
    if (!mine->root_sectors) mine->root_sectors = 60;
    if (!mine->def_extend) mine->def_extend = FSYS_DEFAULT_EXTEND;
    if (!mine->hb_range) mine->hb_range = FSYS_HB_RANGE;
#if 1 && (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    if (!mine->journal_sectors) mine->journal_sectors = FSYS_JOURNAL_FILESIZE;
#endif
    oioq->complete = initfs_compl;
    oioq->user = (void *)0;
    sts = qio_open(oioq, what, O_RDWR);
    if (sts)
    {
        QIOfree(finp);
        uioq->iostatus = sts;
        qio_freeioq(oioq);
    }
    return sts;
}

int fsys_initfs(const char *what, FsysInitVol *his)
{
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) return FSYS_INITFS_BADLVL;   /* cannot call this at AST level */
    if (!his) return FSYS_INITFS_NOIV;              /* invalid argument */
    ioq = qio_getioq();
    if (!ioq) return QIO_NOIOQ;
    sts = fsys_qinitfs(ioq, what, his);
    while (!sts) sts = ioq->iostatus;
    qio_freeioq(ioq);
    return sts;
}
#endif					/* !FSYS_READ_ONLY && !FSYS_USE_PARTITION_TABLE */

void fsys_init(void)
{
    int ii;
#if TEST_DISK_TIMEOUT
    {
        int oldopt = prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);
        txt_str(-1, AN_VIS_ROW/2, "Fake disk timeouts enabled", RED_PAL);
        prc_delay(4*60);
        prc_delay_options(oldopt);
    }
#endif
    for (ii=0; ii < n_elts(volumes); ++ii)
    {
        volumes[ii].id = FSYS_ID_VOLUME;
        volumes[ii].reader.vol = volumes + ii;
#if FSYS_UPD_FH
        volumes[ii].sync_work.our_ioq.vol = volumes + ii;
#endif
    }
    inx_mask = -1;
    while ( inx_mask & QIO_MAX_FILES ) inx_mask <<= 1;
    inx_mask = ~inx_mask;

#if (FSYS_SQUAWKING || FSYS_FREE_SQUAWKING || FSYS_SYNC_SQUAWKING || FSYS_WRITE_SQUAWKING) && !FSYS_SQUAWKING_STDIO
    if (!fsys_iop)
        fsys_iop = iio_open(1);     /* open a connection to thread 1 */
#endif
    qio_install_dvc(&vol0_dvc);
#if FSYS_MAX_VOLUMES > 1
    qio_install_dvc(&vol1_dvc);
#endif
#if FSYS_MAX_VOLUMES > 2
    qio_install_dvc(&vol2_dvc);
#endif
#if FSYS_MAX_VOLUMES > 3
    qio_install_dvc(&vol3_dvc);
#endif
    return;
}

void fsys_force_sync(int millsecs)
{
#if !FSYS_READ_ONLY && !FSYS_NO_AUTOSYNC
    int ii;
    U32 tmr;
    FsysVolume *vol;

    if (!prc_get_actlvl() && prc_get_astlvl() < 0)    /* if not in interrupt or AST routine */
    {
        vol = volumes;
        for (ii=0; ii < n_elts(volumes); ++ii, ++vol)
        {
            if (vol->dirty_ffree && !vol->sync_work.busy) /* if there's something to do and not already busy */
            {
                fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER); /* start a sync task */
                tmr = prc_get_count();
                while (vol->dirty_ffree)
                {
                    U32 t;
                    t = prc_get_count() - tmr;
#if !defined(COUNTS_PER_USEC)
#define COUNTS_PER_USEC (CPU_SPEED/2000000)
#endif
                    t /= 1000*COUNTS_PER_USEC;
                    if (t >= millsecs) break;
#if defined(WDOG) && !NO_WDOG && ST_GAME_CODE
                    WDOG = 0;       /* give WDOG a kick */
#endif
                }
            }
        }
    }
#endif
}

int fsys_sync_status(void)
{
    int sts = 0;
#if !FSYS_READ_ONLY 
    int ii;

    for (ii=0; ii < n_elts(volumes); ++ii)
    {
        if (volumes[ii].sync_work.busy)
        {
            sts |= 1<<ii;
        }
    }
#endif
    return sts;
}

#if !defined(NO_FSYS_TEST) || !NO_FSYS_TEST

    #if 1
        #ifndef LED_ON
            #define LED_ON(x)	do { *(VU32*)LED_OUT &= ~(1<<B_LED_##x); } while (0)
        #endif
        #ifndef LED_OFF
            #define LED_OFF(x)	do { *(VU32*)LED_OUT |= (1<<B_LED_##x); } while (0)
        #endif
    #else
        #ifndef LED_ON
            #define LED_ON(x)	do { ; } while (0)
        #endif
        #ifndef LED_OFF
            #define LED_OFF(x)	do { ; } while (0)
        #endif
    #endif

    #define GET_MEM(x) QIOmalloc(x)
    #define FREE_MEM(a, x) QIOfree(x)

    #ifndef FSYS_TIME_NOISY_TEST
        #define FSYS_TIME_NOISY_TEST (0)
    #endif

typedef struct chksums
{
    U32 fid;
    U32 chksum;
} ChkSum;

typedef struct dir_walker
{
    FsysDIR *dp;
    QioIOQ *ioq;
    void *data_buff_p;      /* pointer to _real_ memory for this buffer */
    char *data_buff;        /* data_buff_p aligned to cache line boundary */
    int data_buff_size;     /* size of usable portion of data_buff_p */
    struct _reent *data_buff_re; /* which area we allocated space */
    int errors;
    int cs_errors;
    int cs;
    int file_size;
    ChkSum *cksums;
    ChkSum *this_cs;
    struct _reent *cksums_re;
    int numb_cksums;
    int banners;
    int correctables;
    int used_alts;
    int pass;           /* which copy to process */
    int filenum;        /* filenumber */
    int loop_count;     /* number of times through the loop */
    char *fname;        /* temporary place for filename */
    char *volfn;
    int fname_size;
    int fname_idx;
    U32 fid;
#if FSYS_TIME_NOISY_TEST
    U32 start_time;     /* MEA added to time test */
    U32 pass1_time;     /* MEA added to time test */
#endif
} Walker;

    #define WALK_EXIT_OK	0	/* walk completed with success */
    #define WALK_EXIT_ABORT	1	/* operator requests abort */
    #define WALK_EXIT_ERR	2	/* walk completed with error */
    #define WALK_EXIT_PAUSE 3	/* pause before continuing */

    #define WALK_ANN_ROW	(AN_VIS_ROW/3)
    #define WALK_FNAME_ROW	(WALK_ANN_ROW+2)
    #define WALK_ERR_ROW	(WALK_ANN_ROW+4)
    #define WALK_COL	(-1)

    #define WALK_CKSUM_ROW	(WALK_ERR_ROW+7)
    #define WALK_CKSUM_MSG	"File checksum errors:          "
    #define WALK_CKSUM_COL	((AN_VIS_COL-sizeof(WALK_CKSUM_MSG)-1-4)/2)

    #define WALK_UCORR_ROW	(WALK_ERR_ROW+8)
    #define WALK_UCORR_MSG	"Uncorrectable disk errors:     "
    #define WALK_UCORR_COL	((AN_VIS_COL-sizeof(WALK_UCORR_MSG)-1-4)/2)

    #define WALK_CORR_ROW	(WALK_ERR_ROW+9)
    #define WALK_CORR_MSG	"Correctable disk read errors:  "
    #define WALK_CORR_COL	((AN_VIS_COL-sizeof(WALK_CORR_MSG)-1-4)/2)

    #define WALK_ALTS_ROW	(WALK_ERR_ROW+10)
    #define WALK_ALTS_MSG	"Alternates used:               "
    #define WALK_ALTS_COL	((AN_VIS_COL-sizeof(WALK_ALTS_MSG)-1-4)/2)

static void find_cs(int fid, Walker *wp)
{
    int ii;
    ChkSum *csp;
    csp = wp->cksums;
    wp->this_cs = 0;
    if (csp) for (ii=0; ii < wp->numb_cksums; ++ii, ++csp)
        {
            if (csp->fid == fid)
            {
                wp->this_cs = csp;
                break;
            }
        }
    return;
}

    #ifndef FSYS_NOISY_CHECK
        #define FSYS_NOISY_CHECK	0	/* don't display filenames */
    #endif

    #if FSYS_NOISY_CHECK
        #include <nsprintf.h>
    #endif

    #if !FSYS_NOISY_CHECK
static int cat_name(const char *name, Walker *wp)
{
    int len;
    len = strlen(name);
    if (len+wp->fname_idx > wp->fname_size)
    {
        char *new, *old;
        int nsize = wp->fname_size + len + 256;
        new = GET_MEM(nsize);
        if (!new) return 0;
        old = wp->fname;
        wp->fname = new;
        if (old) FREE_MEM(wp->fname_size, old);
        wp->fname_size = nsize;
    }
    if (wp->fname[wp->fname_idx] != QIO_FNAME_SEPARATOR && name[0] != QIO_FNAME_SEPARATOR)
    {
        wp->fname[wp->fname_idx++] = QIO_FNAME_SEPARATOR;
    }
    strcpy(wp->fname+wp->fname_idx, name);
    wp->fname_idx += len;
    return 1;
}

static char *fid2name(const char *name, Walker *wp)
{
    int sts;
    FsysDIR *dp;
    QioIOQ *ioq;
    char *ans = 0;

    if (!name || !wp) return 0;
    if (!cat_name(name, wp)) return 0;
    ioq = qio_getioq();
    if (!ioq) return 0;
    sts = qiow_opendir(ioq, (void *)&dp, wp->fname);    /* open directory */
    if (!QIO_ERR_CODE(sts))
    {
        while (1)
        {
            FsysOpenT ot;
            QioIOQ *nioq;
            struct fsys_direct dir;
            sts = qiow_readdir(ioq, dp, &dir);
            if (QIO_ERR_CODE(sts)) break;
            if (strcmp(dir.name, "..") == 0 || strcmp(dir.name, ".") == 0) continue;
            if (dir.fid == wp->fid)
            {
                cat_name(dir.name, wp);
                ans = wp->fname;
                break;
            }
            ot.spc.path = wp->volfn;
            ot.spc.mode = O_RDONLY;
            ot.fid = dir.fid;
            nioq = qio_getioq();
            if (nioq)
            {
                sts = qiow_openspc(nioq, &ot.spc);
                qio_close(nioq);
                qio_freeioq(nioq);
                if (!QIO_ERR_CODE(sts))
                {
                    if (ot.mkdir)
                    {
                        int old_idx;
                        old_idx = wp->fname_idx;
                        ans = fid2name(dir.name, wp);
                        if (ans) break;
                        wp->fname[old_idx] = 0;
                        wp->fname_idx = old_idx;
                    }
                }
            }
        }
    }
    qio_closedir(ioq, dp);
    qio_freeioq(ioq);
    return ans;
}

static void show_fname(Walker *wp)
{
    wp->fname_idx = 0;
    if (fid2name(wp->volfn, wp))
    {
        int len;
        char *start;

        len = strlen(wp->fname);
        if (len > AN_VIS_COL-4)
        {
            start = wp->fname + (len - AN_VIS_COL-4);
            txt_str(2, WALK_FNAME_ROW, "...", WHT_PAL);
            txt_cstr(start, GRN_PAL);
        }
        else
        {
            txt_str(-1, WALK_FNAME_ROW, wp->fname, GRN_PAL);
        }
    }
}
    #endif

static int read_check(QioIOQ *ioq, Walker *wp)
{
    int sts, cs;
    U32 *up;
#ifdef EER_FSYS_USEALT
    int used_alts;
#endif
#ifdef EER_DSK_CORR
    int corrs;
#endif
    cs = 0;
    wp->file_size = 0;
    while (1)
    {
        if ((ctl_read_sw(0)&SW_NEXT)) return WALK_EXIT_ABORT;
#ifdef EER_FSYS_USEALT
        used_alts = eer_gets(EER_FSYS_USEALT);
#endif
#ifdef EER_DSK_CORR
        corrs = eer_gets(EER_DSK_CORR);
#endif
#if TEST_DISK_TIMEOUT
        if ((ctl_read_sw(SW_ACTION)&SW_ACTION))
        {
            ioq->timeout = 1000;        /* short timeout */
        }
        else
        {
            ioq->timeout = 0;
        }
#endif
        sts = qiow_read(ioq, wp->data_buff, wp->data_buff_size);
#ifdef EER_FSYS_USEALT
        if (used_alts != eer_gets(EER_FSYS_USEALT)) ++wp->used_alts;
#endif
#ifdef EER_DSK_CORR
        if (corrs != eer_gets(EER_DSK_CORR)) wp->correctables += eer_gets(EER_DSK_CORR)-corrs;
#endif
        if (QIO_ERR_CODE(sts))
        {
            if (sts == QIO_EOF)
            {
                wp->cs = cs;
                return 0;
            }
#if !FSYS_NOISY_CHECK
            show_fname(wp);
#endif
            qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
            txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
            break;
        }
        wp->file_size += ioq->iocount;
        up = (U32*)wp->data_buff;
        if (wp->this_cs)
        {
            if ((ioq->iocount&3))
            {
                char *scp;
                scp = wp->data_buff + ioq->iocount;
                switch (ioq->iocount&3)
                {
                case 1: *scp++ = 0;
                case 2: *scp++ = 0;
                case 3: *scp++ = 0;
                }
            }
            for (sts=0; sts < ioq->iocount; sts += 4)
            {
                cs += *up++;
            }
        }
    }   
    ++wp->errors;
    return WALK_EXIT_ERR;
}

    #if defined(TEST_DISK_ERRORS) && TEST_DISK_ERRORS
        #define FAKE_CS_ERROR (ctl_read_sw(J1_RIGHT)&J1_RIGHT)
    #else
        #define FAKE_CS_ERROR (0)
    #endif

    #define REPLY_TIMEOUT	(5*60)		/* pause for 5 seconds */

static int wait_for_reply(Walker *wp, int timeout)
{
    int sts, rtc;

    if ((ctl_read_sw(0)&SW_NEXT)) /* see if next button pressed */
    {
        txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
        txt_clr_wid(2, AN_VIS_ROW-2, AN_VIS_COL-4);
        st_insn(AN_VIS_ROW-2, t_msg_ret_menu, "Release button", INSTR_PAL);
    }
    else
    {
        st_insn(AN_VIS_ROW-3, "To continue,", t_msg_action, INSTR_PAL);
    }
    rtc = eer_rtc;
    ctl_read_sw(SW_ACTION|SW_NEXT); /* purge edges */
    while (!(sts=ctl_read_sw(0)&(SW_ACTION|SW_NEXT))) /* wait for button closure */
    {
        if (timeout && (eer_rtc - rtc) > timeout) break;
        prc_delay(0);
    }
    if (sts)              /* got a button */
    {
        while ((ctl_read_sw(0)&(SW_ACTION|SW_NEXT))) prc_delay(0); /* wait for 'em to let go */
    }
    txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
#if !FSYS_NOISY_CHECK
    txt_clr_wid(4, WALK_FNAME_ROW, AN_VIS_COL-6);
#endif
    txt_clr_wid(4, WALK_ERR_ROW+0, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+1, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+2, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+3, AN_VIS_COL-6);
    if ((sts&SW_NEXT))
    {
        return WALK_EXIT_ABORT;
    }
    return 0;
}

static void show_errors(Walker *walk)
{
    if (walk->errors)
    {
        if (!(walk->banners&1))
        {
            txt_str(WALK_UCORR_COL, WALK_UCORR_ROW, WALK_UCORR_MSG, MNORMAL_PAL);
            walk->banners |= 1;
        }
        txt_decnum(WALK_UCORR_COL+sizeof(WALK_UCORR_MSG)-1,
                   WALK_UCORR_ROW, walk->errors, 4, RJ_BF, RED_PAL);
    }
    if (walk->cs_errors)
    {
        if (!(walk->banners&2))
        {
            txt_str(WALK_CKSUM_COL, WALK_CKSUM_ROW, WALK_CKSUM_MSG, MNORMAL_PAL);
            walk->banners |= 2;
        }
        txt_decnum(WALK_CKSUM_COL+sizeof(WALK_CKSUM_MSG)-1,
                   WALK_CKSUM_ROW, walk->cs_errors, 4, RJ_BF, RED_PAL);
    }
#ifdef EER_DSK_CORR
    if (walk->correctables)
    {
        if (!(walk->banners&4))
        {
            txt_str(WALK_CORR_COL, WALK_CORR_ROW, WALK_CORR_MSG, MNORMAL_PAL);
            walk->banners |= 4;
        }
        txt_decnum(WALK_CORR_COL+sizeof(WALK_CORR_MSG)-1,
                   WALK_CORR_ROW, walk->correctables, 4, RJ_BF, YEL_PAL);
    }
#endif
#ifdef EER_FSYS_USEALT
    if (walk->used_alts)
    {
        if (!(walk->banners&8))
        {
            txt_str(WALK_ALTS_COL, WALK_ALTS_ROW, WALK_ALTS_MSG, MNORMAL_PAL);
            walk->banners |= 8;
        }
        txt_decnum(WALK_ALTS_COL+sizeof(WALK_ALTS_MSG)-1,
                   WALK_ALTS_ROW, walk->used_alts, 4, RJ_BF, YEL_PAL);
    }
#endif
}

    #if FSYS_NOISY_CHECK
static int walk_directory(Walker *wp, const char *dir_name)
{
    int sts, memamt;
    struct fsys_direct dir;
    char *new_name=0, *cp;
    QioIOQ *ioq, *nioq=0;
    FsysOpenT ot;
    char emsg[AN_VIS_COL_MAX];

    ioq = wp->ioq;

    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    while (1)
    {
        if (ctl_read_sw(SW_NEXT)&SW_NEXT)
        {
            sts = WALK_EXIT_ABORT;
            break;
        }
        sts = qiow_readdir(ioq, wp->dp, &dir);
        if (QIO_ERR_CODE(sts))
        {
            if (sts == QIO_EOF)
            {
                sts = WALK_EXIT_OK;
            }
            else
            {
                qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
                txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
                ++wp->errors;
            }
            break;
        }
        if (strcmp(dir.name, ".") == 0 || strcmp(dir.name, "..") == 0) continue;
        memamt = sts+strlen(dir_name)+2;
        new_name = GET_MEM(memamt);
        strcpy(new_name, dir_name); /* start with old name */
        cp = new_name + strlen(new_name);
        *cp++ = QIO_FNAME_SEPARATOR;
        strcpy(cp, dir.name);       /* filename */
        nioq = qio_getioq();
        if (!nioq)
        {
            txt_str(-1, WALK_ERR_ROW, "Ran out of IOQ's", RED_PAL);
            ++wp->errors;
            sts = WALK_EXIT_ERR;    /* procedure error */
            break;
        }
        nioq->timeout = ioq->timeout;   /* set same as caller's timeout */
        ot.spc.path = new_name;
        ot.spc.mode = O_RDONLY|O_OPNCPY;
        ot.fid = 0;
        ot.copies = wp->pass;       /* which copy to open */
        wp->filenum += 1;       /* Count even the ones without alts */
        sts = qiow_openspc(nioq, &ot.spc);
        if (QIO_ERR_CODE(sts))
        {
            if (!wp->pass || sts != FSYS_LOOKUP_FNF)
            {
                qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
                txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
                ++wp->errors;
                sts = WALK_EXIT_ERR;
                break;
            }
            qio_freeioq(nioq);      /* done with this */
            nioq = 0;
            FREE_MEM(memamt, new_name);
            new_name = 0;
            continue;           /* FNF's on alternate copies is ok */
        }
        find_cs(ot.fid, wp);        /* find checksum in checksum file */
        nsprintf(emsg, sizeof(emsg), "Checking file %5d, copy %d of %d%s",
                 wp->filenum-1, wp->pass+1, ot.copies, wp->this_cs ? " (CS):" : ":     ");
        txt_str(4, WALK_ANN_ROW, emsg, WHT_PAL);
        txt_clr_wid(4, WALK_ANN_ROW+2, AN_VIS_COL-5);
        sts = strlen(new_name);
        if (sts > AN_VIS_COL-4-2)
        {
            char *np = new_name;
            int dif = (sts - AN_VIS_COL-4-2);
            np += dif + 4;
            txt_str(4, WALK_ANN_ROW+2, "... ", WHT_PAL);
            txt_cstr(np, GRN_PAL);
        }
        else
        {
            txt_str(4, WALK_ANN_ROW+2, new_name, GRN_PAL);
        }
        prc_delay(0);
        if (ot.mkdir) /* if new file is a directory */
        {
            FsysDIR *olddp;

            sts = read_check(nioq, wp); /* read check the directory */
            qio_close(nioq);    /* don't need this resource */
            if (!sts)
            {
                olddp = wp->dp;
                sts = qiow_opendir(nioq, (void *)&wp->dp, new_name); /* open directory */
                if (QIO_ERR_CODE(sts))
                {
                    wp->dp = olddp;
                    qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
                    txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
                    ++wp->errors;
                    sts = WALK_EXIT_ERR;
                }
                else
                {
                    sts = walk_directory(wp, new_name); /* then walk the directory */
                    qio_closedir(nioq, wp->dp);
                    wp->dp = olddp;
                }
            }
        }
        else
        {
            ChkSum *csp;
            sts = read_check(nioq, wp); /* read check the file */
            if (!sts && (csp=wp->this_cs))
            {
                if (csp->chksum != wp->cs || FAKE_CS_ERROR)
                {
#define BAD_CS_MSG1 "Checksum error on file."
#define BAD_CS_MSG2 "expected"
#define BAD_CS_MSG3 "computed"
                    txt_str(-1, WALK_ERR_ROW, BAD_CS_MSG1, YEL_PAL);
                    txt_str((AN_VIS_COL-3-sizeof(BAD_CS_MSG2)-1-8)/2,
                            WALK_ERR_ROW+1, BAD_CS_MSG2, YEL_PAL);
                    txt_cstr(" 0x", GRN_PAL);
                    txt_chexnum(csp->chksum, 8, RJ_ZF, GRN_PAL);
                    txt_str((AN_VIS_COL-3-sizeof(BAD_CS_MSG3)-1-8)/2,
                            WALK_ERR_ROW+2, BAD_CS_MSG3, YEL_PAL);
                    txt_cstr(" 0x", RED_PAL);
                    txt_chexnum(wp->cs, 8, RJ_ZF, RED_PAL);
#if 0
                    txt_str(-1, WALK_ERR_ROW+3, "FID  ", WHT_PAL);
                    txt_chexnum(ot.fid, 8, RJ_ZF, WHT_PAL);
                    txt_str(-1, WALK_ERR_ROW+4, "Size ", WHT_PAL);
                    txt_chexnum(ot.eof, 10, RJ_ZF, WHT_PAL);
#endif
                    sts = WALK_EXIT_ERR;
                    ++wp->cs_errors;
                }
            }
            qio_close(nioq);    /* done with this */
        }
        if (nioq) qio_freeioq(nioq);
        if (new_name) FREE_MEM(memamt, new_name);
        if (sts == WALK_EXIT_ABORT) break;
        show_errors(wp);        /* always update this in case correctable errors are logged */
        if (sts == WALK_EXIT_ERR)
        {
            sts = wait_for_reply(wp, REPLY_TIMEOUT); /* but only pause on hard errors */
        }
    }
    return sts;
}
    #endif

    #if !FSYS_NOISY_CHECK
static int show_filenumbers(QioIOQ *ioq, const char *volfn, FsysVolume *vol, Walker *walk)
{
    int sts = WALK_EXIT_OK;
    U8 *history;
    int fid, pass, row;

#if FSYS_MAX_ALTS > 3
#error *** You will have to fix this!!!
#endif
    history = QIOcalloc(vol->files_ffree, 1);   /* one byte per file */
    if (!history) return -1;            /* no memory */
#define CHECK_MSG "Checking file: "
    txt_str((AN_VIS_COL-sizeof(CHECK_MSG)-5)/2, WALK_ANN_ROW, CHECK_MSG, WHT_PAL);
    for (pass=0; pass < FSYS_MAX_ALTS; ++pass)
    {
        for (fid=0; fid < vol->files_ffree; ++fid)
        {
            FsysOpenT ot;
            FsysRamFH *rfh;
            int afid;

            rfh = fsys_find_ramfh(vol, fid);
            if (!rfh->valid) continue;
            if (!rfh->ramrp[pass].rptrs)
            {
                history[fid] |= 1 << (pass*2);  /* this copy is unusable */
                continue;           /* has no alternate */
            }
            memset((char *)&ot, 0, sizeof(ot));
            ot.spc.path = volfn;
            ot.spc.mode = O_RDONLY|O_OPNCPY;
            walk->fid = afid = ot.fid = (rfh->generation << FSYS_DIR_GENSHF) | fid;
            ot.copies = pass;
            if (walk->cksums) find_cs(afid, walk);
            txt_decnum((AN_VIS_COL-sizeof(CHECK_MSG)-5)/2+sizeof(CHECK_MSG),
                       WALK_ANN_ROW, fid, 5, RJ_BF, GRN_PAL);
            txt_cstr(".", GRN_PAL);
            txt_cdecnum(pass, 1, RJ_ZF, GRN_PAL);
            prc_delay(0);
            sts = qiow_openspc(ioq, &ot.spc);
            if (QIO_ERR_CODE(sts))
            {
                history[fid] |= 1 << (pass*2);
                show_fname(walk);
                qio_errmsg(sts, walk->data_buff, walk->data_buff_size);
                txt_str(-1, WALK_ERR_ROW, walk->data_buff, YEL_PAL);
                ++walk->errors;
                sts = wait_for_reply(walk, REPLY_TIMEOUT);
            }
            else
            {
                ChkSum *csp;
                sts = read_check(ioq, walk);
                qio_close(ioq);
                if (sts == WALK_EXIT_ERR)
                {
                    history[fid] |= 1 << (pass*2);
                }
                if (!sts && ((csp=walk->this_cs) || FAKE_CS_ERROR ))
                {
                    if (!csp || (csp->chksum != walk->cs))
                    {
#define BAD_CS_MSG1 "Checksum error on file."
                        txt_str(-1, WALK_ERR_ROW, BAD_CS_MSG1, YEL_PAL);
                        show_fname(walk);
                        history[fid] |= 2 << (pass*2);
                        ++walk->cs_errors;
                        sts = 1;
                    }
                }
                show_errors(walk);
                if (sts) sts = wait_for_reply(walk, REPLY_TIMEOUT);
            }
            if (sts == WALK_EXIT_ABORT) break;
        }
        if (sts == WALK_EXIT_ABORT) break;
    }
    if (sts != WALK_EXIT_ABORT)
    {
        int pause = 0;
        for (row=WALK_ANN_ROW; row <= WALK_ERR_ROW; ++row)
        {
            txt_clr_wid(2, row, AN_VIS_COL-4);
        }
        if (walk->loop_count < 2)
        {
            if (sts == WALK_EXIT_OK)
            {
                if (!walk->errors && !walk->cs_errors && !walk->used_alts)
                {
                    if (walk->correctables)
                    {
                        txt_str(-1, AN_VIS_ROW/2-1, "The errors listed will be corrected by h/w.",
                                WHT_PAL);
                        txt_str((AN_VIS_COL-16)/2+1, AN_VIS_ROW/2+1, "Filesystem is ", WHT_PAL);
                        txt_cstr("OK", GRN_PAL);
                        pause = WALK_EXIT_PAUSE;
                    }
                }
                else
                {
                    int cs_err=0, fatal=0, jj;
                    for (sts=0; sts < vol->files_ffree; ++sts)
                    {
                        int hw = 0;
                        if ((history[sts]&0xAA))
                        {
                            ++cs_err;
                        }
                        else
                        {
                            for (jj=0; jj < FSYS_MAX_ALTS; ++jj)
                            {
                                if ((history[sts]&(1<<jj*2))) ++hw;
                            }
                            if (hw >= FSYS_MAX_ALTS) ++fatal;
                        }
                    }
                    if (fatal || cs_err)
                    {
                        int c;
                        char *str;
                        str = (fatal+cs_err > 1) ?
                              "Some of the errors listed cannot be corrected." :
                              "The errors listed cannot be corrected.";
                        txt_str(-1, WALK_ERR_ROW+3, str, YEL_PAL);
                        if (fatal)
                        {
                            txt_str(-1, WALK_ERR_ROW+5, "The filesystem is broken and is not usable", RED_PAL);
                        }
                        else
                        {
#define NFG_MSG_P " files are corrupt. The game might not work."
#define NFG_MSG_S " file is corrupt. The game might not work."
                            if (cs_err > 1)
                            {
                                c = (AN_VIS_COL-sizeof(NFG_MSG_P)-1-4)/2;
                                str = NFG_MSG_P;
                            }
                            else
                            {
                                c = (AN_VIS_COL-sizeof(NFG_MSG_S)-1-4)/2;
                                str = NFG_MSG_S;
                            }
                            txt_decnum(c, WALK_ERR_ROW+5, cs_err, 4, RJ_BF, RED_PAL);
                            txt_cstr(str, YEL_PAL);
                        }
                    }
                    else
                    {
                        txt_str(-1, WALK_ERR_ROW+3, "The errors listed can be corrected by s/w.", WHT_PAL);
                        txt_str(-1, WALK_ERR_ROW+5, "The filesystem is unhealthy but remains usable", YEL_PAL);
                    }
                    pause = WALK_EXIT_PAUSE;
                }
            }
            else
            {
                txt_str(-1, WALK_ERR_ROW+3, "Due to the severity of the errors detected,", WHT_PAL);
                txt_str(-1, WALK_ERR_ROW+4, "the filesystem is probably not usable", YEL_PAL);
                pause = WALK_EXIT_PAUSE;
            }
        }
        sts = pause;
    }
    QIOfree(history);
    return sts;
}
    #endif

    #if FSYS_MAX_VOLUMES > 1
extern int ide_choose_drv(int choices, const char *msg);
    #endif

static U32 mvb_cookie;

    #if 0
static void mvb_display(int pcnt)
{
    U32 cookie;
    cookie = txt_setpos(mvb_cookie);
    txt_cdecnum(pcnt, 2, RJ_BF, GRN_PAL);
    txt_setpos(cookie);
    prc_delay(0);
}
    #endif

static int sel_and_mount(char *rawfn, char *volfn, Walker *walk, const struct menu_d *smp)
{
    int active, sts;
    static int been_here;
    QioIOQ *mioq;

    mioq = qio_getioq();
#if FSYS_MAX_VOLUMES > 1
    active = -1;
    sts = qiow_open( mioq, "/rd0", O_RDONLY );
    if ( !QIO_ERR_CODE( sts ) )
    {
        sts = qiow_ioctl( mioq, HD_IOC_DVCMAP, NULL );
        if ( !QIO_ERR_CODE( sts ) )
        {
            if ( (mioq->iocount>>8) == 1 )
            {
                for ( sts=0; sts < NUM_HDRIVES; ++sts )
                {
                    if ( (mioq->iocount&(1<<sts)) )
                    {
                        active = sts;
                        break;
                    }
                }
            }
        }
        qiow_close( mioq );
    }
    qio_freeioq(mioq);
    if ( active < 0 )
        active = ide_choose_drv((1<<FSYS_MAX_VOLUMES)-1, "Select volume to test:");
#else
    active = 0;
#endif
#if FSYS_MAX_VOLUMES > 9
#error You need to rework this code
#endif
    rawfn[0] = QIO_FNAME_SEPARATOR;
    rawfn[1] = 'r';
    rawfn[2] = 'd';
    rawfn[3] = '0' + active;
    rawfn[4] = 0;
    volfn[0] = QIO_FNAME_SEPARATOR;
    volfn[1] = 'd';
    volfn[2] = '0' + active;
    volfn[3] = 0;
    if (!(been_here&(1<<active)))
    {
#ifdef EER_FSYS_USEALT
        int alts;
        alts = eer_gets(EER_FSYS_USEALT);
#endif
        if (!been_here)
        {
            ide_squawk(AN_VIS_ROW/2, (AN_VIS_COL-39-FSYS_MAX_VOLUMES-1)/2);
            ide_init();
            ide_unsquawk();
        }
#define MOUNTING_MSG "Loading filesystem"
        if (smp) txt_str((AN_VIS_COL-sizeof(MOUNTING_MSG)-1-4-(FSYS_MAX_VOLUMES>1?2:0)-4)/2,
                         AN_VIS_ROW/2, MOUNTING_MSG, WHT_PAL);
#if FSYS_MAX_VOLUMES > 1
        txt_cstr(" ", WHT_PAL);
        txt_cdecnum(active, 1, RJ_BF, WHT_PAL);
#endif
        txt_cstr(" ... ", WHT_PAL);
        mvb_cookie = txt_setpos(0);
        txt_cstr("  %", WHT_PAL);
        prc_delay(0);
#if 0
        sts = fsys_mountwcb(rawfn, volfn, mvb_display);
#else
        {
            int pcnt;
            struct stat lst;

            mioq = qio_getioq();
            sts = fsys_mount(mioq, rawfn, volfn, (U32 *)&lst, sizeof(lst));
            pcnt = mioq->iocount;
            while (!sts)
            {
                if (pcnt != mioq->iocount)
                {
                    pcnt = mioq->iocount;
                    txt_setpos(mvb_cookie);
                    txt_cdecnum(pcnt, 2, RJ_BF, GRN_PAL);
                    prc_delay(0);
                }
                sts = mioq->iostatus;
            }
            qio_freeioq(mioq);
        }
#endif
        txt_clr_wid(2, AN_VIS_ROW/2, AN_VIS_COL-4);
        if (QIO_ERR_CODE(sts) && sts != FSYS_MOUNT_MOUNTED)
        {
            char emsg[AN_VIS_COL_MAX];
            txt_str(-1, WALK_ERR_ROW, "Failed to mount filesystem.", WHT_PAL);
            qio_errmsg(sts, emsg, sizeof(emsg));
            txt_str(-1, WALK_ERR_ROW+2, emsg, YEL_PAL);
            st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
            while (!(ctl_read_sw(SW_NEXT)&SW_NEXT))
            {
                prc_delay(0);
            }
            return -1;
        }
#ifdef EER_FSYS_USEALT
        if (walk) walk->used_alts += eer_gets(EER_FSYS_USEALT) - alts;
#endif
        been_here |= 1<<active;
        prc_delay(0);               /* clear the screen */
    }
    return active;
}

    #if !FSYS_READ_ONLY && FSYS_NO_AUTOSYNC 
        #define FSYS_TEST_NAME filesystem_test
    #else
        #define FSYS_TEST_NAME fsys_test
    #endif

    #if !FSYS_READ_ONLY && FSYS_NO_AUTOSYNC 
static
    #endif
int FSYS_TEST_NAME(const struct menu_d *smp)
{
    QioIOQ *ioq;
    int sts, row, active;
    FsysOpenT ot;
    char rawfn[10], volfn[10];
    Walker walk;
    char *msg;
    FsysVolume *vol;
    QioFile *fp;
    U32 lc_pos;
#if FSYS_NOISY_CHECK
    FsysDIR *dp;
#endif
#if defined(GUTS_OPT_RPT_DISK_ERRS)
    int old_debug = debug_mode;

    debug_mode |= GUTS_OPT_RPT_DISK_ERRS;
#endif

    memset((char *)&walk, 0, sizeof(walk));

    active = sel_and_mount(rawfn, volfn, &walk, smp);
    if (active < 0) return 0;
    if (smp) st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    ioq = qio_getioq();
    if (!ioq)
    {
        txt_str(-1, WALK_ERR_ROW, "Ran out of IOQ's", RED_PAL);
        goto err_exit;
    }
    {
        int32_t siz;
        unsigned char *bufp;
#if !FSYS_USE_MALLOC
        int32_t amt;
        struct _reent *where;

        siz = 65536l;
        do
        {
            amt = siz+QIO_CACHE_LINE_SIZE;
            where = malloc_from_any_pool((void **)&bufp, amt);
            if (!where || !bufp)
            {
                siz /= 2;
                continue;
            }
            break;
        } while (siz > 4096);
        walk.data_buff_re = where;
#else
        siz = 65536L-QIO_CACHE_LINE_SIZE;
        bufp = malloc(siz+QIO_CACHE_LINE_SIZE);
#endif
        if (!bufp)
        {
            txt_str(-1, WALK_ERR_ROW, "Not enough free memory to test filesystem", RED_PAL);
            qio_freeioq(ioq);
            ioq = 0;
            goto err_exit;
        }
        walk.data_buff_p = (void *)bufp;
        walk.data_buff = (char *)((int)(bufp + QIO_CACHE_LINE_SIZE-1) & -QIO_CACHE_LINE_SIZE);
        walk.data_buff_size = siz;
    }
    memset(walk.data_buff, 0, walk.data_buff_size);
    volfn[3] = QIO_FNAME_SEPARATOR;
    volfn[4] = '.';
    volfn[5] = 0;
    sts = qiow_open(ioq, volfn, O_RDONLY);
    if (QIO_ERR_CODE(sts))
    {
        strcpy(walk.data_buff, "Error opening ");
        strcat(walk.data_buff, volfn);
        msg = walk.data_buff;
        txt_str(-1, AN_VIS_ROW-7, msg, WHT_PAL);
        qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
        txt_str(-1, AN_VIS_ROW-6, walk.data_buff, YEL_PAL);
        err_exit:
        st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
        if (ioq) qio_freeioq(ioq);
#if !FSYS_USE_MALLOC
        if (walk.data_buff_re && walk.data_buff_p) _free_r(walk.data_buff_re, walk.data_buff_p);
#else
        if (walk.data_buff_p) free(walk.data_buff_p);
#endif
        while (!(ctl_read_sw(SW_NEXT)&SW_NEXT))
        {
            prc_delay(0);
        }
        return 1;
    }
    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
#define DISK_DRIVE_SIZ   "Disk space available:  "
#define SPACE_USED_MSG   "Filesystem occupies:   "
#define FILES_USED_MSG   "Total files used:      "
#define JOURNAL_PRESENT  "Journalling enabled:   "
#define LOOP_COUNT_MSG   "Passes completed:    "
    row = 3;
    {
        struct stat st;
        U32 lim;
        if (!vol->hd_maxlba && !vol->hd_offset)
        {
            sts = stat(rawfn, &st);
        }
        else
        {
            sts = 0;
            st.st_size = vol->hd_maxlba - vol->hd_offset;
        }
        if (!sts && vol->maxlba != (lim=(st.st_size - (st.st_size%FSYS_MAX_ALTS))))
        {
            txt_str((AN_VIS_COL-sizeof(DISK_DRIVE_SIZ)-1-7)/2, row++, DISK_DRIVE_SIZ, WHT_PAL);
            sts = (lim+100000)/200000;
            txt_cdecnum(sts/10, 3, RJ_BF, WHT_PAL);
            txt_cstr(".", WHT_PAL);
            txt_cdecnum(sts - (sts/10)*10, 1, RJ_ZF, WHT_PAL);
            txt_cstr("GB", WHT_PAL);
            txt_str((AN_VIS_COL-sizeof(SPACE_USED_MSG)-1-7)/2, row++, SPACE_USED_MSG, WHT_PAL);
            sts = (vol->maxlba+100000)/200000;
            txt_cdecnum(sts/10, 3, RJ_BF, WHT_PAL);
            txt_cstr(".", WHT_PAL);
            txt_cdecnum(sts - (sts/10)*10, 1, RJ_ZF, WHT_PAL);
            txt_cstr("GB", WHT_PAL);
        }
    }
    txt_str((AN_VIS_COL-sizeof(FILES_USED_MSG)-1-7)/2, row++, FILES_USED_MSG, WHT_PAL);
    txt_cdecnum(vol->files_ffree, 5, RJ_BF, WHT_PAL);
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL) 
    txt_str((AN_VIS_COL-sizeof(JOURNAL_PRESENT)-1-7)/2, row++, JOURNAL_PRESENT, WHT_PAL);
    if (vol->journ_rfh)
    {
        txt_cstr("  Yes", GRN_PAL);
    }
    else
    {
        txt_cstr("   No", RED_PAL);
    }
#endif
    txt_str((AN_VIS_COL-(sizeof(LOOP_COUNT_MSG)+2)-1-7)/2, row++, LOOP_COUNT_MSG, WHT_PAL);
    lc_pos = txt_setpos(0);
    sts = qiow_close(ioq);
    ioq->timeout = 3000000;     /* 3 second timeout */
    strcpy(walk.data_buff, volfn);
    strcat(walk.data_buff, FSYS_CHECKSUM_FILENAME);
    ot.spc.path = walk.data_buff;
    ot.spc.mode = O_RDONLY;
    ot.fid = 0;
    sts = qiow_openspc(ioq, &ot.spc);
    walk.volfn = volfn;    
    if (!QIO_ERR_CODE(sts))
    {
        if (!ot.eof)
        {
#if 0
            txt_str(-1, AN_VIS_ROW-7, "Checksum file is empty", YEL_PAL);
#endif
        }
        else
        {
            void *bss_end;
#if !FSYS_USE_MALLOC
            walk.cksums_re = malloc_from_any_pool(&bss_end, ot.eof);
#else
            bss_end = malloc(ot.eof);
#endif
            if (!bss_end)             /* if no room for checksum file */
            {
                txt_str(-1, AN_VIS_ROW-7, "Not enough memory to hold checksum file.", YEL_PAL);
            }
            else
            {
                sts = qiow_read(ioq, (char *)bss_end, ot.eof);
                if (QIO_ERR_CODE(sts))
                {
                    txt_str(-1, AN_VIS_ROW-7, "Error reading checksum file.", WHT_PAL);
                    qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
                    txt_str(-1, AN_VIS_ROW-6, walk.data_buff, YEL_PAL);
                }
                else
                {
                    walk.cksums = (ChkSum *)bss_end;
                    walk.numb_cksums = ioq->iocount/sizeof(ChkSum);
                }
            }
        }
        qio_close(ioq);     /* we're done with this */
    }
    if (walk.data_buff_size <= 4096)
    {
#if !FSYS_NOISY_CHECK
        no_mem:
#endif
        txt_str(-1, WALK_ERR_ROW, "Sorry, not enough free memory to run this test", RED_PAL);
        while (!(ctl_read_sw(SW_ACTION|SW_NEXT)&(SW_ACTION|SW_NEXT)))
        {
            prc_delay(0);
        }
        ctl_read_sw(-1);
        qio_freeioq(ioq);
#if !FSYS_USE_MALLOC
        if (walk.cksums) _free_r(walk.cksums_re, walk.cksums);
        if (walk.data_buff_re && walk.data_buff_p) _free_r(walk.data_buff_re, walk.data_buff_p);
#else
        if (walk.cksums) free(walk.cksums);
        if (walk.data_buff_p) free(walk.data_buff_p);
#endif
        return 0;
    }
    while (1)
    {
        txt_setpos(lc_pos);
        txt_cdecnum(walk.loop_count++, 7, RJ_BF, GRN_PAL);
        prc_delay(0);
#if !FSYS_NOISY_CHECK
        sts = show_filenumbers(ioq, volfn, vol, &walk);
        if ( sts < 0) goto no_mem;
        if ( sts == WALK_EXIT_ABORT ) break;
        if ( sts == WALK_EXIT_PAUSE )
        {
            wait_for_reply(&walk, 120*60);  /* pause for 2 minutes */
        }
        for (row=WALK_ANN_ROW; row <= WALK_ERR_ROW+3; ++row)
        {
            txt_clr_wid(2, row, AN_VIS_COL-4);
        }
#else
        if (walk.numb_cksums) txt_str(6, AN_VIS_ROW-5, "(CS) = File's checksum is being computed and compared", WHT_PAL);
        for (walk.pass=0; walk.pass < FSYS_MAX_ALTS; ++walk.pass)
        {
#if FSYS_TIME_NOISY_TEST
            U32 duration = 0;
            switch ( walk.pass )
            {
            case 0:
                if ( walk.start_time ) duration = eer_rtc - walk.start_time;
                walk.start_time = eer_rtc;
                break;
            case 1:
                walk.pass1_time = eer_rtc - walk.start_time;
                duration = walk.pass1_time;
                break;
            }
            if ( duration )
            {
                duration *= 167;
                duration += 500;
                duration /= 1000;   /* Rounded seconds */
                txt_setpos(lc_pos);
                txt_cdecnum(walk.loop_count, 7, RJ_BF, GRN_PAL);
                txt_cstr(" (",GRN_PAL);
                txt_cdecnum(duration,4,LJ_NF,GRN_PAL);
                txt_cstr("/",GRN_PAL);
                duration = (walk.pass1_time*167+500)/1000;
                txt_cdecnum(duration,4,LJ_NF,GRN_PAL);
                txt_cstr(" sec.)",GRN_PAL);
            }
#endif
            walk.filenum = 2;
            sts = qiow_opendir(ioq, (void *)&dp, volfn);    /* open root directory */
            if (QIO_ERR_CODE(sts))
            {
                qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
                txt_str(-1, WALK_ERR_ROW, walk.data_buff, YEL_PAL);
                sts = WALK_EXIT_ABORT;
                break;
            }
            else
            {
                int csts;
                walk.dp = dp;
                walk.ioq = ioq;
                sts = walk_directory(&walk, volfn);
                csts = qiow_closedir(ioq, (void *)dp);
                if ( sts == WALK_EXIT_ABORT ) break;
            }
        }    
        if ( sts == WALK_EXIT_ABORT ) break;
        else
        {
            int row;
            for (row=WALK_ANN_ROW; row <= WALK_ERR_ROW; ++row)
            {
                txt_clr_wid(2, row, AN_VIS_COL-4);
            }
            for (row=AN_VIS_ROW-8; row <= AN_VIS_ROW-5; ++row)
            {
                txt_clr_wid(2, row, AN_VIS_COL-4);
            }
            if (walk.loop_count < 2 && walk.errors)
            {
                txt_str(-1, AN_VIS_ROW/2, "Filesystem is not OK", YEL_PAL);
                if ((sts=wait_for_reply(&walk, 120*60))) break;
            }
            sts = 0;
        }
        if (walk.numb_cksums) txt_clr_wid(6, AN_VIS_ROW-5, AN_VIS_COL-6-2);
#endif
    }
#if !FSYS_USE_MALLOC
    if (walk.cksums) _free_r(walk.cksums_re, walk.cksums);
    if (walk.data_buff_re && walk.data_buff_p) _free_r(walk.data_buff_re, walk.data_buff_p);
#else
    if (walk.cksums) free(walk.cksums);
    if (walk.data_buff_p) free(walk.data_buff_p);
#endif
    if (walk.fname) FREE_MEM(walk.fname_size, walk.fname);
    if (!sts) while (!(ctl_read_sw(SW_NEXT)&SW_NEXT))
        {
            prc_delay(0);
        }
#if defined(GUTS_OPT_RPT_DISK_ERRS)
    debug_mode = old_debug;
#endif
    qio_freeioq(ioq);
    ctl_read_sw(-1);
    return 0;
}

    #if !FSYS_READ_ONLY && FSYS_NO_AUTOSYNC 
struct fupd
{
    QioIOQ *rioq;       /* IOQ for raw disk I/O */
    FsysRetPtr *bad;        /* pointer to array of bad sectors */
    int indx;           /* next available entry */
    int size;           /* number of entries */
    int flags;          /* operation flags */
    U32 *diskbuf;       /* disk buffer */
    int diskbuf_size;       /* size of disk buffer */
    struct _reent *where;   /* which pool we obtained buffer */
    U32 total_sectors;      /* total number of sectors to test */
    U32 total_checked;      /* total number of sectors checked so far */
    U32 total_errors;       /* number of sectors in error */
    U32 start_time;     /* interval counter */
    int prevpcnt;
};
        #define FUPD_FLG_ABORT	 (1)	/* ok to abort if switch on */
        #define FUPD_FLG_ABORTED (2)	/* validate aborted due to switch closure */

static int add_to_bad(struct fupd *upd, U32 start, int nblocks)
{
    FsysRetPtr *new;
    int ii;
    if (upd->indx >= upd->size)
    {
        upd->size += 100*sizeof(FsysRetPtr);
        new = (FsysRetPtr *)QIOrealloc(upd->bad, upd->size);
        if (!new)
        {
            return QIO_NOMEM;   /* ran out of memory */
        }
        upd->bad = new;
    }
    new = upd->bad;
    for (ii=0; ii < upd->indx; ++ii, ++new)   /* look to see if we're adding something adjacent */
    {
        int begin, end;
        begin = new->start;
        end = begin + new->nblocks;
        if (start+nblocks < begin) continue;    /* completely outside the limits */
        if (end < start) continue;      /* completely outside the limits */
        if (begin > start)
        {
            new->nblocks += begin-start;    /* overlap */
            new->start = start;
        }
        if (end < start+nblocks)
        {
            new->nblocks += start+nblocks-end;
        }
        return 0;
    }
    new = upd->bad + upd->indx;
    ++upd->indx;
    new->start = start;
    new->nblocks = nblocks;
    upd->total_errors += nblocks;
    return 0;
}

        #if 0
static U32 last_error[3];

static void upd_last_err(QioIOQ *ioq)
{
    QioFile *fp;
    fp = qio_fd2file(ioq->file);
    if (fp->dvc->mutex->current)
    {
        __asm__("BREAK");
    }
    last_error[2] = last_error[1];
    last_error[1] = last_error[0];
    last_error[0] = ioq->iostatus;
}

            #define LAST_ERROR(x) upd_last_err(ioq)
        #else
            #define LAST_ERROR(x) do { ; } while (0)
        #endif

static int validate_sectors(FsysRetPtr *range, struct fupd *upd)
{
    int sts;
    int ii, cs0, cs1, amt, count;
    U32 sector, *bufp;
    QioIOQ *ioq;

    sector = range->start;
    count = range->nblocks*BYTPSECT;
    ioq = upd->rioq;
    ioq->timeout = 1000000; /* 0.1 second timeout */
    while (count > 0)
    {
        amt = upd->diskbuf_size;
        if (amt > count) amt = count;
        while (amt)
        {
            bufp = upd->diskbuf;
            sts = qiow_readwpos(ioq, sector, bufp, amt);
            LAST_ERROR(ioq);
            if (QIO_ERR_CODE(sts) || ioq->iocount != amt) /* error or truncated read */
            {
                amt = ioq->iocount;     /* skip failing sector for subsequent tests */
                if (!amt) break;        /* nothing copied, stop here */
                add_to_bad(upd, sector + amt/BYTPSECT, 1); /* the sector to bad */
            }
            sts = qiow_writewpos(ioq, sector, bufp, amt);
            LAST_ERROR(ioq);
            if (QIO_ERR_CODE(sts) || ioq->iocount != amt) /* error or truncated write */
            {
                amt = ioq->iocount;     /* skip failing sector */
                if (!amt) break;        /* nothing copied, stop here */
                add_to_bad(upd, sector + amt/BYTPSECT, 1); /* the sector to bad */
            }
            for (ii=cs0=0; ii < amt; ii += 4) cs0 += *bufp++;
            bufp = upd->diskbuf;
            sts = qiow_readwpos(ioq, sector, bufp, amt);
            LAST_ERROR(ioq);
            if (QIO_ERR_CODE(sts) || ioq->iocount != amt)
            {
                amt = ioq->iocount;     /* skip failing sector for subsequent tests */
                if (!amt) break;        /* nothing copied, stop here */
                add_to_bad(upd, sector + amt/BYTPSECT, 1); /* the sector to bad */
                continue;           /* start over */
            }
            for (ii=cs1=0; ii < amt; ii += 4) cs1 += *bufp++;
            if (cs0 != cs1)
            {
                add_to_bad(upd, sector, amt/BYTPSECT); /* the whole sector list is suspect */
            }
            break;
        }       
        if (!amt)         /* found a bad sector */
        {
            add_to_bad(upd, sector, 1); /* add the one sector to the bad list */
            amt = BYTPSECT;         /* advance to next sector */
            ++upd->total_errors;
        }
        count -= amt;           /* take from count */
        amt /= BYTPSECT;
        sector += amt;          /* up to next sector */
        upd->total_checked += amt;  /* count sectors copied */
        if (upd->total_sectors)
        {
            int pcnt, show_it = 0;
            if ((upd->flags&FUPD_FLG_ABORT))
            {
                if ((ctl_read_sw(SW_NEXT)&SW_NEXT))
                {
                    upd->flags |= FUPD_FLG_ABORTED;
                    break;
                }
            }
            pcnt = (upd->total_checked*100)/upd->total_sectors;
            if ((eer_rtc - upd->start_time) > 60) show_it = 1;  /* update the screen once/second */
            if (pcnt >= upd->prevpcnt+10) show_it = 1;
            if (show_it)
            {
#define UPD_PCNT_DONE_MSG "  % Complete"
                txt_str((AN_VIS_COL-sizeof(UPD_PCNT_DONE_MSG)-1)/2, AN_VIS_ROW/2, UPD_PCNT_DONE_MSG, WHT_PAL);
                txt_decnum((AN_VIS_COL-sizeof(UPD_PCNT_DONE_MSG)-1)/2, AN_VIS_ROW/2, pcnt, 2, RJ_BF, GRN_PAL);
#define UPD_TOT_ERRS_MSG "Total bad sectors found so far: "
                txt_str((AN_VIS_COL-sizeof(UPD_TOT_ERRS_MSG)-1-7)/2, AN_VIS_ROW/2+2, UPD_TOT_ERRS_MSG, WHT_PAL);
                txt_cdecnum(upd->total_errors, 7, LJ_BF, upd->total_errors ? RED_PAL : GRN_PAL);
#if 0
#define UPD_TOT_BAD_MSG "Total new retrieval pointers so far: "
                txt_str((AN_VIS_COL-sizeof(UPD_TOT_BAD_MSG)-1-7)/2, AN_VIS_ROW/2+4, UPD_TOT_BAD_MSG, WHT_PAL);
                txt_cdecnum(upd->indx, 7, LJ_BF, upd->indx ? RED_PAL : YEL_PAL);
#endif
                prc_delay(0);
                upd->start_time = eer_rtc;
                upd->prevpcnt = pcnt;
            }
        }
    }
    return 0;
}

static FsysRetPtr *get_rpmem(FsysRetPtr **old, int new_indx, int qty, int *size)
{
    int new_size;
    FsysRetPtr *new;

    new = *old;
    new_size = *size;
    if (new_indx+2 < new_size)
    {
        return new + new_indx;
    }
    new_size = new_size ? new_size*50 : qty;
    *size = new_size;
    new = (FsysRetPtr *)QIOrealloc(new, new_size*sizeof(FsysRetPtr));
    if (new)
    {
        *old = new;
        return new + new_indx;
    }
    return 0;
}

static FsysRetPtr *merge_lists(FsysRetPtr *free, int elems, struct fupd *upd)
{
    int num_bad, new_size, new_indx, qty;
    FsysRetPtr *newp=0, *new, *bad;
    U32 frees, freen;

    new_size = new_indx = 0;
    qty = elems+upd->indx+50;
    while (elems > 0)
    {
        int free_end;
        num_bad = upd->indx;
        frees = free->start;
        freen = free->nblocks;
        free_end = frees + freen;
        bad = upd->bad;
        for (; num_bad > 0; --num_bad, ++bad)
        {
            int dif, bad_end;
            bad_end = bad->start + bad->nblocks;
            if (free_end <= bad->start) continue; /* free completely before bad */
            if (frees >= bad_end) continue; /* free completely after bad */
            if (frees >= bad->start)      /* overlap. Free starts after bad */
            {
                if (free_end <= bad_end)    /* free entirely contained in bad */
                {
                    frees = freen = 0;      /* whack out the entry */
                    break;
                }
                dif = bad_end - free->start;
                frees += dif;
                freen -= dif;
                break;
            }
            if (free_end <= bad_end)
            {
                dif = free_end - bad->start;
                freen -= dif;
                break;
            }
            dif = bad->start - frees;
            if (!(new = get_rpmem(&newp, new_indx, qty, &new_size))) return 0;
            new->start = frees;
            new->nblocks = dif;
            ++new_indx;
            frees = bad_end;
            freen = free_end - bad_end;
        }
        if (freen)
        {
            if (!(new = get_rpmem(&newp, new_indx, qty, &new_size))) return 0;
            new->start = frees;
            new->nblocks = freen;
            ++new_indx;
        }
        ++free;
        --elems;
    }
    if (!(new = get_rpmem(&newp, new_indx, qty, &new_size))) return 0;
    new->start = 0;
    new->nblocks = 0;
    bad = upd->bad;
    upd->bad = newp;
    upd->indx = new_indx+1;
    upd->size = new_size;
    return bad;
}

int fsys_validate_freespace(const char *rawfn, const char *volfn, int sts_row, int upd_row, int maxsect, int dosync)
{
    int sts, freei;
    QioIOQ *ioq=0, *sioq=0;
    char emsg[AN_VIS_COL_MAX], *msg;
    int freelist_indx, freelist_size;
    FsysRetPtr *freelist=0, *badlist, *flp;
    U32 *diskbuf=0;
    struct fupd upd;

    memset(&upd, 0, sizeof(upd));
    if (maxsect <= 0)
    {
        upd.flags |= FUPD_FLG_ABORT;        /* abortable */
        if (maxsect < 0) maxsect = -maxsect;    /* flip it for real use */
    }
    do
    {
        FsysOpenT ot;
        ioq = qio_getioq();
        upd.rioq = qio_getioq();
        sioq = qio_getioq();
        if (!ioq || !upd.rioq || !sioq)
        {
            txt_str(-1, sts_row, "Ran out of IOQ's", RED_PAL);
            break;
        }
        ot.spc.path = volfn;
        ot.spc.mode = O_RDONLY;
        ot.fid = (1<<FSYS_DIR_GENSHF) | FSYS_INDEX_FREE;    /* open the freelist */
        sts = qiow_openspc(ioq, &ot.spc);
        if (QIO_ERR_CODE(sts))
        {
            msg = "Error opening freelist file";
            err:
            txt_str(-1, sts_row, msg, RED_PAL);
            if (sts)
            {
                qio_errmsg(sts, emsg, sizeof(emsg));
                txt_str(-1, sts_row+2, emsg, YEL_PAL);
            }
            break;
        }
        sts = qiow_open(upd.rioq, rawfn, O_RDWR);       /* open raw disk */
        if (QIO_ERR_CODE(sts))
        {
            msg = "Error opening raw disk";
            goto err;
        }
        freelist_size = ot.alloc+100*sizeof(FsysRetPtr);
        freelist_indx = ot.eof/sizeof(FsysRetPtr);
        freelist = (FsysRetPtr *)QIOmalloc(freelist_size);
        upd.size = 200*sizeof(FsysRetPtr);
        upd.bad = (FsysRetPtr *)QIOmalloc(upd.size);
        if (!freelist || !upd.bad)
        {
            msg = "Ran out of memory";
            sts = 0;
            goto err;
        }
        sts = qiow_read(ioq, freelist, ot.eof); /* read the whole freelist file */
        if (QIO_ERR_CODE(sts))
        {
            msg = "Error reading freelist file";
            goto err;
        }
        upd.diskbuf_size = BYTPSECT*1024;
#if !FSYS_USE_MALLOC
        do
        {
            upd.where = malloc_from_any_pool((void **)&diskbuf, upd.diskbuf_size+QIO_CACHE_LINE_SIZE);
            if (upd.where && diskbuf)
            {
                break;
            }
            upd.diskbuf_size /= 2;
        } while (upd.diskbuf_size >= 65536);
#else
        diskbuf = malloc(upd.diskbuf_size+QIO_CACHE_LINE_SIZE);
#endif
        if (!diskbuf)
        {
            msg = "No memory for disk buffer";
            sts = 0;
            goto err;
        }
        upd.diskbuf = (U32 *)(((int)diskbuf + 31)&-32);
        for (flp=freelist, freei=0; freei < freelist_indx; ++freei, ++flp)
        {
            if (flp->nblocks == 0 && flp->start == 0)
            {
                freelist_indx = freei;      /* snap to "real" eof */
                break;
            }
            if (maxsect && maxsect < flp->nblocks )
            {
                upd.total_sectors += maxsect;
            }
            else
            {
                upd.total_sectors += flp->nblocks;
            }
        }
        if ((upd.flags&FUPD_FLG_ABORT))
        {
#define MSG_CHK_SECT "Total sectors to check: "
#define MSG_CHK_FRAG "Total free fragments:   "
#define MSG_CHK_ROW  (AN_VIS_ROW/4)
#define MSG_CHK_COL  ((AN_VIS_COL-sizeof(MSG_CHK_SECT)-1-7)/2)
            txt_str(MSG_CHK_COL, MSG_CHK_ROW+0, MSG_CHK_SECT, WHT_PAL);
            txt_cdecnum(upd.total_sectors, 7, RJ_BF, GRN_PAL);
            txt_str(MSG_CHK_COL, MSG_CHK_ROW+2, MSG_CHK_FRAG, WHT_PAL);
            txt_cdecnum(freelist_indx, 7, RJ_BF, GRN_PAL);
        }
        upd.start_time = eer_rtc;
        for (flp=freelist, freei=0; freei < freelist_indx; ++freei, ++flp)
        {
            FsysRetPtr lflp, *lflpp;
            lflpp = flp;
            if (maxsect && maxsect < flp->nblocks)
            {
                lflpp = &lflp;
                lflpp->start = flp->start;
                lflpp->nblocks = maxsect;
            }
            sts = validate_sectors(lflpp, &upd);
            if (sts)
            {
                msg = "Error validating";
                goto err;
            }
            if ((upd.flags&FUPD_FLG_ABORTED)) break;
        }
        if ((upd.flags&FUPD_FLG_ABORTED)) break;
        txt_clr_wid(2, sts_row, AN_VIS_COL-4);
        txt_clr_wid(2, sts_row+2, AN_VIS_COL-4);
        if ((upd.flags&FUPD_FLG_ABORT))
        {
            txt_clr_wid(MSG_CHK_COL, MSG_CHK_ROW+0, sizeof(MSG_CHK_SECT)-1+7);
            txt_clr_wid(MSG_CHK_COL, MSG_CHK_ROW+2, sizeof(MSG_CHK_SECT)-1+7);
        }
        if (upd.indx)     /* if there is anything in the badlist */
        {
            QioFile *fp;
            FsysVolume *vol;
            int row = upd_row;

            badlist = merge_lists(freelist, freelist_indx, &upd);
            if (badlist)
            {
                int sw=0;
                if (badlist != upd.bad) QIOfree(badlist);
                fp = qio_fd2file(ioq->file);
                vol = (FsysVolume *)fp->dvc->private;
                if (dosync
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
                    && !vol->journ_rfh      /* if no journal file */
#endif
                   )
                {
                    txt_str( 10, ++row, "WARNING:  The disk is about to be updated.", RED_PAL );
                    ++row;
                    txt_str( 10, ++row, "After you ", YEL_PAL );
                    txt_cstr( t_msg_action, YEL_PAL );
                    txt_cstr( ", do not", YEL_PAL );
                    txt_str( 10, ++row, "interrupt its operation for any reason", YEL_PAL );
                    txt_str( 10, ++row, "until after it says it is done.",  YEL_PAL );
                    ++row;
                    txt_str( 10, ++row, "Interrupting this process will result in", YEL_PAL );
                    txt_str( 10, ++row, "your game disk becoming unusable.", YEL_PAL );
                    ++row;
                    txt_str( 10, ++row, "The update should take only a few seconds.", WHT_PAL );
                    ++row;  
                    txt_str( 10, ++row, "To continue, ", INSTR_PAL );
                    txt_cstr( t_msg_action, INSTR_PAL );
                    ctl_read_sw( -1 );
                    while ( 1 )
                    {
                        sw = SW_ACTION | ((upd.flags&FUPD_FLG_ABORT) ? SW_NEXT : 0);
                        sw = ctl_read_sw(sw)&sw;
                        if (sw) break;
                        prc_delay(0);
                    }
                    if ((sw&SW_NEXT))
                    {
                        while (row >= upd_row)
                        {
                            txt_clr_wid(2, row, AN_VIS_COL-4);
                            --row;
                        }
                        dosync = 0;
                        break;
                    }
                }
                if (vol->free) QIOfree(vol->free);  /* toss the old freelist */
                vol->free = upd.bad;            /* install the new one */
                vol->free_ffree = upd.indx;     /* new limit */
                vol->free_elems = upd.size;     /* new max */
                upd.bad = 0;                /* so we don't free this one */
                add_to_dirty(vol, FSYS_INDEX_FREE, 1);  /* drop the freefile FH on dirty list */
                if (dosync)
                {
                    sts = qiow_fsync(sioq, volfn);  /* sync the filesystem */
                    sts = qiow_close(ioq);      /* wait for sync to complete */
                    while (row >= upd_row)
                    {
                        txt_clr_wid(2, row, AN_VIS_COL-4);
                        --row;
                    }
                }
            }
            else
            {
                txt_str(-1, sts_row, "Ran out of memory", RED_PAL);
                break;
            }
        }
        if (dosync) txt_str(-1, sts_row, "Done", WHT_PAL);
    } while (0);
    if (ioq)
    {
        sts = qiow_close(ioq);
        qio_freeioq(ioq);
    }
    if (upd.rioq)
    {
        sts = qiow_close(upd.rioq);
        qio_freeioq(upd.rioq);
    }
    if (freelist) QIOfree(freelist);
    if (upd.bad) QIOfree(upd.bad);
#if !FSYS_USE_MALLOC
    if (upd.where && upd.diskbuf) _free_r(upd.where, upd.diskbuf);
#else
    if (upd.diskbuf) free(upd.diskbuf);
#endif
    return(upd.flags&FUPD_FLG_ABORTED) ? 1 : 0;
}

static int chk_space(const struct menu_d *smp)
{
    char rawfn[10], volfn[10];
    if (sel_and_mount(rawfn, volfn, 0, smp) < 0) return 0;
    prc_delay(0);
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    if (!fsys_validate_freespace(rawfn, volfn, AN_VIS_ROW/2, AN_VIS_ROW/4, 0, TRUE))
    {
        ctl_read_sw(-1);
        while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) prc_delay(0);
    }
    return 0;
}

static int chk_space10(const struct menu_d *smp)
{
    char rawfn[10], volfn[10];
    if (sel_and_mount(rawfn, volfn, 0, smp) < 0) return 0;
    prc_delay(0);
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
#ifndef UPD_FSYS_CHK_MAXSECT
#define UPD_FSYS_CHK_MAXSECT	(15000000/BYTPSECT)
#endif
    if (!fsys_validate_freespace(rawfn, volfn, AN_VIS_ROW/2, AN_VIS_ROW/4, -UPD_FSYS_CHK_MAXSECT, TRUE))
    {
        ctl_read_sw(-1);
        while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) prc_delay(0);
    }
    return 0;
}

static const struct menu_d ft_menu[] = {
    { "FILESYSTEM TESTS",       0       },
    { "\nREAD CHECK ALL FILES",     FSYS_TEST_NAME},
    { "\nR/W CHECK ALL FILE FREE SPACE", chk_space},
    { "\nR/W CHECK SOME FILE FREE SPACE", chk_space10},
    { 0, 0}
};

int fsys_test(const struct menu_d *smp)
{
    return st_menu(ft_menu, sizeof(ft_menu[0]), MNORMAL_PAL, 0);
}
    #endif			/* !FSYS_READ_ONLY && FSYS_NO_AUTOSYNC */

#endif			/* !NO_FSYS_TEST */
