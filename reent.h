/* See LICENSE.txt for license details */

#ifndef _REENT_H_
#define _REENT_H_ 1

struct _stdio {
    int dummy;
};

struct _reent {
    int _errno;
    int _next;
    void *_nextf[8];
    struct _stdio *_stdin;
    struct _stdio *_stdout;
    struct _stdio *_stderr;
    struct _stdio __sf[3];
};

#endif
