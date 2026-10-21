/* captureRejections over typed listeners — Node 24's lib/events.js
 * addCatch and emitUnhandledRejectionOrErr. Linked only by programs that
 * register promise-returning listeners and mention captureRejections (the
 * emitter, symbol and checked-dynamic units together).
 *
 * The compiled wrapper of such a listener awaits its promise when the
 * emitter captures rejections and hands a rejection here, with the
 * emitter, its checked-dynamic view, the event and the event's arguments.
 * The compiled side also reads the emitter's Symbol.for('nodejs.rejection')
 * member at the rejection. On the next tick that member, when it is a
 * function, is called with (err, event, ...args) and the emitter as
 * `this`. Otherwise 'error' is emitted with the rejection reason, with
 * capture off for that emit so that a rejecting 'error' listener does not
 * loop. A reason that is not an Error object is thrown as an unhandled
 * 'error' payload, as Node throws it when no 'error' listener exists. */
#include "scr_runtime.h"

#include <stdlib.h>

typedef struct ScrEcState {
  size_t rc;
  ScrEmitter *em; /* owned */
  ScrDyn *em_dyn;  /* owned */
  ScrDyn *handler; /* owned: ee[Symbol.for('nodejs.rejection')] */
  ScrDyn *err;     /* owned */
  ScrStr *name;   /* owned */
  ScrArr *args;   /* owned */
} ScrEcState;

static void *scr_ec_retain(void *p) {
  ((ScrEcState *)p)->rc++;
  return p;
}

static void scr_ec_release(void *p) {
  ScrEcState *st = p;
  if (--st->rc > 0) return;
  scr_emitter_release(st->em);
  scr_dyn_release(st->em_dyn);
  scr_dyn_release(st->handler);
  scr_dyn_release(st->err);
  scr_str_release(st->name);
  scr_arr_release(st->args);
  free(st);
}

/* emitUnhandledRejectionOrErr: caps[0] is the state. */
static void scr_ec_tick(ScrClosure *cb) {
  ScrEcState *st = scr_box_get_ref(cb->caps[0]); /* +1 */
  ScrDyn *handler = st->handler;
  if (handler->kind == SCR_DYN_FUNC) {
    size_t n = st->args->len;
    ScrDyn **argv = malloc((n + 2) * sizeof *argv);
    if (!argv) scr_trap("scriptc: out of memory in captureRejections\n");
    argv[0] = st->err;
    argv[1] = scr_dyn_new_str(st->name);
    for (size_t i = 0; i < n; i++) argv[i + 2] = (ScrDyn *)scr_arr_get_ref(st->args, i);
    scr_dyn_this_push_dyn(st->em_dyn);
    ScrDyn *r = scr_dyn_call(handler, argv, n + 2, "kRejection");
    scr_dyn_this_pop();
    if (r) scr_dyn_release(r);
    scr_dyn_release(argv[1]);
    for (size_t i = 0; i < n; i++) scr_dyn_release(argv[i + 2]);
    free(argv);
  } else {
    ScrError *e = scr_errdyn_err_of(st->err);
    bool prev = scr_emitter_capturing(st->em);
    scr_emitter_set_capturing(st->em, false);
    if (e != NULL) {
      ScrStr *error = scr_str_new("error", 5);
      scr_emitter_emit_error(st->em, error, e);
      scr_str_release(error);
      scr_error_release(e);
    } else {
      scr_dyn_throw(scr_dyn_retain(st->err));
    }
    scr_emitter_set_capturing(st->em, prev);
  }
  scr_ec_release(st);
}

/* Moves nothing in: every argument is borrowed and retained here. */
void scr_emitter_rejected(ScrEmitter *em, ScrDyn *em_dyn, ScrDyn *handler, ScrDyn *err, ScrStr *name,
                          ScrArr *args) {
  ScrEcState *st = calloc(1, sizeof *st);
  if (!st) scr_trap("scriptc: out of memory in captureRejections\n");
  st->rc = 1;
  st->em = scr_emitter_retain(em);
  st->em_dyn = scr_dyn_retain(em_dyn);
  st->handler = scr_dyn_retain(handler);
  st->err = scr_dyn_retain(err);
  st->name = scr_str_retain(name);
  st->args = scr_arr_retain(args);
  ScrClosure *cb = scr_closure_new((void *)&scr_ec_tick, 1);
  ScrBox *box = scr_box_new_obj(&scr_ec_retain, &scr_ec_release, NULL);
  scr_box_set_ref(box, st); /* the box takes the state's reference */
  cb->caps[0] = box;
  scr_next_tick(cb);
}
