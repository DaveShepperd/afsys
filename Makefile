#
# See LICENSE.txt for license details
#
SHELL		= /bin/sh
DEL		= rm -f
PP		= macpp
LLF		= llf
SUFFIX		= #r4kl
PREFIX		= #mipsel-linux-
CC		= $(PREFIX)gcc$(SUFFIX)
LD 		= $(PREFIX)ld$(SUFFIX)
ASM		= $(PREFIX)as$(SUFFIX)
AS		= $(PREFIX)as$(SUFFIX)
AR		= $(PREFIX)ar$(SUFFIX)
#LD 		= ld
#ASM		= as
#AS		= as
#AR		= ar
#OUTPUT_LANG 	= LANG_ASRxk
#CO		= co
ECHO = /bin/echo -e
BELL	= $(ECHO) "\007" > /dev/tty

SPATH = .
HPATH = .
MPATH = .

# Build a list of include paths from those defined in HPATH and MPATH.
# CINCS for gcc and PPINCS for macpp
CINCS	= -I$(subst :, -I,$(HPATH))
PPINCS	= -include=$(subst :, -include=,$(MPATH))

PPFLAGS		= $(PPINCS) -lis	# set to -lis if you also want listings with MACPP'd sources
ASM_PPFLAGS	= $(PPINCS) -line -lis	# ditto, but leave the -line if you want asxx to report the correct
ASFLAGS		= #-mips2 -KPIC #-non_shared #-32 -mips3 -adln #-adhln
POUT		= -out=

DBG = -g
OPT = #-O2

# Compile build mode
CMODE = -std=c99 #-m32 -mtune=generic
# Link mode
LMODE = #-m32

DEFS  = -D_LINUX_
DEFS += -D_LARGEFILE64_SOURCE
DEFS += -DBUILTIN_MEMCPY
DEFS += -DNO_TDFX_TESTS
DEFS += -DINCLUDE_SCOPE_LOOP
DEFS += -DINCLUDE_SCOPE_LOOP_NOINT
DEFS += -DNDEBUG
DEFS += -DAPOLLO_GUTS
#DEFS += -D__NO_LONG_LONG_DEFINES__
DEFS += -D_REENTRANT 
#DEFS += -D__NO_MATH_INLINES
#DEFS += -D_REENT_H_ 
#DEFS += -DNO_DISK_TEST
#DEFS += -DNO_FSYS_TEST
#DEFS += -DFSYS_NO_AUTOSYNC 

CFLAGS = $(DBG) $(OPT) $(CINCS) $(DEFS) $(CMODE) -Wall -ansi
PREFIX = 
SUFFIX =
CC = $(PREFIX)gcc$(SUFFIX)
LD = $(PREFIX)gcc$(SUFFIX)

LINKER_SCRIPT 	= 
LDFLAGS		= $(DBG) $(LMODE) 
LIBDIRS		= 
LIBS		=  

MAJOR_REV = 0
MINOR_REV = 1

# Point to files used by the default rules
DATE	  = sst_date.c

TARGETS = afsys #mkjourn fsysck sstat

default: $(TARGETS)

all: $(TARGETS) Makefile
	$(ECHO) "\tDone."

.SILENT:

define LD_RULE
	$(ECHO) "\tLinking to $@..."
	$(ECHO) "\tMaking $@..."
	$(LD) $(LDFLAGS) -o $@ \
		${LIBDIRS} \
		-Wl,-Map,$(basename $@).map \
		$(filter-out Makefile,$^) \
		$(LIBS)
endef

define CC_RULE
	$(ECHO) "\tCompiling $<..."
	$(CC) $(CFLAGS) -c $<
endef

%.o : %.c
	$(CC_RULE)

%.E : %.c
	$(CC) $(CFLAGS) -E $< > $@

clean:
	rm -rf $(TARGETS) $(SPECIALS) Debug Release *.o *.map *.lis libos.a

afsys: afsys.o afsys_dir.o h_hdparse.o libos.a Makefile
	$(LD_RULE)

mkjourn: mkjourn.o mkjourn_dir.o Makefile
	$(LD_RULE)

fsysck: fsysck.o fsysck_dir.o Makefile
	$(LD_RULE)

sstat : sstat.o
	$(LD_RULE)

CONFIG_FILES =  config.mac def_pp.mac constants.mac pptypes.mac 

config.h	: $(CONFIG_FILES) Makefile
	$(ECHO) "\tMaking $(@F)"
	rm -f $(basename $(@F)).err
	$(PP) $(POUT)$@ $(PPFLAGS) -assem="OUTPUT_LANG == LANG_C"  def_pp.mac $< > $(basename $(@F)).err 2>&1; \
		if [ -s $(basename $(@F)).err ];\
		then \
			$(ECHO) "Errors in $(basename $(@F)).err";\
		else\
			rm -f $(basename $(@F)).err; \
		fi

afsys.o : afsys.c h_hdparse.h config.h os_proto.h qio.h fsys.h nsprintf.h

h_hdparse.o : h_hdparse.c h_hdparse.h

afsys_dir.o: afsys_dir.c config.h os_proto.h qio.h fsys.h 

mkjourn.o : mkjourn.c config.h os_proto.h qio.h fsys.h nsprintf.h

mkjourn_dir.o: mkjourn_dir.c config.h os_proto.h qio.h fsys.h 

fsysck.o : fsysck.c
fsysck_dir.o : fsysck_dir.c
sstat.o : sstat.c

# libos build rules 

DEVICE_FILES = qio.o qio_errs.o fsys.o fsys_ide.o

BASE_H_FILES = config.h os_proto.h st_proto.h

LIB_FILES = $(DEVICE_FILES) 

libos.a : Makefile $(LIB_FILES) $(DATE)
	@rm -f os.err $(basename $@).err $$$$.err;\
	touch $$$$.err;\
	ls *.err | grep -v $$$$.err > /dev/null || true;\
	rm -f $$$$.err;\
	if [ ! -s *.err ];\
	then\
	  $(ECHO) "\tMaking $@...";\
	  $(CC) $(CFLAGS) -c -o sst_date.o -DMAJOR_REV=$(MAJOR_REV) -DMINOR_REV=$(MINOR_REV) $(DATE);\
	  rm -f $@;\
	  echo >  $$$$.ar "CREATE $@"; \
	  echo >> $$$$.ar "ADDMOD $(subst $(space),$(comma),$(filter-out Makefile, $(filter-out $(DATE),$(filter-out atl_root.o,$(filter-out $(XINU)/libxinu.a,$^)))))"; \
	  echo >> $$$$.ar "ADDMOD sst_date.o"; \
	  echo >> $$$$.ar "SAVE";\
	  echo >> $$$$.ar "END";\
	  $(AR) -M < $$$$.ar; \
	  rm -f $$$$.ar; \
	else\
	  $(ECHO) "$@ not made due to .err files";\
	  ls *.err;\
	  exit 1;\
	fi

fsys.o: fsys.c config.h lindefs.h os_proto.h i86_proto.h \
 st_proto.h qio.h reent.h \
 fsys.h
fsys_ide.o: fsys_ide.c config.h lindefs.h \
 os_proto.h st_proto.h \
 reent.h fsys.h i86_proto.h
qio.o: qio.c config.h lindefs.h \
 os_proto.h st_proto.h \
 i86_proto.h \
 qio.h \
 reent.h
qio_errs.o: qio_errs.c config.h lindefs.h \
 os_proto.h \
 qio.h \
 reent.h

