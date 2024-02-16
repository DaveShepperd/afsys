/* See LICENSE.txt for license details */

#ifndef _LINDEFS_H_
#define _LINDEFS_H_
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef MAIN_PRIORITY
#define MAIN_PRIORITY 1
#endif
#ifndef BASE_PRIORITY
#define BASE_PRIORITY (MAIN_PRIORITY+10)
#endif

enum priorities {
    FSYS_PRIORITY=BASE_PRIORITY,
    IDE_PRIORITY,
    AST_PRIORITY,
    ACT_PRIORITY,
    INTS_PRIORITY,
    NOINT_PRIORITY,
    MAX_PRIORITY
};

extern int my_mlock(U32 start, int len, const char *who);

#if defined(LINDEFS_LOCK_SECTIONS) && LINDEFS_LOCK_SECTIONS
#define __locktext   __attribute__ ((__section__ (".text.lock")))
#define __lockrodata __attribute__ ((__section__ (".data.lock")))
#define __lockdata   __attribute__ ((__section__ (".data.lock")))
#define __lockbss    __attribute__ ((__section__ (".data.lock")))
#else
#define __locktext   
#define __lockrodata
#define __lockdata
#define __lockbss
#endif

#if !defined(LINDEFS_NO_INLINES) || !LINDEFS_NO_INLINES
#ifndef LOCAL
#define LOCAL static
#endif
#ifndef INLINE
#define INLINE __inline__
#endif
#else
#undef LOCAL
#define LOCAL 
#undef INLINE
#define INLINE 
#endif

#if MIPS3000 && MIPS4000 && ((PROCESSOR_CLASS & PROCESSOR ) == MIPS3000) || ((PROCESSOR_CLASS & PROCESSOR ) == MIPS4000)
#define CP0_INTS_ONOFF	(0*4)		/* Enable/Disable h/w interrupts (int 1) */
#define CP0_GET_STATUS	(1*4)		/* read STATUS register */
#define CP0_SET_STATUS	(2*4)		/* write _some_ status register bits */
#define CP0_GET_COUNT	(3*4)		/* read COUNT register */
#define CP0_GET_CAUSE	(4*4)		/* read CAUSE register */
#define CP0_GET_COMPARE	(5*4)		/* read COMPARE register */
#define CP0_GET_IOA_TIMES (6*4)		/* read ioasic interrupt times */
#define CP0_IOA_MAIN	(7*4)		/* get/set IOASIC interrupt control register */
#define CP0_INTCTL	(8*4)		/* get/set INTCTL register */

#define HW_INT_BIT	(0x0800)	/* all our h/w interrupts arrive on INT1 */

extern int interrupts_enabled;

LOCAL INLINE U32 prc_get_count(void) {
    U32 ans;
    __asm__ volatile (".set mips2; li $4, %1; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"i" (CP0_GET_COUNT) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE U32 prc_get_cause(void) {
    U32 ans;
    __asm__ volatile (".set mips2; li $4, %1; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"i" (CP0_GET_CAUSE) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE U32 prc_get_compare(void) {
    U32 ans;
    __asm__ volatile (".set mips2; li $4, %1; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"i" (CP0_GET_COMPARE) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE U32 _set_status(U32 _new) {
    U32 ans;
    __asm__ volatile (".set mips2; move $5, %1; li $4, %2; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"r" (_new), "i" (CP0_SET_STATUS) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE U32 prc_get_ipl(void) {
    U32 ans;
    __asm__ volatile (".set mips2; li $4, %1; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"i" (CP0_GET_STATUS) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE void _get_ioa_times(U32 *times) {
    U32 in, out;
    __asm__ volatile (".set mips2; li $4, %2; TEQ $0, $0; move %0, $2; move %1, $3;.set mips0" :
    		"=r" (in), "=r" (out):	/* outputs */
    		"i" (CP0_GET_IOA_TIMES) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    times[0] = in;
    times[1] = out;
}

LOCAL INLINE int prc_set_ipl(int _new) {
    U32 ans;
    __asm__ volatile (".set mips2; move $5, %1; li $4, %2; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (ans) :	/* outputs */
    		"r" (_new), "i" (CP0_INTS_ONOFF) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return ans;
}

LOCAL INLINE int ctl_mod_iomain(int _new) {
    int old;
    __asm__ volatile (".set mips2; move $5, %1; li $4, %2; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (old):	/* outputs */
    		"r" (_new), "i" (CP0_IOA_MAIN) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return old;
}
#define CTL_MOD_IOMAIN(x) ctl_mod_iomain(x)

LOCAL INLINE int ctl_mod_intctl(int _new) {
    int old;
    __asm__ volatile (".set mips2; move $5, %1; li $4, %2; TEQ $0, $0; move %0, $2; .set mips0" :
    		"=r" (old):	/* outputs */
    		"r" (_new), "i" (CP0_INTCTL) : /* inputs */
    		"$1", "$2", "$3", "$4", "$5", "$6" /* clobbered */
    	   );
    return old;
}
#define CTL_MOD_INTCTL(x) ctl_mod_intctl(x)

#endif  /* Any MIPS */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE (1)
#endif
/*
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE (1)
#endif
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

LOCAL INLINE int sem_open(int init) {
    int ans;
    ans = open("/dev/atl_sem", O_RDWR);
    return ans;
}

LOCAL INLINE int sem_wait(int which) {
    int ans;
    ans = ioctl(which, 0, 0);
    return ans;
}

LOCAL INLINE int sem_go(int which) {
    int ans;
    ans = ioctl(which, 0, 1);
    return ans;
}

LOCAL INLINE int sem_close(int which) {
    int ans;
    ans = close(which);
    return ans;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif				/* _LINDEFS_H_ */
