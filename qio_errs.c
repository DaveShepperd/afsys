/* See LICENSE.txt for license details */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE (1)
#endif

#include <config.h>
#include <os_proto.h>
#include <string.h>
#include <qio.h>
# include <stdio.h>

#ifndef n_elts
# define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

typedef struct errmsg {
    int sts;
    const char *msg;
} ErrMsg;

static const ErrMsg errmsgs[] = {
#define QIO_ERR_MSG(name, init, text) { name, text }
#include <qio.h>
#undef QIO_ERR_MSG
    { 0, 0 }
};

/* NOTE: The order of the following must match exactly the
 * order as listed in the enum error_codes found in qio.h.
 * If not, then the facility name listed in message won't match
 * the faciltiy that actually generated the code .
 */

static const char * const facility[] = {
    0,
#define QIO_FACILITY_MSG(name,text) text
#include <qio.h>
#undef QIO_FACILITY_MSG
    0
};

static const char * const sev_msg[] = {
    " error, ",
    " fyi, ",
    " fatal, ",
    " warning, "
};

int qio_errmsg(int sts, char *ans, int size) {
    char *s;
    int ii, fac, sev, len;

    if (size && ans) {
#if !HOST_BOARD
	char tmp[80];
#endif
	ans[size-1] = 0;
	--size;
	sts &= (1<<QIOSTAT_SHIFT)-1;
	fac = sts>>FACILITY_SHIFT;
	sev = (sts>>SEVERITY_SHIFT)&3;
	sts &= ~(3<<SEVERITY_SHIFT);
	if (fac > 0 && fac < n_elts(facility)-1) {
	    strncpy(ans, facility[fac], size);
        } else {
	    if (fac == 0) {
		s = strerror(sts);
		if (s && strlen(s) > 0) {
		    return snprintf(ans, size, "STDIO error, %s", s);
		}
	    }
	    strcpy(ans, "???");
	}
	len = strlen(ans);
	s = ans + len;
	size -= len;
	if (size > 0) {
	    strncpy(s, sev_msg[sev], size);
	    len = strlen(s);
	    size -= len;
	    s += len;
	}
	if (size > 0) {
	    if (sts) {
		for (ii=0; ii < n_elts(errmsgs); ++ii) {
		    if (sts == errmsgs[ii].sts) {
			strncpy(s, errmsgs[ii].msg, size);
			return strlen(ans);
		    }
		}
	    }
	    snprintf(s, size, "Unknown status of 0x%08X", sts);
	}
    }
    return ans ? strlen(ans) : 0;
}


