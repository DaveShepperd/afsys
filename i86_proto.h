/* See LICENSE.txt for license details */

#if !defined(_I86_PROTO_H_)
# define _I86_PROTO_H_ 1
#if __linux__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* $Id$ */

/*
**  getenv_hook(): Get and set the st_getenv "hook"
**
**  At entry:
**	new - pointer to struct st_envar. The search for the variable actually begins
**		at the struct which the _next_ member points to. I.e., search begins
**		at new->next rather than at new.
**		(NULL is a legit value to use for this parameter).
**  At exit:
**	returns previous value of hook.
*/
struct st_envar; 
const struct st_envar *getenv_hook(const struct st_envar *__new);

extern int ide_init(void);

#ifndef MAX_AST_QUEUES
# define MAX_AST_QUEUES 1
#endif

#if MAX_AST_QUEUES
extern int prc_get_astlvl(void);	/* get the current AST level (-1 = not at ast level) */

/*		prc_q_ast(level, new)
 *	Adds an element to the "AST queue" and launches an AST immediately if
 *	not already executing at AST level. 'level' is the AST level at which
 *	to run (from 0 to MAX_AST_QUEUES). The higher the level, the lower
 *	the priority. Level 0 is the highest priority just below 'actions'.
 *	AST functions will never interrupt one another and, as with actions,
 *	are executed in the order they appear in the queue. The queues
 *	are serviced according to the level. Higher level AST's will not be
 *	executed until all the lower level queues are empty. An AST may
 *	queue another AST, but the new one won't begin executing until the
 *	current AST exits (priority permitting).
 *
 *	IRQ action functions (those queued by prc_q_action()) will interrupt
 *	and execute ahead of an AST at any level.
 *
 *	The parameter 'new' is a pointer to a struct act_q which must be
 *	allocated by the user and "live" from the time of its creation to
 *	the time the specified action is called. If the element is already on
 *	some other queue, it will not be added to the AST queue. Returns 0
 *	if action successfully queued, 1 if level is out of range (too small),
 *	2 if level is out of range (too large) or 3 if 'new' is already
 *	present on some other queue.
 *	
 */
extern int prc_q_ast(int level, struct act_q *__new);
#endif

#if defined(MAX_FG_QUEUES) && MAX_FG_QUEUES
extern int prc_get_fglvl(void);	 /* get the current FG level (-1 = not at FG level) */

/*		prc_q_fg(level, new)
 *	Adds an element to the "FG (foreground) queue" and launches the task
 *	immediately if not already executing at FG. 'level' is the FG level at
 *	which to run (from 0 to MAX_FG_QUEUES). The higher the level, the lower
 *	the priority. Level 0 is the highest priority just below 'AST'.
 *	FG functions will never interrupt one another and, as with actions,
 *	are executed in the order they appear in the queue. The queues
 *	are serviced according to the level. Higher level FG's will not be
 *	executed until all the lower level queues are empty. An FG may
 *	queue another FG, but the new one won't begin executing until the
 *	current FG exits (priority permitting).
 *
 *	IRQ action functions (those queued by prc_q_action()) and AST functions
 *	(those queued by prc_q_ast()) will interrupt and execute ahead of an
 *	FG at any level.
 *
 *	The parameter 'new' is a pointer to a struct act_q which must be
 *	allocated by the user and "live" from the time of its creation to
 *	the time the specified action is called. If the element is already on
 *	some other queue, it will not be added to the FG queue. Returns 0
 *	if action successfully queued, 1 if level is out of range (too small),
 *	2 if level is out of range (too large) or 3 if 'new' is already
 *	present on some other queue.
 *	
 */
extern int prc_q_fg(int level, struct act_q *__new);
#endif

#ifdef __cplusplus
}
#endif      /* __cplusplus */
#endif      /* __linux__ */
#endif		/* _I86_PROTO_H_ */
