/* See LICENSE.txt for license details */

#ifndef _ST_PROTO_H_
#define _ST_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __STDC__
#define __STDC__ (0)
#endif

#if __STDC__
#define PARMS(x) x
#else
#define PARMS(x) ()
#define const
#define volatile
#endif

#define n_elts(array) (sizeof(array)/sizeof(array[0]))

/*		DoOptions(menu,current,bail)		       (menus.c)
 *	Set option word using <menu>, starting with <current> value.
 *	User can bail out (restore <current> settings) by use of input
 *	<bail>
 *
 *	This routine is obsolete, slated for demolition. The replacement
 *	is st_optmenu(), described below.
 */
extern unsigned long DoOptions
PARMS((
    const unsigned char *__menu,	/* In form discussed in menus.c */
    unsigned long	__current,	/* current settings */
    unsigned long	__bail		/* which switch bit in ctl_read_sw() */
));

/*		cn_allow(which_mask)
 *	Nominally a "coin routine", this is in st_proto.h because it
 *	is only of use to selftest code. Takes/returns a bitmask of
 *	which mechs cn_add_coin() is allowed to recognize. This may
 *	be useful within the coin-routines themselves in detail, but
 *	for general callers (e.g. switch-test), the safe values are
 *	zero and a previously returned value.
 */
unsigned int cn_allow
PARMS((
    unsigned int __which_mask		/* ones for each mech enabled */
));

/*		exit_to_game( smp )
 *	Provided for Williams-style pushbutton test switch.
 *	Does whatever it needs to reboot/return to game.
 *	It is defined as an int(*)(const struct menu_d *) to
 *	allow use directly as a menu item.
 */
extern int exit_to_game
PARMS((
    const struct menu_d *smp
));

/*		factory_setting(menu)
 *	Computes and returns the "factory setting" as indicated
 *	by the selection (in each menu field) starting with a '*'
 */
extern unsigned long factory_setting
PARMS((
    const unsigned char *__menu		/* In form discussed in menus.c */
));

#ifndef BRAVE_NEW_WORLD
extern void STInitKeepCom();
extern int STKeepCom();
extern int SoundCallOk();
extern void VolumeSetError();
#endif

/*		ExitInst()
 *	Show instructions for exiting screen, in selected palette
 *	(usually INSTR_PAL.) Returns line number suitable for use
 *	in subsequent st_insn() calls.
 */
extern int ExitInst
PARMS((
    int __palette	/*  Which AN Palette to use */
));

#ifndef BRAVE_NEW_WORLD
/*		BorderEdge()
 */
extern void BorderEdge();
/*		flood()
 *	Floods all of Color RAM with a single color, (except one location)
 */
extern void flood
PARMS((
));

/*		asm_full_ram()
 *	Does full RAM test on all RAM areas of board
 */
extern int asm_full_ram();
#endif

/*		bram_test()
 *	Does full RAM test on selected RAM area.
 */
extern int bram_test
PARMS((
    unsigned long *__base_address,	/* of area to test */
    unsigned long __length,
    unsigned long __bits,		/* bits to test */
    struct rrb *__result		/* for detailed error return */
));

/*		f_ram_test()
 *	Does full RAM test on selected RAM area.
 */
extern int f_ram_test
PARMS((
    const struct rdb *__descrip,
    struct rrb *__result
));

/*		q_ram_test()
 *	Does slightly less careful, but quicker, RAM test on selected RAM area.
 */
extern int q_ram_test
PARMS((
    const struct rdb *__descrip,
    struct rrb *__result
));

/*		pm_dump()
 *	Displays a "post mortem" dump in case of unrecoverable error.
 */
extern void pm_dump PARMS(( void ));

/*		pm_dump_param()
 *	Displays a "post mortem" dump in case of unrecoverable error.
 *	Requires three parameters, the first a pointer to the struct containing
 *	the variables, the second a processor type (same values as PROCESSOR)
 *	and the last is the first line on the screen on which to begin the dump.
 */
/* Having this definition here is asking for trouble, but it has to be here because
 * lots of old code won't automatically include it in their build ("the" definition
 * can be found in pptypes.mac which needs to be included in config.mac). It is
 * protected here so if the real one is in config.h and config.h is included _before_
 * this file, it will work provided the definition hasn't changed and there are no
 * assembly functions that care.
 */
#if !defined(_PM_GENERAL_STRUCT_)
# define _PM_GENERAL_STRUCT_
struct pm_general {
    const char *pm_msg;
    char pm_text[80];
    U32 *pm_stack;
    U32 *pm_stkupper;
    U32 *pm_stklower;
    U32 *pm_stkrelative;
    S32  pm_cntr;
    U32  pm_pc;
    U32  pm_sr;
    U32  pm_regs[32];
    U32  pm_cause;
    U32  pm_badvaddr;
};
#endif    
extern void pm_dump_param PARMS(( struct pm_general *ptr, int type, int row ));

/*		pot_display(prev_pots)
 *	Used by selftest to display pot (A->D) values. Called with
 *	a pointer to an array of (S16) pot values, it will draw
 *	the initial screen if prev_pots[0] < -255. Thereafter it
 *	uses prev_pots[] to decide when to re-draw.
 */
extern void pot_display
PARMS((
    S16 *__prev_pots	/* array of POT_CNT shorts for previous values */
));

/*		pot_vb_jobs()
 *	Used by vid_vb if POT_CNT is defined and non-zero.
 */
extern void pot_vb();

/*		rom_cksum(descrip)
 *	checksum the ROM described by struct rdb pointed to by <descrip>.
 */
extern int rom_cksum
PARMS((
    const struct rdb *__descrip
));

/*		setANPal(palette,colors)
 *	Accepts a palette number and longword containg two packed colors,
 *	and sets the corrseponding AlphaNumeric palette to have the
 *	specified foregound (LSW of color) and background (MSW of color),
 *	along with "appropriate" anti-alias colors interpolated from
 *	foreground and background.
 */
extern void SetANPal
PARMS((
    int		  __palette,	/* which palette to set */
    unsigned long __colors	/* bottom 16 bits are foregnd, top are bkgnd */ 
));

/*		setancolors()
 *	Set all AlphaNumeric palettes to values assumed by SelfTest.
 */
extern void setancolors();

/*		shade()
 *	Generic shading function. Writes <ncolors> colors into
 *	user's array <result>. result[0] will contain <start>,
 *	result[<ncolors>-1] will contain <end>, and the intermediate
 *	entries in result will contain intermediate values. <masks>
 *	points to a zero-terminated array of the masks for the individual
 *	components. This allows RGB, IRGB, CRY, or whatever other scheme
 *	of orthogonal components you wish. It will probably _not_ work
 *	for schemes like IQY or HLS where one or more components is "circular"
 *	in nature. 
 */
extern void shade
PARMS((
    unsigned long *result,	/* caller's array of <ncolors> longwords */
    const unsigned long *masks,	/* Masks for individual components */
    unsigned long start,	/* starting color */
    unsigned long end,		/* ending color */
    int 	  ncolors	/* number of colors wanted */
));

/*		st_bottom()
 *	Returns bottom-most line available for instructions, etc.
 *	This value is what st_frame() or st_insn() last returned,
 *	at this "nesting level". st_menu() will save/restore the
 *	variable this uses, to facilitate nested menus.
 */
extern int st_bottom();

/*		st_font(font_num)
 *	Special hook into text.c, to substitute the font specified by
 *	font_num for any occurance of font 0. This allows selftest to be
 *	legible regardless of the efforts of game graphics designers :-)
 */
extern void st_font
PARMS((
    int font_num		/* One of AN_SET* or equivalent */
));

/*		st_frame()
 *	re-paints the border and title, as well as optional date, exit
 *	instructions, etc. This is intended for use mainly by st_menu(),
 *	but will also be needed by any test that messes up the screen and
 *	thus needs to re-paint the boilerplate. Returns bottom-most row
 *	available for further instructions.
 */
extern int st_frame
PARMS((
    const struct menu_d *menu,	/* caller's menu line (for title) */
    int title_pal,		/* palette to use for title */
    int instr,			/* palette to use for instructions */
    int options			/* selects optional elements (#defines below) */
));

/* include os/main dates in "frame" */
#define STF_DATE (1)
/* instructions for "return to menu" say "...and hold..." */
#define STF_HOLD_RET (2)
/* top level menu, no "return to menu" */
#define STF_NOEXIT (4)

/*		st_getenv(name,hook)
 *	Finds the "environment variable" indicated by <name>
 *	and returns a pointer to the (struct st_envar) that
 *	specifes it. These structs are _not_ simply text
 *	strings as in POSIX, for the convenience of routines
 *	that may need to pass arbitrarily complex data. The
 *	search starts after the (struct st_envar) specified by
 *	<hook>, to allow multiple values for the same name
 *	and allow the user to reject an entry and keep looking.
 *	The "version" field of the structure is intended to
 *	facilitate such decisions. If <hook> is 0, the search
 *	starts at the "top" of the chain specified by the game,
 *	if any. After the game's chain is searched, any
 *	defaults supplied by GUTS are searched.
 */
extern const struct st_envar
#if (0) /* For reference */
{
    const char * name;
    void * value;
    struct st_envar *next;
    unsigned long version;
}
#endif
* st_getenv PARMS((
const char *__name,
const struct st_envar *__hook
));

/*		st_insn( row, action, switch, palette )
 *	A nauseatingly simple little routine that I (MEA) got
 *	tired of re-writing. Takes a row number, two text
 *	strings, and a palette, and tries to combine the
 *	two text strings on a single line, which it writes
 *	centered on the specified row. If it cannot, it
 *	writes the messages separately on two rows, the
 *	specified row and the one _ABOVE_ it. This is
 *	because instructions are typically on the bottom
 *	of the screen, and I want them to "grow up" (good
 *	advice for us all :-). Returns the number of the
 *	row above the highest one used, to allow a series
 *	of instructions to be assembled.
 */
extern int st_insn PARMS((
int row,
const char *action,	/* What we accomplish when we hit switch */
const char *swt,	/* What to do to accomplish action */
int palette
));

/*		st_joy_disp()
 *	Just to have a single place to change how we draw the little
 *	"tic-tac-toe" display of a joystick used in swt_test.c and
 *	possibly snd_test.c and pots.c.
 */
extern void st_joy_disp
PARMS((
int cx,			/* center X coordinate */
int cy,			/* center Y cordinate */
unsigned long sw	/* Switch bits */
));

/*		st_last_poll()
 *	Returns frame number (eer_rtc value) at last call to st_poll().
 *	This is a hack to allow any game-specific "check test-switch
 *	in vblank" code to _avoid_ running if the game is explicitely
 *	calling st_poll() in an orderly fashion. Such game-specific
 *	code should refrain from hijacking into selftest if
 *
 *	  (eer_rtc - st_last_poll() < ST_POLLING_HOLDOFF).
 *
 *	Since st_poll()	is intended for general game use, rather than
 *	only within selftest, it is declared in os_proto.h.
 */
extern U32 st_last_poll();

/*		st_menu()
 *	Menu-display harness for tests. Uses controls to select a
 *	menu-line, then runs selected test passing pointer to line.
 *	the size of a "menu line" struct must be passed to allow
 *	different callers to have different additional information.
 */
extern int st_menu
PARMS((
    const struct menu_d *menu,	/* caller's menu */
    int	menu_size,		/* size of an individual item */
    int txt_pal,		/* palette to use for text */
    int options			/* st_frame options (above) */
));

/*		NEW OPTION_SETTING MENU interface:
 *	The replacement for DoOptions(), st_optmenu() allows the user
 *	to set TOP and BOTTOM, and provides for the use of a callback
 *	routine for control interface. This allows, e.g. the use of an
 *	option menu within selftest screen that also includes other
 *	information.
 */

/*	First, a structure that encapsulates the state of an option-select
 *	menu. By using this, we avoid passing a bunch of parameters, some
 *	by reference. The structure itself is passed by reference, of course.
 *	The items[] array is a set of pointers into a menu string as described
 *	below. It serves two purposes: 1) speeds up access to an item by
 *	index, to aid gun-driven operation. 2) potentially allows "editting"
 *	the fields of a menu in response to changes in other fields. The
 *	latter is so far a "future direction", and will take some care
 *	in implementation.
 */
#ifndef MAX_OPT_ITEMS
#define MAX_OPT_ITEMS (32)	/* Number of bits in U32 sets upper limit */
#endif

struct opt_menu_state {
    int	n_items;		/* Number of options, or zero to force re-scan */
    int	select;			/* Index of selected item. */
    int top;			/* Index of top displayed item */
    U32 bits;			/* Current value of options */
    U32 orig_bits;		/* Value of options, at entry to st_optmenu */
    U32 warn_bits;		/* For future "This looks dubious" */
    U16 margin_t, margin_b, margin_l, margin_r;	/* For pointer-type controls */
    void *cb_vars;		/* Keeps track of "static" vars for callback */
    const U8 *items[MAX_OPT_ITEMS+1];
};

/*	Possible values for the "action" parameter of show_menu(), which
 *	are also used as possible returns for the "user control" coroutine.
 */
#define M_ACT_NONE (0)		/* No-operation */
#define M_ACT_ERASE (1)		/* "modifier" for REDRAW and SELECTED */
#define M_ACT_REDRAW (2)	/* redraw entire menu */
#define M_ACT_ERASE_ALL (3)	/* erase entire menu */
#define M_ACT_SELECTED (4)	/* draw (or erase) only selected item */
/*	Plus some that are _only_ used as a return from the "user control"
 *	coroutine.
 */
#define M_ACT_EXIT (8)		/* Return to original caller */
#define M_ACT_INCVAL (16)	/* Increment value of selected option */
#define M_ACT_DECVAL (32)	/* Decrement value of selected option */

extern int (*st_opt_callback())(struct opt_menu_state *mnp);

extern U32 st_optmenu
PARMS((
const U8 *menu,
U32 opt_bits,
int top,
int bottom,
int (*callback)( struct opt_menu_state *mnp ),
void *cb_vars
));

/*		st_optmenu_instr()
 *	For the convenience of tests which want to include
 *	a menu-setting portion, but don't want to duplicate
 *	the byzantine logic involved in showing the right
 *	instructions, st_optmenu_instr() will draw them.
 *	if smp is non-zero, it will call st_frame(), else
 *	it will start from the current bottom of the screen,
 *	perhaps above other instructions.
 */
int st_optmenu_instr
PARMS((
    const struct menu_d *smp
));

/*		st_vblank()
 *	Misc VBlank processing in self-test
 */
extern void st_vblank();

/*	Messages that all tests should use to instruct user
 */

	/*	hold ACTION and press NEXT button			*/
extern const	char	t_msg_clear[];

	/*	press NEXT and ACTION button				*/
extern const	char	t_msg_next2[];

	/*	press NEXT button					*/
extern const	char	t_msg_next[];

	/*	press and hold NEXT button				*/
extern const	char	t_msg_nexth[];

	/*	press ACTION button					*/
extern const	char	t_msg_action[];

	/*	press and hold ACTION button				*/
extern const	char	t_msg_actionh[];

	/*	press EXTRA button					*/
extern const	char	t_msg_extra[];

	/*	press and hold EXTRA button					*/
extern const	char	t_msg_extrah[];

	/* describe principle control device, e.g. Joysticks */
extern const	char	t_msg_control[];

extern const	char	t_msg_ret_menu[] /* =	"to return to menu" */ ;
extern const	char	t_msg_save_ret[] /* =	"to SAVE setting and exit" */;
extern const	char	t_msg_restore[]  /* =	"to RESTORE old Setting" */;

	/* Choose then press ACTION button				*/
extern const	char	t_msg_menu[];

    	/*	Use <buttons> to select */
extern const	char	t_msg_select[];

    	/*	Use <buttons> to set */
extern const	char	t_msg_set[];

/*		total_coins()
 *	Used by stats.c to get total _value_ of all coins for display.
 *	should be namde cn_?*?
 */
extern unsigned long total_coins();

/*		tq_maint(usec)					(time.c)
 *	Maintains the timer queue. This is present, but differently
 *	implemented, in the various flavors of GUTS, with or without
 *	an underlying O.S.
 */
extern void tq_maint
PARMS((
    unsigned long __usec /* best guess at microseconds since last called */
));
struct opaque;

#ifndef __cplusplus
struct parm {
    const char *label;
    U32 val;
    S16	col;
    S16	row;
    U16	sig_dig;
    S16	cur_dig;
    char *string;
    int hlp_fd;
    const char *hlp_msg;
    U32 (*hndl_btns)(U32 edges, struct parm *pp);
    void (*hndl_disp)(struct parm *pp, int idx, struct opaque *op);
    void *user;
};
#define UTL_ENTER_FLG_DIGMSK	(0x007F) /* sig_dig field size mask */
#define HEX_FIELD (0x8000)	/* OR'd with sig_dig for a hex number */
#define SIGNED_FIELD (0x4000)	/* OR'd with sig_dig for a Signed number */
#define BCD_FIELD (HEX_FIELD|0x2000) /* OR'd with sig_dig for BCD number */
#define UTL_ENTER_FLG_BAD	(0x1000) /* 'val' member out of range */
#define UTL_ENTER_FLG_OFF	(0x0800) /* Turn off item (clears the text) */
#define UTL_ENTER_FLG_KP	(0x0400) /* Enable use of the ten-Keypad keys (no star and pound) */
#define UTL_ENTER_FLG_KPSP	(0x0200) /* Enable use of the twelve-keypad keys (include star and pound) */
#define UTL_ENTER_FLG_DISPONLY	(0x0100) /* this field cannot be selected, only displayed */
extern int utl_enter
PARMS((
    struct parm *__pp,
    int (*__cb)(struct parm *_up, int _idx, struct opaque *_op),
    struct opaque *__cbp
));
#endif		/* __cplusplus */

extern const struct pconfigb *pbase;
#ifndef CN_FRESHNESS_DATE
#define CN_FRESHNESS_DATE 19991231 /* Default coin stuff to pre-Y2K */
#endif
#if CN_FRESHNESS_DATE < 20000000
extern const unsigned char coinmenu[]; 
extern const unsigned char cn_val_menu[]; 
#endif
extern void cn_set_defaults(void);
extern unsigned char **draw_pt;
extern unsigned long dbswitch;
extern unsigned long fake_controls;
extern unsigned short bitshd;
extern int debug_mode;		/* derived from EER_GUTS_OPT */
#define AUD_AV_SHF (0)		/* Bottom bits of GUTS_OPT, attract volume */
#define AUD_AV_MSK (3)		/* Bottom bits of GUTS_OPT, attract volume */
#define GUTS_OPT_DBG_SW 	(4)	/* nominal bit for "debug switches active" */
#ifdef VID_SLAVE_SENSE
#define GUTS_OPT_SLVMODE	(8)	/* Temp bit for Link Slave mode value */
#else
#define GUTS_OPT_CACHE		(8)	/* nominal bit for "Cache Enable" */
#endif
#define GUTS_OPT_DEVEL 		(0x00000010)	/* nominal bit for "Development" (RAM) */
#if defined(INCLUDE_GUTS_OPT_RPT_FEXCP) && INCLUDE_GUTS_OPT_RPT_FEXCP
#define GUTS_OPT_ENAB_FEXCP	(0x00000020) /* Enable traps on floating point exceptions */
#define GUTS_OPT_SPARE0		(0x00000040) /* Not used normally */
#else
#define GUTS_OPT_AUD_PANIC	(0x00000020) /* Temp bit for Chuck's "Barf and die" */
#define GUTS_OPT_AUD_TEMPO	(0x00000040) /* 0 = 60Hz, 1 = 240 Hz */
#endif
#define GUTS_OPT_PM_WDOG	(0x00000080)	/* 0 = enable pm dump watchdog reset, 1 = disable pm dump watchdog reset */
#define GUTS_OPT_PM_HOLD	(0x00000100) /* 1=enable PM dump hold forever */
#define GUTS_OPT_RW_VIA_PIO	(0x00000200) /* 1=use disk PIO. 0=use disk DMA */
#if defined(INCLUDE_GUTS_OPT_RPT_DISK_ERRS) && INCLUDE_GUTS_OPT_RPT_DISK_ERRS
#define GUTS_OPT_RPT_DISK_ERRS	(0x00000400) /* 1=report disk errors. 0=disk never errors out */
#endif
#if defined(INCLUDE_GUTS_OPT_IDE_RATE) && INCLUDE_GUTS_OPT_IDE_RATE
#define GUTS_OPT_IDE_RATE_V	(11)	  /* bit number of lsb */
#define GUTS_OPT_IDE_RATE_M	(0x00003800) /* 3 bits worth. Next available bit is 0x04000*/
#endif
#define GUTS_OPT_COUNT_RSTS	(0x00004000) /* Count all resets whether they are pm dumped or not */
#define GUTS_OPT_DISWDOG	(0x00008000) /* disable watchdog on systems that can do it via s/w */
#define GUTS_OPT_DIS_POT_POST	(0x00010000) /* disable pot_POST */
#define GUTS_OPT_TOURN_DEVELOP	(0x00020000) /* Tournament development mode */
#define GUTS_OPT_LASTOPT_V	17	/* Set this to the bit number of the last used GUTS_OPT */

/*	Temporary home for self-test CAGE command index definitions
 *	The "fixed" MOS calls moved, so now we indirect via a table
 *	to get them. aud_cmd() will return 0 if a command is not
 *	implemented under this version of MOS. Else it will return
 *	the command (including the 0x8000)
 */
extern int aud_cmd
PARMS((
int which		/* one of the ST_C_* from below */
));

#define ST_C_ATR_MODE (1)	/* aka S_SOFF, silent, attract */
#define ST_C_GAME_MODE (2)	/* aka S_ON, noisy, game */
#define ST_C_VMUSIC (3)		/* Sample music for volume adj. */
#define ST_C_SMAX (4)		/* Get number of sounds */
#define ST_C_DIAG (5)		/* Run diagnostics + cksums (>1.06) */
#define ST_C_GAME_VOL (6)	/* set "game" mode volume */
#define ST_C_ATR_VOL (7)	/* set "attract" mode volume */
#define ST_C_MOS_VER (8)	/* return MOS version xx.xx */
#define ST_C_PJCT_VER (9)	/* return game version xxxx */
#define ST_C_ERR_CNT (0xA)	/* return number of errors */
#define ST_C_CLR_ERR_LOG (0xB)	/* clear error log */
#define ST_C_ERR_LOG (0xC)	/* get error log (ending in CA6E) */
#define ST_C_CKSUMS (0xD)	/* get cksums (from DIAG) */
#define ST_C_PEEK (0xE)		/* read one word of CAGE mem */
#define ST_C_SPKR (0xF)		/* Speaker location test */
#define ST_C_SINE (0x10)	/* 1K sine wave */
#define ST_C_KSINE (0x11)	/* stop 1K sine wave */
#define ST_C_SWP (0x12)		/* Sine wave sweep (34Hz and up) */
#define ST_C_PSWP (0x13)	/* Pause Sine wave sweep */

#ifdef __cplusplus
}
#endif
#endif			/* _ST_PROTO_H_ */
