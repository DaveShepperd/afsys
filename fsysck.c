/* See LICENSE.txt for license details */

#include <config.h>
#include <os_proto.h>
#include <qio.h>
#include <fsys.h>
#include <nsprintf.h>

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>

char *image_name;
int default_copies = 1;
int debug_level;
int debug_mode;		/* Keep from sucking in all of libos */

void exit(int why) {
    _exit(why);
}

const struct st_envar *st_getenv (const char *__name, const struct st_envar *__hook) {
    return 0;
}

int real_errno;
U32 __real_eer_rtc;

int *__errno_location(void) {
    return &real_errno;
}

int eer_gets(unsigned int what) {
    return 0;
}

int eer_puts(unsigned int where, unsigned int what) {
    return 0;
}

int prc_get_astlvl(void) {
    return -1;
}

static struct act_q ast_q;
static int running_ast;

int prc_q_ast(int level, struct act_q *new) {
    struct act_q **prev = &ast_q.next, *nxt;

    if (new && !new->que) {
	while ((nxt = *prev)) {
	    prev = &nxt->next;
	}
	*prev = new;
	new->next = 0;
	new->que = &ast_q;
	if (!running_ast) {
	    running_ast = 1;
	    while ((nxt = ast_q.next)) {
		ast_q.next = nxt->next;
		nxt->next = 0;
		nxt->que = 0;
		if (nxt->action) nxt->action(nxt->param);
	    }
	    running_ast = 0;
	}
    } else {
	return 1;
    }
    return 0;
}

void prc_panic(const char *msg) {
    fprintf(stderr, "prc_panic: %s\n", msg);
    exit (1);
}

void prc_wait_n_usecs(int amt) {
    int loop = amt*166/4;
    while (--loop > 0) { ; }
}

static FILE *err_status;

static int sho_qio_err(int sts, const char *str) {
    char qio_err[132];

    qio_errmsg(sts, qio_err, sizeof(qio_err));
    if (!err_status) {
	err_status = stdout;
    }
    fputs(str, err_status);
    fprintf(err_status, ": %s\n", qio_err);
    return sts;
}

#if 0
static int get_lbas(const char *fname, U32 *lbas) {
    QioIOQ *ioq;
    int sts, retv = -1;
 
    ioq = qio_getioq();
    sts = qiow_open(ioq, fname, O_RDONLY);
    if (!QIO_ERR_CODE(sts)) {
	sts = qiow_ioctl(ioq, FSYS_IOC_GETFHLBA, lbas);	/* get lbas */
	if (!QIO_ERR_CODE(sts)) {
#if 0
	    printf("LBAs for %s: %08lX, %08lX, %08lX\n", fname, lbas[0], lbas[1], lbas[2]);
#endif
	    retv = ioq->iocount;
	}
	qiow_close(ioq);		/* done with file */
    }
    qio_freeioq(ioq);
    return retv;
}
#endif

FsysGSetHB home_block;

int get_home_block(QioIOQ *ioq, const char *virt) {
    int sts;
    int gotioq=0;

    if (!ioq) {
	ioq = qio_getioq();
	if (!ioq) {
	    printf("No more IOQ's\n");
	    return -1;
	}
	sts = qiow_open(ioq, "/rd0", O_RDWR);
	if (QIO_ERR_CODE(sts)) {
	    sho_qio_err(sts, "Error opening /rd0:");
	    qio_freeioq(ioq);
	    return sts;
	}
	gotioq = 1;
    }
    sts = fsys_gethomeblk(ioq, "/d0", &home_block);
    while (!sts) sts = ioq->iostatus;
    if (QIO_ERR_CODE(sts)) {
	sho_qio_err(sts, "Found no valid home blocks:");
    }
    if (gotioq) {
	qiow_close(ioq);
	qio_freeioq(ioq);
    }
    return sts;
}

#if 0
static int mark_home_block( unsigned long time, U32 *lbas ) {
    FsysHomeBlock *hb;
    unsigned long *lp, cksum;
    int ii, jj, sts;
    QioIOQ *ioq;

    ioq = qio_getioq();
    if (!ioq) {
	printf("No more IOQ's\n");
	return -1;
    }
    ii = qiow_open(ioq, "/rd0", O_RDWR);
    if (QIO_ERR_CODE(ii)) {
	sho_qio_err(ii, "Error opening /rd0:");
	return ii;
    }
    ioq->timeout = 1000000;                /* apply a 1 second timeout */
    ii = get_home_block(ioq, "/d0");
    if (QIO_ERR_CODE(ii)) {
	sho_qio_err(ii, "Found no valid home blocks:");
	return ii;
    }
    hb = &home_block.hbu.homeblk;
    if ( lbas ) {
	memcpy(hb->journal, lbas, sizeof(U32)*FSYS_MAX_ALTS);
	hb->hb_size = sizeof(FsysHomeBlock);	/* update this */
	hb->hb_minor = FSYS_VERSION_HB_MINOR;	/* update this */
    }
    hb->chksum = 0;
    hb->upd_flag = 0;
    for (jj=0,cksum=0,lp=(U32*)hb; jj < 512/4; ++jj) {
	cksum += *lp++; 		/* checksum the homeblock */
    }
    hb->chksum = 0-cksum;
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {   
	int retry; 
	for (retry=0; retry < 8; ++retry) {
	    sts = qiow_writewpos(ioq, home_block.abs_lba[ii], hb, 512);   /* write HB */
	    if (!QIO_ERR_CODE(sts)) break;
	}
    }
    if (!ii && QIO_ERR_CODE(sts)) {
	sho_qio_err( sts, "Home block write failed" );
    	return sts;
    }
    return 0;
}
#endif

extern int ide_init(void);
extern int validate_freelist(int checkall);

int main(int argc, char *argv[]) {
    int cp0, sts=0, checkall=0;
    char emsg[132];

    if (argc > 1) {
	char *s = argv[1];
	if (s[0] == '-' && s[1] == 'a') {
	    checkall = 1;
	} else {
	    printf("Usage: mkjourn [-a]\n");
	    return 1;
	}
    }
    cp0 = open("/dev/atl_cp0", O_RDONLY);
    if (cp0 < 0 && errno != EBUSY) {
	perror("Error opening /dev/atl_cp0");
	return 1;
    }
    qio_init();
    ide_init();
    fsys_init();
    sts = get_home_block(0, "/d0");
    if (QIO_ERR_CODE(sts)) {
	sho_qio_err(sts, "Unable to get home block");
	return 2;
    }	
    sts = fsys_mountw("/rd0", "/d0");
    if (QIO_ERR_CODE(sts) && sts != FSYS_MOUNT_MOUNTED) {
    	snprintf(emsg, sizeof(emsg), "Unable to mount /d0 on /rd0");
	sho_qio_err(sts, emsg);
    } else {
	QioIOQ *ioq = 0;
	sts = validate_freelist(checkall);
#if 0
	if (!sts) do {					/* if the freelist is ok */
	    FsysOpenT ot;
	    int sts;
	    U32 lbas[FSYS_MAX_ALTS];
	    U8 blanks[512];

	    ioq = qio_getioq();
	    if (home_block.hbu.homeblk.journal[0]) {	/* if we've already got a journal */
		printf("Already have a journal\n");
		break;					/* nothing left to do */
	    }
	    memset(&ot, 0, sizeof(ot));
	    ot.spc.path = "/d0/diags/journal_file";
	    ot.spc.mode = O_WRONLY|O_CREAT|O_TRUNC;
	    ot.alloc = 2*1024*1024;
	    ot.copies = 1;
	    ot.def_extend = 1*1024*1024;
	    sts = qiow_openspc(ioq, &ot.spc);
	    if (QIO_ERR_CODE(ioq->iostatus)) {
		sho_qio_err(sts, "Failed to create /d0/diags/journal_file");
		break;
	    } 
	    memset(blanks, 0, sizeof(blanks));
	    sts = qiow_write(ioq, blanks, sizeof(blanks));
	    qiow_close(ioq);
	    if (QIO_ERR_CODE(sts)) {
		sho_qio_err(sts, "Failed to write a blank sector to journal");
		break;
	    }
	    sts = get_lbas("/d0/diags/journal_file", lbas);
	    if (sts != sizeof(lbas)) {
		break;
	    }
	    sts = mark_home_block( 0, lbas );
	    if (QIO_ERR_CODE(sts)) {
		break;
	    }
	} while (0);
#endif
	if (ioq) {
#if 0
	    if (!QIO_ERR_CODE(sts)) {
		printf("Would have done a SYNC\n");
	    }
#endif
	    qio_freeioq(ioq);
	}
    }
    close(cp0);
    return QIO_ERR_CODE(sts) ? 3 : 0;
}
