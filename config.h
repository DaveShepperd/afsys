
/* See LICENSE.txt for license details */
#if !defined(_CONFIG_H_)
#define _CONFIG_H_
#if !defined(_CONSTANTS_H_)
#define _CONSTANTS_H_
#define PROCESSOR_CLASS (0x0FFFFFFF0)
#define M68000 (0x1)
#define M68010 (0x2)
#define M68EC020 (0x3)
#define M68020 (0x3)
#define ASAP (0x10)
#define MIPS3000 (0x20)	/* MIPS 30x1 processor, big endian */
#define MIPS30x1 (0x21)	/* MIPS 30x1 processor, big endian */
#define MIPS30x1L (0x22)	/* MIPS 30x1 processor, little endian */
#define MIPS4000 (0x30)	/* MIPS 4000 processor, big endian */
#define MIPS4000L (0x31)	/* MIPS 4000 processor, little endian */
#define MIPS4600 (0x32)	/* MIPS 4600 processor, big endian */
#define MIPS4600L (0x33)	/* MIPS 4600 processor, little endian */
#define MIPS4650 (0x34)	/* MIPS 4650 processor, big endian */
#define MIPS4650L (0x35)	/* MIPS 4650 processor, little endian */
#define MIPS4700 (0x36)	/* MIPS 4700 processor, big endian */
#define MIPS4700L (0x37)	/* MIPS 4700 processor, little endian */
#define MIPS5000 (0x38)	/* MIPS 5000 processor, big endian */
#define MIPS5000L (0x39)	/* MIPS 5000 processor, little endian */
#define MIPS4300 (0x3A)	/* MIPS 4300 processor, big endian */
#define MIPS4300L (0x3B)	/* MIPS 4300 processor, little endian */
#define MIPS7000 (0x3C)	/* MIPS 7000 processor, big endian */
#define MIPS7000L (0x3D)	/* MIPS 7000 processor, little endian */
#define CX486 (0x41)	/* Cyrix 486/pentium */
#define I86x32 (0x42)	/* Plain 32 bit i86 platform */
#define HOST_BOARD_CLASS (0x0FFFFFFF0)
#define EC020cojag (0x10)	/* McKee's low cost EC020 host board, CoJag version */
#define EC020zoid10 (0x20)	/* McKee's low cost EC020 host board, Zoid 10 version */
#define EC020zoid20 (0x30)	/* McKee's low cost EC020 host board, Zoid 20 version */
#define MCUBE (0x40)	/* Mokris's 68k based host board */
#define ASCLEAP (0x50)	/* The ASAP based ASCLEAP */
#define LCR3K (0x60)	/* Low cost R3k host board */
#define IDT3xEVAL (0x70)	/* IDT's 3000 eval board with XBUS adapter */
#define IDT4xEVAL (0x80)	/* IDT's 4000 eval board with XBUS adapter */
#define MB4600 (0x90)	/* Senthil and Mark's 4600 MathBox board */
#define HCR4K (0x0A0)	/* Senthil and Mark's 4600 Host board */
#define PSX (0x0B0)	/* Sony PSX */
#define PHOENIX (0x0C0)	/* WMS Host board */
#define PHOENIX_AD (0x0C1)	/* WMS Phoenix-AD board (Hockey) board */
#define FLAGSTAFF (0x0C2)	/* WMS Phoenix-Flagstaff (Rush) board */
#define SEATTLE (0x0C3)	/* WMS Phoenix-Seattle (Mace/CSpeed/Genocide) board */
#define VEGAS (0x0C4)	/* WMS Phoenix-Vegas/Dingo/Ringo/Durango board */
#define CHAMELEON (0x0C5)	/* WMS Chameleon board */
#define TAHOE (0x0C6)	/* WMS Tahoe board (defunct) */
#define SAN_FRANCISCO (0x0C7)	/* WMS San Francisco board (defunct) */
#define ATLANTIS (0x0C8)	/* WMS Atlantis board (MIPS/ZEUS) */
#define CYRIX_MGX (0x0D0)	/* Cyrix Media/GX */
#define I86_PC (0x0E0)	/* Plain i86 PC */
#define VIDEO_BOARD_CLASS (0x0FFFFFFF0)
#define ZOID10_V (0x10)	/* Zoid 10 stack		*/
#define ZOID20_V (0x20)	/* Zoid 20 stack 	*/
#define COJAG_V (0x30)	/* CoJag stack 		*/
#define GX1_V (0x40)	/* FSG42 board		*/
#define GX2_V (0x50)	/* GX2 board		*/
#define GT_V (0x60)	/* GT board		*/
#define TDFX_V (0x70)	/* Generic 3DFX board	*/
#define SST_V (0x71)	/* 3DFX Voodoo I or II board */
#define BANSHEE_V (0x72)	/* 3DFX Banshee of Voodoo III board */
#define PSX_V (0x80)	/* Sony PSX */
#define CYRIX_MGX_V (0x90)	/* Cyrix Media/GX */
#define ZEUS_V (0x0A0)	/* WMS Zeus video */
#define DCS_SIO (0x1)	/* Williams SIO Board */
#define DCS_DSIO (0x2)	/* Atari Deluxe SIO Board - with FIFOS */
#define DCS_DSIOI (0x3)	/* Atari Deluxe SIO Board - w/o FIFOS */
#define DCS_DSIOR (0x4)	/* Atari Deluxe SIO Board - DCS RUSH Stereo Sound OS */
#define ADCS_DSIO (0x5)	/* A.D.C.S. DSP O/S (Stereo) and Atari Deluxe SIO Board */
#define ADCS_DSIOM (0x6)	/* A.D.C.S. DSP O/S (Mono) and Atari Deluxe SIO Board */
#define ADAGE_WSIO6 (0x7)	/* A.D.A.G.E. DSP O/S and  Williams SIO Board w/6-channel Sound */
#define ADCS_ATLANTIS (0x8)	/* A.D.C.S. DSP O/S (Stereo) for Atlantis */
#define ADAGE_DSIO6 (0x9)	/* A.D.A.G.E. DSP O/S and Atari Deluxe SIO w/6-channel Sound */
#define COJAG_PROTO (0x1)	/* Non-Game specific running on COJAG */
#define COJAG_HERO (0x2)	/* Hero on COJAG */
#define COJAG_AREA51 (0x4)	/* Area 51 on COJAG */
#define COJAG_RAGE (0x8)	/* RAGE 2 on COJAG */
#define COJAG_FISH (0x10)	/* Tropical Fish on COJAG */
#define ZOID_PROTO (0x1)	/* Non-Game specific running on ZOID */
#define ZOID_HOCKEY (0x2)	/* Wayne Gretzky Hocky running on ZOID */
#define ZOID_GAUNTLET (0x4)	/* 3D Gauntlet running on ZOID */
#define ZOID20_DIAG (0x8)	/* Mike Albaugh developing ZOID 20 */
#define ZOID_RUSH (0x10)	/* SF Rush running on ZOID */
#define ZOID_MACE (0x20)	/* Mace running on ZOID */
#define ZOID20_DMS (0x40)	/* Dave Shepperd test ZOID */
#define SST_PROTO (0x1)	/* Non-Game specific running on 3DFX */
#define SST_DMS (0x2)	/* Dave Shepperd test 3DFX */
#define SST_RUSH (0x4)	/* RUSH 3DFX */
#define SST_HOCKEY (0x8)	/* Hockey 3DFX */
#define SST_MACE (0x10)	/* Mace 3DFX */
#define SST_SPEED (0x20)	/* Speed 3DFX */
#define SST_STAR (0x40)	/* Star (working title) 3DFX */
#define SST_JUKO (0x80)	/* Juko Thread 3DFX */
#define SST_GENO (0x100)	/* Genocide 3DFX */
#define SST_GAUNTLET (0x200)	/* 3Dfx Gauntlet */
#define SST_CRASH (0x400)	/* Crash (game) 3DFX */
#define SST_AIRRACE (0x800)	/* AirRace 3DFX (Blueshift) */
#define SST_WAR (0x1000)	/* War (3DFX) */
#define SST_AIRRACEX (0x2000)	/* Xenotech flavor of Vaportrx */
#define SST_DOZER (0x4000)	/* Dozer 3DFX (Blueshift) */
#define PSX_PROTO (0x1)	/* Non-Game specific running on PSX */
#define PSX_RAGE2 (0x2)	/* Primal Rage II on PSX */
#define CYRIX_BLOODLUST (0x1)	/* Bloodlust */
#define CYRIX_AREA52 (0x2)	/* Area 52 */
#define ZEUS_GOLF (0x1)	/* GOLF game */
#ifndef TRUE
#define TRUE (0x1)
#endif
#ifndef FALSE
#define FALSE (0x0)
#endif
#define ABORT (0x0)
#define FAIL (0x0FFFFFFFF)
#define IDE_COJAG (0x1)	/* For IDE on COJAG boards */
#define IDE_STREAM (0x2)	/* For IDE on STREAM boards */
#define IDE_PCI (0x3)	/* For IDE on PCI bus */
#define IDE_PSX (0x4)	/* For IDE on PSeXtra */
#define ANSI_OK (0x1)
#endif			/* _CONSTANTS_H_ */
#define PROCESSOR (0x42)
#if !defined(_PPTYPES_H_)
#define _PPTYPES_H_
#define NO_LONGLONG (0x1)
#ifndef __VS32_TYPE_DEFINED
#define __VS32_TYPE_DEFINED
typedef volatile long VS32;
#endif /* __VS32_TYPE_DEFINED */
#ifndef __VS16_TYPE_DEFINED
#define __VS16_TYPE_DEFINED
typedef volatile short VS16;
#endif /* __VS16_TYPE_DEFINED */
#ifndef __VS8_TYPE_DEFINED
#define __VS8_TYPE_DEFINED
typedef volatile signed char VS8;
#endif /* __VS8_TYPE_DEFINED */
#ifndef __VS08_TYPE_DEFINED
#define __VS08_TYPE_DEFINED
typedef volatile signed char VS08;
#endif /* __VS08_TYPE_DEFINED */
#ifndef __VU32_TYPE_DEFINED
#define __VU32_TYPE_DEFINED
typedef volatile unsigned long VU32;
#endif /* __VU32_TYPE_DEFINED */
#ifndef __VU16_TYPE_DEFINED
#define __VU16_TYPE_DEFINED
typedef volatile unsigned short VU16;
#endif /* __VU16_TYPE_DEFINED */
#ifndef __VU8_TYPE_DEFINED
#define __VU8_TYPE_DEFINED
typedef volatile unsigned char VU8;
#endif /* __VU8_TYPE_DEFINED */
#ifndef __VU08_TYPE_DEFINED
#define __VU08_TYPE_DEFINED
typedef volatile unsigned char VU08;
#endif /* __VU08_TYPE_DEFINED */
#ifndef __VF32_TYPE_DEFINED
#define __VF32_TYPE_DEFINED
typedef volatile float VF32;
#endif /* __VF32_TYPE_DEFINED */
#ifndef __VF64_TYPE_DEFINED
#define __VF64_TYPE_DEFINED
typedef volatile double VF64;
#endif /* __VF64_TYPE_DEFINED */
#ifndef __m_int_TYPE_DEFINED
#define __m_int_TYPE_DEFINED
typedef int m_int;
#endif /* __m_int_TYPE_DEFINED */
#ifndef __m_uint_TYPE_DEFINED
#define __m_uint_TYPE_DEFINED
typedef unsigned int m_uint;
#endif /* __m_uint_TYPE_DEFINED */
#ifndef __U8_TYPE_DEFINED
#define __U8_TYPE_DEFINED
typedef unsigned char U8;
#endif /* __U8_TYPE_DEFINED */
#ifndef __U08_TYPE_DEFINED
#define __U08_TYPE_DEFINED
typedef unsigned char U08;
#endif /* __U08_TYPE_DEFINED */
#ifndef __S8_TYPE_DEFINED
#define __S8_TYPE_DEFINED
typedef signed char S8;
#endif /* __S8_TYPE_DEFINED */
#ifndef __S08_TYPE_DEFINED
#define __S08_TYPE_DEFINED
typedef signed char S08;
#endif /* __S08_TYPE_DEFINED */
#ifndef __U16_TYPE_DEFINED
#define __U16_TYPE_DEFINED
typedef unsigned short U16;
#endif /* __U16_TYPE_DEFINED */
#ifndef __S16_TYPE_DEFINED
#define __S16_TYPE_DEFINED
typedef short S16;
#endif /* __S16_TYPE_DEFINED */
#ifndef __U32_TYPE_DEFINED
#define __U32_TYPE_DEFINED
typedef unsigned long U32;
#endif /* __U32_TYPE_DEFINED */
#ifndef __S32_TYPE_DEFINED
#define __S32_TYPE_DEFINED
typedef long S32;
#endif /* __S32_TYPE_DEFINED */
#ifndef __F32_TYPE_DEFINED
#define __F32_TYPE_DEFINED
typedef float F32;
#endif /* __F32_TYPE_DEFINED */
#ifndef __F64_TYPE_DEFINED
#define __F64_TYPE_DEFINED
typedef double F64;
#endif /* __F64_TYPE_DEFINED */
#ifndef __RD_TYP_TYPE_DEFINED
#define __RD_TYP_TYPE_DEFINED
typedef struct rdb RD_TYP;
#endif /* __RD_TYP_TYPE_DEFINED */
#ifndef __RR_TYP_TYPE_DEFINED
#define __RR_TYP_TYPE_DEFINED
typedef struct rrb RR_TYP;
#endif /* __RR_TYP_TYPE_DEFINED */
#ifndef __MN_TYP_TYPE_DEFINED
#define __MN_TYP_TYPE_DEFINED
typedef struct menub MN_TYP;
#endif /* __MN_TYP_TYPE_DEFINED */
#ifndef __PB_TYP_TYPE_DEFINED
#define __PB_TYP_TYPE_DEFINED
typedef struct pconfigp PB_TYP;
#endif /* __PB_TYP_TYPE_DEFINED */
#ifndef __CR_TYP_TYPE_DEFINED
#define __CR_TYP_TYPE_DEFINED
typedef struct creditsb CR_TYP;
#endif /* __CR_TYP_TYPE_DEFINED */
struct menu_d {
	char	*mn_label;		    /* menu item label		*/
	int	(*mn_call)(const struct menu_d*); /* menu item routine call	*/
};
struct menub {
	char		*mn_label;	/* menu item label		*/
	void		(*mn_call)();	/* menu item routine call	*/
};
struct creditsb {
	unsigned short	crd_whole;	/* Integer part of coins	*/
	unsigned char	crd_num;	/* numerator			*/
	unsigned char	crd_denom;	/* denominator			*/
};
struct st_envar {
const char * name;	/*  for lookup */
const void * value;	/*  could point to anything */
const struct st_envar * next;	/*  chain  */
unsigned long version;	/*  not yet used */
};
struct rdb {
unsigned long * rd_base;	/*  Starting address  */
unsigned long rd_len;	/*  Length in bytes  */
unsigned long rd_misc;	/*  Which bits exist */
};
#define PM_TEXT_SIZE (80)	/*Up to 80 bytes of postmortem text*/
#define CN_SALVAGE (0)	/*Attept to salvage coins across crashes*/
struct pm_general {
const char * pm_msg;	/* Pointer to message */
char pm_text[PM_TEXT_SIZE];	/* Local copy of text message */
U32* pm_stack;	/* Stack pointer in target's address space */
U32* pm_stkupper;	/* Stack upper limit in target's address space */
U32* pm_stklower;	/* Stack lower limit in target's address space */
U32* pm_stkrelative;	/* Stack pointer in host's address space */
S32 pm_cntr;	/* Post mortem flag */
U32 pm_pc;	/* Program counter */
U32 pm_sr;	/* Status register */
U32 pm_regs[32];	/* ASAP/R3K/R4K have 32. 68k only uses 16 of these */
U32 pm_cause;	/* R3K/R4K cause register */
U32 pm_badvaddr;	/* R3K/R4K bad virtual address register */
U32 pm_cn_salvage;	/* Hiding place for salvaged coins */
U32 pm_reboot_flgs;	/* Reboot flags */
U32 pm_padding[8];	/* Room for additional items */
};
# define _PM_GENERAL_STRUCT_	0	/* Disable the definition in st_proto.h */
# define PM_68K_SIZE	(sizeof(struct pm_general)-(18*4))
# define PM_RxK_SIZE	(sizeof(struct pm_general))
struct rrb {
unsigned long * rr_addr;	/*  Where it choked  */
unsigned long rr_expected;	/*  What it wanted  */
unsigned long rr_got;	/*  What it got */
int rr_test_no;	/*  Which test  */
};
#define B_NO_ROM_TEST (0x0)	/* bit # in p_debug_options to skip ROM checksum	*/
#define NO_ROM_TEST (0x1)
#define B_NO_RAM_TEST (0x1)	/* bit # in p_debug_options to skip RAM test	*/
#define NO_RAM_TEST (0x2)
#define B_NO_LOG_RESET (0x2)	/* bit # in p_debug_options to skip logging RESET*/
#define NO_LOG_RESET (0x4)
#endif			/* _PPTYPES_H_ */
#define HOST_BOARD (0x0E0)
#define _GNU_SOURCE (1)	/*Compile with GCC*/
#define NO_NEWLIB_FUNCS (1)	/*Not using newlib*/
#ifndef _LINUX_
#define _LINUX_ 1
#endif
#define CONFIG_MIDWAY 1
#define CONFIG_MIDWAY_ATLANTIS 1
#define NO_GAME_CODE (1)	/* Go straight into test code */
#define VIS_H_PIX_MAX (512)
#define VIS_V_PIX_MAX (384)
#define INCLUDE_QIO (1)	/* Include the QIO subsystem */
#define INCLUDE_FSYS (1)	/* Include the FSYS subsystem */
#define INCLUDE_IDE (1)	/* Include special IDE interrupt in shims */
#define FSYS_READ_ONLY (0)	/* filesys is r/w */
#define FSYS_TIGHT_MEM (1)	/* filesys uses little mem */
#define NO_FSYS_TEST (1)	/* Don't include filesystem test code */
#define FSYS_NO_AUTOSYNC (1)	/* Don't include automatic sync */
#define FSYS_ASYNC_CLOSE (1)	/* FSYS can use async close */
#define FSYS_HAS_TQ_INS (0)	/* FSYS cannot use tq_ins */
#define FSYS_USE_MALLOC (1)	/* Use malloc instead of _malloc_r() */
#define HAVE_TIME		(1)		/* Allow the use of time() for file create */
#define FSYS_FEATURES (FSYS_FEATURES_CMTIME|FSYS_FEATURES_JOURNAL)
#define FSYS_OPTIONS FSYS_FEATURES
#define FSYS_FIX_FREELIST	(1)		/* This global's back_to_free() and add_to_dirty() */
#define QIO_NOSTDIO_SHIMS (1)	/* No shims */
#define INCLUDE_QIO (1)	/* Need this */
#define MAX_AST_QUEUES (1)	/* Enable AST processing */
#define INTS_OFF (0)	/* Disable interrupts */
#define NUM_HDRIVES (1)	/* Allow it to be used with a single drive */
#define FSYS_INCLUDE_HB_READER (1)	/* Need to be able to read home blocks */
#include "lindefs.h"
#endif				/* _CONFIG_H_ */
