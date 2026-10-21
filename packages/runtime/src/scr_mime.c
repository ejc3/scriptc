/* scriptc runtime — util.MIMEType and util.MIMEParams, Node 24's
 * lib/internal/mime.js as native checked-dynamic handles.
 *
 * Parsing and validation work on UTF-16 code units, as Node's do, so the
 * positions in ERR_INVALID_MIME_SYNTAX messages and the slicing of type,
 * subtype and parameter text are Node's. MIMEParams keeps its entries in
 * insertion order; a deleted entry leaves a tombstone, so a live iterator
 * continues past a deletion the way a Map iterator does. */
#include "scr_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── UTF-16 views of ScrStr text ─────────────────────────────────────── */

typedef struct {
  uint16_t *u;
  size_t n;
} ScrMimeU16;

/* The UTF-16 code units of s (WTF-8 storage; a lone surrogate is one unit). */
static ScrMimeU16 scr_mime_u16(const ScrStr *s) {
  ScrMimeU16 out = { malloc((s->len + 1) * sizeof(uint16_t)), 0 };
  if (!out.u) abort();
  const unsigned char *p = (const unsigned char *)s->data;
  size_t i = 0;
  while (i < s->len) {
    unsigned c = p[i];
    uint32_t cp;
    if (c < 0x80) {
      cp = c;
      i += 1;
    } else if (c < 0xE0) {
      cp = ((c & 0x1Fu) << 6) | (p[i + 1] & 0x3Fu);
      i += 2;
    } else if (c < 0xF0) {
      cp = ((c & 0x0Fu) << 12) | ((p[i + 1] & 0x3Fu) << 6) | (p[i + 2] & 0x3Fu);
      i += 3;
    } else {
      cp = ((c & 0x07u) << 18) | ((p[i + 1] & 0x3Fu) << 12) | ((p[i + 2] & 0x3Fu) << 6) | (p[i + 3] & 0x3Fu);
      i += 4;
    }
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.u[out.n++] = (uint16_t)(0xD800 + (cp >> 10));
      out.u[out.n++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
    } else {
      out.u[out.n++] = (uint16_t)cp;
    }
  }
  return out;
}

/* The string of n code units at u: a high surrogate followed by a low one
 * is one code point; a lone surrogate keeps its own encoding. +1. */
static ScrStr *scr_mime_str(const uint16_t *u, size_t n) {
  char *buf = malloc(n * 3 + 1);
  if (!buf) abort();
  size_t len = 0;
  for (size_t i = 0; i < n; i++) {
    uint32_t cp = u[i];
    if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (u[i + 1] - 0xDC00);
      i++;
    }
    len += scr_wtf8_put(buf + len, cp);
  }
  ScrStr *out = scr_str_new(buf, len);
  free(buf);
  return out;
}

/* HTTP token code points: !#$%&'*+-.^_`|~ and ASCII alphanumerics. */
static bool scr_mime_token(uint16_t c) {
  if (c >= 0x80) return false;
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
  return c != 0 && strchr("!#$%&'*+-.^_`|~", (int)c) != NULL;
}

/* HTTP quoted-string code points: tab, U+0020..U+007E and U+0080..U+00FF. */
static bool scr_mime_quoted(uint16_t c) {
  return c == '\t' || (c >= 0x20 && c <= 0x7E) || (c >= 0x80 && c <= 0xFF);
}

static bool scr_mime_ws(uint16_t c) { return c == '\r' || c == '\n' || c == '\t' || c == ' '; }

/* The first code unit that is not a token code point, or -1. */
static long scr_mime_not_token_at(const uint16_t *u, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (!scr_mime_token(u[i])) return (long)i;
  return -1;
}

static long scr_mime_not_quoted_at(const uint16_t *u, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (!scr_mime_quoted(u[i])) return (long)i;
  return -1;
}

/* Where the trailing run of HTTP whitespace starts (n when there is none). */
static size_t scr_mime_trailing_ws(const uint16_t *u, size_t n) {
  while (n > 0 && scr_mime_ws(u[n - 1])) n--;
  return n;
}

static long scr_mime_index_of(const uint16_t *u, size_t n, uint16_t c, size_t from) {
  for (size_t i = from; i < n; i++)
    if (u[i] == c) return (long)i;
  return -1;
}

/* toASCIILower: only A-Z change. */
static void scr_mime_lower(uint16_t *u, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (u[i] >= 'A' && u[i] <= 'Z') u[i] = (uint16_t)(u[i] + 32);
}

/* Throws ERR_INVALID_MIME_SYNTAX for `production` in `input`. */
static void scr_mime_syntax_error(const char *production, const ScrStr *input, long index) {
  size_t cap = input->len + strlen(production) + 64;
  char *msg = malloc(cap);
  if (!msg) abort();
  int n = snprintf(msg, cap, "The MIME syntax for a %s in \"%.*s\" is invalid", production, (int)input->len,
                   input->data);
  if (n >= 0 && index != -1) n += snprintf(msg + n, cap - (size_t)n, " at %ld", index);
  scr_throw_error_msg_code(SCR_ERR_TYPE, msg, n < 0 ? 0 : (size_t)n, "ERR_INVALID_MIME_SYNTAX");
  free(msg);
}

/* `${value}`: ToString, with a Symbol rejected as a template literal does. +1, or NULL pending. */
static ScrStr *scr_mime_to_string(const ScrDyn *value) {
  if (value->kind == SCR_DYN_SYMBOL) {
    static const char message[] = "Cannot convert a Symbol value to a string";
    scr_throw_error_msg(SCR_ERR_TYPE, message, sizeof message - 1);
    return NULL;
  }
  return scr_dyn_string_coerce_js(value);
}

/* ── MIMEParams ──────────────────────────────────────────────────────── */

typedef struct ScrMimeParams {
  size_t rc;
  size_t n, cap; /* entries, tombstones included */
  ScrStr **keys; /* NULL marks a deleted entry */
  ScrStr **vals;
} ScrMimeParams;

static ScrMimeParams *scr_mime_params_alloc(void) {
  ScrMimeParams *p = calloc(1, sizeof *p);
  if (!p) abort();
  p->rc = 1;
  return p;
}

static void scr_mime_params_free(ScrMimeParams *p) {
  for (size_t i = 0; i < p->n; i++) {
    if (!p->keys[i]) continue;
    scr_str_release(p->keys[i]);
    scr_str_release(p->vals[i]);
  }
  free(p->keys);
  free(p->vals);
  free(p);
}

static void *scr_mime_params_retain_v(void *h) {
  ((ScrMimeParams *)h)->rc++;
  return h;
}

static void scr_mime_params_release_v(void *h) {
  ScrMimeParams *p = h;
  if (--p->rc == 0) scr_mime_params_free(p);
}

static long scr_mime_params_find(const ScrMimeParams *p, const ScrStr *key) {
  for (size_t i = 0; i < p->n; i++)
    if (p->keys[i] && p->keys[i]->len == key->len && memcmp(p->keys[i]->data, key->data, key->len) == 0)
      return (long)i;
  return -1;
}

/* Map#set: an existing key keeps its position. Takes both strings. */
static void scr_mime_params_put(ScrMimeParams *p, ScrStr *key, ScrStr *val) {
  long at = scr_mime_params_find(p, key);
  if (at >= 0) {
    scr_str_release(key);
    scr_str_release(p->vals[at]);
    p->vals[at] = val;
    return;
  }
  if (p->n == p->cap) {
    p->cap = p->cap ? p->cap * 2 : 4;
    p->keys = realloc(p->keys, p->cap * sizeof *p->keys);
    p->vals = realloc(p->vals, p->cap * sizeof *p->vals);
    if (!p->keys || !p->vals) abort();
  }
  p->keys[p->n] = key;
  p->vals[p->n] = val;
  p->n++;
}

/* MIMEParams.prototype.toString: name=value pairs joined by ';', each value
 * quoted (with '"' and '\' escaped) when empty or not a token. +1. */
static ScrStr *scr_mime_params_string(const ScrMimeParams *p) {
  ScrJsonBuf b;
  scr_jb_init(&b);
  bool first = true;
  for (size_t i = 0; i < p->n; i++) {
    if (!p->keys[i]) continue;
    if (!first) scr_jb_putc(&b, ';');
    first = false;
    scr_jb_put_str(&b, p->keys[i]);
    scr_jb_putc(&b, '=');
    ScrStr *v = p->vals[i];
    ScrMimeU16 u = scr_mime_u16(v);
    if (u.n == 0) {
      scr_jb_puts(&b, "\"\"");
    } else if (scr_mime_not_token_at(u.u, u.n) == -1) {
      scr_jb_put_str(&b, v);
    } else {
      scr_jb_putc(&b, '"');
      for (size_t k = 0; k < v->len; k++) {
        char c = v->data[k];
        if (c == '"' || c == '\\') scr_jb_putc(&b, '\\');
        scr_jb_putc(&b, c);
      }
      scr_jb_putc(&b, '"');
    }
    free(u.u);
  }
  return scr_jb_finish(&b);
}

/* removeBackslashes: each '\' takes the next code unit literally; a final
 * lone '\' is kept. */
static size_t scr_mime_unescape(uint16_t *u, size_t n) {
  size_t out = 0, i;
  for (i = 0; i + 1 < n; i++) {
    if (u[i] == '\\') i++;
    u[out++] = u[i];
  }
  if (n > 0 && i == n - 1) u[out++] = u[i];
  return out;
}

/* MIMEParams#parse over the parameter text after the subtype's ';'. */
static void scr_mime_params_parse(ScrMimeParams *p, const uint16_t *str, size_t len) {
  size_t end = scr_mime_trailing_ws(str, len);
  size_t pos = 0;
  while (pos < end) {
    while (pos < len && scr_mime_ws(str[pos])) pos++;
    size_t name_end = pos;
    while (name_end < len && str[name_end] != ';' && str[name_end] != '=') name_end++;
    size_t name_start = pos;
    pos = name_end;
    if (pos < end) {
      uint16_t c = str[pos];
      pos++;
      if (c == ';') continue;
    }
    if (pos >= end) break;
    uint16_t *value = NULL;
    size_t value_len = 0;
    if (str[pos] == '"') {
      pos++;
      /* QUOTED_VALUE_PATTERN: up to the closing '"', or to the end; a '\'
       * that ends the text is an unmatched escape. */
      size_t start = pos, k = pos;
      bool trailing_backslash = false, closed = false;
      while (k < len) {
        if (str[k] == '\\') {
          if (k + 1 == len) {
            trailing_backslash = true;
            k++;
            break;
          }
          k += 2;
        } else if (str[k] == '"') {
          closed = true;
          k++;
          break;
        } else {
          k++;
        }
      }
      size_t matched = k - start;
      pos += matched;
      size_t inside = trailing_backslash || closed ? matched - 1 : matched;
      value = malloc((inside + 2) * sizeof(uint16_t));
      if (!value) abort();
      memcpy(value, str + start, inside * sizeof(uint16_t));
      value_len = scr_mime_unescape(value, inside);
      if (trailing_backslash) value[value_len++] = '\\';
    } else {
      long semi = scr_mime_index_of(str, len, ';', pos);
      size_t raw_end = semi == -1 ? len : (size_t)semi;
      size_t raw_len = raw_end - pos;
      size_t trimmed = scr_mime_trailing_ws(str + pos, raw_len);
      size_t value_start = pos;
      pos += raw_len;
      if (trimmed == 0) continue;
      value = malloc((trimmed + 1) * sizeof(uint16_t));
      if (!value) abort();
      memcpy(value, str + value_start, trimmed * sizeof(uint16_t));
      value_len = trimmed;
    }
    size_t name_len = name_end - name_start;
    uint16_t *name = malloc((name_len + 1) * sizeof(uint16_t));
    if (!name) abort();
    memcpy(name, str + name_start, name_len * sizeof(uint16_t));
    scr_mime_lower(name, name_len);
    if (name_len > 0 && scr_mime_not_token_at(name, name_len) == -1 &&
        scr_mime_not_quoted_at(value, value_len) == -1) {
      ScrStr *key = scr_mime_str(name, name_len);
      if (scr_mime_params_find(p, key) == -1) scr_mime_params_put(p, key, scr_mime_str(value, value_len));
      else scr_str_release(key);
    }
    free(name);
    free(value);
    pos++;
  }
}

/* ── MIMEType ────────────────────────────────────────────────────────── */

typedef struct ScrMime {
  size_t rc;
  ScrStr *type;
  ScrStr *subtype;
  ScrMimeParams *params;
} ScrMime;

static void *scr_mime_retain_v(void *h) {
  ((ScrMime *)h)->rc++;
  return h;
}

static void scr_mime_release_v(void *h) {
  ScrMime *m = h;
  if (--m->rc) return;
  scr_str_release(m->type);
  scr_str_release(m->subtype);
  scr_mime_params_release_v(m->params);
  free(m);
}

/* parseTypeAndSubtype plus the parameters: a +1 MIMEType, or NULL — with
 * ERR_INVALID_MIME_SYNTAX thrown when `throws`, else silently. */
static ScrMime *scr_mime_parse_str(const ScrStr *input, bool throws) {
  ScrMimeU16 s = scr_mime_u16(input);
  size_t pos = 0;
  while (pos < s.n && scr_mime_ws(s.u[pos])) pos++;
  long type_end = scr_mime_index_of(s.u, s.n, '/', pos);
  size_t type_len = (type_end == -1 ? s.n : (size_t)type_end) - pos;
  long bad = scr_mime_not_token_at(s.u + pos, type_len);
  if (type_len == 0 || bad != -1 || type_end == -1) {
    if (throws) scr_mime_syntax_error("type", input, bad);
    free(s.u);
    return NULL;
  }
  uint16_t *type = s.u + pos;
  scr_mime_lower(type, type_len);
  pos = (size_t)type_end + 1;
  long sub_end = scr_mime_index_of(s.u, s.n, ';', pos);
  size_t raw_len = (sub_end == -1 ? s.n : (size_t)sub_end) - pos;
  uint16_t *subtype = s.u + pos;
  pos += raw_len;
  if (sub_end != -1) pos += 1;
  size_t sub_len = scr_mime_trailing_ws(subtype, raw_len);
  bad = scr_mime_not_token_at(subtype, sub_len);
  if (sub_len == 0 || bad != -1) {
    if (throws) scr_mime_syntax_error("subtype", input, bad);
    free(s.u);
    return NULL;
  }
  scr_mime_lower(subtype, sub_len);
  ScrMime *m = calloc(1, sizeof *m);
  if (!m) abort();
  m->rc = 1;
  m->type = scr_mime_str(type, type_len);
  m->subtype = scr_mime_str(subtype, sub_len);
  m->params = scr_mime_params_alloc();
  scr_mime_params_parse(m->params, s.u + pos, s.n - pos);
  free(s.u);
  return m;
}

/* MIMEType.prototype.toString: essence, then ';' and the parameters. +1. */
static ScrStr *scr_mime_string(const ScrMime *m) {
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_put_str(&b, m->type);
  scr_jb_putc(&b, '/');
  scr_jb_put_str(&b, m->subtype);
  ScrStr *params = scr_mime_params_string(m->params);
  if (params->len) {
    scr_jb_putc(&b, ';');
    scr_jb_put_str(&b, params);
  }
  scr_str_release(params);
  return scr_jb_finish(&b);
}

static ScrDyn *scr_mime_str_box(ScrStr *s) {
  ScrDyn *out = scr_dyn_new_str(s);
  scr_str_release(s);
  return out;
}

static void scr_mime_not_a_function(const char *what) {
  char msg[200];
  int n = snprintf(msg, sizeof msg, "%s is not a function", what);
  scr_throw_error_msg(SCR_ERR_TYPE, msg, n < 0 ? 0 : (size_t)n);
}

/* Node's V8 names the receiver "[object Object]" here (the prototype has its
 * own toString). */
static bool scr_mime_getter_only(const char *key, size_t len) {
  char msg[200];
  int n = snprintf(msg, sizeof msg, "Cannot set property %.*s of [object Object] which has only a getter", (int)len, key);
  scr_throw_error_msg(SCR_ERR_TYPE, msg, n < 0 ? 0 : (size_t)n);
  return true;
}

static bool scr_mime_key(const char *key, size_t len, const char *name) {
  return strlen(name) == len && memcmp(key, name, len) == 0;
}

static ScrDyn *scr_mime_params_box(ScrMimeParams *p);

/* A method read as a value (`m.toString` without a call): the handle
 * answers the method only when called, so the read throws the "not
 * supported yet" Error rather than answering undefined. */
static ScrDyn *scr_mime_method_value(const char *cls, const char *key, size_t len) {
  char msg[200];
  int n = snprintf(msg, sizeof msg, "'%s.prototype.%.*s' on a dynamic value is not supported yet", cls, (int)len,
                   key);
  scr_throw_error_msg(SCR_ERR_ERROR, msg, n < 0 ? 0 : (size_t)n);
  return NULL;
}

static ScrDyn *scr_mime_get(void *h, const char *key, size_t len) {
  ScrMime *m = h;
  if (scr_mime_key(key, len, "type")) return scr_dyn_new_str(m->type);
  if (scr_mime_key(key, len, "subtype")) return scr_dyn_new_str(m->subtype);
  if (scr_mime_key(key, len, "essence")) {
    ScrJsonBuf b;
    scr_jb_init(&b);
    scr_jb_put_str(&b, m->type);
    scr_jb_putc(&b, '/');
    scr_jb_put_str(&b, m->subtype);
    return scr_mime_str_box(scr_jb_finish(&b));
  }
  if (scr_mime_key(key, len, "params")) return scr_mime_params_box(m->params);
  if (scr_mime_key(key, len, "toString") || scr_mime_key(key, len, "toJSON"))
    return scr_mime_method_value("MIMEType", key, len);
  return NULL;
}

/* The type and subtype setters: ToString, a non-empty token, lowercased. */
static bool scr_mime_set(void *h, const char *key, size_t len, const ScrDyn *value) {
  ScrMime *m = h;
  bool type = scr_mime_key(key, len, "type");
  if (!type && !scr_mime_key(key, len, "subtype")) {
    if (scr_mime_key(key, len, "essence") || scr_mime_key(key, len, "params"))
      return scr_mime_getter_only(key, len);
    return false;
  }
  ScrStr *text = scr_mime_to_string(value);
  if (!text) return true;
  ScrMimeU16 u = scr_mime_u16(text);
  long bad = scr_mime_not_token_at(u.u, u.n);
  if (u.n == 0 || bad != -1) {
    scr_mime_syntax_error(type ? "type" : "subtype", text, bad);
  } else {
    scr_mime_lower(u.u, u.n);
    ScrStr **slot = type ? &m->type : &m->subtype;
    scr_str_release(*slot);
    *slot = scr_mime_str(u.u, u.n);
  }
  free(u.u);
  scr_str_release(text);
  return true;
}

static ScrDyn *scr_mime_invoke(void *h, ScrDyn *self, const char *method, ScrDyn *const *args, size_t argc,
                               const char *what) {
  (void)self;
  (void)args;
  (void)argc;
  if (!strcmp(method, "toString") || !strcmp(method, "toJSON")) return scr_mime_str_box(scr_mime_string(h));
  scr_mime_not_a_function(what);
  return NULL;
}

static ScrDyn *scr_mime_inspect_view(void *h) {
  (void)h;
  return scr_dyn_new_obj();
}

/* structuredClone copies no private fields: a plain empty object. */
static ScrDyn *scr_mime_clone(void *h) {
  (void)h;
  return scr_dyn_new_obj();
}

static const ScrDynHandleOps scr_mime_ops = {
    .cls = "MIMEType",
    .retain = scr_mime_retain_v,
    .release = scr_mime_release_v,
    .invoke = scr_mime_invoke,
    .get = scr_mime_get,
    .set = scr_mime_set,
    .inspect_view = scr_mime_inspect_view,
    .clone = scr_mime_clone,
    .inspect_view_empty = true,
};

/* ── MIMEParams handle ───────────────────────────────────────────────── */

static ScrDyn *scr_mime_params_get(void *h, const char *key, size_t len) {
  (void)h;
  static const char *const methods[] = {"get", "set", "has", "delete", "entries", "keys", "values",
                                        "toString", "toJSON", NULL};
  for (size_t i = 0; methods[i]; i++)
    if (scr_mime_key(key, len, methods[i])) return scr_mime_method_value("MIMEParams", key, len);
  return NULL;
}

static bool scr_mime_params_set(void *h, const char *key, size_t len, const ScrDyn *value) {
  (void)h;
  (void)key;
  (void)len;
  (void)value;
  return false;
}

/* A live step over the entries (selection 2), keys (1) or values (0). */
static ScrDyn *scr_mime_params_step(void *h, double *index, unsigned selection) {
  ScrMimeParams *p = h;
  size_t i = (size_t)*index;
  while (i < p->n && !p->keys[i]) i++;
  bool done = i >= p->n;
  ScrDyn *value;
  if (done) {
    *index = (double)p->n;
    value = scr_dyn_retain(scr_dyn_undefined());
  } else {
    *index = (double)(i + 1);
    if (selection == 2) {
      value = scr_dyn_new_arr();
      scr_dyn_arr_push(value, scr_dyn_new_str(p->keys[i]));
      scr_dyn_arr_push(value, scr_dyn_new_str(p->vals[i]));
    } else {
      value = scr_dyn_new_str(selection == 1 ? p->keys[i] : p->vals[i]);
    }
  }
  ScrDyn *result = scr_dyn_new_obj();
  scr_dyn_obj_set(result, "value", 5, value);
  scr_dyn_obj_set(result, "done", 4, scr_dyn_new_bool(done));
  return result;
}

/* The name argument: `${name}`, lowercased. +1, or NULL pending. */
static ScrStr *scr_mime_params_name(ScrDyn *const *args, size_t argc) {
  ScrStr *name = scr_mime_to_string(argc > 0 ? args[0] : scr_dyn_undefined());
  if (!name) return NULL;
  ScrMimeU16 u = scr_mime_u16(name);
  scr_mime_lower(u.u, u.n);
  ScrStr *lower = scr_mime_str(u.u, u.n);
  free(u.u);
  scr_str_release(name);
  return lower;
}

static ScrDyn *scr_mime_params_invoke(void *h, ScrDyn *self, const char *method, ScrDyn *const *args, size_t argc,
                                      const char *what) {
  ScrMimeParams *p = h;
  if (!strcmp(method, "entries") || !strcmp(method, "keys") || !strcmp(method, "values"))
    return scr_dyn_native_handle_iterator(self, !strcmp(method, "entries") ? 2 : !strcmp(method, "keys") ? 1 : 0);
  if (!strcmp(method, "toString") || !strcmp(method, "toJSON")) return scr_mime_str_box(scr_mime_params_string(p));
  bool get = !strcmp(method, "get"), has = !strcmp(method, "has"), del = !strcmp(method, "delete");
  bool set = !strcmp(method, "set");
  if (!get && !has && !del && !set) {
    scr_mime_not_a_function(what);
    return NULL;
  }
  ScrStr *name = scr_mime_params_name(args, argc);
  if (!name) return NULL;
  if (get || has) {
    long at = scr_mime_params_find(p, name);
    scr_str_release(name);
    if (has) return scr_dyn_new_bool(at >= 0);
    return at >= 0 ? scr_dyn_new_str(p->vals[at]) : scr_dyn_new_null();
  }
  if (del) {
    long at = scr_mime_params_find(p, name);
    scr_str_release(name);
    if (at >= 0) {
      scr_str_release(p->keys[at]);
      scr_str_release(p->vals[at]);
      p->keys[at] = NULL;
      p->vals[at] = NULL;
    }
    return scr_dyn_retain(scr_dyn_undefined());
  }
  ScrStr *value = scr_mime_to_string(argc > 1 ? args[1] : scr_dyn_undefined());
  if (!value) {
    scr_str_release(name);
    return NULL;
  }
  ScrMimeU16 n = scr_mime_u16(name), v = scr_mime_u16(value);
  long bad_name = scr_mime_not_token_at(n.u, n.n);
  long bad_value = scr_mime_not_quoted_at(v.u, v.n);
  free(n.u);
  free(v.u);
  if (n.n == 0 || bad_name != -1) {
    scr_mime_syntax_error("parameter name", name, bad_name);
  } else if (bad_value != -1) {
    scr_mime_syntax_error("parameter value", value, bad_value);
  } else {
    scr_mime_params_put(p, name, value);
    return scr_dyn_retain(scr_dyn_undefined());
  }
  scr_str_release(name);
  scr_str_release(value);
  return NULL;
}

static const ScrDynHandleOps scr_mime_params_ops = {
    .cls = "MIMEParams",
    .retain = scr_mime_params_retain_v,
    .release = scr_mime_params_release_v,
    .invoke = scr_mime_params_invoke,
    .get = scr_mime_params_get,
    .set = scr_mime_params_set,
    .iter_step = scr_mime_params_step,
    .inspect_view = scr_mime_inspect_view,
    .clone = scr_mime_clone,
    .inspect_view_empty = true,
};

static void scr_mime_install(void) {
  scr_dyn_handle_install(SCR_DYNH_MIME_TYPE, &scr_mime_ops);
  scr_dyn_handle_install(SCR_DYNH_MIME_PARAMS, &scr_mime_params_ops);
}

static ScrDyn *scr_mime_params_box(ScrMimeParams *p) {
  scr_mime_install();
  return scr_dyn_new_handle(p, SCR_DYNH_MIME_PARAMS);
}

static ScrDyn *scr_mime_box(ScrMime *m) {
  scr_mime_install();
  ScrDyn *out = scr_dyn_new_handle(m, SCR_DYNH_MIME_TYPE);
  scr_mime_release_v(m);
  return out;
}

/* ── entry points ────────────────────────────────────────────────────── */

ScrDyn *scr_mime_type_new(ScrDyn *args) {
  size_t argc = args->kind == SCR_DYN_ARR ? args->v.arr.len : 0;
  ScrStr *input = scr_mime_to_string(argc > 0 ? args->v.arr.items[0] : scr_dyn_undefined());
  if (!input) return NULL;
  ScrMime *m = scr_mime_parse_str(input, true);
  scr_str_release(input);
  return m ? scr_mime_box(m) : NULL;
}

ScrDyn *scr_mime_type_parse(ScrDyn *value) {
  ScrStr *input = scr_mime_to_string(value);
  if (!input) return NULL;
  ScrMime *m = scr_mime_parse_str(input, false);
  scr_str_release(input);
  return m ? scr_mime_box(m) : scr_dyn_new_null();
}

ScrDyn *scr_mime_params_new(void) {
  ScrMimeParams *p = scr_mime_params_alloc();
  ScrDyn *out = scr_mime_params_box(p);
  scr_mime_params_release_v(p);
  return out;
}

bool scr_mime_is(const ScrDyn *value, double kind) {
  return value && value->kind == SCR_DYN_HANDLE &&
         value->v.handle.tag == (kind == 0 ? SCR_DYNH_MIME_TYPE : SCR_DYNH_MIME_PARAMS);
}
