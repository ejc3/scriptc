/* events.on(emitter, name[, { signal, close }]) over typed listeners — Node
 * 24's lib/events.js `on`, consumed by a `for await` loop. Linked only by
 * programs that iterate it (the emitter and event-target units together).
 *
 * The loop drives the iterator through a handle: scr_events_on_open
 * validates the signal and registers the event's listener and (unless the
 * event is 'error') an 'error' one, scr_events_on_close registers one
 * listener per close event, and scr_events_on_arm adds the signal's
 * 'abort' listener, in Node's order. scr_events_on_next is the iterator's
 * next(): queued events first, then a stored error, then done, else a
 * pending promise the next event fulfills. A done result fulfills with an
 * empty array and sets the flag scr_events_on_done reads after the loop's
 * await. scr_events_on_return is return(). An 'error' event or the abort
 * rejects the waiting promise (or is stored for the next call), and then,
 * like a close event or return(), the close handler runs: it removes the
 * abort listener and the emitter listeners, last added first, and settles
 * every waiting promise as done.
 *
 * Ownership: the handle table owns each state until the loop is done with
 * it (a done result read, an error delivered, or return()). The state owns
 * the emitter while its listeners are registered, its name, the signal,
 * the queued argument arrays, the waiting promises and a stored error. The
 * listener closures reach the state through link objects; a link clears
 * the state's pointer to its closure when that closure is freed, so
 * removals never compare a stale identity. Iterators still open at exit
 * are released without removal events. */
#include "scr_runtime.h"

#include <stdlib.h>
#include <string.h>

typedef struct ScrEonClose {
  ScrStr *name;   /* owned */
  ScrClosure *cb; /* weak, NULL once freed */
} ScrEonClose;

typedef struct ScrEonState {
  size_t rc;
  ScrEmitter *em;        /* owned until the close handler removes its listeners */
  ScrStr *name;          /* owned */
  ScrDyn *signal;        /* owned; undefined without an abort listener */
  ScrClosure *event_cb;  /* weak, NULL once freed */
  ScrClosure *error_cb;  /* weak, NULL once freed or for 'error' */
  ScrEonClose *close;    /* the close events' listeners, in registration order */
  size_t nclose, cclose;
  ScrDyn *listener;      /* weak abort listener, NULL once freed or removed */
  ScrArr **queue;        /* owned unconsumed argument arrays (a ring) */
  size_t qhead, qlen, qcap;
  ScrPromise **waiting;  /* owned unsettled next() promises, oldest first */
  size_t nwaiting, cwaiting;
  ScrCaught *error;      /* owned; the error the next next() rejects with */
  bool finished;
  bool last_done; /* the last promise settled was a done result */
  size_t slot;    /* index + 1 in the handle table; 0 once dropped */
} ScrEonState;

enum { SCR_EON_EVENT, SCR_EON_ERROR, SCR_EON_CLOSE, SCR_EON_ABORT };

typedef struct ScrEonLink {
  size_t rc;
  ScrEonState *st; /* owned */
  int kind;
  size_t index; /* the close listener's index */
} ScrEonLink;

static SCR_TL ScrEonState **scr_eon_table;
static SCR_TL size_t scr_eon_n, scr_eon_cap;
static SCR_TL bool scr_eon_teardown_registered;

static void scr_eon_oom(void) { scr_trap("scriptc: out of memory in events.on\n"); }

static void scr_eon_state_release(ScrEonState *st) {
  if (--st->rc > 0) return;
  scr_emitter_release(st->em);
  scr_str_release(st->name);
  scr_dyn_release(st->signal);
  for (size_t i = 0; i < st->nclose; i++) scr_str_release(st->close[i].name);
  free(st->close);
  for (size_t i = 0; i < st->qlen; i++) scr_arr_release(st->queue[(st->qhead + i) % st->qcap]);
  free(st->queue);
  for (size_t i = 0; i < st->nwaiting; i++) scr_promise_release(st->waiting[i]);
  free(st->waiting);
  if (st->error) scr_caught_release(st->error);
  free(st);
}

static void *scr_eon_link_retain(void *p) {
  ((ScrEonLink *)p)->rc++;
  return p;
}

static void scr_eon_link_release(void *p) {
  ScrEonLink *link = p;
  if (--link->rc > 0) return;
  ScrEonState *st = link->st;
  if (link->kind == SCR_EON_EVENT) st->event_cb = NULL;
  else if (link->kind == SCR_EON_ERROR) st->error_cb = NULL;
  else if (link->kind == SCR_EON_CLOSE) st->close[link->index].cb = NULL;
  else st->listener = NULL;
  scr_eon_state_release(st);
  free(link);
}

static ScrBox *scr_eon_link_box(ScrEonState *st, int kind, size_t index) {
  ScrEonLink *link = calloc(1, sizeof *link);
  if (!link) scr_eon_oom();
  link->rc = 1;
  link->st = st;
  link->kind = kind;
  link->index = index;
  st->rc++;
  ScrBox *box = scr_box_new_obj(&scr_eon_link_retain, &scr_eon_link_release, NULL);
  scr_box_set_ref(box, link);
  return box;
}

/* An emitter listener: caps[0] its boxer (the compiled adapter calls it),
 * caps[1] its link. */
static ScrClosure *scr_eon_closure(void *adapter, ScrClosure *boxer, ScrEonState *st, int kind,
                                   size_t index) {
  ScrClosure *c = scr_closure_new(adapter, 2);
  ScrBox *bx = scr_box_new(SCR_BOX_FUNC);
  scr_box_set_ref(bx, scr_closure_retain(boxer));
  c->caps[0] = bx;
  c->caps[1] = scr_eon_link_box(st, kind, index);
  return c;
}

/* new AbortError(undefined, { cause: signal.reason }), thrown. */
static void scr_eon_throw_abort(ScrDyn *signal) {
  ScrStr *message = scr_str_new("The operation was aborted", 25);
  ScrError *e = scr_error_new(SCR_ERR_ERROR, message);
  scr_str_release(message);
  scr_str_release(e->name);
  e->name = scr_str_new("AbortError", 10);
  /* AbortError assigns code, then name: own enumerable properties */
  scr_error_set_code(e, "ABORT_ERR");
  e->name_present = true;
  e->name_enumerable = true;
  ScrStr *k = scr_str_new("reason", 6);
  ScrDyn *reason = scr_dyn_handle_key_get(signal, k);
  scr_str_release(k);
  scr_error_define_cause(e, reason ? reason : scr_dyn_undefined());
  if (reason) scr_dyn_release(reason);
  scr_throw_obj(e, &scr_error_retain_v, &scr_error_release_v, scr_error_trace_arg());
}

/* The done result's payload: an empty array of checked-dynamic values. */
static const ScrArrOps scr_eon_dyn_ops = {scr_dyn_retain_v, scr_dyn_release_v, scr_dyn_trace_v};

static ScrArr *scr_eon_empty(void) { return scr_arr_new_ops(&scr_eon_dyn_ops, 0); }

static void scr_eon_fulfill(ScrPromise *p, ScrArr *args /*moves*/) {
  scr_promise_fulfill_ref(p, args, scr_arr_retain_v, scr_arr_release_v,
                          args->cyc ? scr_arr_trace_v : NULL);
}

static ScrPromise *scr_eon_take_waiting(ScrEonState *st) {
  ScrPromise *p = st->waiting[0];
  st->nwaiting--;
  memmove(st->waiting, st->waiting + 1, st->nwaiting * sizeof *st->waiting);
  return p;
}

/* The handle table's reference goes: the loop has seen its last result. */
static void scr_eon_drop(ScrEonState *st) {
  if (st->slot == 0) return;
  scr_eon_table[st->slot - 1] = NULL;
  st->slot = 0;
  scr_eon_state_release(st);
}

/* removeEventListener('abort', listener) on the signal, when the listener
 * is still registered (the abort listener disposable). */
static void scr_eon_remove_abort(ScrEonState *st) {
  if (st->listener == NULL) return;
  bool aborted;
  ScrEventTargetCore *core = scr_et_signal_core_fn ? scr_et_signal_core_fn(st->signal, &aborted) : NULL;
  if (!core) return;
  ScrStr *abort = scr_str_new("abort", 5);
  ScrDyn *type = scr_dyn_new_str(abort);
  scr_str_release(abort);
  ScrDyn *listener = scr_dyn_retain(st->listener);
  ScrDyn *remove_args[2] = {type, listener};
  ScrDyn *r = scr_et_remove_listener(core, remove_args, 2);
  if (r) scr_dyn_release(r);
  scr_dyn_release(type);
  scr_dyn_release(listener);
  st->listener = NULL;
}

/* emitter.removeListener(name, cb) for a listener closure still alive;
 * false when a 'removeListener' listener threw. */
static bool scr_eon_off(ScrEmitter *em, ScrStr *name, ScrClosure *cb) {
  if (cb == NULL) return true;
  scr_emitter_release(scr_emitter_off(em, name, cb));
  return !scr_exc_pending();
}

/* Node's closeHandler: dispose of the abort listener, remove the emitter
 * listeners (removeAll pops the last added first), finish, and settle
 * every waiting promise as done. A throwing 'removeListener' listener
 * leaves the rest registered and its exception pending, as in Node. */
static void scr_eon_close_handler(ScrEonState *st) {
  scr_eon_remove_abort(st);
  if (st->em != NULL) {
    ScrEmitter *em = st->em;
    bool ok = true;
    for (size_t i = st->nclose; ok && i-- > 0;) {
      ok = scr_eon_off(em, st->close[i].name, st->close[i].cb);
      if (ok) st->close[i].cb = NULL;
    }
    if (ok) {
      ScrStr *error_name = scr_str_new("error", 5);
      ok = scr_eon_off(em, error_name, st->error_cb);
      scr_str_release(error_name);
      if (ok) st->error_cb = NULL;
    }
    if (ok) {
      ok = scr_eon_off(em, st->name, st->event_cb);
      if (ok) st->event_cb = NULL;
    }
    if (!ok) return;
    st->em = NULL;
    scr_emitter_release(em);
  }
  st->finished = true;
  while (st->nwaiting > 0) {
    ScrPromise *p = scr_eon_take_waiting(st);
    st->last_done = true;
    scr_eon_fulfill(p, scr_eon_empty());
    scr_promise_release(p);
  }
}

/* Node's errorHandler with the pending exception as the error: the oldest
 * waiting promise rejects, or the error waits for the next call. */
static void scr_eon_error_handler(ScrEonState *st) {
  ScrCaught *err = scr_exc_take();
  st->rc++;
  if (st->nwaiting > 0) {
    ScrPromise *p = scr_eon_take_waiting(st);
    scr_rethrow(err);
    scr_caught_release(err);
    scr_promise_reject_pending(p);
    scr_promise_release(p);
    /* The loop's await throws: it reads nothing more. */
    scr_eon_close_handler(st);
    if (!scr_exc_pending()) scr_eon_drop(st);
  } else {
    if (st->error) scr_caught_release(st->error);
    st->error = err;
    scr_eon_close_handler(st);
  }
  scr_eon_state_release(st);
}

/* The compiled adapter's tail: `cb` is the firing emitter listener, `args`
 * the boxed event arguments (moves in). */
void scr_events_on_fire(ScrClosure *cb, ScrArr *args) {
  ScrEonLink *link = scr_box_get_ref(cb->caps[1]); /* +1, released below */
  ScrEonState *st = link->st;
  if (link->kind == SCR_EON_EVENT) {
    if (st->nwaiting > 0) {
      ScrPromise *p = scr_eon_take_waiting(st);
      st->last_done = false;
      scr_eon_fulfill(p, args);
      scr_promise_release(p);
    } else {
      if (st->qlen == st->qcap) {
        size_t cap = st->qcap ? st->qcap * 2 : 8;
        ScrArr **q = malloc(cap * sizeof *q);
        if (!q) scr_eon_oom();
        for (size_t i = 0; i < st->qlen; i++) q[i] = st->queue[(st->qhead + i) % st->qcap];
        free(st->queue);
        st->queue = q;
        st->qhead = 0;
        st->qcap = cap;
      }
      st->queue[(st->qhead + st->qlen) % st->qcap] = args;
      st->qlen++;
    }
  } else if (link->kind == SCR_EON_ERROR) {
    ScrDyn *reason = (ScrDyn *)scr_arr_get_ref(args, 0);
    scr_dyn_throw(reason); /* moves reason */
    scr_arr_release(args);
    scr_eon_error_handler(st);
  } else {
    scr_arr_release(args);
    scr_eon_close_handler(st);
  }
  scr_eon_link_release(link);
}

/* The signal's 'abort' listener: caps[0] its link. */
static ScrDyn *scr_eon_abort_thunk(ScrClosure *clo, ScrDyn *const *args, size_t argc) {
  (void)args;
  (void)argc;
  ScrEonLink *link = scr_box_get_ref(clo->caps[0]); /* +1, released below */
  ScrEonState *st = link->st;
  scr_eon_throw_abort(st->signal);
  scr_eon_error_handler(st);
  bool ok = !scr_exc_pending();
  scr_eon_link_release(link);
  return ok ? scr_dyn_retain(scr_dyn_undefined()) : NULL;
}

static void scr_eon_teardown(void) {
  ScrEonState **table = scr_eon_table;
  size_t n = scr_eon_n;
  scr_eon_table = NULL;
  scr_eon_n = scr_eon_cap = 0;
  for (size_t i = 0; i < n; i++) {
    ScrEonState *st = table[i];
    if (st == NULL) continue;
    st->slot = 0;
    /* The emitter's entries still hold the listener closures: take them
     * out without removal events, which would run program code at exit. */
    if (st->em != NULL) {
      for (size_t k = st->nclose; k-- > 0;)
        if (st->close[k].cb) scr_emitter_unadd_quiet(st->em, st->close[k].name, st->close[k].cb);
      if (st->error_cb) {
        ScrStr *error_name = scr_str_new("error", 5);
        scr_emitter_unadd_quiet(st->em, error_name, st->error_cb);
        scr_str_release(error_name);
      }
      if (st->event_cb) scr_emitter_unadd_quiet(st->em, st->name, st->event_cb);
    }
    scr_eon_state_release(st);
  }
  free(table);
}

static ScrEonState *scr_eon_lookup(double h) {
  if (!(h >= 1) || h > (double)scr_eon_n) return NULL;
  return scr_eon_table[(size_t)h - 1];
}

/* boxer and err_boxer are borrowed (err_boxer is NULL when the event is
 * 'error'); signal is borrowed (undefined without one). Returns the
 * handle, or 0 with the exception pending: a signal that is not an
 * AbortSignal, an aborted signal, or a throwing 'newListener' listener
 * (Node's on() throws, and the listeners added before stay). */
double scr_events_on_open(ScrEmitter *em, ScrStr *name, ScrClosure *boxer, void *adapter,
                          ScrEeInvoke inv, ScrClosure *err_boxer, void *err_adapter,
                          ScrEeInvoke err_inv, ScrDyn *signal) {
  if (signal->kind != SCR_DYN_UNDEF) {
    bool aborted = false;
    ScrEventTargetCore *core =
        scr_et_signal_core_fn ? scr_et_signal_core_fn(signal, &aborted) : NULL;
    if (!core) {
      scr_dyn_prop_type_fail("options.signal", "an instance of AbortSignal", signal);
      return 0;
    }
    if (aborted) {
      scr_eon_throw_abort(signal);
      return 0;
    }
  }
  ScrEonState *st = calloc(1, sizeof *st);
  if (!st) scr_eon_oom();
  st->rc = 1;
  st->em = scr_emitter_retain(em);
  st->name = scr_str_retain(name);
  st->signal = scr_dyn_retain(signal);
  if (scr_eon_n == scr_eon_cap) {
    scr_eon_cap = scr_eon_cap ? scr_eon_cap * 2 : 8;
    scr_eon_table = realloc(scr_eon_table, scr_eon_cap * sizeof *scr_eon_table);
    if (!scr_eon_table) scr_eon_oom();
  }
  if (!scr_eon_teardown_registered) {
    scr_eon_teardown_registered = true;
    scr_atexit(scr_eon_teardown);
  }
  scr_eon_table[scr_eon_n++] = st;
  st->slot = scr_eon_n;
  double handle = (double)st->slot;
  ScrClosure *event_cb = scr_eon_closure(adapter, boxer, st, SCR_EON_EVENT, 0);
  st->event_cb = event_cb;
  scr_emitter_release(scr_emitter_on(em, name, event_cb, inv, false, false));
  if (scr_exc_pending()) return 0;
  if (err_boxer != NULL) {
    ScrClosure *error_cb = scr_eon_closure(err_adapter, err_boxer, st, SCR_EON_ERROR, 0);
    st->error_cb = error_cb;
    ScrStr *error = scr_str_new("error", 5);
    scr_emitter_release(scr_emitter_on(em, error, error_cb, err_inv, false, false));
    scr_str_release(error);
    if (scr_exc_pending()) return 0;
  }
  return handle;
}

/* Registers the listener of one close event (name and boxer borrowed). */
void scr_events_on_close(double h, ScrStr *name, ScrClosure *boxer, void *adapter, ScrEeInvoke inv) {
  ScrEonState *st = scr_eon_lookup(h);
  if (st == NULL || st->em == NULL) return;
  if (st->nclose == st->cclose) {
    st->cclose = st->cclose ? st->cclose * 2 : 2;
    st->close = realloc(st->close, st->cclose * sizeof *st->close);
    if (!st->close) scr_eon_oom();
  }
  size_t index = st->nclose++;
  st->close[index].name = scr_str_retain(name);
  ScrClosure *cb = scr_eon_closure(adapter, boxer, st, SCR_EON_CLOSE, index);
  st->close[index].cb = cb;
  scr_emitter_release(scr_emitter_on(st->em, name, cb, inv, false, false));
}

/* Adds the signal's 'abort' listener, once every emitter listener is in. */
void scr_events_on_arm(double h) {
  ScrEonState *st = scr_eon_lookup(h);
  if (st == NULL || st->signal->kind == SCR_DYN_UNDEF || st->finished) return;
  ScrClosure *clo = scr_closure_new((void *)&scr_eon_abort_thunk, 1);
  clo->caps[0] = scr_eon_link_box(st, SCR_EON_ABORT, 0);
  ScrDyn *listener = scr_dyn_new_func(clo, &scr_eon_abort_thunk, 0, "()", "abortListener");
  st->listener = listener;
  ScrDyn *dispose = scr_events_add_abort_listener(st->signal, listener);
  if (dispose) scr_dyn_release(dispose);
  /* The signal's listener entry keeps it; the state's pointer is weak. */
  scr_dyn_release(listener);
}

/* The iterator's next(): a +1 promise of the next event's arguments, or of
 * an empty array with the done flag set (scr_events_on_done). */
ScrPromise *scr_events_on_next(double h) {
  ScrPromise *p = scr_promise_new();
  ScrEonState *st = scr_eon_lookup(h);
  if (st == NULL) {
    scr_eon_fulfill(p, scr_eon_empty());
    return p;
  }
  if (st->qlen > 0) {
    ScrArr *args = st->queue[st->qhead];
    st->qhead = (st->qhead + 1) % st->qcap;
    st->qlen--;
    st->last_done = false;
    scr_eon_fulfill(p, args);
    return p;
  }
  if (st->error != NULL) {
    ScrCaught *err = st->error;
    st->error = NULL;
    scr_rethrow(err);
    scr_caught_release(err);
    scr_promise_reject_pending(p);
    /* Only the first call after an error rejects; the loop stops there. */
    scr_eon_drop(st);
    return p;
  }
  if (st->finished) {
    scr_eon_close_handler(st);
    st->last_done = true;
    scr_eon_fulfill(p, scr_eon_empty());
    return p;
  }
  if (st->nwaiting == st->cwaiting) {
    st->cwaiting = st->cwaiting ? st->cwaiting * 2 : 2;
    st->waiting = realloc(st->waiting, st->cwaiting * sizeof *st->waiting);
    if (!st->waiting) scr_eon_oom();
  }
  st->waiting[st->nwaiting++] = scr_promise_retain(p);
  return p;
}

/* Whether the result the loop just awaited was done; a done iterator
 * leaves the handle table. */
bool scr_events_on_done(double h) {
  ScrEonState *st = scr_eon_lookup(h);
  if (st == NULL) return true;
  if (!st->last_done) return false;
  scr_eon_drop(st);
  return true;
}

/* The iterator's return(): the close handler, and a +1 promise resolved
 * with the done result, which the loop awaits. */
ScrPromise *scr_events_on_return(double h) {
  ScrPromise *p = scr_promise_new();
  ScrEonState *st = scr_eon_lookup(h);
  if (st != NULL) {
    st->rc++;
    scr_eon_close_handler(st);
    if (!scr_exc_pending()) scr_eon_drop(st);
    scr_eon_state_release(st);
  }
  if (scr_exc_pending()) scr_promise_reject_pending(p);
  else scr_eon_fulfill(p, scr_eon_empty());
  return p;
}
