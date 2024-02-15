/* See LICENSE.txt for license details */

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <qio.h>
#include <fsys.h>
#include <any_proto.h>
#include <sys/stat.h>
#include <ctype.h>

#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC) && NUM_HDRIVES != 1
#error Cannot have \NUM_HDRIVES set to anything other than 1
#endif

#if USE_SYSCALL5
_syscall5(int, _llseek, uint, fd, ulong, hi, ulong, lo, loff_t *, res, uint, wh)
#endif

enum ide_funcs
{
    IDE_READWPOS,
    IDE_READ,
    IDE_WRITEWPOS,
    IDE_WRITE,
    IDE_LSEEK,
    IDE_FSTAT,
    IDE_IOCTL,
    IDE_OPEN,
    IDE_CLOSE,
    IDE_MAXFUNC
};

typedef struct pinfo
{
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
    char name[1024];            /* base file name */
    char part_name[1024+4];     /* partition name */
#else
    char name[16];      /* master disk name */
    char part_name[16];     /* partition name */
#endif
    U32 base_sect;
    U32 num_sects;
    U32 relative_sect;
    int partition_option;
    int fd;         /* fd to use to do I/O on this device */
    QioMutex mutex;     /* just for fun */
} Pinfo;

#if 0
    #define FS_DEBUG(x) do { printf x ; fflush(stdout); } while (0)
#else
    #define FS_DEBUG(x) do { ; } while (0)
#endif

#if 1
    #define RPT_ERR(x...) \
    do { \
	int len; \
	char _emsg[132]; \
	len = snprintf(_emsg, sizeof(_emsg), ## x); \
	write(fileno(stderr), _emsg, len); \
    } while (0)
#else
    #define RPT_ERR(x...) do { ; } while (0)
#endif

static void ide_task(QioIOQ *ioq)
{
    QioFile *fp = qio_fd2file(ioq->file);
    const QioDevice *dvc = fp->dvc;
    Pinfo *info;
    int sts=0, idefd;
    off64_t targetSector, resultSector;
#if USE_SYSCALL5
    loff_t ans;
#endif

    info  = (Pinfo *)dvc->private;
    idefd = info->fd;
    if (!idefd)
    {
        ioq->iostatus = QIO_NOTOPEN;
        RPT_ERR("Error in ide_task. idefd not open\n");
    }
    else
    {
        while (1)     /* so we can restart after a EINTR */
        {
            switch (ioq->semaphore)
            {
            case IDE_LSEEK:
                FS_DEBUG(("Seeking to relative sector %ld\n", ioq->iparam0));
                targetSector = ioq->iparam1;
                targetSector += info->relative_sect;
                targetSector <<= 9;
#if USE_SYSCALL5
                sts = _llseek(idefd, targetSector >> 32, targetSector&0xFFFFFFFF, &ans, ioq->iparam1);
                if (sts == -1 )
#else
                resultSector = lseek64(idefd, targetSector, SEEK_SET);
                if (resultSector == -1 || resultSector != targetSector)
#endif
                {
                    if (errno == EINTR) continue;   /* Loop on EINTR's */
                    ioq->iostatus = errno;
                    RPT_ERR("IDE_LSEEK: Error %d seeking to sector %08lX (whence=%ld)\n", errno, ioq->iparam0, ioq->iparam1);
                    break;
                }
                ioq->iocount = ioq->iparam0;
                fp->sect = ioq->iparam0;
                fp->bws = 0;
                ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                break;
            case IDE_READWPOS:
                FS_DEBUG(("Readwpos seek to relative sector %ld. idefd=%d\n", ioq->iparam1, idefd));
                targetSector = ioq->iparam1;
                targetSector += info->relative_sect;
                targetSector <<= 9;
#if USE_SYSCALL5
                sts = _llseek(idefd, targetSector >> 32, targetSector&0xFFFFFFFF, &ans, SEEK_SET);
                if (sts == -1)
#else
                targetSector = lseek64(idefd, targetSector, SEEK_SET);
                if (targetSector == -1)
#endif
                {
                    if (errno == EINTR) continue;   /* Loop on EINTR's */
                    ioq->iostatus = errno;
                    RPT_ERR("IDE_READWPOS: Error %d seeking to sector %08lX\n", errno, ioq->iparam0);
                    break;
                }
                fp->sect = ioq->iparam1;
                fp->bws = 0;
            case IDE_READ:
                FS_DEBUG(("Read %ld bytes. idefd=%d\n", ioq->iparam0, idefd));
                if (!ioq->iparam0)
                {
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                    ioq->iocount = 0;
                    break;
                }
                sts = read(idefd, ioq->pparam0, ioq->iparam0);
                if (!sts)
                {
                    ioq->iostatus = QIO_EOF;
                }
                else if (sts < 0)
                {
                    if (errno == EINTR) continue;   /* Loop on EINTR's */
                    ioq->iostatus = errno ? errno : FSYS_IO_RDERR;  /* make sure we don't use a 0 */
                    RPT_ERR("IDE_READ: Error %ld reading %ld bytes\n", ioq->iostatus, ioq->iparam0);
                }
                else
                {
                    U32 byt;
                    byt = (fp->sect<<9) + fp->bws + sts;
                    fp->sect = byt>>9;
                    fp->bws = byt&511;
                    ioq->iocount = sts;
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                }
                break;
            case IDE_WRITEWPOS:
                FS_DEBUG(("Writewpos seek to relative sector %ld. idefd=%d\n", ioq->iparam1, idefd));
                targetSector = ioq->iparam1;
                targetSector += info->relative_sect;
                targetSector <<= 9;
#if USE_SYSCALL5
                sts = _llseek(idefd, targetSector >> 32, targetSector&0xFFFFFFFF, &ans, SEEK_SET);
                if (sts == -1)
#else
                targetSector = lseek64(idefd, targetSector, SEEK_SET);
                if (targetSector == -1)
#endif
                {
                    if (errno == EINTR) continue;   /* Loop on EINTR's */
                    ioq->iostatus = errno;
                    RPT_ERR("IDE_WRITEWPOS: Error %d seeking to sector %ld\n", errno, ioq->iparam1);
                    break;
                }
                fp->sect = ioq->iparam1;
                fp->bws = 0;
            case IDE_WRITE:
                FS_DEBUG(("Write %ld bytes. idefd=%d\n", ioq->iparam0, idefd));
                if (!ioq->iparam0)
                {
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                    ioq->iocount = 0;
                    break;
                }
                sts = write(idefd, ioq->pparam0, ioq->iparam0);
                if (!sts)
                {
                    ioq->iostatus = QIO_EOF;
                }
                else if (sts < 0)
                {
                    if (errno == EINTR) continue;   /* Loop on EINTR's */
                    ioq->iostatus = errno;
                    RPT_ERR("IDE_WRITE: Error %d writing %ld bytes\n", errno, ioq->iparam0);
                }
                else
                {
                    U32 byt;
                    byt = (fp->sect<<9) + fp->bws + sts;
                    fp->sect = byt>>9;
                    fp->bws = byt&511;
                    ioq->iocount = sts;
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                }
                break;
            case IDE_OPEN: {
                    FsysOpenT *ot = (FsysOpenT *)fp->private;
                    if (ot)
                    {
                        struct stat64 st;
                        int ssts;
                        ssts = fstat64(info->fd, &st);
                        if (ssts >= 0)
                        {
                            ot->alloc = ot->eof = st.st_blocks;
                            ot->placement = 0;
                            ot->copies = 1;
                            ot->def_extend = 0;
                            ot->mkdir = S_ISDIR(st.st_mode);
                            ot->ctime = st.st_ctime;
                            ot->mtime = st.st_mtime;
                            ot->atime = st.st_atime;
                        }
                        else
                        {
                            if (errno == EINTR) continue;   /* Loop on EINTR's */
                        }
                    }
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                    ioq->iocount = ioq->file;
                    fp->private = (void *)sts;
                    fp->sect = 0;
                    fp->bws = 0;
                    fp->size = 0;
                    fp->flags = 0;
                    fp->next = 0;
                    break;
                }
            case IDE_CLOSE:
                ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                fp->private = 0;
                ioq->file = -1;
                qio_freefile(fp);
                break;
            case IDE_FSTAT:
                {
                    struct stat *stp = (struct stat *)ioq->pparam0;
                    memset(stp, 0, sizeof(struct stat));
                    stp->st_blocks = info->num_sects;
                    stp->st_size = info->num_sects;     /* drive returns size in sectors */
                    stp->st_blksize = 512;
                    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
                    ioq->iocount = sizeof(struct stat);
                    break;
                }
            case IDE_IOCTL:
            default:
                ioq->iostatus = QIO_NOTSUPP;
                ioq->iocount = 0;
            }
            break;
        }
    }
    qio_freemutex(&info->mutex, ioq);
    qio_complete(ioq);
}

static int  que_it(QioIOQ *ioq)
{
    QioFile *fp = qio_fd2file(ioq->file);
    const QioDevice *dvc = fp->dvc;
    Pinfo *info;

    info  = (Pinfo *)dvc->private;
    ioq->iostatus = 0;
    ioq->iocount = 0;
    return qio_getmutex(&info->mutex, ide_task, ioq);
}

static int  ide_readwpos(QioIOQ *ioq, off_t where, void *buf, long len)
{
    ioq->pparam0 = buf;
    ioq->iparam0 = len;
    ioq->iparam1 = where;
    ioq->semaphore = IDE_READWPOS;
    return que_it(ioq);
}

static int  ide_read(QioIOQ *ioq, void *buf, long len)
{
    ioq->pparam0 = buf;
    ioq->iparam0 = len;
    ioq->semaphore = IDE_READ;
    return que_it(ioq);
}

static int  ide_writewpos(QioIOQ *ioq, off_t where, const void *buf, long len)
{
    ioq->pparam0 = (void *)buf;
    ioq->iparam0 = len;
    ioq->iparam1 = where;
    ioq->semaphore = IDE_WRITEWPOS;
    return que_it(ioq);
}

static int  ide_write(QioIOQ *ioq, const void *buf, long len)
{
    ioq->pparam0 = (void *)buf;
    ioq->iparam0 = len;
    ioq->semaphore = IDE_WRITE;
    return que_it(ioq);
}

static int  ide_lseek(QioIOQ *ioq, off_t where, int whence)
{
    ioq->iparam0 = where;
    ioq->iparam1 = whence;
    ioq->semaphore = IDE_LSEEK;
    return que_it(ioq);
}

static int  ide_open(QioIOQ *ioq, const char *path)
{
    ioq->pparam0 = (void *)path;
    ioq->semaphore = IDE_OPEN;
    return que_it(ioq);
}

static int  ide_close(QioIOQ *ioq)
{
    ioq->semaphore = IDE_CLOSE;
    return que_it(ioq);
}

static int  ide_fstat( QioIOQ *ioq, struct stat *stat )
{
    ioq->pparam0 = stat;
    ioq->semaphore = IDE_FSTAT;
    return que_it(ioq);
}

static const QioFileOps ide_fops  = {
    ide_lseek,  /* a dummy lseek is allowed but it doesn't do anything */
    ide_read,   /* FYI: read always returns EOF */
    ide_write,  /* FYI: writes always succeed without error */
    0,      /* ioctl not allowed on null device */
    ide_open,   /* use open on null device */
    ide_close,  /* use default close on null device */
    0,      /* delete not allowed */
    0,      /* fsync not allowed on null device */
    0,      /* mkdir not allowed */
    0,      /* rmdir not allowed */
    0,      /* rename not allowed */
    0,      /* truncate not allowed */
    0,      /* statfs not allowed */
    ide_fstat,  /* fstat allowed */
    0,      /* nothing to cancel on this device */
    0,      /* cannot be a tty */
    ide_readwpos, /* read with built-in lseek */
    ide_writewpos /* write with built-in lseek */
};

#if !defined(NUM_HDRIVES)
    #define NUM_HDRIVES 1
#endif

static Pinfo dinfo[] = { 
    { "/dev/hda", "", 0, 0 }       /* Drive 0 */
    ,{ "/dev/hdb", "", 0, 0}       /* Drive 1 */
    ,{ "/dev/hda", "", 0, 0}       /* Drive 2 (raw disk) */
    ,{ "/dev/hdb", "", 0, 0}       /* Drive 3 (raw disk) */
};

int fsys_set_hd_names(int number, char *names[], int partition_option )
{
    int ii, lim=n_elts(dinfo);
    for (ii=0; ii < lim; ++ii)
    {
        memset(dinfo[ii].name,0,sizeof(dinfo[ii].name));
    }
    if ( lim > number )
        lim = number;
    for (ii=0; ii < lim; ++ii)
    {
        strncpy(dinfo[ii].name,names[ii],sizeof(dinfo[ii].name));
        dinfo[ii].partition_option = partition_option;
    }
    return lim;
}

static const QioDevice ide_dvc[] = {
    {
        "rd0",      /* device name */
        3,      /* length of name */
        &ide_fops,  /* list of operations allowed on this device */
        0,      /* no mutex required */
        0,      /* unit 0 */
        (void *)dinfo   /* Partition info is stored in here */
    },
    {
        "rd1",      /* device name */
        3,      /* length of name */
        &ide_fops,  /* list of operations allowed on this device */
        0,      /* no mutex required */
        1,      /* unit 1 */
        (void *)(dinfo+1) /* Partition info is stored in here */
    },
    {
        "rd2",      /* device name */
        3,      /* length of name */
        &ide_fops,  /* list of operations allowed on this device */
        0,      /* no mutex required */
        0,      /* unit 0 */
        (void *)(dinfo+2) /* Partition info is stored in here */
    },
    {
        "rd3",      /* device name */
        3,      /* length of name */
        &ide_fops,  /* list of operations allowed on this device */
        0,      /* no mutex required */
        1,      /* unit 1 */
        (void *)(dinfo+3) /* Partition info is stored in here */
    }
};

typedef struct part
{
    U8 status;
    U8 st_head;
    U16 st_sectcyl;
    U8 type;
    U8 en_head;
    U16 en_sectcyl;
    U32 abs_sect;
    U32 num_sects;
} Partition;

struct bootsect
{
    U8 jmp[3];          /* 0x000 x86 jump */
    U8 oem_name[8];     /* 0x003 OEM name */
    U8 bps[2];          /* 0x00B bytes per sector */
    U8 sects_clust;     /* 0x00D sectors per cluster */
    U16 num_resrv;      /* 0x00E number of reserved sectors */
    U8 num_fats;        /* 0x010 number of FATs */
    U8 num_roots[2];        /* 0x011 number of root directory entries */
    U8 total_sects[2];      /* 0x013 total sectors in volume */
    U8 media_desc;      /* 0x015 media descriptor */
    U16 sects_fat;      /* 0x016 sectors per FAT */
    U16 sects_trk;      /* 0x018 sectors per track */
    U16 num_heads;      /* 0x01A number of heads */
    U32 num_hidden;     /* 0x01C number of hidden sectors */
    U32 total_sects_vol;    /* 0x020 total sectors in volume */
    U8 drive_num;       /* 0x024 drive number */
    U8 reserved0;       /* 0x025 unused */
    U8 boot_sig;        /* 0x026 extended boot signature */
    U8 vol_id[4];       /* 0x027 volume ID */
    U8 vol_label[11];       /* 0x02B volume label */
    U8 reserved1[8];        /* 0x036 unused */
    U8 bootstrap[384];      /* 0x03E boot code */
    Partition parts[4];     /* 0x1BE partition table */
    U16 end_sig;        /* 0x1FE end signature */
} __attribute__ ((packed));

typedef struct bootsect BootSect;

extern int debug_level;

static void computeRawDiskSize(Pinfo *info)
{
    const char *dvcName;
    FILE *fp;
    char buff[128];

    if (debug_level)
        printf("Looking up raw disk size in sectors for %s\n", info->name);
    dvcName = strrchr(info->name,'/');
    if ( !dvcName )
        dvcName = info->name;
    else
        ++dvcName;
    fp = fopen("/proc/partitions","r");
    if ( fp )
    {
        while (fgets(buff,sizeof(buff),fp))
        {
            char name[128];
            int major, minor, qty;
            long long kilobytes;
            qty = sscanf(buff,"%d %d %lld %s", &major, &minor, &kilobytes, name);
            if ( qty == 4 && !strcmp(dvcName,name) )
            {
                double gb;
                fclose(fp);
                gb = kilobytes;
                gb /= 1024.0*1024.0;
                info->num_sects = kilobytes*2;
                if ( debug_level)
                    printf("    Found %s. blocks=%lld, %.2fGB\n", dvcName, kilobytes, gb );
                return;
            }
        }
        fprintf(stderr,"ERROR: Didn't find \"%s\" in /proc/partitions\n", dvcName );
        fclose(fp);
    }
}

static int find_part(Pinfo *info)
{
    int fd, sts;
    struct stat64 st;
/*    char emsg[132]; */
    BootSect sector __attribute__ ((aligned)), *bp;
    Partition lclpart, *lp = &lclpart;

    sts = stat64(info->name,&st);
    if ( sts < 0 )
    {
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
        fprintf(stderr,"Error (%d) stat64()'ing %s: %s\n", errno, info->name, strerror(errno));
#endif
        return 1;
    }
    if ( info->partition_option != FSYS_PARTITION_NEVER )
    {
        fd = open(info->name, O_RDONLY|O_LARGEFILE);    /* open the raw disk */
        if (fd < 0)
        {
    #if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
            fprintf(stderr,"Error (%d) opening %s: %s\n", errno, info->name, strerror(errno));
    #endif
            return 1;
        }
        bp = &sector;
        do
        {
            sts = read(fd, bp, sizeof(BootSect));
        } while (sts < 0 && errno == EINTR);
        if (sts <= 0)
        {
            fprintf(stderr, "Error reading sector 0 on %s", info->name);
            close(fd);
            return 2;
        }
        close(fd);
        for (sts=0; sts < 4; ++sts)
        {
            memcpy(lp, bp->parts + sts, sizeof(Partition));
            if (debug_level)
                printf("Partition table entry %d: type %02X sects: 0x%08lX-0x%08lX\n",sts, lp->type, lp->abs_sect, lp->num_sects ? lp->abs_sect+lp->num_sects-1 : lp->abs_sect );
            if (lp->status == 0x80 && lp->type == 0x8f)   /* partition bootable and one of ours? */
            {
                if ( S_ISREG(st.st_mode) )
                {
                    snprintf(info->part_name, sizeof(info->part_name), "%s(-%d)", info->name, sts+1);
                    info->fd = open(info->name, O_RDWR|O_LARGEFILE);
                    if (info->fd < 0)
                    {
                        fprintf(stderr, "Error (%d) opening %s for r/w: %s\n", errno, info->part_name, strerror(errno));
                        return 3;
                    }
                    info->relative_sect = lp->abs_sect;
                    info->base_sect = lp->abs_sect;
                    info->num_sects = lp->num_sects;
                    if (debug_level)
                        printf("Opened fake partition %s for r/w on fd %d; sectors 0x%08lX-0x%08lX\n",
                                            info->part_name, info->fd, lp->abs_sect, lp->abs_sect+lp->num_sects-1);
                    return 0;
                }
                snprintf(info->part_name, sizeof(info->part_name), "%s%d", info->name, sts+1);
                info->fd = open(info->part_name, O_RDWR);
                if (info->fd < 0)
                {
                    fprintf(stderr, "Error opening %s for r/w: %s\n", info->part_name, strerror(errno));
                    return 3;
                }
                info->relative_sect = 0;
                info->base_sect = lp->abs_sect;
                info->num_sects = lp->num_sects;
                if (debug_level)
                    printf("Opened %s for r/w on fd %d; sectors 0x%08lX-0x%08lX\n",
                                        info->part_name, info->fd, lp->abs_sect, lp->abs_sect+lp->num_sects-1);
                return 0;
            }
        }    
        if (info->partition_option == FSYS_PARTITION_ALWAYS )
        {
            fprintf(stderr, "Error: Didn't find an Atari fsys partition on %s\n", info->name);
            return 4;
        }
        if (debug_level)
            printf("INFO: Didn't find an Atari fsys partition on %s\n", info->name);
    }
    snprintf(info->part_name, sizeof(info->part_name), "%s", info->name);
    info->fd = open(info->name, O_RDWR|O_LARGEFILE);    /* open the raw disk */
    if (info->fd < 0)
    {
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
        fprintf(stderr,"Error (%d) opening for r/w %s: %s\n", errno, info->name, strerror(errno));
#endif
        return 1;
    }
    info->relative_sect = 0;
    info->base_sect = 0;
    info->num_sects = st.st_size >> 9;
    if ( S_ISBLK(st.st_mode) && !st.st_size )
    {
        computeRawDiskSize(info);
    }
    if (debug_level)
    {
        printf("Opened whole file %s for r/w on fd %d; sectors 0x00000000-0x%08lX (num_sects=0x%08lX)\n",
                            info->part_name, info->fd, info->num_sects-1, info->num_sects);
        printf("     S_ISBLK=%s, st_mode=0x%06X, st_size=0x%08llX, st_blksize=%ld, st_blocks=%lld\n",
               S_ISBLK(st.st_mode) ? "yes" : "no", st.st_mode, st.st_size, st.st_blksize, st.st_blocks );
    }
    return 0;
}

#if !defined(NUM_HDRIVES)
    #define NUM_HDRIVES 1
#endif

int ide_init(void)
{
    static int been_here;
    if (!been_here)
    {
        int ii;
        for (ii=0; ii < 4; ++ii)
        {
            switch (ii)
            {
#if NUM_HDRIVES > 1
            case 1:
#endif
            case 0:
                if (!find_part(dinfo+ii))     /* init the partition info */
                {
                    qio_install_dvc(ide_dvc+ii);    /* load the device */
                }
                break;
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) != I86_PC)
#if NUM_HDRIVES > 1
            case 3: 
#endif
            case 2: {
                    int fd, sts;

                    do
                    {
                        fd = open(dinfo[ii].name, (ii == 2) ? O_RDONLY : O_RDWR );  /* raw drive 0 is read_only */
                    } while (fd < 0 && errno == EINTR);
                    if (fd >= 0)
                    {
                        char procnam[128];
                        snprintf(procnam, sizeof(procnam), "/proc/ide/hd%c/capacity", ii == 3 ? 'b' : 'a');
                        do
                        {
                            sts = open(procnam, O_RDONLY);
                        } while (sts < 0 && errno == EINTR);
                        if (sts >= 0)
                        {
                            int len;
                            do
                            {
                                len = read(sts, procnam, sizeof(procnam));
                            } while (len < 0 && errno == EINTR);
                            if (len > 0)
                            {
                                dinfo[ii].num_sects = strtoul(procnam, 0, 0);
                            }
                            close(sts);
                            dinfo[ii].fd = fd;
                            dinfo[ii].base_sect = 0;
                            if (debug_level)
                            {
                                printf("Opened %s for %s as /%s on fd %d; sectors %ld-%ld\n",
                                       dinfo[ii].name, (ii == 3) ? "rw" : "ro", ide_dvc[ii].name,
                                       fd, dinfo[ii].base_sect, dinfo[ii].num_sects-1);
                            }
                            qio_install_dvc(ide_dvc+ii);    /* put the drive in place */
                        }
                        else if (debug_level)
                        {
                            printf("Unable to open %s: %s\n", procnam, strerror(errno));
                        }
                    }
                    else if (debug_level)
                    {
                        printf("Unable to open %s: %s\n", dinfo[ii].name, strerror(errno));
                    }
                    break;
                }
#endif
            default:
                break;
            }
        }
        been_here = 1;
    }
    return 0;
}

void ide_squawk(int row, int col)
{
}

void ide_unsquawk(void)
{
}
