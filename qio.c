/* See LICENSE.txt for license details */

#include "config.h"
#include "os_proto.h"
#include "st_proto.h"
#include <any_proto.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define QIO_LOCAL_DEFINES 1
#include "qio.h"

#ifndef __locktext
#define __locktext
#define __lockdata
#define __lockrodata
#define __lockbss
#endif

#ifndef USE_QIO_SEMAPHORES
    #define USE_QIO_SEMAPHORES (0)
#endif
#if USE_QIO_SEMAPHORES
    #include <guts_sem.h>
    #define QIO_SEM_GET(x) sem_wait(sem_qio)
    #define QIO_SEM_RELEASE(x) sem_go(sem_qio)
#else
    #define QIO_SEM_GET(x) prc_set_ipl(x)
    #define QIO_SEM_RELEASE(x) prc_set_ipl(x)
#endif

#ifndef QIO_FIN_SHIM
    #define QIO_FIN_SHIM() do { ; } while (0)
#endif

#if defined(__GNUC__) && __GNUC__
    #ifndef inline
        #define inline __inline__
    #endif
#else
    #define inline 
#endif

#ifndef n_elts
    #define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifndef QIO_GENERATION_MASK
    #define QIO_GENERATION_MASK (0x7FFF)	/* libc uses short to hold fd's, so we mask our use of it */
/* Don't let it go negative else fseek will break */
#endif

#if !defined(QIO_NO_DEFAULT_DEVICE)
    #define QIO_NO_DEFAULT_DEVICE (0)
#endif
#if !QIO_NO_DEFAULT_DEVICE && !defined(QIO_DEFAULT_DEVICE)
    #define QIO_DEFAULT_DEVICE (QIO_FNAME_SEPARATOR_STR "d0")
#endif

static U32 inx_mask __lockbss;

#if !HOST_BOARD
    #include <stdio.h>
    #include <time.h>
    #include <assert.h>
#endif		/* HOST_BOARD */

#include <sys/stat.h>

static QioFile files[QIO_MAX_FILES] __lockbss;  /* A file descriptor indexes to one of these */
static QioFile *files_freelist __lockbss;
static unsigned int files_gotten __lockbss, files_freed __lockbss;

static const QioDevice *device_table[QIO_MAX_DEVICES] __lockbss; /* array of devices */
static int num_devices __lockbss;           /* number of entries in device_table */
int (*qio_complete_vec)(struct act_q *new) __lockbss;

#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
int prc_set_ipl(int newIPL)
{
    return 0;
}
#endif

#if __SIZEOF_POINTER__ > __SIZEOF_INT__
void *qio_align(void *ptr, int algn)
{
	union
	{
		uint64_t var;
		void *ptr;
	} uu;
	uu.ptr = ptr;
	uu.var = (uu.var+(algn-1))&-algn;
	return uu.ptr;
}
#endif

#if !defined(__SIZEOF_POINTER__) || (__SIZEOF_POINTER__ > __SIZEOF_INT__)
uint32_t qio_cvtFromPtr( const void *inp)
{
	union
	{
		uint64_t big;
		uint32_t little;
	} u;
	u.big = (uint64_t)inp;
	return u.little;
}

void *qio_cvtToPtr( uint32_t inp)
{
	union
	{
		void *big;
		uint32_t little;
	} u;
	u.big = NULL;
	u.little = inp;
	return u.big;
}
#endif

/************************************************************
 * qio_complete - call user's completion routine.
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	returns nothing. May have queued the user's completion
 *	routine on the AST queue.
 */
void __locktext qio_complete(QioIOQ *ioq)
{
    int sts;
    if (ioq->complete)
    {
        ioq->aq.action = ioq->complete; /* else we need to queue it up */
        ioq->aq.param = (void *)ioq;
        if (qio_complete_vec)
        {
            sts = qio_complete_vec(&ioq->aq);
        }
        else
        {
            sts = prc_q_ast(QIO_ASTLVL, &ioq->aq);
        }
        if (!ioq->iostatus && sts)
        {
            ioq->iostatus = QIO_FATAL;  /* couldn't queue it for some reason */
        }
    }
    return;
}

static int null_readwpos(QioIOQ *ioq, off_t where, void *buf, int32_t len)
{
    ioq->iostatus = QIO_EOF;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int null_read(QioIOQ *ioq, void *buf, int32_t len)
{
    return null_readwpos(ioq, 0, buf, len);
}

static int null_writewpos(QioIOQ *ioq, off_t where, const void *buf, int32_t len)
{
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    ioq->iocount = len;
    qio_complete(ioq);
    return 0;
}

static int null_write(QioIOQ *ioq, const void *buf, int32_t len)
{
    return null_writewpos(ioq, 0, buf, len);
}

static int null_lseek(QioIOQ *ioq, off_t where, int whence)
{
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int null_open(QioIOQ *ioq, const char *path)
{
    QioFile *file;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    file = qio_fd2file(ioq->file);
    file->private = 0;
    file->sect = 0;
    file->bws = 0;
    file->size = 0;
    file->flags = 0;
    file->next = 0;
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int null_fstat( QioIOQ *ioq, struct stat *stat )
{
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    stat->st_mode = S_IFCHR;
    stat->st_size = 0;
#if !WATCOM_LIBC
    stat->st_blksize = 0;
    stat->st_blocks = 0;
#endif
    qio_complete(ioq);
    return 0;
}

static const QioFileOps null_fops =
{
    null_lseek, /* a dummy lseek is allowed but it doesn't do anything */
    null_read,  /* FYI: read always returns EOF */
    null_write, /* FYI: writes always succeed without error */
    0,      /* ioctl not allowed on null device */
    null_open,  /* use open on null device */
    0,      /* use default close on null device */
    0,      /* delete not allowed */
    0,      /* fsync not allowed on null device */
    0,      /* mkdir not allowed */
    0,      /* rmdir not allowed */
    0,      /* rename not allowed */
    0,      /* truncate not allowed */
    0,      /* statfs not allowed */
    null_fstat, /* fstat allowed */
    0,      /* nothing to cancel on this device */
    0,      /* cannot be a tty */
    null_readwpos, /* read with built-in lseek */
    null_writewpos /* write with built-in lseek */
};

static const QioDevice null_dvc =
{
    "null",             /* device name */
    4,                  /* length of name */
    &null_fops,             /* list of operations allowed on this device */
    0,                  /* no mutex required for null device */
    0,                  /* unit 0 */
};

/************************************************************
 * qio_fd2file - convert fd to pointer to QioFile
 * 
 * At entry:
 *	fd - file descriptor
 *
 * At exit:
 *	returns pointer to QioFile or 0 if error
 */
QioFile * __locktext qio_fd2file( int fd )
{
    QioFile *file = files + ( fd & inx_mask );
    if (file >= (files + n_elts(files))) return 0;
    if ( file->gen == ( fd & ~inx_mask & QIO_GENERATION_MASK ) ) return file;
    return 0;
}

/************************************************************
 * qio_file2fd - convert pointer to QioFile to fd
 * 
 * At entry:
 *	file - pointer to QioFile struct
 *
 * At exit:
 *	returns fd or -1 if error
 */
int __locktext qio_file2fd( QioFile *file )
{
    if (!file || file < files || file >= (files + n_elts(files))) return -1;
    return(file - files) | file->gen;
}

/************************************************************
 * qio_install_dvc - Add a new device to the device table
 * 
 * At entry:
 *	dvc - pointer to new device 
 *
 * At exit:
 *	device number installed or negative if failed to install
 *	because no room or already installed.
 */
int qio_install_dvc(const QioDevice *dvc)
{
    const QioDevice **dp, *d;
    int ii, oldipl;

    dp = device_table;
    oldipl = QIO_SEM_GET(INTS_OFF);
    for (ii=0; ii < num_devices; ++ii)
    {
        d = *dp++;
        if (strcmp(d->name, dvc->name) == 0)
        {
            QIO_SEM_RELEASE(oldipl);
            return -QIO_ALREADY_INST;   /* already in the table */
        }
    }
    if (num_devices >= QIO_MAX_DEVICES)
    {
        QIO_SEM_RELEASE(oldipl);
        return -QIO_TOO_MANY_DVCS;
    }
    *dp = dvc;
    ++num_devices;
    QIO_SEM_RELEASE(oldipl);
    return ii;
}

#ifndef QIO_IOQ_BATCH
    #define QIO_IOQ_BATCH	32
#endif

static QioIOQ *ioq_pool_head;
static int32_t ioq_pool_batch;
static int num_ioq_gets, num_ioq_frees;

QioIOQ * __locktext qio_getioq_ptr(QioIOQ **head, int size, int batch)
{
    int oldipl, ii;
    QioIOQ *q;

    oldipl = QIO_SEM_GET(INTS_OFF); /* this cannot be interrupted */
    q = *head;          /* get next item */
    if (!q)           /* need to get more ioq's */
    {
        QioIOQ *new;
        QIO_SEM_RELEASE(oldipl);    /* interrupts are ok now */
        if (batch <= 0) return 0; /* Sorry, can't extend */
        new = QIOcalloc(batch, size);
        if (!new)
        {
            return 0;       /* no more */
        }
        /* Link all our new entities into one chain
        */
        for (q=new, ii=0; ii < batch-1; ++ii)
        {
            QioIOQ *prev;
            q->owner = head;
            prev = q;
            q = (QioIOQ *)((char *)q + size);
            prev->next = q;
        }
        q->owner = head;
        /* interrupts not allowed for the following.
         * Somebody _might_ have returned something while
         * we were initializing the new batch
         */
        oldipl = QIO_SEM_GET(INTS_OFF);
        q->next = *head;
        q = new;
    }
    *head = q->next;
    QIO_SEM_RELEASE(oldipl);    /* interrupts ok again */
    memset((void *)q, 0, size); /* zap everything in ioq */
    q->owner = head;        /* reset the owner field */
    q->file = -1;       /* assume FD is nfg */
    return q;
}

/************************************************************
 * qio_getioq - Get a QioIOQ from the system's QIO queue pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
QioIOQ * __locktext qio_getioq(void)
{
    QioIOQ *ioq;
    if (!ioq_pool_head)
    {
        const struct st_envar *st;
        if (!ioq_pool_batch)
        {
            st = st_getenv("QIO_IOQ_BATCH", 0);
            if (!st || !st->value)
            {
                ioq_pool_batch =  QIO_IOQ_BATCH;
            }
            else
            {
                ioq_pool_batch = qio_cvtFromPtr(st->value);
            }
        }
        else
        {
            st = st_getenv("QIO_IOQ_BATCH_GROWABLE", 0);
            if (!st || !st->value) ioq_pool_batch = -1;
        }
        if (ioq_pool_batch <= 0) return 0;
    }
    ioq = qio_getioq_ptr(&ioq_pool_head, sizeof(QioIOQ), ioq_pool_batch);
    if (ioq) ++num_ioq_gets;
    return ioq;
}

int __locktext qio_freeioq_ptr(QioIOQ *que, QioIOQ **head)
{
    int oldipl;

    if (!que || !head) return 1;
    if (que->owner != head) return 1;
    que->head = head;
    oldipl = QIO_SEM_GET(INTS_OFF); /* this cannot be interrupted */
    que->next = *head;
    *head = que;
    QIO_SEM_RELEASE(oldipl);
    return 0;
}

/************************************************************
 * qio_freeioq - Free a QioIOQ as obtained from a previous
 * call to qio_getioq().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
int __locktext qio_freeioq(QioIOQ *que)
{
    if (que) ++num_ioq_frees;
    return qio_freeioq_ptr(que, &ioq_pool_head);
}

/************************************************************
 * qio_getfile - Get a free QioFile from the system's pool
 * 
 * At entry:
 *	No requirements.
 *
 * At exit:
 *	returns pointer to QioFile or 0 if none available.
 */
static QioFile * __locktext qio_getfile(void)
{
    int oldipl;
    QioFile *f;

    oldipl = QIO_SEM_GET(INTS_OFF); /* this cannot be interrupted */
    f = files_freelist;     /* get next item */
    if (f)
    {
        files_freelist = f->next;
        ++files_gotten;
    }
    QIO_SEM_RELEASE(oldipl);
    if (f)
    {
        uint32_t gen = f->gen;
        memset(f, 0, sizeof(QioFile)); /* zap all fields in this struct */
        f->gen = gen;
    }
    return f;
}

/************************************************************
 * qio_freefile - Put an unused QioFile back into the system's pool
 * 
 * At entry:
 *	file - pointer to QioFile which to free
 *
 * At exit:
 *	returns 0 if success or 1 if failure.
 */
int __locktext qio_freefile(QioFile *file)
{
    int oldipl, pos, cnt;
    QioFile **prev, *f;

    if (file < files || file > files+QIO_MAX_FILES) return 1;
    if (file->next) return 1;
    file->gen = (file->gen + 1 + inx_mask) & QIO_GENERATION_MASK;
    prev = &files_freelist;
    pos = file - files;
    oldipl = QIO_SEM_GET(INTS_OFF); /* this should not be interrupted */
    cnt = 0;
    while ((f = *prev))
    {
        if (f-files > pos)
        {
            file->next = f;
            break;
        }
        if (++cnt >= QIO_MAX_FILES)   /* prevent runaway loops with ints off */
        {
            QIO_SEM_RELEASE(oldipl);
            return 1;
        }
        prev = &f->next;
    }
    *prev = file;
    ++files_freed;
    QIO_SEM_RELEASE(oldipl);
    return 0;
}

/***********************************************************************
 * qio_init - Initialize the QIO data structs. To be called once during
 * boot sequence.
 * 
 * At entry:
 *	No requirements.
 *
 * At exit:
 *	Returns 0 for success or QIO_xxx if error.
 */
int qio_init(void)
{
    int ii;
    QioFile *file;
    QioIOQ *ioq;

    inx_mask = -1;
    while ( inx_mask & QIO_MAX_FILES ) inx_mask <<= 1;
    inx_mask = ~inx_mask;

    files_freelist = file = files;
    for (ii=0; ii < QIO_MAX_FILES-1; ++ii, ++file) file->next = file+1;
    files_freed = files_gotten;

    qio_install_dvc(&null_dvc);     /* install the null device */
    ioq = qio_getioq();
    ioq->complete = 0;      /* no completion required for this */
    ioq->timeout = 0;       /* no timeout required either */
    qio_open(ioq, QIO_FNAME_SEPARATOR_STR "null", O_RDONLY); /* stdin to FD 0 */
    qio_open(ioq, QIO_FNAME_SEPARATOR_STR "null", O_WRONLY); /* stdout to FD 1 */
    qio_open(ioq, QIO_FNAME_SEPARATOR_STR "null", O_WRONLY); /* stderr to FD 2 */
    qio_freeioq(ioq);
    return 0;
}

/************************************************************
 * qio_filecount - return count of files in use.
 *
 * At entry:
 *	No requirements.
 *
 * At exit:
 *	Returns number of open files.
 */
int qio_filecount(void)
{
    return files_gotten-files_freed;
}

/************************************************************
 * qio_lookupdvc - Get a pointer to device
 * 
 * At entry:
 *	name - pointer to null terminated string with device name
 *
 * At exit:
 *	Returns pointer to QioDevice if one is found or 0 if not.
 */

const QioDevice * __locktext qio_lookupdvc(const char *name)
{
    int ii, len;
    const QioDevice **dvc, *d;

    if (name && *name == QIO_FNAME_SEPARATOR)
    {
        ++name;
        len = strlen(name);
        dvc = device_table;
        for (ii=0; ii < QIO_MAX_DEVICES; ++ii)
        {
            d = *dvc++;
            if (!d) continue;
            if (len < d->name_len) continue;
            if (strncmp(name, d->name, d->name_len) == 0)
            {
                if (len > d->name_len && name[d->name_len] != QIO_FNAME_SEPARATOR) continue;
                return d;
            }
        }
    }
    return 0;
}

/************************************************************
 * qio_fstat - stat a file or device
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct (ioq->file must be open)
 *	stat - pointer to struct stat into which the stats are to
 *		be placed.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when stat completes.
 *	non-zero if unable to queue the stat and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_fstat(QioIOQ *ioq, struct stat *stat)
{
    const QioDevice *dvc;
    QioFile *file;
    int sts;

    if (!ioq) return QIO_INVARG;        /* must have an ioq ptr */
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!stat) return(ioq->iostatus = QIO_INVARG); /* must have a param */
    memset((void *)stat, 0, sizeof(struct stat));
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    dvc = file->dvc;
    if (!dvc->fio_ops->fstat) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    sts = (dvc->fio_ops->fstat)(ioq, stat);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_statfs - stat a filesystem
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct (ioq->file must be open)
 *	name - name of filesystem
 *	stat - pointer to struct stat into which the stats are to
 *		be placed.
 *	len - sizeof 'stat'
 *	fstyp - expected type of filesystem (not used at this time)
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when statfs completes.
 *	non-zero if unable to queue the stat and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_statfs(QioIOQ *ioq, const char *name, struct qio_statfs *stat, int len, int fstyp)
{
    const QioDevice *dvc;
    int sts;

    if (!ioq) return QIO_INVARG; /* must have ioq ptr */
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    if (ioq->aq.que) return QIO_IOQBUSY;    /* ioq already in use */
    ioq->iocount = 0;
    if (len < sizeof(struct qio_statfs) ||  /* size has to be right */
        !stat ||            /* has to be a pointer */
        !name)            /* and a name */
    {
        return(ioq->iostatus = QIO_INVARG); /* bad param */
    }
    memset((void *)stat, 0, len);   /* prezero everything */
    dvc = qio_lookupdvc(name);
    if (!dvc || !dvc->fio_ops || !dvc->fio_ops->statfs)
    {
        return(ioq->iostatus = QIO_NOTSUPP);
    }
    ioq->private = (void *)dvc;     /* pass this to statfs */
    ioq->iostatus = 0;
    sts = (dvc->fio_ops->statfs)(ioq, name, stat, len, fstyp);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/****************************************************************
 * qio_readwpos - read bytes from file after positioning to where
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - position of place in file to begin reading
 *	buf - pointer to buffer into which to read
 *	len - number of bytes to read
 * 
 * This function is equivalent to a
 *	qio_lseek(ioq, where, SEEK_SET) followed by a
 *	qio_read(ioq, buf, len);
 *
 * NOTE: the following members in the ioq must have already been
 *	set before calling this function:
 *	ioq->file - must be a valid file descriptor
 *	ioq->complete - if completion required else 0
 *	ioq->user (and/or ioq->user2) - if ioq->complete also set
 *	ioq->timeout - if a timeout (in microseconds) is desired else 0
 *	(other members are don't cares).
 *
 * At exit:
 *	returns 0 if success or non-zero if error
 */
int __locktext qio_readwpos(QioIOQ *ioq, off_t where, void *buf, int32_t len)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!buf) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return(ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->readwpos) return(ioq->iostatus = QIO_NOTSUPP);
#ifdef QIO_VALIDATE_BUFFER_PTR
    QIO_VALIDATE_BUFFER_PTR(buf, len);
#endif
    ioq->iostatus = 0;              /* assume we're to queue */
    sts = (ops->readwpos)(ioq, where, buf, len);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_read - read bytes from file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	buf - pointer to buffer into which to read
 *	len - number of bytes to read
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when read completes.
 *	non-zero if unable to queue the read and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_read(QioIOQ *ioq, void *buf, int32_t len)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!buf) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return(ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->read) return(ioq->iostatus = QIO_NOTSUPP);
#ifdef QIO_VALIDATE_BUFFER_PTR
    QIO_VALIDATE_BUFFER_PTR(buf, len);
#endif
    ioq->iostatus = 0;      /* assume we're to queue */
    sts = (ops->read)(ioq, buf, len);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/****************************************************************
 * qio_writewpos - write bytes to file after position to 'where'
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - position in file/device to start writing
 *	buf - pointer to buffer which to write
 *	len - number of bytes to write
 * 
 * This function is equivalent to a
 *	qio_lseek(ioq, where, SEEK_SET) followed by a
 *	qio_write(ioq, buf, len);
 *
 * NOTE: the following members in the ioq must have already been
 *	set before calling this function:
 *	ioq->file - must be a valid file descriptor
 *	ioq->complete - if completion required else 0
 *	ioq->user (and/or ioq->user2) - if ioq->complete also set
 *	ioq->timeout - if a timeout (in microseconds) is desired else 0
 *	(other members are don't cares).
 *
 * At exit:
 *	returns 0 if success or non-zero if error
 */
int __locktext qio_writewpos(QioIOQ *ioq, off_t where, const void *buf, int32_t len)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!buf) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return(ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->writewpos) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    sts = (ops->writewpos)(ioq, where, buf, len);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_write - write bytes to file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	buf - pointer to buffer which to write
 *	len - number of bytes to write
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when read completes.
 *	non-zero if unable to queue the read and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_write(QioIOQ *ioq, const void *buf, int32_t len)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!buf) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return(ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->write) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    sts = (ops->write)(ioq, buf, len);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_open - Open a device or file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	path - pointer to a null terminated string with dvc/path/name.
 *	mode - the logical 'or' of one or more of the O_xxx flags found
 *		in fcntl.h
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will contain the FD of the newly
 *	opened file or -1 if the open failed.
 */
int __locktext qio_open(QioIOQ *ioq, const char *path, int mode)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;           /* just on general principle */
    if (!path) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->open) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;          /* no special */
    file->dvc = dvc;            /* remember which device we're talking to */
    file->mode = mode+1;        /* remember I/O mode */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->open)(ioq, path+fake);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_delete - delete a file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	path - pointer to a null terminated string with dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_delete(QioIOQ *ioq, const char *path)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;           /* just on general principle */
    if (!path) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->delete) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;          /* no special */
    file->dvc = dvc;            /* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->delete)(ioq, path+fake);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_rmdir - delete a directory
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	path - pointer to a null terminated string with dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_rmdir(QioIOQ *ioq, const char *path)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;           /* just on general principle */
    if (!path) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->rmdir) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;          /* no special */
    file->dvc = dvc;            /* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->rmdir)(ioq, path+fake);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_rename - rename a file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	source - pointer to a null terminated string with src dvc/path/name.
 *	dest - pointer to a null terminated string with dst dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_rename(QioIOQ *ioq, const char *source, const char *dest)
{
    const QioFileOps *fops;
    const QioDevice *dvcs, *dvcd;
    QioFile *file;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;           /* just on general principle */
    if (!source || !*source || !dest || !*dest) return(ioq->iostatus = QIO_INVARG);
    dvcs = qio_lookupdvc(source);
    if (!dvcs) return(ioq->iostatus = QIO_NSD);
    dvcd = qio_lookupdvc(dest);
    if (!dvcd) return(ioq->iostatus = QIO_NSD);
    if (dvcs != dvcd) return(ioq->iostatus = QIO_NOTSAMEDVC);
    fops = dvcs->fio_ops;
    if (!fops || !fops->rename) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;          /* no special */
    file->dvc = dvcs;           /* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->rename)(ioq, source+dvcs->name_len+1, dest+dvcs->name_len+1);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_openspc - Open a device or file with special parameters
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	spc - pointer to a QioOpenSpc struct which may be defined
 *		differently depending on the device which is being opened.
 *		The first two members of the struct _MUST_ be path and
 *		mode respectively.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will contain the FD of the newly
 *	opened file or -1 if the open failed.
 */
int __locktext qio_openspc(QioIOQ *ioq, QioOpenSpc *spc)
{
    const QioFileOps *fops;
    const char *path;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;           /* general principle */
    if (!spc) return(ioq->iostatus = QIO_INVARG);
    path = spc->path;
    if (!path) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->open) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = spc;        /* remember ptr to user's special if any */
    file->dvc = dvc;            /* remember which device we're talking to */
    file->mode = spc->mode+1;
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->open)(ioq, path+fake);
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_ioctl - issue an ioctl to specified file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	cmd - command argument
 *	arg - argument defined by command and device
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_ioctl(QioIOQ *ioq, unsigned int cmd, void *arg)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (!ops->ioctl) return(ioq->iostatus = QIO_NOTSUPP);
    sts = (ops->ioctl)(ioq, cmd, arg); /* call driver's ioctl routine */
    QIO_FIN_SHIM();             /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_close - close a file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_close(QioIOQ *ioq)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (ops->close)
    {
        sts = (ops->close)(ioq); /* call driver's close routine */
        QIO_FIN_SHIM();     /* allow for I/O shim */
        return sts;
    }
    file->dvc = 0;      /* use the default close */
    ioq->file = -1;     /* tell 'em fd is no good anymore */
    qio_freefile(file); /* put file back on freelist */
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);  /* call his completion routine */
    QIO_FIN_SHIM(); /* allow for I/O shim */
    return 0;
}

/************************************************************
 * qio_isatty - check if device is a tty
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_isatty(QioIOQ *ioq)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (!ops->isatty) return(ioq->iostatus = QIO_NOTSUPP);
    sts = (ops->isatty)(ioq);       /* call driver's isatty routine */
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_lseek - seek a file to a specific position
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - new position
 *	whence - one of SEEK_SET, SEEK_END or SEEK_CUR (as defined
 *		in <unistd.h>
 *
 * At exit:
 *	0 if successfully queued, although it normally does not
 *	require that any queuing take place. iostatus contains
 *	error code if any and iocount contains the new position.
 */
int __locktext qio_lseek(QioIOQ *ioq, off_t where, int whence )
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (!ops->lseek) return QIO_NOTSUPP;    /* function is not supported by that driver */
    sts = (ops->lseek)(ioq, where, whence); /* call driver's close routine */
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;    
}

/************************************************************
 * qio_mkdir - make a directory 
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	arg - pointer to null terminated string with dvc/path/dirname
 *	mode - not used at this time.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will be set to -1 in any case.
 */
int __locktext qio_mkdir(QioIOQ *ioq, const char *name, int mode)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;
    if (!name || *name == 0) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(name);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->mkdir) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->dvc = dvc;            /* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->mkdir)(ioq, name+fake, mode);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;    
}

/************************************************************
 * qio_fsync - sync a filesystem
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	arg - pointer to null terminated string with dvc/path/dirname
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when fsync completes.
 *	non-zero if unable to queue the fsync open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will be set to -1 in any case.
 */
int __locktext qio_fsync(QioIOQ *ioq, const char *name)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->file = -1;         /* assume failure */
    ioq->iocount = 0;
    if (!name || *name == 0) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(name);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fops = dvc->fio_ops;
    if (!fops || !fops->fsync) return(ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();       /* get a free file */
    if (!file) return(ioq->iostatus = QIO_NOFILE); /* ran out of file descriptors */
    file->dvc = dvc;            /* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    sts = (fops->fsync)(ioq);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_opendir - Open a directory 
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	dirp - pointer to pointer to type DIR (as defined in fsys.h for fsys directories)
 *	path - pointer to a null terminated string with dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	A pointer to a kernel provided struct (DIR in the case of fsys files)
 *	will have been placed into the location pointed to by the dirp parameter
 *	or 0 if an error prevented the open.
 */
int __locktext qio_opendir(QioIOQ *ioq, void **dirp, const char *path)
{
    const QioFileOps *fops;
    const QioDevice *dvc;
    int fake = 0, sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;           /* just on general principle */
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    *dirp = 0;              /* assume failure */
    if (!path) return(ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
#if defined(QIO_DEFAULT_DEVICE)
    if (!dvc)             /* name not in device table */
    {
        dvc = qio_lookupdvc(QIO_DEFAULT_DEVICE); /* assume open to filesystem on default device */
        fake = 1;
    }
#endif
    if (!dvc) return(ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->opendir) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    ioq->private = (void *)dvc;
    sts = (fops->opendir)(ioq, dirp, path+fake);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_readdir - read a directory entry.
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	dirp - pointer to pointer to type DIR (as defined in fsys.h for fsys directories)
 *	path - pointer to a type 'struct direct' into which the results will be placed.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int __locktext qio_readdir(QioIOQ *ioq, void *dirp, void *direct)
{
    const QioDevice *dvc;
    const QioFileOps *fops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;           /* just on general principle */
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    if (!(dvc=(const QioDevice *)ioq->private)) return(ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->readdir) return(ioq->iostatus = QIO_NOTSUPP);
    sts = fops->readdir(ioq, dirp, direct);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

int __locktext qio_rewdir(QioIOQ *ioq, void *dirp)
{
    const QioDevice *dvc;
    const QioFileOps *fops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    if (!(dvc = (const QioDevice *)ioq->private)) return(ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->rewdir) return(ioq->iostatus = QIO_NOTSUPP);
    sts = fops->rewdir(ioq, dirp);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

int __locktext qio_closedir(QioIOQ *ioq, void *dirp)
{
    const QioDevice *dvc;
    const QioFileOps *fops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;           /* just on general principle */
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    if (!(dvc = (const QioDevice *)ioq->private)) return(ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->closedir) return(ioq->iostatus = QIO_NOTSUPP);
    sts = fops->closedir(ioq, dirp);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

int __locktext qio_seekdir(QioIOQ *ioq, void *dirp, int32_t loc)
{
    if (!ioq) return QIO_INVARG;
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    return(ioq->iostatus = QIO_NOTSUPP);
}

int __locktext qio_telldir(QioIOQ *ioq, void *dirp)
{
    if (!ioq) return QIO_INVARG;
    if (!dirp) return(ioq->iostatus = QIO_INVARG);
    return(ioq->iostatus = QIO_NOTSUPP);
}

/************************************************************
 * qio_getmutex - get mutex and switch to AST level.
 * 
 * At entry:
 *	mutex - pointer to mutex struct
 *	func - ptr to AST function
 *	arg - argument to pa
 *
 * At exit:
 *	returns 0 if success or one of QIO_MUTEX_xxx if not.
 *
 * The function is queued at the specified AST level. If the mutex is
 * busy, the function is put on a wait queue and will be queued
 * when the mutex is free'd.
 */
int __locktext qio_getmutex(QioMutex *mutex, void (*func)(QioIOQ *), QioIOQ *ioq)
{
    int oldipl, sts;
    struct act_q *q;

    if (!mutex || !func || !ioq) return QIO_MUTEX_INVARG;
    oldipl = QIO_SEM_GET(INTS_OFF); /* stop all activity */
    q = &ioq->aq;
    if ( q->que )
    {
        QIO_SEM_RELEASE(oldipl);    /* interrupts ok now */
        return QIO_MUTEX_BUSY;
    }
    if (!mutex->current)      /* if the mutex is available */
    {
        q->next = 0;            /* ready this for execution */
        q->action = func;       /* remember caller's function */
        q->param = (void *)ioq;     /* and ptr to QioIOQ */
        mutex->current = q;     /* claim the mutex */
        sts = prc_q_ast(QIO_ASTLVL, q); /* queue his function at AST level */
        if (sts)
        {
            mutex->current = 0;     /* and not busy */
            QIO_SEM_RELEASE(oldipl);
            return QIO_MUTEX_FATAL; /* couldn't queue AST for some reason */
        }
        QIO_SEM_RELEASE(oldipl);
        QIO_FIN_SHIM();         /* flush que if necessary */
        return 0;           /* success */
    }
    if (mutex->current->param == (void *)ioq)
    {
        QIO_SEM_RELEASE(oldipl);
        return QIO_MUTEX_NESTED;    /* cannot claim mutex with identical parameter */
    }
    q->next = 0;            /* ready this for execution */
    q->action = func;           /* remember caller's function */
    q->param = (void *)ioq;     /* and ptr to QioIOQ */
    if (mutex->tail)          /* if there is a tail pointer */
    {
        mutex->tail->next = q;      /* put the new guy at the end of waiting list */
    }
    else
    {
        mutex->waiting = q;     /* else first is also the last */
    }
    mutex->tail = q;            /* new guy becomes the new tail */
    q->que = (struct act_q *)&mutex->waiting;   /* this is the head of this queue */
    QIO_SEM_RELEASE(oldipl);        /* interrupts ok now */
    return 0;               /* success */    
}

/************************************************************
 * qio_freemutex - free a previously claimed volume mutex.
 * 
 * At entry:
 *	mutex - pointer to mutex struct
 *	valid - pointer to QioIOQ that was passed to the getmutex
 *		function. (this parameter is used just to validate that
 *		the 'free' is being done by the same I/O that
 *		claimed the mutex).
 *
 * At exit:
 *	returns 0 if success or one of QIO_MUTEX_xxx if error.
 */
int __locktext qio_freemutex(QioMutex *mutex, QioIOQ *ioq)
{
    int oldipl, sts;
    struct act_q *q;

    if (!mutex) return QIO_MUTEX_INVARG;        /* no mutex pointer */
    if (!(q=mutex->current)) return QIO_MUTEX_NONE; /* no mutex claimed */
    if (q->param != (void *)ioq) return QIO_MUTEX_NOTOWN; /* mutex claimed by someone else */
    oldipl = QIO_SEM_GET(INTS_OFF); /* can't have interrupts for the following */
    q = mutex->waiting;         /* pluck the next guy off the waiting list */
    mutex->current = q;         /* make it current */
    if (!q)               /* if nobody is waiting */
    {
        mutex->tail = 0;        /* call me paranoid */
        QIO_SEM_RELEASE(oldipl);    /* interrupts ok now */
        return 0;           /* success */
    }
    if (!(mutex->waiting = q->next))  /* next guy in list is first one waiting */
    {
        mutex->tail = 0;        /* if we took the last one, then we also took the tail */
    }
    QIO_SEM_RELEASE(oldipl);        /* interrupts ok now */
    q->que = q->next = 0;       /* ready the queue for execution */
    sts = prc_q_ast(QIO_ASTLVL, q); /* jump to ast level */
    if (sts) return QIO_MUTEX_FATAL;    /* I do not believe it is possible for prc_q_ast to fail */
    QIO_FIN_SHIM();         /* flush queue if necessary */
    return 0;
}

/************************************************************
 * qio_cancel - cancel a pending qio
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when cancel completes.
 *	Non-zero if unable to queue the cancel; completion routine
 *	will _not_ be called in that case.
 */
int __locktext qio_cancel(QioIOQ *ioq)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (!ops->cancel) return(ioq->iostatus = QIO_NOTSUPP);
    sts = (ops->cancel)(ioq);       /* call driver's cancel routine */
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_ioq_enq - Add an IOQ to a list.
 *
 * At entry:
 *	head - pointer to pointer to list head
 *	tail - pointer to pointer to list tail
 *	new  - pointer to new item
 * At exit:
 *	returns nothing
 */

void __locktext qio_ioq_enq( QioIOQ **head, QioIOQ **tail, QioIOQ *new )
{
    if (new && head && tail)
    {
        new->aq.next = 0;
        new->aq.que = (struct act_q *)head; /* point back to head */

        if ( *tail )          /* list has members */
        {
            (*tail)->aq.next = (struct act_q *)new;
            *tail = new;
        }
        else
        {
            *head = *tail = new;
        }
    }
}

/*
 * qio_strip_ioq - remove QioIOQ structures associated with "file"
 *		 from mutex waiting list specified by "pqm" and
 *		 put them in a new list. Return pointer to first
 *		 item on this new list.
 * 
 * At entry:
 *	pqm    - pointer to QioMutex structure
 *	file   - file id to match during search
 *
 * At exit:
 *	Returns pointer to first item in new list (or 0 if no items).
 */
QioIOQ * __locktext qio_strip_ioq( QioMutex *pqm, int file )
{
    int old_ipl;
    struct act_q **head = &pqm->waiting;
    QioIOQ *ph = 0;
    QioIOQ *pt = 0;
    QioIOQ *pioq;

    old_ipl = QIO_SEM_GET(INTS_OFF);
    pqm->tail = 0;
/*
 * Remove any QioIOQ associated with this file and place
 * it on a temporary list while interrupts are disabled.
 */
    while ( *head )
    {
        pioq = (QioIOQ *)(*head)->param;
        if ( pioq->file == file )
        {
            *head = (*head)->next;
            qio_ioq_enq( &ph, &pt, pioq );
        }
        else
        {
            pqm->tail = *head;
            head = &(*head)->next;
        }
    }
    QIO_SEM_RELEASE(old_ipl);
    return ph;
}

/*
 * qio_cleanup - remove QioIOQ structures associated with "file"
 *		 from mutex waiting list specified by "pqm", and
 *		 complete them with "status".
 * 
 * At entry:
 *	pqm    - pointer to QioMutex structure
 *	file   - file id to match during search
 *	status - completion iostatus
 *
 * At exit:
 *	Non-zero if QioIOQ structure on mutex.current is associated
 *	with "file" -- pointer to QioIOQ but mutex is not released.
 *	Zero if QioIOQ structure on mutex.current is not associated
 *	with "file"
 *
 *	Used in qio_eth.c by eth_cancel_ast and cmd_ide, phx_ide and fsys.c 
 */
QioIOQ * __locktext qio_cleanup( QioMutex *pqm, int file, off_t status )
{
    QioIOQ *ph;
    QioIOQ *pioq;

    ph = qio_strip_ioq(pqm, file);

    while ( ( pioq = ph ) )
    {
        ph = (QioIOQ *)pioq->aq.next;
        pioq->aq.next = 0;
        pioq->aq.que = 0;
        pioq->iostatus = status;
        qio_complete(pioq);
    }
    if ( pqm->current )
    {
        pioq = (QioIOQ *)pqm->current->param;
        if ( pioq->file == file ) return pioq;
    }
    return 0;
}

/*
 * qio_complete_list - call qio_complete for each item in list.
 * 
 * At entry:
 *	ioq - pointer to first item on list (uses aq field for links)
 *
 * At exit:
 *	Returns number of items queued for completion
 */
int __locktext qio_complete_list(QioIOQ *ioq, U32 status)
{
    int cnt = 0;
    QioIOQ *i0;
    while (ioq)
    {
        i0 = (QioIOQ *)ioq->aq.next;
        ioq->aq.next = ioq->aq.que = 0;
        ioq->iostatus = status;
        ioq->iocount = 0;
        qio_complete(ioq);
        ioq = i0;
        ++cnt;
    }
    return cnt;
}

/************************************************************
 * qio_readv - read input to scattered buffers
 *
 * At entry:
 *	ioq    - pointer to QioIOQ struct
 *	iov    - pointer to array of IOVect struct
 *	iovcnt - number of IOVect struct in array
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when readv completes.
 *	Non-zero if unable to queue the readv; completion routine
 *	will _not_ be called in that case.
 */

int __locktext qio_readv(QioIOQ *ioq, const IOVect *iov, int32_t iovcnt)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!iov || !iovcnt) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return(ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->readv) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;      /* assume we're to queue */
    sts = (ops->readv)(ioq, iov, iovcnt);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

/************************************************************
 * qio_writev - write output from scattered buffers
 *
 * At entry:
 *	ioq    - pointer to QioIOQ struct
 *	iov    - pointer to array of IOVect struct
 *	iovcnt - number of IOVect struct in array
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when writev completes.
 *	Non-zero if unable to queue the writev; completion routine
 *	will _not_ be called in that case.
 */

int __locktext qio_writev(QioIOQ *ioq, const IOVect *iov, int32_t iovcnt)
{
    QioFile *file;
    const QioFileOps *ops;
    int sts;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que)
    {
        return ioq->iostatus = QIO_IOQBUSY;
    }
    ioq->iocount = 0;
    if (!iov || !iovcnt) return(ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return(ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return(ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->writev) return(ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    sts = (ops->writev)(ioq, iov, iovcnt);
    QIO_FIN_SHIM();         /* allow for I/O shim */
    return sts;
}

#if !defined(KICK_WDOG)
    #if defined(WDOG) && !NO_EER_WRITE && !NO_WDOG
        #define KICK_WDOG() WDOG = 0
    #else
        #define KICK_WDOG() do { ; } while (0)
    #endif
#endif

#if !defined(NO_QIOW_FUNCTIONS) || !NO_QIOW_FUNCTIONS

/*************************************************************
 * The following functions are wait mode flavors of qio_xxx()
 * calls. They all return with the value as set in iostatus
 * field of the ioq.
 *
 * NOTE: Being blocking wait-mode I/O, they cannot
 * be called at ASTLVL or action level.
 */
int qiow_open(QioIOQ *ioq, const char *name, int mode)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_open(ioq, name, mode);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_openspc(QioIOQ *ioq, QioOpenSpc *spc)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_openspc(ioq, spc);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_close(QioIOQ *ioq)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_close(ioq);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_ioctl(QioIOQ *ioq, unsigned int cmd, void *arg)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_ioctl(ioq, cmd, arg);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_delete(QioIOQ *ioq, const char *name)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_delete(ioq, name);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_rmdir(QioIOQ *ioq, const char *name)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_rmdir(ioq, name);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_rename(QioIOQ *ioq, const char *old, const char *new)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_rename(ioq, old, new);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_isatty(QioIOQ *ioq)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_isatty(ioq);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_fstat(QioIOQ *ioq, struct stat *stat)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_fstat(ioq, stat);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_statfs(QioIOQ *ioq, const char *name, struct qio_statfs *stat, int len, int fstyp)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_statfs(ioq, name, stat, len, fstyp);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

static int wait_for_rw(QioIOQ *ioq, int sts)
{
    int cnt=0;
    while (!sts)
    {
        sts = ioq->iostatus;
        if (cnt != ioq->iocount)
        {
            KICK_WDOG();        /* making progress, kick the dog */
            cnt = ioq->iocount;
        }
    }
    return sts;
}

int qiow_read(QioIOQ *ioq, void *buf, int32_t len)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_read(ioq, buf, len);
    return wait_for_rw(ioq, sts);
}

int qiow_readwpos(QioIOQ *ioq, off_t where, void *buf, int32_t len)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_readwpos(ioq, where, buf, len);
    return wait_for_rw(ioq, sts);
}

int qiow_write(QioIOQ *ioq, const void *buf, int32_t len)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_write(ioq, buf, len);
    return wait_for_rw(ioq, sts);
}

int qiow_writewpos(QioIOQ *ioq, off_t where, const void *buf, int32_t len)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_writewpos(ioq, where, buf, len);
    return wait_for_rw(ioq, sts);
}

int qiow_lseek(QioIOQ *ioq, off_t where, int whence )
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_lseek(ioq, where, whence);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_mkdir(QioIOQ *ioq, const char *name, int mode)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_mkdir(ioq, name, mode);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_opendir(QioIOQ *ioq, void **dirp, const char *path)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_opendir(ioq, dirp, path);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_readdir(QioIOQ *ioq, void *dirp, void *direct)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_readdir(ioq, dirp, direct);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_rewdir(QioIOQ *ioq, void *dirp)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_rewdir(ioq, dirp);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_closedir(QioIOQ *ioq, void *dirp)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_closedir(ioq, dirp);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_seekdir(QioIOQ *ioq, void *dirp, int32_t loc)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_seekdir(ioq, dirp, loc);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_telldir(QioIOQ *ioq, void *dirp)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_telldir(ioq, dirp);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_fsync(QioIOQ *ioq, const char *name)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_fsync(ioq, name);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_cancel(QioIOQ *ioq)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_cancel(ioq);
    while (!sts) sts = ioq->iostatus;
    return sts;
}

int qiow_readv(QioIOQ *ioq, const IOVect *iov, int32_t iovcnt)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_readv(ioq, iov, iovcnt);
    return wait_for_rw(ioq, sts);
}

int qiow_writev(QioIOQ *ioq, const IOVect *iov, int32_t iovcnt)
{
    int sts;
    if (!ioq) return QIO_INVARG;
    if (prc_get_astlvl() >= 0) return ioq->iostatus = QIO_BADLVL;
    sts = qio_writev(ioq, iov, iovcnt);
    return wait_for_rw(ioq, sts);
}
#endif

#if !defined(QIO_NOSTDIO_SHIMS) || !QIO_NOSTDIO_SHIMS
/*************************************************************
 * The following functions are UNIX(tm) shims for the functions
 * found in stdio (libc). See the man pages for details about
 * these commands. Errors are reported as -1 with errno set to
 * the QIO_xxx error.
 *
 * NOTE: Being shims doing blocking wait-mode I/O, they cannot
 * be called at ASTLVL.
 */

int unlink(const char *name)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!name || !*name)
        {
            sts = QIO_INVARG;
            break;
        }
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_delete(ioq, name);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
        }
    } while (0);
    errno = sts;
    return retval;
}

int rmdir(char *name)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!name || !*name)
        {
            sts = QIO_INVARG;
            break;
        }
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_rmdir(ioq, name);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
        }
    } while (0);
    errno = sts;
    return retval;
}

int rename(const char *source, const char *dest)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!source || !*source || !dest || !*dest)
        {
            sts = QIO_INVARG;
            break;
        }
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_rename(ioq, source, dest);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
        }
    } while (0);
    errno = sts;
    return retval;
}

int open(const char *name, int mode, ...)
{
    QioIOQ *ioq;
    int sts, fd = -1;

    do
    {
        if (!name)
        {
            sts = QIO_INVARG;
            break;
        }
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_open(ioq, (void *)name, mode);
        fd = ioq->file;         /* save assigned fd */
        qio_freeioq(ioq);           /* done with ioq */
        if (!QIO_ERR_CODE(sts))
        {
            sts = 0;
            break;
        }
        fd = -1;
    } while (0);
    errno = sts;
    return fd;
}

int close(int fd)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_close(ioq);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
        }
    } while (0);
    errno = sts;
    return retval;
}

off_t lseek(int fd, off_t off, int wh)
{
    QioIOQ *ioq;
    int sts;
    off_t new = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_lseek(ioq, off, wh);
        new = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = 0;
            break;
        }
        new = -1;
    } while (0);
    errno = sts;
    return new;
}

int mkdir(const char *path, mode_t mode)
{
    QioIOQ *ioq;
    int sts;

    ioq = qio_getioq();
    if (!ioq)
    {
        errno = QIO_NOIOQ;
        return -1;
    }
    if (!path)
    {
        errno = QIO_INVARG;
        return -1;
    }
    if ((mode&MKDIR_RECURSE))     /* If he's asking to recursively create dirs */
    {
        struct stat st;
        char *newname, *start, *end;

        if (!qio_lookupdvc(path)) /* if doesn't start with a device name */
        {
            errno = QIO_NOTSUPP;    /* not supported for this device */
            qio_freeioq(ioq);
            return -1;
        }
        sts = strlen(path);     /* get length of path */
        newname = QIOmalloc(sts+1); /* get a local buffer to store it */
        if (!newname)         /* no memory */
        {
            errno = QIO_NOMEM;
            qio_freeioq(ioq);
            return -1;
        }
        strcpy(newname, path);      /* copy his to ours */
        start = strchr(newname+1, QIO_FNAME_SEPARATOR);
        end = newname + sts -1;     /* point to last char in the line */
        if (start) ++start;     /* skip over leading delimiter */
        while (start && end > start && *end == QIO_FNAME_SEPARATOR)
        {
            *end-- = 0;         /* eat the trailing delimiters */
        }
        if (start && start < end)
        {
            for (;(end = strchr(start, QIO_FNAME_SEPARATOR));
                *end = QIO_FNAME_SEPARATOR, start = end+1) /* while there's dirs */
            {
                *end = 0;           /* replace delimiter with null */
                if (strcmp(start, "..") == 0 || strcmp(start, ".") == 0)
                {
                    continue;       /* skip dirs named '.' and '..' */
                }
                sts = stat(newname, &st);   /* stat the file */
                if (sts)          /* if no such file */
                {
                    if (errno == FSYS_LOOKUP_FNF) /* need to add this directory */
                    {
                        sts = qiow_mkdir(ioq, newname, 0);  /* create the directory */
                        if (!QIO_ERR_CODE(sts))
                        {
                            continue;   /* success */
                        }
                        errno = sts;    /* die if it didn't work */
                    }
                    break;
                }
                if (S_ISDIR(st.st_mode))
                {
                    continue;       /* skip it if it is already a directory */
                }
                errno = FSYS_LOOKUP_NOTDIR; /* else the filename is already in use */
                break;
            }
        }
        else
        {
            errno = FSYS_LOOKUP_NOPATH;
        }
        QIOfree(newname);       /* done with the temp memory */
        if (end)          /* if aborted the loop */
        {
            qio_freeioq(ioq);       /* didn't work, so return with error in errno */
            return -1;
        }
        mode &= ~MKDIR_RECURSE;
    }
    sts = qiow_mkdir(ioq, path, mode);
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts))
    {
        errno = sts;
        return -1;
    }
    errno = 0;          /* assume success */
    return 0;
}

int read(int fd, void *buf, int len)
{
    QioIOQ *ioq;
    int sts, val = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_read(ioq, buf, len);
        val = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = 0;
            break;
        }
        if (sts == QIO_EOF)
        {
            sts = val = 0;
        }
    } while (0);
    errno = sts;
    return val;
}

int write(int fd, const void *buf, int len)
{
    QioIOQ *ioq;
    int sts, val = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_write(ioq, buf, len);
        val = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = 0;
            break;
        }
        if (sts == QIO_EOF)
        {
            val = 0;
            sts = 0;
        }
    } while (0);
    errno = sts;
    return val;
}

int ioctl(int fd, unsigned int cmd, uint32_t arg)
{
    QioIOQ *ioq;
    int sts, val = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_ioctl(ioq, cmd, (void *)arg);
        val = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = 0;
            break;
        }
        val = -1;
    } while (0);
    errno = sts;
    return val;
}

int stat(const char *name, struct stat *stat)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!name || !stat)
        {
            sts = QIO_INVARG;
            break;
        }
        memset((void *)stat, 0, sizeof(struct stat));
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_open(ioq, (void *)name, O_RDONLY);
        if (!QIO_ERR_CODE(sts))
        {
            sts = qiow_fstat(ioq, stat);
            qiow_close(ioq);
        }
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
            break;
        }
    } while (0);
    errno = sts;
    return retval;
}

int fstat(int fd, struct stat *stat)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!stat)
        {
            sts = QIO_INVARG;
            break;
        }
        memset((void *)stat, 0, sizeof(struct stat));
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_fstat(ioq, stat);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
            break;
        }
    } while (0);
    errno = sts;
    return retval;
}

int statfs(const char *name, struct statfs *stat, int len, int fstyp)
{
    QioIOQ *ioq;
    int sts, retval = -1;

    do
    {
        if (!name || !stat)
        {
            sts = QIO_INVARG;
            break;
        }
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_statfs(ioq, name, (struct qio_statfs *)stat, len, fstyp);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
            break;
        }
    } while (0);
    errno = sts;
    return retval;
}

int fstatfs(int fd, struct statfs *stat, int len, int fstyp)
{
    QioIOQ *ioq;
    QioFile *file;
    const char *name;
    char tmp[32], *tp=tmp;
    int sts, retval = -1;

    do
    {
        file = qio_fd2file(fd);
        if (!file)
        {
            sts = QIO_NOTOPEN;
            break;
        }
        name = file->dvc->name;
        if (!name)
        {
            sts = QIO_NOTSUPP;
            break;
        }
        if ((sts=strlen(name)) > sizeof(tmp)-2)
        {
            tp = QIOmalloc(sts+2);
            if (!tp)
            {
                sts = QIO_NOMEM;
                break;
            }
        }
        tp[0] = QIO_FNAME_SEPARATOR;
        strcpy(tp+1, name);
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        sts = qiow_statfs(ioq, tp, (struct qio_statfs *)stat, len, fstyp);
        if (tp != tmp) QIOfree(tp);
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = retval = 0;
            break;
        }
    } while (0);
    errno = sts;
    return retval;
}

int isatty(int fd)
{
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) return 0;
    ioq = qio_getioq();
    if (!ioq) return 0;
    ioq->file = fd;
    sts = qio_isatty(ioq);
    while (!sts)
    {
        sts = ioq->iostatus;
    }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) return 0;
    return 1;
}

int readv(int fd, const void *iov, int iovcnt)
{
    QioIOQ *ioq;
    int sts, val = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_readv(ioq, iov, iovcnt);
        val = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = val = 0;
            break;
        }
    } while (0);
    errno = sts;
    return val;
}

int writev(int fd, const void *iov, int iovcnt)
{
    QioIOQ *ioq;
    int sts, val = -1;

    do
    {
        ioq = qio_getioq();
        if (!ioq)
        {
            sts = QIO_NOIOQ;
            break;
        }
        ioq->file = fd;
        sts = qiow_writev(ioq, iov, iovcnt);
        val = ioq->iocount;
        qio_freeioq(ioq);
        if (!QIO_ERR_CODE(sts))
        {
            sts = val = 0;
            break;
        }
    } while (0);
    errno = sts;
    return val;
}
#endif				/* !QIO_NOSTDIO_SHIMS */
