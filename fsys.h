/* See LICENSE.txt for license details */
#ifndef _FSYS_H_
#define _FSYS_H_
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/types.h>		/* need off_t */
#include <qio.h>		/* need struct qio_ioq */

#ifndef BYTES_PER_SECTOR
# define BYTES_PER_SECTOR (512)
#endif

/*

This file describes the on disk structure for the AGC filesystem.

NOTE: LBA (Logical Block Address) 0 is not used in this filesystem and remains
reserved for future use. LBA of 0, when appearing in retrieval pointers etc.,
is reserved as a flag to indicate "end of list", etc. LBA 0 generally contains
the DOS partition table which is significant if FSYS_USE_PARTITION_TABLE is set.

There are FSYS_MAX_ALTS copies of what is called the "home block" on the disk.
Each home block is one sector long (BYTPSECT bytes). The home block contains some
details about the volume (see FsysHomeBlock below) plus some pointers (LBA's)
to the file headers for index.sys. There is also FSYS_MAX_ALTS copies of
index.sys on the disk and there is a pointer to the header of each of those
files in each home block. The Home Blocks reside at known (fixed) LBA's on the
volume which are determined by the definition of the macros FSYS_HB_ALG() for
the home blocks. At volume mount time, all home blocks can be read (if
possible) and verified that they contain identical information.

The contents of the index file can be thought of as a linear array of arrays
(groups) of FSYS_MAX_ALTS LBA's with the first entry in each LBA group
pointing to the file header for a file and each (optional) entry containing an
LBA to a file header for a duplicate copy of the file. The low level file open
routine will try each of the file headers in order until it successfully reads
one. There are three reserved entries in the index file:

Entry 0 in index.sys points to itself (index.sys)
Entry 1 in index.sys points the the free list file
Entry 2 in index.sys points to the root level directory.

Each file on the disk (including index.sys) has an associated file header. A
file header is one sector long (BYTPSECT bytes) and contains details about the file
(see FsysHeader below). Among those details are an array of "retrieval
pointers" (see FsysRetPtr below). A retrieval pointer contains the physical
starting disk LBA, a number of sectors and a compile time optional repeat and
skip counts. As the filesystem accesses the file, it begins at the first
retrieval pointer and reads n contigious sectors begining at the specified LBA
then optionally skips m contigious sectors and repeats this cycle 'repeat
count' times. Once the first retriveal pointer is exhausted, it continues with
the next until all retrieval pointers have been exhausted. A starting LBA of 0
in a retrieval pointer indicates the end of the list. If the file needs more
retrieval pointers than what will fit in a file header, additional retrieval
pointers will be placed in subsequent extension file header(s).

A directory is a plain file. The contents of a directory is a stream of bytes
consisting of 3 bytes of file ID (FID, which is simply the index into the
index.sys file), 1 byte of "generation number" followed by 1 byte of string
length ("string length" includes the trailing null and a length of 0 means a
length of 256) and a null terminated ASCII string representing the filename.
The "generation number" needs to match the one found in the file header for
the directory entry to be valid. The first entry in a directory contains an
index to the parent directory file with the name "..". The second entry
contains an index to itself and the name ".". 

***************************************************************************/
/**************************************************************************

The following is the list of features that the file system may have. The
definition of FSYS_FEATURES is the composite of all the avilable features
compiled into the code of this system. That is to say, space in data
structures in the file headers, etc. will have been reserved (for features
that require them) for those bits listed in FSYS_FEATURES.

The definition of FSYS_OPTIONS lists which of the features also have code
installed in the system to handle those features.

The purpose of the two separate items is so potentially one could initialise
(and/or use) a disk with a filesystem created on one host (perhaps a PC) and
be able to use it on a system with different filesystem feature list. Maybe
the s/w could cope, then again, maybe not.

 **************************************************************************

SKIP_REPEAT - is an idea where the retrieval pointers contain, in
addition to "start" and "nblocks", a "skip" and a "repeat" parameter.
The "skip" parameter says how many sectors to skip and the "repeat" says
how many times to repeat the "nblocks"/"skip" pair. The idea behind this
scheme is to allow for files to be "interleaved" with one another. This is
a feature that could prove useful if playing a movie and streaming audio,
for example. It complicates the internals of the filesystem quite a bit
to implement this feature, especially with the auto fail-over to alternate
copies of files, etc. For this reason, I provided very little code to support
the feature at this time. Implementation of this feature is left as an exercise
for the reader.
*/

#define FSYS_FEATURES_SKIP_REPEAT	0x0001

/**************************************************************************
All relevant details about a file are contained in the file header, including
the retrieval pointer set. Since the file header is exactly one sector, there
is a potential problem of there not being enough room to hold all the file's
retrieval pointers. To correct for this shortcoming, there is an "extension"
file header that contains mainly additional retrieval pointers. At this time I
did not provide any code to create or read extension file headers. Use of
extension file headers complicates the internals of the filesystem slightly
and I assumed (perhaps incorrectly) that there would be no files in a
production system with more than 20 retrieval pointers (the number that will
fit in the fileheader with the skip/repeat feature turned off). Adding code to
support extension file headers is left as an exercise for the reader.
*/

#define FSYS_FEATURES_EXTENSION_HEADER	0x0002

/**************************************************************************
There may come a time where we will want an "access" and "backup" time
recorded with each file. At the present time, there is no code to support them.
*/

#define FSYS_FEATURES_ABTIME		0x0004

/**************************************************************************
There may come a time where we will want a "modified" time recorded with each
file. The field for this time is always present in the file header, but the
code that updates it may or may not be included in the filesystem.
*/

#define FSYS_FEATURES_CMTIME		0x0008

/**************************************************************************
There may come a time where we will want a list of "permissions" recorded with
each file to allow/disallow access based on some criteria. At the present time
there is no support for this feature.
*/

#define FSYS_FEATURES_PERMS		0x0010

/**************************************************************************
This enables the journalling feature of the filesystem. The journalling
in the Atari filesystem exists only as a crash recovery device. It does
not journal every change to every sector. Consequently, it does not
protect against the cases where the game opens files for
read-modify-write ("r+") and partially updates a file just before the
system crashes. In those instances, upon reboot, the contents of those
files may be stale or outright corrupt. The journalling system only
preserves the directory structure across a system crash.
*/

#define FSYS_FEATURES_JOURNAL		0x0020

/**************************************************************************
This enables the quick mount feature where directories are indicated by
hijacking a bit in the LBA to the file header found in the index file.
*/

#define FSYS_FEATURES_DIRLBA		0x0040

/**************************************************************************
As described above, the following two variables are to be defined to the list
of features and options desired in this filesystem. The default is to include
nothing of substance. The definitions may be overridden by lines in config.mac.
*/

#ifndef FSYS_FEATURES
# define FSYS_FEATURES	(FSYS_FEATURES_CMTIME)
#endif

#ifndef FSYS_OPTIONS
# define FSYS_OPTIONS	(FSYS_FEATURES_CMTIME)
#endif

/**************************************************************************
For special features we swipe some bits from the LBAs to the file headers as
stored in the index file.
Bit 31 indicates the slot is empty.
Bit 30 indicates the file is a directory (if the FSYS_FEATURES_DIRLBA bit is set).
The following defines a mask that is used to isolate the actual LBA from the data
stored in the index file. 28 bits of mask allows for 128GBytes of disk space.
*/

#define FSYS_LBA_MASK (0x0FFFFFFF)

#define FSYS_EMPTYLBA_BIT	(1<<31)
#define FSYS_DIRLBA_BIT		(1<<30)
/* Room for two more bits */

/* FSYS_JOURNAL_FILESIZE sets the default number of sectors for the journal
file allocation. This can be overridden by the journal_sectors member of
the FsysInitVol struct at volume initialisation time.
*/

#ifndef FSYS_JOURNAL_FILESIZE
# define FSYS_JOURNAL_FILESIZE (1*1024*1024/BYTPSECT)	/* default starting size of journal file */
#endif
#ifndef FSYS_DEFAULT_JOU_EXTEND
# define FSYS_DEFAULT_JOU_EXTEND (512*1024/BYTPSECT)	/* # of sectors to grow journal file */
#endif

/**************************************************************************
A slightly faster and much more compact filesystem code can be generated if it
is known in advance that there will be no file creation/deletion, etc. This is
referred to as a READ_ONLY file system, although individual files may be
updated in place as long as they are not extended (attempts to extend a file
on a read-only filesystem will result in an error). Updates to the eof and
time fields of a file header are allowed if the FSYS_UPD_FH variable is set
otherwise no fileheader updates are performed either. The default is to
produce a read only filesystem, with updates to only the eof and time fields
of the file header allowed. Set FSYS_TIGHT_MEM if you need to have a r/w
filesystem with conservative memory usage.
*/

#ifndef FSYS_READ_ONLY
# define FSYS_READ_ONLY 1
#endif

#ifndef FSYS_UPD_FH
# define FSYS_UPD_FH 0
#endif

#ifndef FSYS_TIGHT_MEM
# if FSYS_READ_ONLY
#  define FSYS_TIGHT_MEM 1
# else
#  define FSYS_TIGHT_MEM 0
# endif
#endif

#if !FSYS_READ_ONLY
# undef FSYS_UPD_FH
# define FSYS_UPD_FH 1
#endif

/**************************************************************************
FSYS_MAX_VOLUMES defines how many concurrent volumes will be supported with
this code. The default is 1. There are some additional checks to ensure the
value is within a reasonable range. NUM_HDRIVES is a value set (optionally and
externally) to the maximum number of hard drives that the system supports.
As written, the filessystem supports only one volume per harddrive.
*/

#ifndef NUM_HDRIVES
# define NUM_HDRIVES		1
#endif

#ifndef FSYS_MAX_VOLUMES
# define FSYS_MAX_VOLUMES	(NUM_HDRIVES)	/* number of concurrent volumes supported */
#endif

#if FSYS_MAX_VOLUMES > NUM_HDRIVES
# undef FSYS_MAX_VOLUMES
# define FSYS_MAX_VOLUMES NUM_HDRIVES
#endif

#if FSYS_MAX_VOLUMES <= 0
# error Need to set FSYS_MAX_VOLUMES > 0
#endif

/**************************************************************************
FSYS_MAX_ALTS - defines how many duplicate files are to be supported by this
filesystem. The home blocks, index file, free_list file, file headers and
directory files are always automatically duplicated FSYS_MAX_ALTS times. User
files (ordinary files) are duplicated up to a max of FSYS_MAX_ALTS (selected
by the user as each file is created). Automatic failover to an alternate copy
is performed should there be a read error in one of the other copies. The
default is set to 3.
*/

#ifndef FSYS_MAX_ALTS
# define FSYS_MAX_ALTS	3		/* maximum number of alternate files */
#endif

/**************************************************************************
FSYS_DEFAULT_EXTEND sets the default number of sectors a file is extended.
Writing past the end of file (or more exactly, past the end of allocation)
will cause the file to be extended (an additional retrieval pointer added to
its list of retrieval pointers). Increasing a file's allocation by number of
sectors other than 1 is done for effeciency's sake.
*/

#ifndef FSYS_DEFAULT_EXTEND
# define FSYS_DEFAULT_EXTEND	10	/* default number of sectors to extend file */
#endif

/**************************************************************************
FSYS_DEFAULT_DIR_EXTEND sets the default number of sectors a directory is
extended. Writing past the end of file (or more exactly, past the end of
allocation) will cause the file to be extended (an additional retrieval
pointer added to its list of retrieval pointers). Increasing a file's
allocation by number of sectors other than 1 is done for effeciency's sake.
*/

#ifndef FSYS_DEFAULT_DIR_EXTEND
# define FSYS_DEFAULT_DIR_EXTEND 50	/* default number of sectors to extend directory */
#endif

/**************************************************************************
FSYS_HB_ALG - defines the algorithm with which the filesystem code can use to
find a specific home block. The default is to use the ratio of FSYS_MAX_ALTS
and the maximum LBA on the volume rounded to a sector on a 256 sector boundary
plus 1. 
*/

#if !defined(FSYS_HB_RANGE)
# define FSYS_HB_RANGE	(512000&-256)
#endif

#ifndef FSYS_HB_ALG			/* Home block location algorithm */
# define FSYS_HB_ALG(idx, max) (((((max)*(idx))/FSYS_MAX_ALTS)&-256)+1)
#endif

/**************************************************************************
FSYS_COPY_ALG - defines the algorithm with which the filesystem code can use to
find approximately where to start allocating space for a file copy. This is
only advisory in that if no room can be found, the allocator will find where
ever it can to put the file copy. Generally, it trys to distribute the copies
evenly across the volume_size/FSYS_MAX_ALTS areas.
*/

#ifndef FSYS_COPY_ALG			/* Alternate copy location algorithm */
# define FSYS_COPY_ALG(idx, max) (((((max)*(idx))/FSYS_MAX_ALTS)&-256)+1)
#endif

#ifndef FSYS_CLUSTER_SIZE
# define FSYS_CLUSTER_SIZE	1	/* blocks per cluster */
#endif

#ifndef FSYS_DIR_HASH_SIZE
# define FSYS_DIR_HASH_SIZE	31	/* number of entries in directory hash table */
    					/* (works best if it is a prime number) */
#endif

#define FSYS_VERSION_HB_MAJOR	1	/* home block major version */
#define FSYS_VERSION_HB_MINOR	7	/* home block minor version */
/* Home block version history:
 *	1.2 - cleaned up garbage left in copies of freelist files
 *	1.3 - added boot and max_lba members
 *	1.4 - added hb_range member
 *	1.5 - added upd_flag member
 *	1.6 - added more boot file lbas
 *	1.7 - added journalling
 */
#define FSYS_VERSION_FH_MAJOR	1	/* file header major version */
#define FSYS_VERSION_FH_MINOR	3	/* file header minor version */
/* File header version history:
 *	Versions prior to 1.2 have garbage in the flags field.
 */
#define FSYS_VERSION_EFH_MAJOR	1	/* extension file header major version */
#define FSYS_VERSION_EFH_MINOR	1	/* extension file header minor version */

#if defined(FSYS_REPEAT_SKIP) && FSYS_REPEAT_SKIP
#define FSYS_VERSION_RP_MAJOR	2	/* retrieval pointer major version */
#else
#define FSYS_VERSION_RP_MAJOR	1	/* retrieval pointer major version */
#endif
#define FSYS_VERSION_RP_MINOR	1	/* retrieval pointer minor version */

#define FSYS_ID_HOME	0xFEEDF00Dl
#define FSYS_ID_INDEX	0xF00DFACEl
#define FSYS_ID_HEADER	0xC0EDBABEl
#define FSYS_ID_VOLUME	((('V'+0l)<<24) | (('O'+0l)<<16) | ('L'<<8) | 'M')

enum filesys {
    FSYS_INDEX_INDEX=0,		/* first after generation # are ptrs to index.sys */
    FSYS_INDEX_FREE,		/* next are ptrs to freemap.sys */
    FSYS_INDEX_ROOT,		/* next are ptrs to root directory */
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
    FSYS_INDEX_JOURNAL,		/* next are ptrs to the journal file (only during init) */
#endif
    FSYS_INDEX_MAX,
    FSYS_TYPE_EMPTY=0,		/* file header is unused */
    FSYS_TYPE_INDEX,		/* file is an index file */
    FSYS_TYPE_DIR,		/* file is a directory */
    FSYS_TYPE_LINK,		/* file is a symlink */
    FSYS_TYPE_FILE		/* file is plain file */
};

/* Description of volume home block */

typedef struct home_block {
    unsigned long id;			/* block ID type. s/b FEEDF00D */
    unsigned short hb_minor;		/* home block minor version */
    unsigned short hb_major;		/* home block major version */
    unsigned short hb_size;		/* size in bytes of home block struct */
    unsigned short fh_minor;		/* file header minor version */
    unsigned short fh_major;		/* file header major version */
    unsigned short fh_size;		/* size in bytes of file header struct */
    unsigned short fh_ptrs;		/* number of retrieval pointers in a file header */
    unsigned short efh_minor;		/* extension file header minor version */
    unsigned short efh_major;		/* extension file header major version */
    unsigned short efh_size;		/* size in bytes of extension file header struct */
    unsigned short efh_ptrs;		/* number of retrieval pointers in an extension file header */
    unsigned short rp_minor;		/* retrieval pointer minor version */
    unsigned short rp_major;		/* retrieval pointer major version */
    unsigned short rp_size;		/* size in bytes of retrieval pointer struct */
    unsigned short cluster;		/* blocks per cluster */
    unsigned short maxalts;		/* number of alternates on this volume */
    unsigned long def_extend;		/* default number of clusters to extend files */
    unsigned long ctime;		/* volume creation date/time */
    unsigned long mtime;		/* volume modification date/time */
    unsigned long atime;		/* volume access date/time */
    unsigned long btime;		/* volume backup date/time */
    unsigned long chksum;		/* home block checksum */
    unsigned long features;		/* file system features */
    unsigned long options;		/* file system options */
    unsigned long index[FSYS_MAX_ALTS]; /* up to n ptrs to index.sys files */
    unsigned long boot[FSYS_MAX_ALTS];	/* up to n ptrs to boot file */
    unsigned long max_lba;		/* number of LBA's allocated to this volume */
    unsigned long hb_range;		/* just for fsysdmp's benefit */
    unsigned long upd_flag;		/* update indicator when set */
    unsigned long boot1[FSYS_MAX_ALTS];	/* up to n ptrs to secondary boot file */
    unsigned long boot2[FSYS_MAX_ALTS];	/* up to n ptrs to tertiary boot file */
    unsigned long boot3[FSYS_MAX_ALTS];	/* up to n ptrs to (?)ary boot file */
    unsigned long journal[FSYS_MAX_ALTS]; /* ptrs to the journal fileheader */
} FsysHomeBlock;

/* Description of retrieval pointer */

typedef struct file_retptr {
    unsigned long start;		/* starting lba */
    unsigned long nblocks;		/* number of contigious clusters */
# if defined(FSYS_REPEAT_SKIP) && FSYS_REPEAT_SKIP
    unsigned long repeat;		/* number of times to repeat the nblocks/skip pair */
    unsigned long skip;			/* number of clusters to skip */
# endif
} FsysRetPtr;

/* Description of file header */

typedef struct file_header {
    unsigned long id;			/* file header type */
    unsigned long size;			/* file size in bytes */
    unsigned long clusters;		/* number of clusters allocated for this file */
    unsigned char generation;		/* file's generation number */
    unsigned char type;			/* file type (see above for types) */
    unsigned short flags;		/* spare (to fill out to longword) */
#define FSYS_FH_FLAGS_NEW	(0x0001) /* File is newly created but not closed */
#define FSYS_FH_FLAGS_OPEN	(0x0002) /* File was opened for write but not closed */
#if (FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
    unsigned long extension;		/* index to file header extension */
# define FSYS_FHEADER_EXT	1
#else
# define FSYS_FHEADER_EXT	0
#endif
#if (FSYS_FEATURES&FSYS_FEATURES_CMTIME)
    unsigned long ctime;		/* file creation time */
    unsigned long mtime;		/* file modification time */
# define FSYS_FHEADER_CMTIME	2
#else
# define FSYS_FHEADER_CMTIME	0
#endif
#if (FSYS_FEATURES&FSYS_FEATURES_ABTIME)
    unsigned long atime;		/* file access time */
    unsigned long btime;		/* file backup time */
# define FSYS_FHEADER_ABTIME	2
#else
# define FSYS_FHEADER_ABTIME	0
#endif
#if (FSYS_FEATURES&FSYS_FEATURES_PERMS)
    unsigned long owner;		/* file owner (for future use) */
    unsigned long perms;		/* file permissions (for future use) */
# define FSYS_FHEADER_PERMS	2
#else
# define FSYS_FHEADER_PERMS	0
#endif
#define FSYS_FHEADER_MEMBS (4+FSYS_FHEADER_EXT+FSYS_FHEADER_CMTIME+FSYS_FHEADER_ABTIME+FSYS_FHEADER_PERMS)
/*
 * Set this to the maximum number of retrieval pointers in a file header. It is set
 * such that the sizeof(struct file_header) is <= BYTPSECT. The FSYS_FHEADER_MEMBS in the
 * expression below is the number of long's appearing ahead of this member.
 */
#define FSYS_MAX_FHPTRS	(((BYTES_PER_SECTOR-sizeof(long)*FSYS_FHEADER_MEMBS)/sizeof(FsysRetPtr))/FSYS_MAX_ALTS)
/*
 */
    FsysRetPtr pointers[FSYS_MAX_ALTS][FSYS_MAX_FHPTRS]; /* retrieval pointers */
} FsysHeader;

#if (FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
/* Description of extension file header */

typedef struct efile_header {
    unsigned long id;			/* file header type */
    unsigned long head;			/* pointer to first header in list */
    unsigned long extension;		/* ptr to next file header extension */
/*
 * Set this to the maximum number of retrieval pointers in an extension file header.
 * It is set such that the sizeof(struct efile_header) is <= BYTPSECT. The '3' in the
 * expression below is the number of long's appearing ahead of this member.
 */
#define FSYS_MAX_EFHPTRS	(((BYTES_PER_SECTOR-sizeof(long)*3)/sizeof(FsysRetPtr))/FSYS_MAX_ALTS)
/*
 */
    FsysRetPtr pointers[FSYS_MAX_ALTS][FSYS_MAX_EFHPTRS]; /* retrieval pointers */
} FsysEHeader;

#else
# define FSYS_MAX_EFHPTRS	0
#endif

typedef struct ram_rp {
#if !FSYS_READ_ONLY
    struct ram_rp *next;		/* pointer to next block of these */
#endif
    FsysRetPtr *rptrs;			/* pointer to array of retrieval pointers */
    unsigned int rptrs_size:12;		/* size of array of pointers */
    unsigned int num_rptrs:12;		/* number of active pointers in array */
    unsigned int mallocd:1;		/* rptrs is an individually malloc'd area */
    unsigned int filler:7;		/* pad it to a longword boundary */
} FsysRamRP;

typedef struct ram_fileheader {		/* RAM based file header */
    struct fsys_dirent **directory;	/* pointer to directory hash table if this is a directory */
#if !FSYS_READ_ONLY
    unsigned long clusters;		/* file allocation in sectors */
#endif
    unsigned long size;			/* size of file in bytes */
# if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    unsigned long ctime;		/* creation time of this file */
    unsigned long mtime;		/* modification time on this file */
# endif
# if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    unsigned long atime;
    unsigned long btime;
# endif
# if defined(FSYS_OPTIONS_PERMS) && (FSYS_FEATURES&FSYS_OPTIONS&FSYS_OPTIONS_PERMS)
    unsigned long perms;		/* file access permissions */
    unsigned long owner;		/* who owns this file */
# endif
    FsysRamRP ramrp[FSYS_MAX_ALTS];	/* retrieval pointers */
    unsigned int def_extend:16;		/* amount to extend file by default */
    unsigned int generation:8;		/* pack some bits */
    unsigned int valid:1;		/* signals rfh is valid */
    unsigned int fh_new:1;		/* fileheader is new */
    unsigned int fh_upd:1;		/* add to dirty at close */
    unsigned int active_rp:2;		/* which of the following ramrp's is currently in use */
    unsigned int not_deleteable:1;	/* not a deletable file */
    unsigned int filler:2;		/* fill it out to an int */
} FsysRamFH;

/* Description of filesystem I/O argument block */

typedef struct fsys_qio {
    QioIOQ our_ioq;			/* !!!This must be the first member!!! for fsys I/O we use a different IOQ */
    struct act_q astq;			/* place to use to queue up operation */
    FsysRamRP *ramrp;			/* pointer to list of retrieval pointers */
    int state;				/* read state */
    int flag;				/* I/O control flag */
    unsigned long sector;		/* user's relative sector number */
    unsigned char *buff;		/* pointer to user's buffer */
    unsigned long total;		/* total bytes xferred */
    unsigned long count;		/* number of sectors to read/write */
    unsigned long u_len;		/* user's byte count */
    unsigned char *o_buff;		/* original buffer pointer */
    unsigned long o_len;		/* original length */
    unsigned long o_where;		/* original relative sector number */
    int bws;				/* current bws */
    int o_bws;				/* original byte witin sector */
    int o_which;			/* which of the ALTS we are writing */
    unsigned long o_iostatus;		/* remember the last error, if any */
    void (*complt)(struct qio_ioq *);	/* pointer to completion routine */
    QioFile *fsys_fp;			/* pointer to file sys's QioFile */
    QioIOQ *callers_ioq;		/* remember ptr to caller's IOQ */
    struct fsys_volume *vol;		/* pointer to our volume */
} FsysQio;

typedef struct fsys_dirent {
    struct fsys_dirent *next;		/* ptr to next entry */
    const char *name;			/* ptr to null terminated name of file */
    unsigned long gen_fid;		/* combined generation/fid */
} FsysDirEnt;
#define FSYS_DIR_GENSHF		(24)	/* upper 8 bits is generation number */
#define FSYS_DIR_FIDMASK	((1l<<FSYS_DIR_GENSHF)-1)

#ifndef FSYS_SYNC_ERRLOG
# define FSYS_SYNC_ERRLOG		(20)	/* assume a log of 20 entries */
#endif
#ifndef FSYS_SYNC_TIMER
# define FSYS_SYNC_TIMER		(500000) /* sync automatically runs every 500000 microseconds */
#endif
#ifndef FSYS_SYNC_BUFFSIZE
#define FSYS_SYNC_BUFFSIZE		(4096)
#endif
#define FSYS_SYNC_BUSY_NONTIMER		(1)
#define FSYS_SYNC_BUSY_TIMER		(2)
#define FSYS_SYNC_UPD_INDEX		(1)
#define FSYS_SYNC_UPD_FREE		(2)

typedef struct fsys_sync {
    FsysQio our_ioq;			/* !!!MUST BE FIRST MEMBER!!! an IOQ for sync to use */
    struct fsys_volume *vol;		/* ptr to volume */
# if !defined(FSYS_NO_AUTOSYNC) || !FSYS_NO_AUTOSYNC
    struct tq sync_t;			/* entry for sync timer function */
# endif
    int status;				/* return status from last sync */
    unsigned long errcnt;		/* total number of errors encountered during syncs */
    unsigned long errlog[FSYS_SYNC_ERRLOG]; /* error history log */
    int err_in;				/* index into errlog to place next entry */
    FsysRamRP ramrp;			/* fake ram retrieval pointer */
    FsysRetPtr rptr;			/* fake retrieval pointer used to r/w the file header */
    int buffer_size;			/* size of buffers in bytes */
    unsigned long *buffers;		/* virtual mem ptr to following buffer */
    unsigned long *output;		/* adjusted non-cached pointer to output buffer */
    int buff_p;				/* next byte to write in 'output' */
    int state;				/* current sync state */
    int nxt_state;			/* next sync state */
    int substate;			/* sync state's substate */
    int findx;				/* free file counter */
    int alts;				/* which of the alternate files we are reading/writing */
    int busy;				/* flag indicating sync is currently executing and how is was started */
    int sects;				/* saved sector count */
    int start;				/* saved starting sector */
    FsysRamFH *dirfh;			/* ptr to rfh of directory we're copying at the moment */
    int htbl_indx;			/* which entry in hash table we last used */
    FsysDirEnt *dir;			/* which directory entry we last referenced */
    int dir_size;			/* directory size accumulator */
    int dir_state;			/* which part of the string formation we were in when we ran out of buffer */
} FsysSyncT;

typedef struct fsys_files_link {
    struct fsys_files_link *next;	/* pointer to next chunk */
    int items;				/* total number of items in this list */
} FsysFilesLink;

#if !FSYS_READ_ONLY
typedef struct fsys_index_link {
    struct fsys_index_link *next;	/* pointer to next chunk */
    int items;				/* number of items in this list */
} FsysIndexLink;
#endif

#ifndef FSYS_USE_MALLOC
#if (defined(_LINUX_) && _LINUX_)
#define FSYS_USE_MALLOC (1)
#else
#define FSYS_USE_MALLOC (0)
#endif
#endif

#ifndef FSYS_USE_BUFF_POOLS
#define FSYS_USE_BUFF_POOLS (!FSYS_USE_MALLOC && FSYS_READ_ONLY && !FSYS_UPD_FH)
#endif

#define FSYS_VOL_FLG_RO		(0x001)	/* Filesystem is read only */

/* Description of mounted volume */

typedef struct fsys_volume {
    FsysQio reader;			/* !!!This absolutely must be the first member !!! */
    unsigned long id;			/* distinguishing marker for this struct */
    U32 features;			/* feature list from home block */
    QioMutex mutex;			/* I/O mutex for this volume */
    int iofd;				/* FD to use to do I/O */
    int flags;				/* One or more of FSYS_VOL_FLG_xxx */
    volatile int status;		/* volume mount status */
    volatile int state;			/* volume mount state */
    int substate;			/* mount substate */
    unsigned long hd_offset;		/* amount to add before doing any physical disk I/O */
    unsigned long hd_maxlba;		/* max physical lba allowed to write on this volume */
    unsigned long maxlba;		/* max (relative) lba for this volume (logical limit) */
    unsigned long hb_range;		/* range for home blocks */
    volatile int files_indx;		/* current 'files' index (tmp used by mount) */
    int rw_amt;				/* amt to r/w (tmp used by mount) */
    unsigned long *contents;		/* pointer to where to read/write (tmp used by mount) */
    unsigned long *buff;		/* pointer to non-cached sector buffer (tmp used by mount) */
    int buff_size;			/* size of buffer in bytes (tmp used by mount) */
#if FSYS_USE_BUFF_POOLS
    struct _reent *buff_pool;		/* where we obtained the mount buffer */
    struct _reent *index_pool;		/* where we obtained the index buffer */
#endif
    FsysRamRP tmpramrp;			/* retrieval pointer (tmp used by mount) */
    FsysRetPtr tmprp;			/* retrieval pointer (tmp used by mount) */
    struct act_q tmpq;			/* place to use with prc_q_ast (tmp used by mount) */
    unsigned char file_buff[BYTES_PER_SECTOR+32];
    unsigned char *filep;
    QioMutex filep_mutex;
    unsigned long index_lbas[FSYS_MAX_ALTS]; /* lbas to index.sys file headers */
    FsysFilesLink *filesp;		/* pointer to list of ram fileheaders */
    volatile int files_ffree;		/* first free element in files */
    int files_elems;			/* number of elements in files */
#if FSYS_READ_ONLY
    U32 *index;				/* pointer to index file contents */
#else
    FsysIndexLink *indexp;		/* pointer to index file contents */
#endif
    unsigned long total_free_clusters;	/* number of available clusters */
    unsigned long total_alloc_clusters;	/* number of used clusters */
#if FSYS_TIGHT_MEM
    FsysRetPtr *rp_pool;		/* ptr to preallocated pool of retrieval pointers */
    int rp_pool_size;			/* number of entries in remaining in pool */
#endif
#if !FSYS_READ_ONLY
    FsysRetPtr *free;			/* freemap.sys file contents */
    int free_ffree;			/* first free element in freemap */
    int free_elems;			/* size of freemap in elements */
    unsigned long *index_bits;		/* pointer to array of bits, one bit per sector in index */
    int index_bits_elems;		/* number of elements in index_bits */
    int free_start;			/* first element in freelist that has been updated */
    unsigned long *unused;		/* ptr to list of unused (deleted) fid's */
    int unused_ffree;			/* first free element in unused */
    int unused_elems;			/* number of elements in unused */
#endif
#if FSYS_UPD_FH
    FsysSyncT sync_work;		/* work area for sync code */
    unsigned long *dirty;		/* list of FID's to update */
    volatile int dirty_ffree;		/* first free entry in dirty */
    int dirty_elems;			/* number of entries in dirty */
#endif
#if defined(FSYS_UMOUNT) && FSYS_UMOUNT
    void **freemem;			/* record of malloc's (tmp used by mount) */
    int freem_elems;			/* number of items in freemem (tmp used by mount) */
    int freem_indx;			/* next available item in freemem (tmp used by mount) */
#endif
#if defined(FSYS_INCLUDE_BOOT_MARKERS) && FSYS_INCLUDE_BOOT_MARKERS
    unsigned long boot_lbas[4][FSYS_MAX_ALTS]; /* lbas to index.sys file headers */
#endif
#if (FSYS_OPTIONS&FSYS_FEATURES_JOURNAL)
    unsigned long journ_lbas[FSYS_MAX_ALTS];
    FsysRamFH *journ_rfh;
    FsysFilesLink *limbo;		/* retrieval ptrs that need to be put back on the freelist */
    U32 journ_sect;			/* next sector in journal for r/w */
    int journ_id;
    int journ_hiwater;			/* max LBA written in journal file */
    void *journ_data;			/* used during journal reading during mount */
#if !FSYS_USE_MALLOC
    struct _reent *journ_data_re;	/* where we got the buffer */
#endif
#endif
} FsysVolume;

/* Description of volume init parameters */

typedef struct fsys_initv {
    int cluster;		/* number of sectors per cluster */
    int index_sectors;		/* number of sectors for initial index file allocation */
    int free_sectors;		/* number of sectors for initial freemap file allocation */
    int root_sectors;		/* number of sectors for initial root directory allocation */
    int def_extend;		/* number of sectors to extend files by default */
    unsigned long max_lba;	/* number of LBA's to allocate for this volume */
    unsigned long hb_range;	/* range of sectors for home block */
#if (FSYS_FEATURES&FSYS_FEATURES_JOURNAL)
    int journal_sectors;	/* number of sectors for initial journal file allocation */
#endif
} FsysInitVol;

typedef struct fsys_opent {
    QioOpenSpc spc;		/* This must be first (note: not a pointer) */
    int fid;			/* FID of file to open (set to FID of open'ed file) */
    int parent;			/* FID of parent */
    off_t alloc;		/* file allocation size in bytes */
    off_t eof;			/* position of eof marker in bytes */
    int placement;		/* which area of disk to place file */
    int copies;			/* number of copies of file to make */
    int def_extend;		/* amount to extend file by default */
    int mkdir;			/* .ne. if to create a directory */
    unsigned long ctime;	/* time to assign for creation */
    unsigned long mtime;	/* time to assign for modification */
    unsigned long btime;	/* time to assign for backup */
    unsigned long atime;	/* time to assign for access */
    unsigned long hd_offset;	/* starting sector for filesystem */
    unsigned long hd_maxlba;	/* ending sector +1 for filesystem */
} FsysOpenT;

/* Additional flags that can be set in the mode field and that reside in the */
/* mode field of the QioFile struct */

#if FSYS_MAX_ALTS > 3
# error ** Need to fix definition of FSYS_OPNCPY_x
#endif
#define FSYS_OPNCPY_V	30			/* bit position for number */
#define FSYS_OPNCPY_M	(3l<<FSYS_OPNCPY_V)	/* mask for copy numbers (0=no copy) */
#define O_OPNCPY	(1l<<FSYS_OPNCPY_V)	/* open only the copy spec'd in FsysOpenT.copies */
#define FSYS_OPNNOERR_V	29			/* Don't report I/O errors on this channel */
#define FSYS_OPNNOERR_M	(1l<<FSYS_OPNNOERR_V)
#define O_RPTNOERR	FSYS_OPNNOERR_M

typedef struct lookupfile_t {
    FsysVolume *vol;	/* volume on which to look for file */
    FsysRamFH *top;	/* directory in which to start the search */
    const char *path;	/* pointer to path/filename */
    const char *fname;	/* filename only portion of above */
    FsysDirEnt *dir;	/* pointer to directory entry where file found */
    FsysRamFH *file;	/* pointer to found file */
    FsysRamFH *owner;	/* pointer to directory that owns the file */
    int depth;		/* number of directories traversed to reach this file */
} FsysLookUpFileT;

struct fsys_direct {
    const char *name;
    unsigned long fid;
};

typedef struct fsys_fdirent {
    int filler;
} FsysFDirEnt;

typedef struct fsys_dir {
    QioIOQ ioq;			/* place to allow us to manage these entries */
    FsysVolume *vol;
    const char *dirname;	/* filename of directory */
    FsysDirEnt **hash;		/* pointer to hash table */
    FsysDirEnt *current;	/* pointer to next entry to read */
    int entry;			/* hash table entry currently processing */
    int state;			/* internal state */
} FsysDIR;

#if 0 && (!defined(_LINUX_) && !_LINUX_)
struct direct {
    const char *name;
    unsigned long fid;
};

struct dirent {
    int filler;
};

typedef struct fsys_dir DIR;
#endif

typedef struct fsys_fhioctl {
    int fields;			/* bit mask indicating which of the following are valid */
    U32 ctime;			/* creation time */
    U32 mtime;			/* modified time */
    U32 atime;			/* access time */
    U32 btime;			/* backup time */
    off_t alloc;		/* file allocation in bytes */
    off_t size;			/* file size in bytes */
    U32 sect;			/* current file pointer sector */
    int bws;			/* current file pointer byte within sector */
    off_t def_extend;		/* default file extension in bytes */
    int copies;			/* number of copies maintained */
    int fid;			/* FID of file */
    int dir;			/* .ne. if file is directory */
} FsysFHIoctl;

#define FSYS_FHFIELDS_CTIME	0x01	/* FsysFHIoctl.fields bits */
#define FSYS_FHFIELDS_MTIME	0x02
#define FSYS_FHFIELDS_ATIME	0x04
#define FSYS_FHFIELDS_BTIME	0x08
#define FSYS_FHFIELDS_ALLOC	0x10
#define FSYS_FHFIELDS_SIZE	0x20
#define FSYS_FHFIELDS_DEFEXT	0x40
#define FSYS_FHFIELDS_COPIES	0x80

typedef struct fsys_rpioctl {
    int which;			/* which set of RP's to act on (0: FYS_MAX_ALTS-1, input: user sets this) */
    int inp_rp;			/* number of entries in RP array (input: user sets this) */
    int out_rp;			/* number of entries returned (output: fsys sets this) */
    int tot_rp;			/* total number of retrieval pointers used by file */
    struct file_retptr *rps;	/* pointer to array of RP's (both input and output depending on ioctl) */
} FsysRPIoctl;

#define FSYS_IOC_GETFH		(QIO_IOC_FSYS|1)  /* get file header contents */
#define FSYS_IOC_SETFH		(QIO_IOC_FSYS|2)  /* set (some) file header contents */
#define FSYS_IOC_GETFHLBA	(QIO_IOC_FSYS|3)  /* get the LBA's to the file header */
#define FSYS_IOC_GETRP		(QIO_IOC_FSYS|4)  /* get the retrevial pointer set for file */
#define FSYS_IOC_PURGERP	(QIO_IOC_FSYS|5)  /* purge the specified RP set from file */
#define FSYS_IOC_REALLRP	(QIO_IOC_FSYS|6)  /* alloc a new set of RP's */
#define FSYS_IOC_FREERP		(QIO_IOC_FSYS|7)  /* add the provided RP's to the free list */
#define FSYS_IOC_PLUCKRP	(QIO_IOC_FSYS|8)  /* Pluck the provided RP's from the free list */
#define FSYS_IOC_EXPIRE		(QIO_IOC_FSYS|9)  /* Expire the filesystem */

#ifndef FSYS_USE_PARTITION_TABLE
#define FSYS_USE_PARTITION_TABLE (0)
#endif

typedef struct dos_part {
    U8 status;
    U8 st_head;
    U16 st_sectcyl;
    U8 type;
    U8 en_head;
    U16 en_sectcyl;
    U32 abs_sect;
    U32 num_sects;
} DOSPartition;
            
struct dos_bootsect {
    U8 jmp[3];                  /* 0x000 x86 jump */
    U8 oem_name[8];             /* 0x003 OEM name */
    U8 bps[2];                  /* 0x00B bytes per sector */
    U8 sects_clust;             /* 0x00D sectors per cluster */
    U16 num_resrv;              /* 0x00E number of reserved sectors */
    U8 num_fats;                /* 0x010 number of FATs */
    U8 num_roots[2];            /* 0x011 number of root directory entries */
    U8 total_sects[2];          /* 0x013 total sectors in volume */
    U8 media_desc;              /* 0x015 media descriptor */  
    U16 sects_fat;              /* 0x016 sectors per FAT */
    U16 sects_trk;              /* 0x018 sectors per track */
    U16 num_heads;              /* 0x01A number of heads */
    U32 num_hidden;             /* 0x01C number of hidden sectors */
    U32 total_sects_vol;        /* 0x020 total sectors in volume */
    U8 drive_num;               /* 0x024 drive number */
    U8 reserved0;               /* 0x025 unused */
    U8 boot_sig;                /* 0x026 extended boot signature */
    U8 vol_id[4];               /* 0x027 volume ID */
    U8 vol_label[11];           /* 0x02B volume label */
    U8 reserved1[8];            /* 0x036 unused */
    U8 bootstrap[384];          /* 0x03E boot code */ 
    DOSPartition parts[4];         /* 0x1BE partition table */
    U16 end_sig;                /* 0x1FE end signature */
} __attribute__ ((packed));
  
typedef struct dos_bootsect DOSBootSect;

#ifndef FSYS_INCLUDE_HB_READER
#define FSYS_INCLUDE_HB_READER 0
#endif

#if FSYS_INCLUDE_HB_READER
typedef struct fsys_gsethb {
    DOSPartition parts[4];	/* clone of the 4 DOS partitions */
    union {
	U8 filler[512];		/* has to be one sector in length */
	FsysHomeBlock homeblk;	/* contents of home block */
    } hbu;
    int copy;			/* which copy of home block to find (or found) */
    U32 abs_lba[FSYS_MAX_ALTS];	/* absolute lbas of home blocks */
    U32 rel_lba[FSYS_MAX_ALTS];	/* relative lbas of home blocks */
    U32 hd_offset;		/* sector offset for filesystem */
    U32 hd_maxlba;		/* highest sector number available */
} FsysGSetHB;

/************************************************************
 * fsys_gethomeblk - Get home block. Finds and reads the first
 *	home block from disk.
 * 
 * At entry:
 *	ioq - pointer to QioIOQ. Must be opened to physical device
 *	virt - null terminated string with name of virtual device
 *	home - pointer to FsysGSetHB. The member 'copy' must be set
 *		to a number from 0 to FSYS_MAX_ALTS-1 to indicate
 *		which home block to begin the search.
 *	hb->parts can be set ahead of time to setup the partition
 *		table information (i.e. this function does a
 *		fsys_partition() call with the contents of hb->parts
 *		if it hasn't already been done). NOTE: The contents
 *		of this member are only significant if partitions are
 *		being used.
 *
 * At exit:
 *	ioq->iostatus will be set to success or failure code.
 *	hb->xxx
 *		copy - set to a number 0 to FSYS_MAX_ALTS-1 indicating
 *			which copy of home block was found.
 *		abs_lba - absolute sector on hard disk where hb found.
 *		rel_lba - relative sector in partition where hb found.
 *		parts - filled in with partition information if partitions
 *			are in use.
 *		homeblk - contents of home block.
 */
extern int fsys_gethomeblk(QioIOQ *ioq, const char *virt, FsysGSetHB *home);
#endif				/* FSYS_INCLUDE_HB_READER */

/************************************************************
 * fsys_mount - volume mount. This function queue's the volume
 * mount primitive.
 * 
 * At entry:
 *	ioq - pointer to QioIOQ
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *	tmp - pointer to scratch memory required for mount task.
 *	tmpsize - size of tmp area in chars. Must be at least as large
 *		as sizeof(struct stat)
 *
 * At exit:
 *	ioq->iostatus will be set to success or failure code when mount
 *		completes. Remains 0 until then.
 *	ioq->iocount is updated during the mount procedure every 5milliseconds
 *		with the current percent completion (values range from 0-99).
 *	returns 0 if successfully queued or one of the errors defined in qio.h
 *		if not queued. Completion routine, if one specified, will be
 *		when mount completes.
 */
extern int fsys_mount(QioIOQ *ioq, const char *where, const char *what, U32 *tmp, int tmpsize);

/************************************************************
 * fsys_mountw - volume mount. This function queue's the volume
 * mount primitive and waits for its completion.
 * 
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *
 * At exit:
 *	returns 0 if success or one of FSYS_MOUNT_xxx if error.
 */
extern int fsys_mountw(const char *where, const char *what);

/************************************************************
 * fsys_mountw - volume mount. This function queue's the volume
 * mount primitive and waits for its completion.
 * 
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *
 * At exit:
 *	returns 0 if success or one of FSYS_MOUNT_xxx if error.
 */
extern int fsys_mountw(const char *where, const char *what);

/************************************************************
 * fsys_mountwcb - volume mount. This function queue's the volume
 * mount primitive and waits for its completion.
 * 
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *	wait - pointer to function to be called while waiting. This
 *		function is passed a single parameter which is a number
 *		from 0 to 99 representing the percentage of the mount
 *		that is complete.
 *
 * At exit:
 *	returns 0 if success or one of FSYS_MOUNT_xxx if error.
 */
extern int fsys_mountwcb(const char *where, const char *what,
    			 void (*wait)(int));

/************************************************************
 * fsys_initfs - initialize volume. This function writes all the
 * necessary sectors to disk to initialize a volume for use by the
 * filesystem.
 * 
 * At entry:
 *	what - null terminated string naming physcal device to write
 *	v - pointer to struct with init params:
 *		->cluster - must be 1 at this time
 *		->index_sectors - # of sectors to allocate to index file
 *		->free_sectors - # of sectors to allocate to freelist file
 *		->root_sectors - # of sectors to allocate to root directory
 *
 * At exit:
 *	returns 0 if success or one of FSYS_INIT_xxx if error.
 */
extern int fsys_initfs(const char *what, FsysInitVol *v);

/************************************************************
 * fsys_qinitfs - initialize volume. This function writes all the
 * necessary sectors to disk to initialize a volume for use by the
 * filesystem. Operates asych as other qio_xxx functions do.
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct. The only relavant field is:
 *		complete - pointer to caller's completion routine if any.
 *		all other fields are don't cares.
 *	what - null terminated string naming physcal device to write
 *	v - pointer to struct with init params:
 *		->cluster - must be 1 at this time
 *		->index_sectors - # of sectors to allocate to index file
 *		->free_sectors - # of sectors to allocate to freelist file
 *		->root_sectors - # of sectors to allocate to root directory
 *
 * At exit:
 *	returns 0 if success or one of FSYS_INIT_xxx if error.
 */
extern int fsys_qinitfs(QioIOQ *ioq, const char *what, FsysInitVol *v);

/************************************************************
 * fsys_init - initialize filesystem internals. This function must
 * be called once at boot.
 * 
 * At entry:
 *	no requirements.
 *
 * At exit:
 *	returns nothing.
 */
extern void fsys_init(void);

/*********************************************************************
 * fsys_sync_delay - set period at which the sync task runs.
 *
 * At entry:
 *	time - number of microseconds between runs of Sync task
 *
 * At exit:
 *	returns previous value of sync delay. Requeues the sync
 *	task at its new rate. Also forces a sync on each volume
 *	in the system. If the parameter 'time' is 0, the delay 
 *	is set to the value assigned to the cpp variable
 *	FSYS_SYNC_TIMER.
 */

extern int fsys_sync_delay(int time);

/*********************************************************************
 * fsys_validate_freespace - do a read/write test on all the sectors
 *	listed in the freelist.
 *
 * At entry:
 *	rawfn - pointer to ASCII string with disk device name (i.e. /rd0)
 *	volfn - pointer to ASCII string with volume device name (i.e. /d0)
 *	sts_row - row to use to put up status messages
 *	upd_row - row to use to put up "update warning"
 *	sectors - max number of sectors to verify in each region
 *	dosync - if true, will perform a sync on the volume after
 *		the validation is complete.
 *
 * At exit:
 *	Always returns 0. The freelist may have been updated in memory
 *	if any bad sectors were detected. The updated freelist will be
 *	written back to disk if the dosync paramter is true.
 */

extern int fsys_validate_freespace(const char *rawfn, const char *volfn,
    				   int sts_row, int upd_row, int maxsect, int dosync);

#if FSYS_READ_ONLY
#define ENTRY_IN_INDEX(vol, fid) (vol->index + (fid)*FSYS_MAX_ALTS)
#else
#define ENTRY_IN_INDEX(vol, fid) fsys_entry_in_index(vol, fid)
extern unsigned long *fsys_entry_in_index(FsysVolume *vol, int fid);
#endif

/*********************************************************************
 * fsys_find_ramfh - find a pointer to FsysRamFH struct in list of files.
 *
 * At entry:
 *	vol - pointer to volume struct
 *	id - index of file to get
 *
 * At exit:
 *	returns pointer to FsysRamFH if id in range, else returns 0.
 */
extern FsysRamFH *fsys_find_ramfh(FsysVolume *_vol, int _id);

/*********************************************************************
 * fsys_find_id - find index of file given the FsysRamFH pointer
 *
 * At entry:
 *	vol - pointer to volume struct
 *	rfh - pointer to FsysRamFH struct
 *
 * At exit:
 *	returns id of file or -1 if not in range.
 */
extern int fsys_find_id(FsysVolume *_vol, FsysRamFH *_rfh);

#define FSYS_PARTITION_NEVER  (0)
#define FSYS_PARTITION_MAYBE  (1)
#define FSYS_PARTITION_ALWAYS (2)

/******************************************************************* 
  * fsys_set_hd_names - set the string for the filenames
  *  
  * At entry: 
  *  num - number of entries
  *  names - pointer to array of names
  *  partition_option - One of FSYS_PARTITION_xxx above
  * 
  *  At exit:
  *  pinfo table updated.
  *  returns number of entries updated.
  */
extern int fsys_set_hd_names(int number, char **names, int partition_option );

/* Mop up any undefineds to keep Watcom C happy */

#ifndef FSYS_REPEAT_SKIP
#define FSYS_REPEAT_SKIP	0
#endif
#ifndef FSYS_OPTIONS_PERMS
#define FSYS_OPTIONS_PERMS	0
#endif
#ifndef FSYS_UMOUNT
#define FSYS_UMOUNT		0
#endif
#ifndef FSYS_SQUAWKING
#define FSYS_SQUAWKING		0
#endif
#ifndef FSYS_SYNC_SQUAWKING
#define FSYS_SYNC_SQUAWKING	0
#endif
#ifndef FSYS_FREE_SQUAWKING
#define FSYS_FREE_SQUAWKING	0
#endif
#ifndef FSYS_DEFAULT_TIMEOUT
#define FSYS_DEFAULT_TIMEOUT	0
#endif

#ifndef FSYS_CHECKSUM_FILENAME
# define FSYS_CHECKSUM_FILENAME QIO_FNAME_SEPARATOR_STR "diags" QIO_FNAME_SEPARATOR_STR "checksums"
#endif
#ifdef __cplusplus
}
#endif 
#endif		/* _FSYS_H_ */
