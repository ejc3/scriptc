/* scriptc runtime — node:zlib's Brotli one-shot forms over the vendored
 * google/brotli library (packages/runtime/vendor/brotli, the version Node
 * bundles, so compressed bytes match Node's). Compiled and linked only into
 * programs that use the Brotli functions (the `brotli` runtime feature).
 *
 * The option ladder follows lib/zlib.js: Brotli() reads options.params and
 * options.dictionary and initializes the engine (node_zlib.cc's
 * BrotliEncoderContext/BrotliDecoderContext::Init and SetParams), then
 * ZlibBase() reads chunkSize, flush, finishFlush, maxOutputLength and
 * rejectGarbageAfterEnd. The run is processChunkSync: one stream call per
 * output chunk of chunkSize bytes, continuing while a call fills its chunk. */
#include "scr_runtime.h"

#include <brotli/decode.h>
#include <brotli/encode.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lib/zlib.js kMaxBrotliParam: the largest BROTLI_PARAM_* constant. */
#define SCR_BROTLI_MAX_PARAM 8
#define SCR_BROTLI_UNSET UINT32_MAX
#define SCR_BROTLI_MAX_LENGTH 9007199254740991.0

typedef struct ScrBrotliEngine ScrBrotliEngine;

typedef struct {
  unsigned char *data;
  size_t len, cap;
  /* 0 ok; 1 engine error (err); 2 more than max_output bytes */
  int status;
  ScrZlibError err;
  char code[64]; /* the decoder's "ERR_" code string */
} ScrBrotliRun;

/* One direction's Brotli calls. Each entry point names its codec, so a
 * program that only decompresses links no encoder, and one that only
 * compresses links no decoder. */
typedef struct {
  /* Creates the instance, attaches e->dict and applies the params that are
   * set: false when Brotli rejects one (Node's "Initialization failed"). */
  bool (*init)(ScrBrotliEngine *e, const uint32_t *params);
  /* One stream call: false with run->status and run->err set on an error. */
  bool (*step)(ScrBrotliEngine *e, size_t *avail_in, const uint8_t **next_in, size_t *avail_out,
               uint8_t **next_out, ScrBrotliRun *run);
  void (*destroy)(ScrBrotliEngine *e);
} ScrBrotliCodec;

struct ScrBrotliEngine {
  const ScrBrotliCodec *codec;
  BrotliEncoderState *enc;
  BrotliDecoderState *dec;
  unsigned char *dict; /* owned copy; the encoder's prepared dictionary and the decoder read it */
  size_t dict_len;
  BrotliEncoderPreparedDictionary *prepared;
  uint32_t chunk;
  int finish_flush;
  double max_output;
};

SCR_COLD static void scr_brotli_oom(void) { scr_trap("scriptc: out of memory\n"); }

static bool scr_brotli_encoder_init(ScrBrotliEngine *e, const uint32_t *params) {
  e->enc = BrotliEncoderCreateInstance(NULL, NULL, NULL);
  if (!e->enc) scr_brotli_oom();
  bool ok = true;
  if (e->dict) {
    e->prepared = BrotliEncoderPrepareDictionary(BROTLI_SHARED_DICTIONARY_RAW, e->dict_len, e->dict,
                                                 BROTLI_MAX_QUALITY, NULL, NULL, NULL);
    ok = e->prepared != NULL && BrotliEncoderAttachPreparedDictionary(e->enc, e->prepared);
  }
  for (int i = 0; ok && i <= SCR_BROTLI_MAX_PARAM; i++)
    if (params[i] != SCR_BROTLI_UNSET) ok = BrotliEncoderSetParameter(e->enc, (BrotliEncoderParameter)i, params[i]);
  return ok;
}

static bool scr_brotli_encoder_step(ScrBrotliEngine *e, size_t *avail_in, const uint8_t **next_in,
                                    size_t *avail_out, uint8_t **next_out, ScrBrotliRun *run) {
  if (BrotliEncoderCompressStream(e->enc, (BrotliEncoderOperation)e->finish_flush, avail_in, next_in, avail_out,
                                  next_out, NULL))
    return true;
  run->status = 1;
  run->err = (ScrZlibError){"Compression failed", "ERR_BROTLI_COMPRESSION_FAILED", -1};
  return false;
}

static void scr_brotli_encoder_destroy(ScrBrotliEngine *e) {
  if (e->enc) BrotliEncoderDestroyInstance(e->enc);
  if (e->prepared) BrotliEncoderDestroyPreparedDictionary(e->prepared);
}

static const ScrBrotliCodec scr_brotli_encoder = {scr_brotli_encoder_init, scr_brotli_encoder_step,
                                                  scr_brotli_encoder_destroy};

static bool scr_brotli_decoder_init(ScrBrotliEngine *e, const uint32_t *params) {
  e->dec = BrotliDecoderCreateInstance(NULL, NULL, NULL);
  if (!e->dec) scr_brotli_oom();
  bool ok = true;
  if (e->dict) ok = BrotliDecoderAttachDictionary(e->dec, BROTLI_SHARED_DICTIONARY_RAW, e->dict_len, e->dict);
  for (int i = 0; ok && i <= SCR_BROTLI_MAX_PARAM; i++)
    if (params[i] != SCR_BROTLI_UNSET) ok = BrotliDecoderSetParameter(e->dec, (BrotliDecoderParameter)i, params[i]);
  return ok;
}

static bool scr_brotli_decoder_step(ScrBrotliEngine *e, size_t *avail_in, const uint8_t **next_in,
                                    size_t *avail_out, uint8_t **next_out, ScrBrotliRun *run) {
  BrotliDecoderResult r = BrotliDecoderDecompressStream(e->dec, avail_in, next_in, avail_out, next_out, NULL);
  if (r == BROTLI_DECODER_RESULT_ERROR) {
    BrotliDecoderErrorCode code = BrotliDecoderGetErrorCode(e->dec);
    snprintf(run->code, sizeof run->code, "ERR_%s", BrotliDecoderErrorString(code));
    run->status = 1;
    run->err = (ScrZlibError){"Decompression failed", run->code, (int)code};
    return false;
  }
  if (e->finish_flush == BROTLI_OPERATION_FINISH && r == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
    run->status = 1;
    run->err = (ScrZlibError){"unexpected end of file", "Z_BUF_ERROR", -5};
    return false;
  }
  return true;
}

static void scr_brotli_decoder_destroy(ScrBrotliEngine *e) {
  if (e->dec) BrotliDecoderDestroyInstance(e->dec);
}

static const ScrBrotliCodec scr_brotli_decoder = {scr_brotli_decoder_init, scr_brotli_decoder_step,
                                                  scr_brotli_decoder_destroy};

static void scr_brotli_engine_free(ScrBrotliEngine *e) {
  if (!e) return;
  e->codec->destroy(e);
  free(e->dict);
  free(e);
}

static const ScrDyn *scr_brotli_opt(const ScrDyn *opts, const char *key) {
  if (opts == NULL || opts->kind != SCR_DYN_OBJ) return NULL;
  return scr_dyn_obj_get((ScrDyn *)opts, key, strlen(key));
}

/* ToUint32 of a params value (a number or a boolean). */
static uint32_t scr_brotli_uint32(const ScrDyn *v) {
  if (v->kind == SCR_DYN_BOOL) return v->v.b ? 1 : 0;
  double d = v->v.num;
  if (!isfinite(d)) return 0;
  d = trunc(d);
  d = fmod(d, 4294967296.0);
  if (d < 0) d += 4294967296.0;
  return (uint32_t)d;
}

SCR_COLD static void scr_brotli_chunk_too_small(double v) {
  char recv[48], msg[160];
  scr_num_received(v, recv);
  int n = snprintf(msg, sizeof msg,
                   "The value of \"options.chunkSize\" is out of range. It must be >= 64. Received %s", recv);
  scr_throw_error_msg_code(SCR_ERR_RANGE, msg, (size_t)n, "ERR_OUT_OF_RANGE");
}

/* zlibBufferSync's "buffer" check, or the stream write's "chunk" check for
 * the callback forms: strings encode as UTF-8, and TypedArrays, DataViews
 * and ArrayBuffers contribute their bytes. +1; NULL after a throw. */
static ScrBytes *scr_brotli_input_dyn(const ScrDyn *v, bool callback) {
  if (v->kind == SCR_DYN_TYPED_REF) {
    ScrDyn *materialized = scr_dyn_typed_ref_materialize(v);
    if (!materialized) return NULL;
    ScrBytes *out = scr_brotli_input_dyn(materialized, callback);
    scr_dyn_release(materialized);
    return out;
  }
  if (v->kind == SCR_DYN_STR) {
    ScrStr *str = v->v.str;
    ScrBytes *b = scr_bytes_new(SCR_BYTES_U8, (double)str->len);
    memcpy(b->data, str->data, str->len);
    if (!str->ascii) scr_wtf8_to_utf8((char *)b->data, str->len);
    return b;
  }
  if (v->kind == SCR_DYN_BYTES) return scr_bytes_raw_view(v->v.bytes);
  if (scr_buffer_storage_is(v))
    return scr_array_buffer_view(SCR_BYTES_U8, (ScrDyn *)v, scr_dyn_undefined(), scr_dyn_undefined());
  if (callback)
    scr_dyn_arg_type_fail("chunk", "of type string or an instance of Buffer, TypedArray, or DataView", v);
  else
    scr_dyn_arg_type_fail("buffer", "of type string or an instance of Buffer, TypedArray, DataView, or ArrayBuffer",
                          v);
  return NULL;
}

SCR_COLD static void scr_brotli_invalid_param(const char *key, size_t len) {
  char msg[160];
  int n = snprintf(msg, sizeof msg, "%.*s is not a valid Brotli parameter", (int)(len > 100 ? 100 : len), key);
  scr_throw_error_msg_code(SCR_ERR_RANGE, msg, (size_t)n, "ERR_BROTLI_INVALID_PARAM");
}

/* One own key of options.params: Node's `+origKey` must name an unset slot
 * in [0, kMaxBrotliParam], and its value must be a number or a boolean. */
static bool scr_brotli_param(uint32_t *params, const char *key, size_t key_len, const ScrDyn *value) {
  ScrStr *k = scr_str_new(key, key_len);
  double index = scr_string_to_number(k);
  scr_str_release(k);
  if (isnan(index) || index < 0 || index > SCR_BROTLI_MAX_PARAM || index != floor(index) ||
      params[(int)index] != SCR_BROTLI_UNSET) {
    scr_brotli_invalid_param(key, key_len);
    return false;
  }
  if (value == NULL || (value->kind != SCR_DYN_NUM && value->kind != SCR_DYN_BOOL)) {
    scr_dyn_prop_type_fail("options.params[key]", "of type number", value ? value : scr_dyn_undefined());
    return false;
  }
  params[(int)index] = scr_brotli_uint32(value);
  return true;
}

/* Object.keys(options.params) with its values: an object's own keys, an
 * array's indices, a string's indices; other values have none. */
static bool scr_brotli_params(const ScrDyn *p, uint32_t *params) {
  if (p->kind == SCR_DYN_TYPED_REF) {
    ScrDyn *view = scr_dyn_typed_ref_materialize(p);
    if (!view) return false;
    bool ok = scr_brotli_params(view, params);
    scr_dyn_release(view);
    return ok;
  }
  char index[24];
  if (p->kind == SCR_DYN_OBJ) {
    ScrDynEntryPairs *pairs = scr_dyn_obj_entry_pairs(p);
    if (!pairs) return false;
    bool ok = true;
    for (size_t i = 0; ok && i < pairs->count; i++)
      ok = scr_brotli_param(params, pairs->pairs[i].key->data, pairs->pairs[i].key->len, pairs->pairs[i].value);
    /* The pairs are owned. */
    for (size_t i = 0; i < pairs->count; i++) {
      scr_str_release(pairs->pairs[i].key);
      scr_dyn_release(pairs->pairs[i].value);
    }
    scr_dyn_entry_pairs_free(pairs);
    return ok;
  }
  if (p->kind == SCR_DYN_ARR) {
    double n = scr_dyn_arr_len(p);
    for (double i = 0; i < n; i++) {
      ScrDyn *v = scr_dyn_arr_at(p, i);
      int len = snprintf(index, sizeof index, "%.0f", i);
      bool ok = scr_brotli_param(params, index, (size_t)len, v);
      scr_dyn_release(v);
      if (!ok) return false;
    }
    return true;
  }
  if (p->kind == SCR_DYN_STR && p->v.str->len > 0) {
    /* The first index's value is a one-code-unit string: not a number. */
    ScrStr *unit = scr_str_char_at(p->v.str, 0);
    ScrDyn *first = scr_dyn_new_str(unit);
    scr_str_release(unit);
    bool ok = scr_brotli_param(params, "0", 1, first);
    scr_dyn_release(first);
    return ok;
  }
  return true;
}

SCR_COLD static void scr_brotli_init_failed(void) {
  static const char msg[] = "Initialization failed";
  scr_throw_error_msg_code(SCR_ERR_ERROR, msg, sizeof msg - 1, "ERR_ZLIB_INITIALIZATION_FAILED");
}

/* The engine of brotli{Compress,Decompress}{,Sync}(data, opts): NULL after a
 * throw. */
static ScrBrotliEngine *scr_brotli_engine_from(const ScrBrotliCodec *codec, const ScrDyn *opts_in) {
  ScrDyn *materialized = NULL;
  const ScrDyn *opts = opts_in;
  if (opts != NULL && opts->kind == SCR_DYN_TYPED_REF) {
    materialized = scr_dyn_typed_ref_materialize(opts);
    if (!materialized) return NULL;
    opts = materialized;
  }
  ScrBrotliEngine *e = NULL;
  uint32_t params[SCR_BROTLI_MAX_PARAM + 1];
  for (int i = 0; i <= SCR_BROTLI_MAX_PARAM; i++) params[i] = SCR_BROTLI_UNSET;
  const ScrDyn *p = scr_brotli_opt(opts, "params");
  if (p != NULL && scr_dyn_truthy(p) && !scr_brotli_params(p, params)) goto fail;
  const ScrDyn *d = scr_brotli_opt(opts, "dictionary");
  ScrBytes *dict = NULL;
  if (d != NULL && d->kind != SCR_DYN_UNDEF) {
    if (d->kind == SCR_DYN_BYTES) {
      dict = scr_bytes_raw_view(d->v.bytes);
    } else if (scr_buffer_storage_is(d)) {
      dict = scr_array_buffer_view(SCR_BYTES_U8, (ScrDyn *)d, scr_dyn_undefined(), scr_dyn_undefined());
    } else {
      scr_dyn_prop_type_fail("options.dictionary",
                             "an instance of Buffer, TypedArray, DataView, or ArrayBuffer", d);
      goto fail;
    }
  }
  e = calloc(1, sizeof *e);
  if (!e) scr_brotli_oom();
  e->codec = codec;
  if (dict != NULL && dict->len > 0) {
    e->dict = malloc(dict->len);
    if (!e->dict) scr_brotli_oom();
    memcpy(e->dict, dict->data, dict->len);
    e->dict_len = dict->len;
  }
  if (dict) scr_bytes_release(dict);
  if (!codec->init(e, params)) {
    scr_brotli_init_failed();
    goto fail;
  }
  /* ZlibBase(): `if (opts)` — a falsy value reads no options. */
  e->chunk = 16384;
  e->finish_flush = BROTLI_OPERATION_FINISH;
  e->max_output = SCR_BROTLI_MAX_LENGTH;
  const ScrDyn *o = opts != NULL && scr_dyn_truthy(opts) ? opts : NULL;
  if (o != NULL) {
    /* validateFiniteNumber through the unbounded range, then chunkSize's
     * one-sided bound. */
    double chunk;
    if (!scr_zlib_check_range(scr_brotli_opt(o, "chunkSize"), "options.chunkSize", true, -INFINITY, INFINITY,
                              16384, &chunk))
      goto fail;
    if (chunk < 64) {
      scr_brotli_chunk_too_small(chunk);
      goto fail;
    }
    if (chunk > 4294967295.0) {
      static const char msg[] = "a zlib chunkSize above 4294967295 is not supported yet";
      scr_throw_error_msg(SCR_ERR_ERROR, msg, sizeof msg - 1);
      goto fail;
    }
    e->chunk = (uint32_t)floor(chunk);
    double flush, finish, max;
    if (!scr_zlib_check_range(scr_brotli_opt(o, "flush"), "options.flush", true, BROTLI_OPERATION_PROCESS,
                              BROTLI_OPERATION_EMIT_METADATA, BROTLI_OPERATION_PROCESS, &flush) ||
        !scr_zlib_check_range(scr_brotli_opt(o, "finishFlush"), "options.finishFlush", true,
                              BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_EMIT_METADATA,
                              BROTLI_OPERATION_FINISH, &finish) ||
        !scr_zlib_check_range(scr_brotli_opt(o, "maxOutputLength"), "options.maxOutputLength", true, 1,
                              SCR_BROTLI_MAX_LENGTH, SCR_BROTLI_MAX_LENGTH, &max))
      goto fail;
    e->finish_flush = (int)finish;
    e->max_output = max;
    const ScrDyn *rg = scr_brotli_opt(o, "rejectGarbageAfterEnd");
    if (rg != NULL && rg->kind != SCR_DYN_UNDEF && rg->kind != SCR_DYN_BOOL) {
      scr_dyn_prop_type_fail("options.rejectGarbageAfterEnd", "of type boolean", rg);
      goto fail;
    }
    const ScrDyn *info = scr_brotli_opt(o, "info");
    if (info != NULL && scr_dyn_truthy(info)) {
      static const char msg[] = "the zlib option 'info' is not supported yet (it returns the engine object)";
      scr_throw_error_msg(SCR_ERR_ERROR, msg, sizeof msg - 1);
      goto fail;
    }
  }
  if (materialized) scr_dyn_release(materialized);
  return e;
fail:
  scr_brotli_engine_free(e);
  if (materialized) scr_dyn_release(materialized);
  return NULL;
}

static void scr_brotli_append(ScrBrotliRun *out, const unsigned char *p, size_t n) {
  if (out->len + n > out->cap) {
    size_t cap = out->cap ? out->cap : 4096;
    while (cap < out->len + n) cap *= 2;
    unsigned char *grown = realloc(out->data, cap);
    if (!grown) scr_brotli_oom();
    out->data = grown;
    out->cap = cap;
  }
  memcpy(out->data + out->len, p, n);
  out->len += n;
}

/* processChunkSync: one stream call per chunk of output, while a call fills
 * its chunk. Touches only the engine and *out (worker-pool safe). */
static void scr_brotli_run(ScrBrotliEngine *e, const unsigned char *in, size_t in_len, ScrBrotliRun *out) {
  unsigned char *chunk = malloc(e->chunk);
  if (!chunk) scr_brotli_oom();
  size_t avail_in = in_len;
  const uint8_t *next_in = in;
  size_t offset = 0;
  for (;;) {
    size_t avail_out = e->chunk - offset;
    uint8_t *next_out = chunk + offset;
    size_t avail_out_before = avail_out;
    if (!e->codec->step(e, &avail_in, &next_in, &avail_out, &next_out, out)) break;
    size_t have = avail_out_before - avail_out;
    if (have > 0) {
      scr_brotli_append(out, chunk + offset, have);
      offset += have;
      if ((double)out->len > e->max_output) {
        out->status = 2;
        break;
      }
    }
    if (avail_out == 0 || offset >= e->chunk) offset = 0;
    if (avail_out != 0) break;
  }
  free(chunk);
}

static ScrError *scr_brotli_run_error(const ScrBrotliRun *run, double max_output) {
  if (run->status == 1) return scr_zlib_error_new(&run->err);
  char num[32], msg[96];
  scr_f64_to_str(max_output, num);
  snprintf(msg, sizeof msg, "Cannot create a Buffer larger than %s bytes", num);
  ScrStr *message = scr_str_new(msg, strlen(msg));
  ScrError *err = scr_error_new(SCR_ERR_RANGE, message);
  scr_str_release(message);
  scr_error_set_code(err, "ERR_BUFFER_TOO_LARGE");
  return err;
}

static ScrBytes *scr_brotli_result(const ScrBrotliRun *run) {
  ScrBytes *result = scr_bytes_new(SCR_BYTES_U8, (double)run->len);
  result->is_buffer = true; /* Node returns a Buffer */
  if (run->len) memcpy(result->data, run->data, run->len);
  return result;
}

/* Runs the whole input and frees e. */
static ScrBytes *scr_brotli_sync_run(ScrBrotliEngine *e, const ScrBytes *data) {
  SCR_BYTES_SNAPSHOT(data);
  ScrBrotliRun run = {0};
  scr_brotli_run(e, data->data, data->len, &run);
  ScrBytes *result = NULL;
  if (run.status != 0) {
    ScrError *err = scr_brotli_run_error(&run, e->max_output);
    scr_throw_obj(err, &scr_error_retain_v, &scr_error_release_v, scr_error_trace_arg());
  } else {
    result = scr_brotli_result(&run);
  }
  free(run.data);
  scr_brotli_engine_free(e);
  return result;
}

static ScrBytes *scr_brotli_sync(const ScrBrotliCodec *codec, const ScrBytes *data, ScrDyn *opts) {
  ScrBrotliEngine *e = scr_brotli_engine_from(codec, opts);
  if (!e) return NULL;
  return scr_brotli_sync_run(e, data);
}

static ScrBytes *scr_brotli_sync_dyn(const ScrBrotliCodec *codec, ScrDyn *data, ScrDyn *opts) {
  ScrBrotliEngine *e = scr_brotli_engine_from(codec, opts);
  if (!e) return NULL;
  ScrBytes *input = scr_brotli_input_dyn(data, false);
  if (!input) {
    scr_brotli_engine_free(e);
    return NULL;
  }
  ScrBytes *out = scr_brotli_sync_run(e, input);
  scr_bytes_release(input);
  return out;
}

ScrBytes *scr_brotli_compress_sync(const ScrBytes *data, ScrDyn *opts) {
  return scr_brotli_sync(&scr_brotli_encoder, data, opts);
}

ScrBytes *scr_brotli_decompress_sync(const ScrBytes *data, ScrDyn *opts) {
  return scr_brotli_sync(&scr_brotli_decoder, data, opts);
}

ScrBytes *scr_brotli_compress_sync_dyn(ScrDyn *data, ScrDyn *opts) {
  return scr_brotli_sync_dyn(&scr_brotli_encoder, data, opts);
}

ScrBytes *scr_brotli_decompress_sync_dyn(ScrDyn *data, ScrDyn *opts) {
  return scr_brotli_sync_dyn(&scr_brotli_decoder, data, opts);
}

#ifndef SCR_LIB
typedef struct {
  ScrBrotliEngine *eng;
  unsigned char *data;
  size_t len;
  ScrBrotliRun run;
  ScrClosure *cb;
  ScrZlibBytesFn fn;
} ScrBrotliOp;

static void scr_brotli_work(void *payload) {
  ScrBrotliOp *op = payload;
  scr_brotli_run(op->eng, op->data, op->len, &op->run);
}

static void scr_brotli_after(void *payload) {
  ScrBrotliOp *op = payload;
  if (op->run.status == 0) {
    op->fn(op->cb, NULL, scr_brotli_result(&op->run)); /* moves the value */
    return;
  }
  ScrError *err = scr_brotli_run_error(&op->run, op->eng->max_output);
  op->fn(op->cb, err, NULL);
  scr_error_release(err);
}

static void scr_brotli_destroy(void *payload) {
  ScrBrotliOp *op = payload;
  scr_brotli_engine_free(op->eng);
  free(op->data);
  free(op->run.data);
  if (op->cb) scr_closure_release(op->cb);
  free(op);
}

/* Moves cb and e. */
static void scr_brotli_submit(ScrBrotliEngine *e, const ScrBytes *data, ScrClosure *cb, ScrZlibBytesFn fn) {
  SCR_BYTES_SNAPSHOT(data);
  ScrBrotliOp *op = calloc(1, sizeof *op);
  if (!op) scr_brotli_oom();
  op->eng = e;
  op->data = malloc(data->len ? data->len : 1);
  if (!op->data) scr_brotli_oom();
  if (data->len) memcpy(op->data, data->data, data->len);
  op->len = data->len;
  op->cb = cb;
  op->fn = fn;
  scr_work_submit(op, &scr_brotli_work, &scr_brotli_after, &scr_brotli_destroy);
}

static void scr_brotli_async(const ScrBrotliCodec *codec, const ScrBytes *data, ScrDyn *opts, ScrClosure *cb,
                             ScrZlibBytesFn fn) {
  ScrBrotliEngine *e = scr_brotli_engine_from(codec, opts);
  if (!e) {
    scr_closure_release(cb);
    return;
  }
  scr_brotli_submit(e, data, cb, fn);
}

static void scr_brotli_async_dyn(const ScrBrotliCodec *codec, ScrDyn *data, ScrDyn *opts, ScrClosure *cb,
                                 ScrZlibBytesFn fn) {
  ScrBrotliEngine *e = scr_brotli_engine_from(codec, opts);
  if (!e) {
    scr_closure_release(cb);
    return;
  }
  /* Node ends the stream with end(data), so null and undefined end it with
   * no input. */
  ScrBytes *input = data->kind == SCR_DYN_NULL || data->kind == SCR_DYN_UNDEF
                        ? scr_bytes_new(SCR_BYTES_U8, 0)
                        : scr_brotli_input_dyn(data, true);
  if (!input) {
    scr_brotli_engine_free(e);
    scr_closure_release(cb);
    return;
  }
  scr_brotli_submit(e, input, cb, fn);
  scr_bytes_release(input);
}

void scr_brotli_compress_async(const ScrBytes *data, ScrDyn *opts, ScrClosure *cb, ScrZlibBytesFn fn) {
  scr_brotli_async(&scr_brotli_encoder, data, opts, cb, fn);
}

void scr_brotli_decompress_async(const ScrBytes *data, ScrDyn *opts, ScrClosure *cb, ScrZlibBytesFn fn) {
  scr_brotli_async(&scr_brotli_decoder, data, opts, cb, fn);
}

void scr_brotli_compress_async_dyn(ScrDyn *data, ScrDyn *opts, ScrClosure *cb, ScrZlibBytesFn fn) {
  scr_brotli_async_dyn(&scr_brotli_encoder, data, opts, cb, fn);
}

void scr_brotli_decompress_async_dyn(ScrDyn *data, ScrDyn *opts, ScrClosure *cb, ScrZlibBytesFn fn) {
  scr_brotli_async_dyn(&scr_brotli_decoder, data, opts, cb, fn);
}
#endif
