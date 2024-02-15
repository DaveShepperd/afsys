/* See LICENSE.txt for license details */

#include <config.h>
#include <os_proto.h>
#include <qio.h>
#include <fsys.h>

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

extern FsysVolume volumes[];

#define DUMP_IDX (0)
#define DUMP_RPS (0)

#define USE_REM_RPS	(1)
#define REM_RPS_SQUAWK	(0)
#define REM_RPS_DMP	(1)

#if REM_RPS_SQUAWK
#define RPSSQK(x) printf x
/* #define RPSDMP(x) dump_alloc_rps(x) */
#define RPSDMP(x) do { ; } while (0);
#else
#define RPSSQK(x) do { ; } while (0)
#define RPSDMP(x) do { ; } while (0)
#endif

#if USE_REM_RPS 
static FsysRetPtr *alloc_rps, *alloc_rps_lim;
static int alloc_rps_ffree, alloc_rps_elems;

#if REM_RPS_DMP
static void dump_alloc_rps(const char *msg) {
# if 0
    int ii, tot=0;
    FsysRetPtr *rp;

    if (msg) printf("rem_rp: %s\n", msg);
    printf("rem_rp: ffree=%4d, elems=%4d\n", alloc_rps_ffree, alloc_rps_elems);
    rp = alloc_rps;
    for (ii=0; ii < alloc_rps_ffree; ++ii, ++rp) {
	printf("rem_rp:\tEntry %4d: s=%08lX-%08lX (%6ld)\n", ii, rp->start, rp->start+rp->nblocks-1, rp->nblocks);
	tot += rp->nblocks;
    }
    printf("rem_rp: Total space allocated: %d sectors\n", tot);    
# endif
}
#endif

#if DUMP_RPS
static void dump_rp(FsysRamFH *rfh) {
    int alt;
    FsysRetPtr *retp;
    FsysRamRP *rp;

    for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
	rp = rfh->ramrp + alt;
	while (rp) {
	    int ii;
	    retp = rp->rptrs;
	    for (ii=0; ii < rp->num_rptrs; ++ii, ++retp) {
		printf("[%d][%02d]: sect=%08lX, nsec=%ld\n", alt, ii, retp->start, retp->nblocks);
	    }
	    rp = rp->next;
	}		
    }
}
#endif

extern int back_to_free(FsysVolume *vol, FsysRetPtr *rp);
extern int add_to_dirty(FsysVolume *vol, int indx, int end);

static void upd_freelist(void) {
    int ii, tot=0;
    FsysRetPtr *rp;

    rp = alloc_rps;
    for (ii=0; ii < alloc_rps_ffree; ++ii, ++rp) {
	back_to_free(volumes, rp);
	tot += rp->nblocks;
    }
    if (tot) add_to_dirty(volumes, FSYS_INDEX_FREE, 1);
}

static int rem_rp(FsysVolume *vol, FsysRetPtr *rp, FsysRetPtr *ovr) {
    FsysRetPtr *frp;
    int ii, sects=0;
    U32 our_start, our_end;
    U32 his_start, his_end;

    if (alloc_rps_elems-alloc_rps_ffree < 2) {
	alloc_rps_elems += 4096;
	RPSSQK(("rem_rp: reallocing %d elems. ffree=%d\n", alloc_rps_elems, alloc_rps_ffree));
	alloc_rps = (FsysRetPtr *)realloc(alloc_rps, alloc_rps_elems*sizeof(FsysRetPtr));
	if (!alloc_rps) {
	    printf("Ran out of memory\n");
	    exit (2);
	}
	memset(alloc_rps + alloc_rps_ffree, 0, (alloc_rps_elems-alloc_rps_ffree)*sizeof(FsysRetPtr));
	if (!alloc_rps_ffree) {
	    frp = alloc_rps;
	    frp->start = 1;
	    frp->nblocks = vol->maxlba-1;
	    alloc_rps_ffree = 1;
	}
	alloc_rps_lim = alloc_rps + alloc_rps_elems;
    }
    frp = alloc_rps;
    his_start = rp->start;
    his_end = rp->start+rp->nblocks;
    for (ii=0; ii < alloc_rps_ffree; ++ii, ++frp) {
	our_start = frp->start;
	our_end =   frp->start+frp->nblocks;
	if (his_end <= our_start) continue;
	if (his_start >= our_end) continue;
	if (his_start < our_start && his_end > our_start) {
	    if (ovr) *ovr = *frp;
	    return -1;			/* error */
	}
	if (his_start < our_end && his_end > our_end) {
	    if (ovr) *ovr = *frp;
	    return -2;			/* error */
	}
	if (his_start == our_start) {
	    RPSSQK(("rem_rp: Plucking %08lX-%08lX from %08lX-%08lX. entry=%4d (starts==)\n",
		    his_start, his_end-1, our_start, our_end-1, ii));
	    RPSDMP(("Before"));
	    RPSSQK(("rem_rp: Before: window %08lX-%08lX (%6ld) ffree=%4d\n",
	    		frp->start, frp->start+frp->nblocks-1, frp->nblocks, alloc_rps_ffree));
	    frp->start += rp->nblocks;
	    frp->nblocks -= rp->nblocks;
	    RPSSQK(("rem_rp: After:  window %08lX-%08lX (%6ld) ffree=%4d\n",
	    		frp->start, frp->start+frp->nblocks-1, frp->nblocks, alloc_rps_ffree));
	    if (!frp->nblocks) {
		RPSSQK(("rem_rp: zapped the window\n"));
		memmove(frp, frp+1, (alloc_rps_elems-ii-1)*sizeof(FsysRetPtr));
		if (frp+alloc_rps_elems-ii-1 >= alloc_rps_lim) {
		    printf("Warning: (1) Ran off end of buffer. Touched 0x%08lX, lim 0x%08lX\n",
			(U32)(frp+alloc_rps_elems-ii-1), (U32)alloc_rps_lim);
		}
		--alloc_rps_ffree;
	    }
	    RPSDMP(("After"));
	    sects = rp->nblocks;
	    break;
	}
	if (his_end == our_end) {
	    RPSSQK(("rem_rp: Plucking %08lX-%08lX from %08lX-%08lX. entry=%4d (ends==)\n",
		    his_start, his_end-1, our_start, our_end-1, ii));
	    RPSDMP(("Before"));
	    RPSSQK(("rem_rp: Before: window %08lX-%08lX (%6ld) ffree=%4d\n",
	    		frp->start, frp->start+frp->nblocks-1, frp->nblocks, alloc_rps_ffree));
	    frp->nblocks -= rp->nblocks;
	    RPSSQK(("rem_rp: After:  window %08lX-%08lX (%6ld) ffree=%4d\n",
	    		frp->start, frp->start+frp->nblocks-1, frp->nblocks, alloc_rps_ffree));
	    RPSDMP(("After"));
	    sects = rp->nblocks;
	    break;
	}
	RPSSQK(("rem_rp: Plucking %08lX-%08lX from %08lX-%08lX. entry=%4d\n",
    		his_start, his_end-1, our_start, our_end-1, ii));
	RPSDMP(("Before"));
	RPSSQK(("rem_rp: Before: window %08lX-%08lX (%6ld), %08lX-%08lX (%6ld), ffree=%4d\n",
		    frp->start, frp->start+frp->nblocks-1, frp->nblocks, 
		    (frp+1)->start, (frp+1)->start+(frp+1)->nblocks-1, (frp+1)->nblocks, alloc_rps_ffree));
	memmove(frp+1,frp, (alloc_rps_elems-ii-1)*sizeof(FsysRetPtr));
	if (frp+1+alloc_rps_elems-ii-1 > alloc_rps_lim) {
	    printf("Warning: (2) Ran off end of buffer. Touched 0x%08lX, lim 0x%08lX\n",
		(U32)(frp+1+alloc_rps_elems-ii-1), (U32)alloc_rps_lim);
	}
	++alloc_rps_ffree;
	frp->nblocks = his_start-our_start;
	++frp;
	frp->start = his_end;
	frp->nblocks = our_end - his_end;
	--frp;
	RPSSQK(("rem_rp: After:  window %08lX-%08lX (%6ld), %08lX-%08lX (%6ld), ffree=%4d\n",
		    frp->start, frp->start+frp->nblocks-1, frp->nblocks, 
		    (frp+1)->start, (frp+1)->start+(frp+1)->nblocks-1, (frp+1)->nblocks, alloc_rps_ffree));
	RPSDMP(("After"));
	sects = rp->nblocks;
	break;
    }
    if (!sects) {
	if (ovr) {
	    ovr->start = 0;
	    ovr->nblocks = 0;
	}
	return -3;
    }
    return sects;
}
#endif

static int in_free(FsysVolume *vol, int fid, FsysRetPtr *retp, int set, int item, int ok) {
    FsysRetPtr *frp;
    int ii;

    frp = vol->free;
    for (ii=0; ii < vol->free_ffree; ++ii, ++frp) {
	if (retp->start+retp->nblocks <= frp->start) continue;
	if (retp->start >= frp->start+frp->nblocks) continue;
	if (ii == ok) continue;
	printf("ERROR!! fid %08X, rp set %d, item %2d overlap freelist ent %2d. Star=%08lX, nblo=%08lX\n",
	    fid, set, item, ii, retp->start, retp->nblocks);
	return 1;
    }
    return 0;
}

static int chk_free(FsysVolume *vol, FsysRamFH *rfh, int fid, int *err) {
    int alt, item, allocs=0;
    FsysRetPtr *retp;
    FsysRamRP *rp;

    for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
	rp = rfh->ramrp + alt;
	item = 0;
	while (rp) {
	    int ii;
	    retp = rp->rptrs;
	    for (ii=0; ii < rp->num_rptrs; ++ii, ++retp) {
		in_free(vol, fid, retp, alt, item, -1);
#if USE_REM_RPS
		{
		    int sts;
		    FsysRetPtr ovr;
		    sts = rem_rp(vol, retp, &ovr);
		    if (sts < 0) {
			printf("rem_rp: Sectors %08lX-%08lX (%6ld) doubly allocated. FID %08X, set %d, item %d\n",
			    retp->start, retp->start+retp->nblocks-1, retp->nblocks, fid, alt, ii);
			printf("\tsts=%d, window %08lX-%08lX\n", sts, ovr.start, ovr.start+ovr.nblocks-1);
		    }
		}
#endif
		allocs += retp->nblocks;
		++item;
	    }
	    rp = rp->next;
	}		
    }
    return allocs;
}

static int rp_overlap(FsysRamRP *src, FsysRetPtr *rp) {
    int alt;
    FsysRetPtr *retp;
    FsysRamRP *ramrp;
    U32 his_start;
    U32 his_end;

    if (src && rp) {
	his_start = rp->start;
	his_end = rp->start+rp->nblocks;
	for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
	    ramrp = src + alt;
	    while (ramrp) {
		int ii;
		retp = ramrp->rptrs;
		for (ii=0; ii < ramrp->num_rptrs; ++ii, ++retp) {
		    if (his_end <= retp->start) continue;
		    if (his_start >= retp->start+retp->nblocks) continue;
		    return -(alt+1);
		}
		ramrp = ramrp->next;
	    }		
	}
    }
    return 0;
}
	
static int chk_ovr(FsysVolume *vol, FsysRamFH *rfh, int fid) {
    FsysRamFH *lrfh;
    int lfid;

    for (lfid=0; lfid < vol->files_ffree; ++lfid) {
	lrfh = fsys_find_ramfh(vol, lfid);
	if (lrfh) {
	    int alt, sts;
	    FsysRamRP *ramrp;

	    if (lrfh == rfh) continue;
	    for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
		ramrp = lrfh->ramrp + alt;
		while (ramrp) {
		    int rpc;
		    FsysRetPtr *retp;

		    rpc = ramrp->num_rptrs;
		    retp = ramrp->rptrs;
		    for (rpc=0; rpc < ramrp->num_rptrs; ++rpc, ++retp) {
			sts = rp_overlap(rfh->ramrp, retp);
			if (sts < 0) {
			    printf("File %08X set %d overlaps file %08X set %d\n",
				lfid, alt, fid, -sts-1);
			}
		    }
		    ramrp = ramrp->next;
		}		
	    }
	}
    }
    return 0;
}

extern FsysGSetHB home_block;
extern int get_home_block(QioIOQ *ioq, const char *virt);

int validate_freelist(int checkall) {
    int used, used_mb, fcnt, ii, sts=0;
    int free, free_mb, compl=0;
    FsysRamFH *rfh;
    FsysRetPtr *rp;

    used = volumes[0].total_alloc_clusters;
    used_mb = (used*512 + 512*1024-1)/(1024*1024);
    free = volumes[0].total_free_clusters;
    free_mb = (free*512 + 512*1024-1)/(1024*1024);
    printf("A total of %d file headers allocated\n", volumes[0].files_ffree);
    printf("Of the %ld sectors on disk, %d sectors (%dMB, %d%%) are used and %d sectors (%dMB, %d%%) are available\n",
	volumes[0].maxlba,
	used, used_mb, used*100/(used+free),
	free, free_mb, free*100/(used+free));
    printf("A total of %ld sectors unaccounted for\n", volumes[0].maxlba - (used+free));
    printf("Freelist size:   %d elements\n", volumes[0].free_ffree);
    printf("Freelist alloc:  %d elements\n", volumes[0].free_elems);
    rp = volumes[0].free;
    free_mb = 0;
    printf("Checking for freelist overlapping entries...\n");
    for (ii=0; ii < volumes[0].free_ffree; ++ii, ++rp) {
	in_free(volumes, -1, rp, 0, ii, ii);
#if USE_REM_RPS
	{
	    FsysRetPtr ovr;
	    int sts;
	    sts = rem_rp(volumes, rp, &ovr);
	    if (sts < 0) {
		printf("rem_rp: freelist ent %3d %08lX-%08lX (%6ld) doubly allocated. Sts=%d, window=%08lX-%08lX\n",
		    ii, rp->start, rp->start+rp->nblocks-1, rp->nblocks, sts, ovr.start, ovr.start+ovr.nblocks-1);
		++compl;
	    }
	}
#endif
	free_mb += rp->nblocks;
    }
    printf("Computed free sectors: %d (diff: %d)\n", free_mb, free-free_mb);
    used_mb = 1+FSYS_MAX_ALTS;		/* count sector 0 plus the home blocks */
    printf("Checking for files overlapping freelist...\n");
    for (fcnt=ii=0; ii < volumes[0].files_ffree; ++ii) {
	rfh = fsys_find_ramfh(volumes, ii);
	if (!rfh) {
	    printf("No FH for FID %08X\n", ii);
	} else if (rfh->valid) {
	    int err = 0, amt;
	    amt = chk_free(volumes, rfh, ii, &err);
#if DUMP_RPS
	    if (amt != rfh->clusters) {
		printf("File %08X's FH has %ld clusters alloced, RP's total %d\n", ii, rfh->clusters, amt);
		dump_rp(rfh);
	    }
#endif
	    used_mb += amt + FSYS_MAX_ALTS;	/* count allocated sectors + FH's */
	    if (err) ++compl;
	    ++fcnt;				/* count the file */
	}
    }
    printf("Computed used sectors: %d (diff: %d) on %d files\n", used_mb, used-used_mb, fcnt);
    if (checkall) {
	printf("Checking for files overlapping each other...\n");
	for (ii=0; ii < volumes[0].files_ffree; ++ii) {
	    rfh = fsys_find_ramfh(volumes, ii);
	    if (rfh) {
		if (chk_ovr(volumes, rfh, ii)) ++compl;
	    }
	}
    }
    printf("Checking for fileheaders overlapping the freelist...\n");
    for (ii=0; ii < volumes[0].files_ffree; ++ii) {
	U32 *lbap;
	int alt;
	FsysRetPtr lrp;

	lbap = fsys_entry_in_index(volumes, ii);
	if (lbap) {
	    for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
		U32 lba;
		lba = *lbap++;
		if (!(lba&0x80000000)) {
		    lrp.start = lba;
		    lrp.nblocks = 1;
		    if (in_free(volumes, ii, &lrp, alt, 0, -1)) ++compl;
		}
	    }
	}
    }
    printf("Checking for fileheaders overlapping other files...\n");
    for (ii=0; ii < volumes[0].files_ffree; ++ii) {
	U32 *lbap;
	int alt, sts;
	FsysRetPtr lrp, ovr;

	lbap = fsys_entry_in_index(volumes, ii);
	if (lbap) {
	    for (alt=0; alt < FSYS_MAX_ALTS; ++alt) {
		U32 lba;
		lba = *lbap++;
		if (!(lba&0x80000000)) {
		    lrp.start = lba;
		    lrp.nblocks = 1;
		    sts = rem_rp(volumes, &lrp, &ovr);
		    if (sts < 0) {
			printf("File header %08X.%d overlaps another file\n", ii, alt);
			printf("\tlba=%08lX, window=%08lX-%08lX (%6ld)\n",
				lba, ovr.start, ovr.start+ovr.nblocks, ovr.nblocks);
			++compl;
		    }
		    if (in_free(volumes, ii, &lrp, alt, 0, -1)) ++compl;
		}
	    }
	}
    }
    printf("Checking for home blocks overlapping other files...\n");
    {
	int ii;
	FsysRetPtr lrp, ovr;

	get_home_block(0, "/d0");
	for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
	    lrp.start = home_block.rel_lba[ii];
	    lrp.nblocks = 1;
	    sts = rem_rp(volumes, &lrp, &ovr);
	    if (sts < 0) {
		printf("Home block at %08lX overlaps another file\n", lrp.start);
		printf("\tlba=%08lX, window=%08lX-%08lX (%6ld)\n",
			lrp.start, ovr.start, ovr.start+ovr.nblocks, ovr.nblocks);
		++compl;
	    }
	    if (in_free(volumes, 0, &lrp, ii, 0, -1)) ++compl;
	}
    }
#if USE_REM_RPS
    dump_alloc_rps("After all done");
    if (!compl) {
	upd_freelist();
    }
#endif
    fflush(stdout);
    if (compl) {
	printf("Check completed with %d errors\n", compl);
	compl = 1;
    }
    return compl;
}
