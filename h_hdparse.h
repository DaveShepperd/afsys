/* See LICENSE.txt for license details */

#ifndef _H_HDPARSE_H_
#define _H_HDPARSE_H_

#if 0 && defined(LINUX)
typedef unsigned long off_t;
#endif

typedef struct parse_vol {
    int type;			/* struct type */
    struct parse_vol *next;	/* allow linked lists of these */
    char *phys;			/* physical device name on which to create volume */
    char *virt;			/* virtual device name on which to create volume */
    int cluster;                /* number of sectors per cluster */
    int index_sectors;          /* number of clusters for initial index file allocation */
    int free_sectors;           /* number of clusters for initial freemap file allocation */
    int root_sectors;           /* number of clusters for initial root directory allocation */
    int def_extend;             /* default file extension amount (in sectors) */
    unsigned long max_lba;	/* number of LBA's on the volume */
    unsigned long hb_range;	/* range for home blocks */
    int jou_sectors;		/* size of journal */
} ParseVol;

typedef struct parse_file {
    int type;			/* struct type */
    struct parse_file *next;	/* allow linked lists of these */
    char *unix_name;		/* name of file on local host */
    char *game_name;		/* name of file on game system */
    char *fid_name;		/* name to #define to FID */
    off_t alloc;		/* file allocation size in bytes */
    off_t eof;			/* position of eof marker in bytes */
    int def_extend;		/* amount to extend file by default (in bytes) */
    int placement;              /* which area of disk to place file */
    int copies;                 /* number of copies of file to make */
    int mkdir;                  /* .ne. if to create a directory */
    unsigned long ctime;        /* time to assign for creation (set to value on opened file) */
    unsigned long fid;		/* FID of file to open (set to FID of open'ed file) */
    unsigned long parent;	/* FID of parent (set to FID of open'ed file) */
    unsigned long cksum;	/* checksum of file */
    int nocksum;		/* .ne. if not to checksum the file */
} ParseFile;

typedef struct parse_part {
    int type;			/* struct type */
    struct parse_part *next;	/* allow linked lists of these */
    int index;			/* partition index */
    int ptype;			/* partition type */
    int status;			/* partition status */
    off_t start_lba;		/* starting sector */
    off_t num_lbas;		/* number of sectors */
    unsigned long size;		/* size in bytes */
    off_t end_lba;		/* allow him to specify an end lba */
} ParsePartition;

typedef struct parse_pipe {
    int type;			/* struct type */
    struct parse_list *next;	/* allow linked lists of these */
    char *command;		/* command string */
    char *output;		/* output filename */
} ParsePipe;

typedef union parse_union {
    int type;
    ParseVol v;
    ParseFile f;
    ParsePartition p;
    ParsePipe pipe;
} ParseUnion;

enum parse_type {
    PARSE_TYPE_UNK,		/* not defined */
    PARSE_TYPE_VOL,
/* KEEP FILE and all the BOOTs together with FILE first and BOOT3 last */
    PARSE_TYPE_FILE,
    PARSE_TYPE_BOOT0,
    PARSE_TYPE_BOOT1,
    PARSE_TYPE_BOOT2,
    PARSE_TYPE_BOOT3,
/* End of togetherness */
    PARSE_TYPE_DEFAULT,
    PARSE_TYPE_CS,
    PARSE_TYPE_PART,
    PARSE_TYPE_DEL,
    PARSE_TYPE_SYNC,
    PARSE_TYPE_PIPE,
    PARSE_TYPE_EOF
};

#define PARSE_ERR_EOF	 -1	/* End of file */
#define PARSE_ERR_INVARG -2	/* invalid argument */
#define PARSE_ERR_NOMEM	 -3	/* ran out of memory */    
#define PARSE_ERR_IOERR  -4	/* File input error */
#define PARSE_RESCAN	 -5	/* Not really an error, but a signal to the parser to fetch and rescan input */

extern char *expandPathname(const char *path);
extern int parse_it(FILE *ifp, ParseUnion **ans);
extern int parse_command(const char *input, ParseUnion **ans);
extern int input_line_no;
extern int debug_level;
extern int parse_errors;
#endif		/* _H_HDPARSE_H_ */
