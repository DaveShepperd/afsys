/* See LICENSE.txt for license details */

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <stddef.h>
#include "h_hdparse.h"

#if !defined(WATCOM_LIBC) || !WATCOM_LIBC
#define QIOmalloc(x) malloc(x)
#define QIOcalloc(x,y) calloc(x,y)
#define QIOrealloc(x,y) realloc(x,y)
#define QIOfree(x) free(x)
#endif
#ifndef FSYS_MAX_ALTS
# define FSYS_MAX_ALTS 3
#endif

/*************************************************************************************/
/* from ~farnham/oxl/io.{h,c} */

#define CHK_BUF(buf, siz, max) do { if ((siz) > (max)) { max = (siz) + 1024; buf = realloc(buf, max); \
		if (!buf) return 0; }} while (0)

/******************************************************************************
*
*       Function:       ExpandPathname
*
*       Description:    Expand ~'s and environment variables in a path.
*
*                       This routine was stolen from filemgr. I've (?author unknown?) made a
*                       couple of small changes to better handle "~user"
*                       at the start of a path.
*
*       Parameters:     path    Unexpanded path
*
*       Returns:        ptr to malloc'ed string containing expanded text.
*
******************************************************************************/
char *expandPathname(const char *opath) {
    const char *path, *path_end, *save_p;
    char *tmp, *p, *b_p;		/* String pointers */
    char *ans;				/* answer goes here */
    int ii, ans_size=0;			/* size of answer */
    struct passwd *pw;			/* Password file entry */

    path = opath;
    ans_size = strlen(path);
    if (!ans_size) return 0;		/* nothing to do */
    path_end = path+ans_size;		/* ptr to end of input */
    if (ans_size < 1024) ans_size = 1024; /* make it worthwhile */
    tmp = QIOmalloc(ans_size);		/* get some temporary memory */
    if (!tmp) return 0;			/* ran out of memory */
    if (*path == '~') {			/* name starts with tilda */
	path++;				/* eat it */
	if (*path && *path != '/') {	/* Perhaps somebody else's home directory */
	    if ((b_p = strchr(path, '/'))) {	/* skip to end of name */
		ii = b_p - path;
		strncpy(tmp+1, path, b_p-path);	/* copy the name to scratch ram */
		tmp[ii+1] = 0;		/* null terminate it */
	    } else {
		strcpy(tmp+1, path);
	    }
	    if ((pw = getpwnam(tmp+1))) {	/* lookup the name */
		ii = strlen(pw->pw_dir);	/* get the length of the directory */
		CHK_BUF(tmp, ii, ans_size);
		strcpy(tmp, pw->pw_dir);	/* copy in the name */
	    } else {
		tmp[0] = '~';			/* else put the tilda in ahead of name */
	    }

	    if (!b_p) {
		ii = strlen(tmp);	/* get length of string */
		ans = QIOmalloc(ii+1);	/* get a new string exactly as long as we need */
		strcpy(ans, tmp);	/* copy it */
		QIOfree(tmp);		/* done with this buffer */
		return ans;		/* hand back what we computed */
	    }
	    path = b_p;			/* advance pointer to '/' */
	} else {
	    p = getenv("HOME");
	    if (p) {
		ii = strlen(p);
		CHK_BUF(tmp, ii, ans_size);
		strcpy(tmp, p);
	    } else {
		tmp[0] = '~';		/* no home directory */
		tmp[1] = 0;
	    }
	}
	b_p = tmp + strlen(tmp);
    } else {
	b_p = tmp;
    }

    while (*path) {
	if (*path == '$') {
	    int len, pos;
	    /* Expand environment variable */
	    ++path;
	    save_p = path;
	    p = b_p;
	    while ((isalnum(*path) || *path == '_')) *p++ = *path++;
	    *p = 0;
	    if ((p = getenv(b_p))) {
		len = strlen(p);
		pos = b_p - tmp;
		ii = path_end - path;
		CHK_BUF(tmp, pos+len+ii, ans_size);
		b_p = tmp + pos;
		strcpy(b_p, p);
		b_p += len;
	    } else {
		path = save_p;
		*b_p++ = '$';
	    }
	} else {
	    *b_p++ = *path++;
	}
    }
    *b_p++ = 0;
    ii = b_p - tmp;
    ans = QIOmalloc(ii);
    if (ans) strcpy(ans, tmp);
    QIOfree(tmp);
    return ans;
}

/*************************************************************************************/
static char *lclbuf;
static int lclbuf_size;
static ParseUnion *defaults;
int input_line_no;
int parse_errors;
extern char *image_name;
extern int debug_level;

static char *eat_ws(char *ptr) {
    while (isspace(*ptr)) ++ptr;
    return ptr;
}

static int getline(FILE *ifp) {
    int sts;
    char *s, *bp;

    if (!lclbuf) {
	lclbuf_size = 10240;
	lclbuf = QIOmalloc(lclbuf_size);		/* get some memory */
	if (!lclbuf) return PARSE_ERR_NOMEM;	/* ran out of memory */
    }
    bp = lclbuf;
    sts = 0;					/* keep gcc quiet */
    while (1) {
	s = fgets(bp, lclbuf_size - (bp-lclbuf), ifp);
	if (!s) {
	    sts = feof(ifp) ? PARSE_ERR_EOF : PARSE_ERR_IOERR;
	    break;
	}
	++input_line_no;			/* count the line */
	sts = strlen(lclbuf);			/* how long is our string? */
	if (sts < lclbuf_size-1 || lclbuf[sts-1] == '\n') {		/* it fit completely within the allocated buffer */
	    break;				/* return length of string */
	}
	sts = lclbuf_size;			/* remember where the end of buffer is */
	lclbuf_size += 10240;			/* get more memory */
	lclbuf = QIOrealloc(lclbuf, lclbuf_size);
	if (!lclbuf) return PARSE_ERR_NOMEM;	/* out of memory */
	bp = lclbuf + sts;			/* new position in buffer */
    }
    if (sts > 1 && lclbuf[sts-1] == '\n') {
	--sts;
	lclbuf[sts] = 0;			/* stomp on trailing \n */
    }
    return sts;					/* return error code or length of buffer */
}

typedef struct key_str {
    const char *name;
    off_t offset;
    int type;
} KeyStr;

static const KeyStr *sindex(const char *str, const KeyStr *keys) {
    int len;
    len = strlen(str);
    while (keys->name) {
	if (strncmp(str, keys->name, len) == 0) return keys;
	++keys;
    }
    return 0;
}

#define KEY_STR		0		/* plain string */
#define KEY_FNAME	1		/* filename string */
#define KEY_NUMB	2		/* number */
#define KEY_DATE	3		/* date/time string */

static const KeyStr vol_key[] = {
    { "PHYSICAL", offsetof(ParseVol, phys), KEY_STR },
    { "VIRTUAL", offsetof(ParseVol, virt), KEY_STR },
    { "CLUSTER_SIZE", offsetof(ParseVol, cluster), KEY_NUMB },
    { "INDEX_SIZE", offsetof(ParseVol, index_sectors), KEY_NUMB },
    { "FREE_SIZE", offsetof(ParseVol, free_sectors), KEY_NUMB },
    { "ROOT_SIZE", offsetof(ParseVol, root_sectors), KEY_NUMB },
    { "JOURNAL_SIZE", offsetof(ParseVol, jou_sectors), KEY_NUMB },
    { "EXTEND_SIZE", offsetof(ParseVol, def_extend), KEY_NUMB },
    { "MAX_LBA", offsetof(ParseVol, max_lba), KEY_NUMB },
    { "HB_RANGE", offsetof(ParseVol, hb_range), KEY_NUMB },
    { 0, 0, 0}
};

static const KeyStr dir_key[] = {
    { "NAME", offsetof(ParseFile, game_name), KEY_FNAME },
    { "GAME_NAME", offsetof(ParseFile, game_name), KEY_FNAME },
    { "ALLOCATION", offsetof(ParseFile, alloc), KEY_NUMB },
    { "CTIME", offsetof(ParseFile, ctime), KEY_DATE },
    { 0, 0, 0}
};

static const KeyStr cs_key[] = {
    { "GAME_NAME", offsetof(ParseFile, game_name), KEY_FNAME },
    { "COPIES", offsetof(ParseFile, copies), KEY_NUMB },
    { 0, 0, 0}
};

static const KeyStr file_key[] = {
    { "UNIX_NAME", offsetof(ParseFile, unix_name), KEY_FNAME },
    { "DOS_NAME", offsetof(ParseFile, unix_name), KEY_FNAME },
    { "GAME_NAME", offsetof(ParseFile, game_name), KEY_FNAME },
    { "ALLOCATION", offsetof(ParseFile, alloc), KEY_NUMB },
    { "EOF", offsetof(ParseFile, eof), KEY_NUMB },
    { "PLACEMENT", offsetof(ParseFile, placement), KEY_NUMB },
    { "COPIES", offsetof(ParseFile, copies), KEY_NUMB },
    { "CTIME", offsetof(ParseFile, ctime), KEY_DATE },
    { "FID_NAME", offsetof(ParseFile, fid_name), KEY_FNAME },
    { "NO_CHECKSUM", offsetof(ParseFile, nocksum), KEY_NUMB },
    { 0, 0, 0}
};

static const KeyStr part_key[] = {
    { "INDEX", offsetof(ParsePartition, index), KEY_NUMB },
    { "TYPE", offsetof(ParsePartition, ptype), KEY_NUMB },
    { "STATUS", offsetof(ParsePartition, status), KEY_NUMB },
    { "START", offsetof(ParsePartition, start_lba), KEY_NUMB },
    { "COUNT", offsetof(ParsePartition, num_lbas), KEY_NUMB },
    { "SIZE", offsetof(ParsePartition, size), KEY_NUMB },
    { "END", offsetof(ParsePartition, end_lba), KEY_NUMB },
    { 0, 0, 0}
};

static const KeyStr part_pipe[] = {
    { "COMMAND", offsetof(ParsePipe, command), KEY_STR },
    { "OUTPUT", offsetof(ParsePipe, output), KEY_STR },
    { 0, 0, 0}
};

static char *get_keyword(char **src, int *len, const char *cmd) {
    char c, *s, *tmp, *str;

    str = eat_ws(*src);		/* skip leading ws */
    s = strchr(str, '=');	/* find delimiter */
    tmp = str;
    if (!s) {			/* there must be an = */
	while (*tmp && !isspace(*tmp)) ++tmp;	/* skip to end of keyword */
	c = *tmp;
	*tmp = 0;
	fprintf(stderr, "%s:%d - %s cmd: unknown keyword \"%s\"\n", image_name, input_line_no, cmd, str);
	*tmp = c;
	return 0;
    }
    while (tmp < s && !isspace(*tmp)) {		/* skip to end of keyword */
	if (islower(*tmp)) *tmp = toupper(*tmp); /* while upcasing it */
	++tmp;
    }
    if ((tmp - str) <= 0) {
	fprintf(stderr, "%s:%d - %s cmd: No keyword present before '=' sign\n",
		image_name, input_line_no, cmd);
	return 0;
    }
    *len = tmp - str;
    *src = s + 1;		/* return ptr to char just after '=' */
    return str;
}

static int do_string(char **src, const KeyStr *keys, ParseUnion *pu, const char *cmd) {
    int len;
    char c, *s, *tmp, *new, **trgt;
    
    s = eat_ws(*src);		/* skip leading ws */
    tmp = s;
    while (*tmp && !isspace(*tmp)) ++tmp; /* skip over argument */
    c = *tmp;			/* save old char */
    *tmp = 0;			/* null terminate arg */
    len = tmp - s;		/* compute length of string */
    if (!len) {
	fprintf(stderr, "%s:%d - %s cmd: no argument provided for keyword \"%s\"\n", image_name, input_line_no, cmd, keys->name);
	return -1;
    }
    if (keys->type) {		/* if the string is a filename */
	new = expandPathname(s); /* expand the pathname into a malloc'd space */
    } else {
	new = QIOmalloc(len+1);	/* else malloc a new place for a copy of his */
	if (!new) {
	    fprintf(stderr, "%s:%d - %s cmd: ran out of memory processing \"%s\"\n", image_name, input_line_no, cmd, keys->name);
	    exit(1);
	}
	strcpy(new, s);		/* and make a copy of caller's string */
    }
    *tmp = c;			/* put trailing char back */
    trgt = (char **)((char *)pu + keys->offset);
    *trgt = new;
    *src = tmp;
    return 0;
}

static int do_integer(char **src, const KeyStr *keys, ParseUnion *pu, const char *cmd) {
    long ans, *ip;
    int ok=0;
    char *numb, *str, *s, c;

    str = eat_ws(*src);
    ans = strtol(str, &numb, 0);	/* get a number */
    if (str == numb || (*numb && !isspace(*numb))) {	/* if we didn't stop on ws */
	c = numb[1];
	if (!c || isspace(c)) {		/* maybe followed by a K or M or G */
	    c = *numb;
	    if (islower(c)) c = toupper(c);
	    if (c == 'G') {
		if (ans >= 4) {
		    ans = 0xFFFFFFFF;
		} else {
		    ans <<= 30;
		}
		ok = 1;
	    } else if (c == 'M') {
		ans <<= 20;
		ok = 1;
	    } else if (c == 'K') {
		ans <<= 10;
		ok = 1;
	    }
	}
	if (!ok) {
	    s = str;
	    while (*s && !isspace(*s)) ++s;
	    c = *s;
	    *s = 0;
	    fprintf(stderr, "%s:%d - %s cmd: illegal number (%s) as argument to \"%s\"\n", 
			    image_name, input_line_no, cmd, str, keys->name);
	    *s = c;
	    return -1;
	} else {
	    ++numb;		/* eat the K, M or G */
	}
    }
    ip = (long *)((char *)pu + keys->offset);
    *ip = ans;
    *src = numb;
    return 0;
}

static int do_date(char **src, const KeyStr *keys, ParseUnion *pu, const char *cmd) {
    fprintf(stderr, "%s:%d - DATE type argument not supported yet\n", image_name, input_line_no);
    return -1;
}

static int do_generic(char *src, ParseUnion *pu, const KeyStr *keys, const char *cmd) {
    int sts, len;
    char *end, *s, c;
    const KeyStr *found_key;
    
    end = src + strlen(src);
    while (src < end) {
	src = eat_ws(src);		/* skip white space */
	if (!*src || *src == '#' || *src == '!') break; /* null or comment ends the line */
	s = get_keyword(&src, &len, cmd);
	if (!s) return 1;		/* syntax error */
	c = s[len];
	s[len] = 0;			/* null terminate the keyword */
	found_key = sindex(s, keys);	/* find the key */
	if (!found_key) {			/* if no key */
	    fprintf(stderr, "%s:%d - %s cmd: unknown keyword \"%s\"\n", image_name, input_line_no, cmd, s);
	    s[len] = c;			/* put back the char */
	    return 1;
	}
	s[len] = c;			/* put back the char */
	switch (found_key->type) {
	    case KEY_FNAME:
	    case KEY_STR:	sts = do_string(&src, found_key, pu, cmd); break;
	    case KEY_NUMB:	sts = do_integer(&src, found_key, pu, cmd); break;
	    case KEY_DATE:	sts = do_date(&src, found_key, pu, cmd); break;
	    default:
		fprintf(stderr, "%s:%d - fatal internal error processing %s command\n", image_name, input_line_no, found_key->name);
		exit(1);
	}
	if (sts < 0) return 1;		/* syntax error */
    }
    return 0;
}

static int apply_defaults(ParseUnion *pu) {
    int sts;
    char *new;
    if (pu->f.unix_name && defaults->f.unix_name && (pu->f.unix_name[0] != '/' && pu->f.unix_name[0] != '\\')) {
	sts = strlen(pu->f.unix_name);
	new = QIOmalloc(sts + 1 + defaults->f.alloc + 1);
	if (!new) return PARSE_ERR_NOMEM;
	sts = defaults->f.alloc;
	strcpy(new, defaults->f.unix_name);
#if defined(WATCOM_LIBC) && WATCOM_LIBC
	new[sts] = '\\';
#else
	new[sts] = '/';
#endif
	strcpy(new + sts + 1, pu->f.unix_name);
	if (pu->f.unix_name != pu->f.game_name) QIOfree(pu->f.unix_name);
	pu->f.unix_name = new;
    }						
    if (pu->f.game_name && defaults->f.game_name && (pu->f.game_name[0] != '/' && pu->f.game_name[0] != '\\')) {
	sts = strlen(pu->f.game_name);
	new = QIOmalloc(sts + 1 + defaults->f.eof + 1);
	if (!new) return PARSE_ERR_NOMEM;
	sts = defaults->f.eof;
	strcpy(new, defaults->f.game_name);
#if defined(WATCOM_LIBC) && WATCOM_LIBC
	new[sts] = '\\';
#else
	new[sts] = '/';
#endif
	strcpy(new + sts + 1, pu->f.game_name);
	if (pu->f.unix_name != pu->f.game_name) QIOfree(pu->f.game_name);
	pu->f.game_name = new;
    }						
    if (!pu->f.copies) pu->f.copies = defaults->f.copies;
    return 0;
}
    
static int do_volume(char *src, ParseUnion *pu) {
    int sts;
    sts = do_generic(src, pu, vol_key, "VOLUME");
    if (sts) return 1;
    if ((debug_level&1)) {
	printf("Line %4d: VOLUME: type=%d, cluster=%d, index=%d, free=%d, root=%d, extend=%d\n",
	    input_line_no, pu->v.type, pu->v.cluster, pu->v.index_sectors, pu->v.free_sectors, pu->v.root_sectors, pu->v.def_extend);
	printf("phys=%s, virt=%s\n", pu->v.phys, pu->v.virt);
    }
    return 0;
}

static int do_directory(char *str, ParseUnion *pu) {
    int sts;
    sts = do_generic(str, pu, dir_key, "DIRECTORY");
    if (sts) return 1;
    if (!pu->f.game_name) {
	fprintf(stderr, "%s:%d - GAME_NAME required on DIRECTORY command\n", image_name, input_line_no);
	return 1;
    }
    if (defaults) {
	sts = apply_defaults(pu);
	if (sts) return sts;
    }
    if ((debug_level&1)) {
	printf("Line %4d: DIRECTORY: type=%d, alloc=%ld, eof=%ld, place=%d, copies=%d, mkdir=%d\n",
	    input_line_no, pu->f.type, pu->f.alloc, pu->f.eof, pu->f.placement, pu->f.copies, pu->f.mkdir);
	printf("\tunix_name=%s,\n\tgame_name=%s,\n\tfid_name=%s\n", pu->f.unix_name, pu->f.game_name, pu->f.fid_name);
    }
    return 0;
}

static int do_file(char *str, ParseUnion *pu, char *cmdname) {
    int sts;
    sts = do_generic(str, pu, file_key, cmdname);
    if (sts) return 1;
    if (!pu->f.game_name && !pu->f.fid_name) {
	if (!pu->f.unix_name) {
	    fprintf(stderr, "%s:%d - GAME_NAME and/or FID_NAME required on %s command\n",
    			image_name, input_line_no, cmdname);
	    return 1;
	} 
	pu->f.game_name = pu->f.unix_name;	/* default game name to same as Unix name */
    }
    if (defaults) {
	sts = apply_defaults(pu);
	if (sts) return sts;
    }
    if ((debug_level&1)) {
	printf("Line %4d: %s: type=%d, alloc=%ld, eof=%ld, place=%d, copies=%d, mkdir=%d\n",
	    input_line_no, cmdname, pu->f.type, pu->f.alloc, pu->f.eof, pu->f.placement, pu->f.copies, pu->f.mkdir);
	printf("\tunix_name=%s,\n\tgame_name=%s,\n\tfid_name=%s\n", pu->f.unix_name, pu->f.game_name, pu->f.fid_name);
    }
    return 0;
}

static int do_partition(char *str, ParseUnion *pu, char *cmdname) {
    int sts;
    sts = do_generic(str, pu, part_key, cmdname);
    if (sts) return 1;
    if (pu->p.size) ++sts;
    if (pu->p.num_lbas) ++sts;
    if (pu->p.end_lba) ++sts;
    if (sts > 1) {
	fprintf(stderr, "%s:%d - Can have only one of COUNT, SIZE or END on %s command\n",
	    image_name, input_line_no, cmdname);
	return 1;
    }
    if (pu->p.size) {
	pu->p.num_lbas = pu->p.size >> 9;
	pu->p.size = 0;
    }
    if (pu->p.end_lba) {
	if (pu->p.end_lba < pu->p.start_lba) {
	    printf("%s:%d - END parameter less than START parameter on %s command\n",
		image_name, input_line_no, cmdname);
	    return 1;
	}
	pu->p.num_lbas = pu->p.end_lba - pu->p.start_lba + 1;
	pu->p.end_lba = 0;
    }
    if ((debug_level&1)) {
	printf("Line %4d: %s: type=%d, index=%d, ptype=0x%02X, status=0x%02X, start_lba=0x%08lX, num_lbas=0x%08lX\n",
	    input_line_no, cmdname, pu->p.type, pu->p.index, pu->p.ptype, pu->p.status,
    		pu->p.start_lba, pu->p.num_lbas);
    }
    return 0;
}

static int do_pipe(char *str, ParseUnion *pu, char *cmdname) {
    int sts;
    sts = do_generic(str, pu, part_pipe, cmdname);
    if (sts) return 1;
    if ((debug_level&1)) {
	printf("Line %4d: %s: type=%d, command=%s, output=%s\n", input_line_no, cmdname,
    		pu->pipe.type, pu->pipe.command, pu->pipe.output);
    }
    return 0;
}

static int do_defaults(char *str, ParseUnion *pu) {
    int sts;
    sts = do_generic(str, pu, file_key, "DEFAULT");
    if (sts) return 1;
    if (pu->f.unix_name) pu->f.alloc = strlen(pu->f.unix_name);
    if (pu->f.game_name) pu->f.eof = strlen(pu->f.game_name);
    if (!pu->f.copies) pu->f.copies = defaults->f.copies;
    if ((debug_level&1)) {
	printf("Line %4d: DEFAULTS: type=%d, alloc=%ld, eof=%ld, copies=%d\n",
    		input_line_no, pu->f.type, pu->f.alloc, pu->f.eof, pu->f.copies);
	printf("\tunix_name=%s,\n\tgame_name=%s\n", pu->f.unix_name, pu->f.game_name);
    }
    return 0;
}

static int do_cs(char *str, ParseUnion *pu) {
    int sts;
    sts = do_generic(str, pu, cs_key, "CHECKSUM");
    if (sts) return 1;
    if (pu->f.game_name) pu->f.eof = strlen(pu->f.game_name);
    if (defaults) {
	sts = apply_defaults(pu);
	if (sts) return sts;
    }
    if ((debug_level&1)) {
	printf("Line %4d: CHECKSUM: type=%d, eof=%ld, game_name=%s\n",
    		input_line_no, pu->f.type, pu->f.eof, pu->f.game_name);
    }
    return 0;
}

static int do_delete(char *str, ParseUnion *pu) {
    int sts;
    sts = do_generic(str, pu, cs_key, "DELETE");
    if (sts) return 1;
    if (pu->f.game_name) pu->f.eof = strlen(pu->f.game_name);
    if (defaults) {
	sts = apply_defaults(pu);
	if (sts) return sts;
    }
    if ((debug_level&1)) {
	printf("Line %4d: DELETE: type=%d, eof=%ld, game_name=%s\n",
    		input_line_no, pu->f.type, pu->f.eof, pu->f.game_name);
    }
    return 0;
}

extern int default_copies;

static int parse_line(char *line, ParseUnion **ans) {
    ParseUnion *pu;
    char *s, *bp;
    int sts;

    pu = *ans;
    s = eat_ws(line);			/* skip white space */
    if (!*s || *s == '#' || *s == '!' || *s == '*') {
	return PARSE_RESCAN; 		/* eat commented lines */
    }
    bp = s;
    while (*bp && !isspace(*bp)) {
	if (islower(*bp)) *bp = toupper(*bp);
	++bp;
    }
    *bp = 0;
    sts = bp - s;
    bp = eat_ws(bp+1);
    if (strncmp(s, "SYNC", sts) == 0) {
	pu->type = PARSE_TYPE_SYNC;
	return 0;
    }
    if (strncmp(s, "EOF", sts) == 0) {
	pu->type = PARSE_TYPE_EOF;
	return 0;
    }
    if (strncmp(s, "VOLUME", sts) == 0) {
	pu->type = PARSE_TYPE_VOL;
	sts = do_volume(bp, pu);
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (strncmp(s, "DIRECTORY", sts) == 0) {
	pu->type = PARSE_TYPE_FILE;
	pu->f.mkdir = 1;
	sts = do_directory(bp, pu);
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (strncmp(s, "DEFAULT", sts) == 0) {
	pu->type = PARSE_TYPE_DEFAULT;
	sts = do_defaults(bp, pu);
	if (sts) {
	    ++parse_errors;
	    return PARSE_RESCAN;
	}
	return 0;					/* no error */
    }
    if (strncmp(s, "FILE", sts) == 0) {
	pu->type = PARSE_TYPE_FILE;
	pu->f.mkdir = 0;
	sts = do_file(bp, pu, "FILE");
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (strncmp(s, "PARTITION", sts) == 0) {
	pu->type = PARSE_TYPE_PART;
	sts = do_partition(bp, pu, "PARTITION");
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (strncmp(s, "PIPE", sts) == 0) {
	pu->type = PARSE_TYPE_PIPE;
	sts = do_pipe(bp, pu, "PIPE");
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (sts <= 5 && strncmp(s, "BOOT", sts > 4 ? 4 : sts) == 0) {
	int ok=0;
	if (sts <= 4) {
	    pu->type = PARSE_TYPE_BOOT0;
	    ok = 1;
	} else {
	    int c = s[4] - '0';
	    if (c >= 0 && c <= 3) {
		int cmds[] = { PARSE_TYPE_BOOT0, PARSE_TYPE_BOOT1, PARSE_TYPE_BOOT2, PARSE_TYPE_BOOT3 };
		ok = 1;
		pu->type = cmds[c];
	    }
	}
	if (ok) {
	    pu->f.mkdir = 0;
	    sts = do_file(bp, pu, "BOOT");
	    if (!sts) {
		if (pu->f.copies < FSYS_MAX_ALTS) {
		    pu->f.copies = FSYS_MAX_ALTS;	/* boot always gets 3 copies */
		}
		return 0;
	    }
	    ++parse_errors;
	    return PARSE_RESCAN;
	}
    }
    if (strncmp(s, "CHECKSUM", sts) == 0) {
	pu->type = PARSE_TYPE_CS;
	pu->f.mkdir = 0;
	sts = do_cs(bp, pu);
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    if (strncmp(s, "DELETE", sts) == 0) {
	pu->type = PARSE_TYPE_DEL;
	pu->f.mkdir = 0;
	sts = do_delete(bp, pu);
	if (!sts) return 0;
	++parse_errors;
	return PARSE_RESCAN;
    }
    fprintf(stderr, "hd_parse:%d unrecognized command word \"%s\"\n", input_line_no, s);
    ++parse_errors;
    return PARSE_RESCAN;
}

static void fixup_defaults(ParseUnion *pu) {
    if (defaults) {
	if (!pu->f.unix_name) {
	    pu->f.unix_name = defaults->f.unix_name;
	    pu->f.alloc = defaults->f.alloc;
	} else if (defaults->f.unix_name) {
	    QIOfree(defaults->f.unix_name);
	}
	if (!pu->f.game_name) {
	    pu->f.game_name = defaults->f.game_name;
	    pu->f.eof = defaults->f.eof;
	} else if (defaults->f.game_name) {
	    QIOfree(defaults->f.game_name);
	}
	QIOfree(defaults);
    }
    defaults = pu;
}

int parse_it(FILE *ifp, ParseUnion **ans) {
    ParseUnion *pu;
    int sts;

    if (!ifp || !ans) return PARSE_ERR_INVARG;	/* invalid argument */
    if (!defaults) {
	defaults = (ParseUnion *)QIOcalloc(1, sizeof(ParseUnion));
	if (!defaults) return PARSE_ERR_NOMEM;
	defaults->f.copies = default_copies;
    }
    *ans = pu = (ParseUnion *)QIOcalloc(1, sizeof(ParseUnion));	/* get some memory */
    if (!pu) {
	QIOfree(defaults);
	defaults = 0;
	return PARSE_ERR_NOMEM;		/* out of memory */
    }
    while (1) {
	sts = getline(ifp);			/* get the next line of input */
	if (sts < 0) {
	    QIOfree(pu);			/* don't use this memory */
	    QIOfree(defaults);
	    defaults = 0;			/* done */
	    *ans = 0;				/* nothing to return */
	    break;
	}
	if (sts == 0) continue;			/* eat blank lines */
	sts = parse_line(lclbuf, ans);
	if (sts) {
	    if (sts == PARSE_RESCAN) continue;	/* loop locally */
	} else {
	    if (pu->type == PARSE_TYPE_DEFAULT) {	/* keep defaults local */
		fixup_defaults(pu);			/* record these */
		*ans = pu = (ParseUnion *)QIOcalloc(1, sizeof(ParseUnion));	/* get another struct */
		if (!pu) return PARSE_ERR_NOMEM;		/* out of memory */
		continue;
	    }
	}
	break;
    }
    return sts;
}

int parse_command(const char *cmd, ParseUnion **ans) {
    ParseUnion *pu;
    int sts;

    input_line_no++;
    if (!cmd || !ans) return PARSE_ERR_INVARG;	/* invalid argument */
    if (!defaults) {
	defaults = (ParseUnion *)QIOcalloc(1, sizeof(ParseUnion));
	if (!defaults) return PARSE_ERR_NOMEM;
	defaults->f.copies = default_copies;
    }
    *ans = pu = (ParseUnion *)QIOcalloc(1, sizeof(ParseUnion));	/* get some memory */
    if (!pu) {
	QIOfree(defaults);
	defaults = 0;
	return PARSE_ERR_NOMEM;		/* out of memory */
    }
    sts = strlen(cmd)+1;
    if (sts > lclbuf_size) {
	lclbuf_size += sts+10240;
	if (lclbuf) QIOfree(lclbuf);
	lclbuf = QIOmalloc(lclbuf_size);
	if (!lclbuf) return PARSE_ERR_NOMEM;
    }
    strcpy(lclbuf, cmd);
    sts = parse_line(lclbuf, ans);
    if (!sts) {
	if (pu->type == PARSE_TYPE_DEFAULT) {	/* keep defaults local */
	    fixup_defaults(pu);			/* record these */
	}
    }
    return sts;
}

