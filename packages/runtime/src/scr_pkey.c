/* node:crypto public and private KeyObjects and RSA/ECDSA signatures over
 * the vendored mbedTLS public-key code: createPrivateKey, createPublicKey,
 * the KeyObject getters, export() and equals(), crypto.sign, crypto.verify,
 * createSign and createVerify. Key input follows Node 24's
 * lib/internal/crypto/keys.js (prepareAsymmetricKey and parseKeyEncoding)
 * and ncrypto's PEM and DER parsing order; export writes the DER and PEM
 * that OpenSSL 3 writes for RSA (pkcs1, pkcs8, spki) and EC (sec1, pkcs8,
 * spki) keys, and JWK. Signatures follow lib/internal/crypto/sig.js and
 * OpenSSL's providers: PKCS#1 v1.5 and PSS padding with OpenSSL's salt
 * length rules for RSA, and ECDSA with DER or IEEE P1363 encoding. The
 * message digest comes from the runtime's hash core, so every digest name
 * createHash accepts resolves the same way. Keys of other types (Ed25519,
 * X25519, DSA, DH, RSA-PSS), curves mbedTLS does not implement, encrypted
 * export and the OpenSSL STORE inputs throw a coded SC2020 Error. All
 * arguments are borrowed. */
#include "scr_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/asn1.h"
#include "mbedtls/base64.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/rsa.h"

/* ── errors ───────────────────────────────────────────────────────────── */

static void pk_throw(int kind, const char *code, const char *message) {
  if (code) scr_throw_error_msg_code(kind, message, strlen(message), code);
  else scr_throw_error_msg(kind, message, strlen(message));
}

static void pk_refuse(const char *message) {
  pk_throw(SCR_ERR_ERROR, "SC2020", message);
}

static int pk_rng(void *context, unsigned char *out, size_t len) {
  (void)context;
  arc4random_buf(out, len);
  return 0;
}

/* An OpenSSL error as Node's ThrowCryptoError reports it: the message,
 * an own opensslErrorStack when the error queue held more entries (stack,
 * NULL for none), then library, reason and code when library is
 * non-NULL. */
static void pk_throw_openssl(const char *message, const char *library, const char *reason,
                             const char *code, const char *const *stack, size_t stack_len) {
  ScrStr *text = scr_str_new(message, strlen(message));
  ScrError *e = scr_error_new(SCR_ERR_ERROR, text);
  scr_str_release(text);
  if (stack) {
    ScrDyn *list = scr_dyn_new_arr();
    for (size_t i = 0; i < stack_len; i++) {
      ScrStr *entry = scr_str_new(stack[i], strlen(stack[i]));
      scr_dyn_arr_push(list, scr_dyn_new_str(entry));
      scr_str_release(entry);
    }
    scr_error_set_prop(e, "opensslErrorStack", 17, list);
  }
  if (library) {
    ScrStr *value = scr_str_new(library, strlen(library));
    scr_error_set_prop(e, "library", 7, scr_dyn_new_str(value));
    scr_str_release(value);
    value = scr_str_new(reason, strlen(reason));
    scr_error_set_prop(e, "reason", 6, scr_dyn_new_str(value));
    scr_str_release(value);
  }
  if (code) {
    scr_error_set_code(e, code);
    ScrStr *value = scr_str_new(code, strlen(code));
    scr_error_set_prop(e, "code", 4, scr_dyn_new_str(value));
    scr_str_release(value);
  }
  scr_throw_obj(e, &scr_error_retain_v, &scr_error_release_v, scr_error_trace_arg());
}

/* An OpenSSL error "error:<packed>:<library>::<reason>". The decoders
 * queue it twice, so the stack repeats the message; the providers queue
 * it once. */
static void pk_openssl(const char *packed, const char *library, const char *reason, const char *code) {
  char message[160];
  snprintf(message, sizeof message, "error:%s:%s::%s", packed, library, reason);
  const char *const stack[] = {message};
  pk_throw_openssl(message, library, reason, code, stack, 1);
}

static void pk_provider(const char *packed, const char *reason, const char *code) {
  char message[160];
  snprintf(message, sizeof message, "error:%s:Provider routines::%s", packed, reason);
  pk_throw_openssl(message, "Provider routines", reason, code, NULL, 0);
}

/* EVP_DigestFinal over an extendable-output digest in a Sign or Verify. */
static void pk_xof_final(void) {
  pk_throw_openssl("error:078C0100:common libcrypto routines::malloc failure", "common libcrypto routines",
                   "malloc failure", "ERR_OSSL_CRYPTO_MALLOC_FAILURE", NULL, 0);
}

static void pk_decoder_unsupported(void) {
  pk_openssl("1E08010C", "DECODER routines", "unsupported", "ERR_OSSL_UNSUPPORTED");
}

/* Where an encrypted private key came from: OpenSSL reports each reader's
 * passphrase errors with its own error queue. */
typedef enum { PK_FROM_PEM_PKCS8, PK_FROM_PEM_LEGACY, PK_FROM_DER } PkSource;

/* An encrypted key read without a passphrase. */
static void pk_passphrase_unavailable(PkSource source) {
  if (source == PK_FROM_DER) {
    pk_throw(SCR_ERR_TYPE, "ERR_MISSING_PASSPHRASE", "Passphrase required for encrypted key");
    return;
  }
  static const char *const pkcs8[] = {
    "error:04800068:PEM routines::bad password read",
    "error:07880109:common libcrypto routines::interrupted or cancelled",
    "error:1C80009F:Provider routines::unable to get passphrase",
    "error:07880109:common libcrypto routines::interrupted or cancelled",
    "error:07880109:common libcrypto routines::interrupted or cancelled",
  };
  static const char *const legacy[] = {
    "error:04800068:PEM routines::bad password read",
    "error:07880109:common libcrypto routines::interrupted or cancelled",
    "error:07880109:common libcrypto routines::interrupted or cancelled",
  };
  bool is_pkcs8 = source == PK_FROM_PEM_PKCS8;
  pk_throw_openssl("error:07880109:common libcrypto routines::interrupted or cancelled",
                   "common libcrypto routines", "interrupted or cancelled",
                   "ERR_OSSL_CRYPTO_INTERRUPTED_OR_CANCELLED", is_pkcs8 ? pkcs8 : legacy,
                   is_pkcs8 ? sizeof pkcs8 / sizeof pkcs8[0] : sizeof legacy / sizeof legacy[0]);
}

/* An encrypted key read with the wrong passphrase. */
static void pk_bad_decrypt(PkSource source) {
  static const char *const pkcs8[] = {
    "error:11800074:PKCS12 routines::pkcs12 cipherfinal error",
    "error:1C800064:Provider routines::bad decrypt",
    "error:11800074:PKCS12 routines::pkcs12 cipherfinal error",
    "error:1C800064:Provider routines::bad decrypt",
  };
  static const char *const legacy[] = {
    "error:04800065:PEM routines::bad decrypt",
    "error:1C800064:Provider routines::bad decrypt",
  };
  const char *const *stack = source == PK_FROM_PEM_LEGACY ? legacy : pkcs8;
  size_t n = source == PK_FROM_PEM_PKCS8 ? 4 : 2;
  pk_throw_openssl("error:1C800064:Provider routines::bad decrypt", "Provider routines", "bad decrypt",
                   "ERR_OSSL_BAD_DECRYPT", stack, n);
}

/* The DER decoders' errors for a malformed private key: an outer element
 * that does not fit the input reports "not enough data", and a SEQUENCE
 * whose contents do not match the structure reports "wrong tag" under
 * two nested errors, as d2i reports them. */
static void pk_der_failure(const unsigned char *der, size_t len) {
  unsigned char *p = (unsigned char *)der, *end = p + len;
  size_t n;
  if (len < 2 || mbedtls_asn1_get_len(&(unsigned char *){p + 1}, end, &n) != 0) {
    pk_openssl("0680008E", "asn1 encoding routines", "not enough data", "ERR_OSSL_ASN1_NOT_ENOUGH_DATA");
    return;
  }
  if (der[0] != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
    pk_openssl("068000A8", "asn1 encoding routines", "wrong tag", "ERR_OSSL_ASN1_WRONG_TAG");
    return;
  }
  static const char *const stack[] = {
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:068000A8:asn1 encoding routines::wrong tag",
  };
  pk_throw_openssl("error:068000A8:asn1 encoding routines::wrong tag", "asn1 encoding routines", "wrong tag",
                   "ERR_OSSL_ASN1_WRONG_TAG", stack, sizeof stack / sizeof stack[0]);
}

/* A failed public key structure: "Failed to read asymmetric key" with the
 * decoder's error stack and no code. */
static void pk_public_der_failure(bool pkcs1) {
  static const char *const spki[] = {
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:06800066:asn1 encoding routines::bad object header",
    "error:0680009B:asn1 encoding routines::too long",
  };
  static const char *const rsa[] = {"error:1E08010C:DECODER routines::unsupported"};
  pk_throw_openssl("Failed to read asymmetric key", NULL, NULL, NULL, pkcs1 ? rsa : spki,
                   pkcs1 ? 1 : sizeof spki / sizeof spki[0]);
}

/* ERR_INVALID_ARG_VALUE's property flavor: "The property 'x' is invalid.
 * Received <value>", with an optional reason in place of "is invalid". */
static void pk_invalid_property(const char *name, const ScrDyn *value, const char *reason) {
  char rendered[160];
  const char *text = scr_dyn_inspect_lite(value, rendered, sizeof rendered);
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, "The property '");
  scr_jb_puts(&b, name);
  scr_jb_puts(&b, "' ");
  scr_jb_puts(&b, reason ? reason : "is invalid");
  scr_jb_puts(&b, ". Received ");
  scr_jb_puts(&b, text);
  ScrStr *message = scr_jb_finish(&b);
  scr_throw_error_msg_code(SCR_ERR_TYPE, message->data, message->len, "ERR_INVALID_ARG_VALUE");
  scr_str_release(message);
}

static void pk_incompatible(const char *encoding, const char *problem) {
  char message[160];
  if (encoding) snprintf(message, sizeof message, "The selected key encoding %s %s.", encoding, problem);
  else snprintf(message, sizeof message, "The selected key encoding is incompatible with the key type");
  pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS", message);
}

static void pk_invalid_jwk(const char *message) {
  pk_throw(SCR_ERR_TYPE, "ERR_CRYPTO_INVALID_JWK", message);
}

static void pk_key_object_type(const char *got, const char *expected) {
  char message[96];
  snprintf(message, sizeof message, "Invalid key object type %s, expected %s.", got, expected);
  pk_throw(SCR_ERR_TYPE, "ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE", message);
}

static void pk_not_a_function(const char *what) {
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, what);
  scr_jb_puts(&b, " is not a function");
  scr_throw_error(SCR_ERR_TYPE, scr_jb_finish(&b));
}

/* V8's TypeError for a property read on null or undefined. */
static void pk_nullish_read(const ScrDyn *value, const char *property) {
  char message[128];
  snprintf(message, sizeof message, "Cannot read properties of %s (reading '%s')",
           value->kind == SCR_DYN_NULL ? "null" : "undefined", property);
  pk_throw(SCR_ERR_TYPE, NULL, message);
}

/* ── argument values ──────────────────────────────────────────────────── */

static bool pk_ascii_eq_ci(const char *a, size_t a_len, const char *b) {
  size_t b_len = strlen(b);
  if (a_len != b_len) return false;
  for (size_t i = 0; i < a_len; i++) {
    char x = a[i];
    if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
    if (x != b[i]) return false;
  }
  return true;
}

static bool pk_str_is(const ScrDyn *value, const char *literal) {
  if (value->kind != SCR_DYN_STR) return false;
  size_t len = strlen(literal);
  return value->v.str->len == len && memcmp(value->v.str->data, literal, len) == 0;
}

static bool pk_nullish(const ScrDyn *value) {
  return value->kind == SCR_DYN_UNDEF || value->kind == SCR_DYN_NULL;
}

/* Node's normalizeEncoding: the canonical name, or NULL. */
static const char *pk_normalize_encoding(const ScrStr *name) {
  static const struct { const char *from; const char *to; } map[] = {
    {"utf8", "utf8"},       {"utf-8", "utf8"},       {"hex", "hex"},
    {"base64", "base64"},   {"base64url", "base64url"}, {"latin1", "latin1"},
    {"binary", "latin1"},   {"ascii", "ascii"},      {"utf16le", "utf16le"},
    {"utf-16le", "utf16le"}, {"ucs2", "utf16le"},    {"ucs-2", "utf16le"},
  };
  for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
    if (pk_ascii_eq_ci(name->data, name->len, map[i].from)) return map[i].to;
  return NULL;
}

static void pk_unknown_encoding(const ScrDyn *encoding) {
  ScrStr *text = scr_dyn_string_coerce_js(encoding);
  if (!text) return;
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, "Unknown encoding: ");
  scr_jb_put_str(&b, text);
  scr_str_release(text);
  ScrStr *message = scr_jb_finish(&b);
  scr_throw_error_msg_code(SCR_ERR_TYPE, message->data, message->len, "ERR_UNKNOWN_ENCODING");
  scr_str_release(message);
}

/* Buffer.from(string, encoding) as getArrayBufferOrView and
 * encodeKeyString call it: a non-string or empty encoding and "buffer"
 * read as utf8; an unknown name throws ERR_UNKNOWN_ENCODING. +1 or NULL. */
static ScrBytes *pk_string_bytes(const ScrStr *text, const ScrDyn *encoding) {
  const char *name = "utf8";
  if (encoding && encoding->kind == SCR_DYN_STR && encoding->v.str->len > 0 &&
      !pk_str_is(encoding, "buffer")) {
    name = pk_normalize_encoding(encoding->v.str);
    if (!name) {
      pk_unknown_encoding(encoding);
      return NULL;
    }
  }
  ScrStr *canonical = scr_str_new(name, strlen(name));
  ScrBytes *bytes = scr_bytes_from_str(text, canonical);
  scr_str_release(canonical);
  return bytes;
}

static bool pk_is_view(const ScrDyn *value) {
  return value->kind == SCR_DYN_BYTES;
}

static bool pk_is_buffer_source(const ScrDyn *value) {
  return pk_is_view(value) || scr_buffer_storage_is(value);
}

/* getArrayBufferOrView(value, name, encoding). +1 or NULL. */
static ScrBytes *pk_buffer_or_view(const ScrDyn *value, const char *name, const ScrDyn *encoding) {
  if (value->kind == SCR_DYN_STR) return pk_string_bytes(value->v.str, encoding);
  if (pk_is_buffer_source(value)) return scr_bytes_buffer_source(value);
  scr_dyn_arg_type_fail(name, "of type string or an instance of ArrayBuffer, Buffer, TypedArray, or DataView",
                        value);
  return NULL;
}

/* The value a typed-ref box holds when it is a string, view, buffer or
 * native handle (+1); other values are retained as passed. NULL after
 * throwing. */
static ScrDyn *pk_unbox(ScrDyn *value) {
  if (value->kind != SCR_DYN_TYPED_REF) return scr_dyn_retain(value);
  ScrDyn *view = scr_dyn_typed_ref_materialize(value);
  if (!view) return NULL;
  if (view->kind == SCR_DYN_STR || pk_is_view(view) || view->kind == SCR_DYN_HANDLE || scr_buffer_storage_is(view))
    return view;
  scr_dyn_release(view);
  return scr_dyn_retain(value);
}

static bool pk_is_object(const ScrDyn *value) {
  return value->kind == SCR_DYN_OBJ || value->kind == SCR_DYN_TYPED_REF || value->kind == SCR_DYN_PROXY ||
         value->kind == SCR_DYN_ARR || value->kind == SCR_DYN_FUNC || value->kind == SCR_DYN_HANDLE ||
         pk_is_buffer_source(value);
}

/* value[key] for an object (+1, undefined when absent), undefined for a
 * primitive. NULL after a getter threw. */
static ScrDyn *pk_read(const ScrDyn *value, const char *key) {
  if (value->kind == SCR_DYN_OBJ || value->kind == SCR_DYN_TYPED_REF || value->kind == SCR_DYN_PROXY ||
      value->kind == SCR_DYN_ARR) {
    ScrDyn *view = value->kind == SCR_DYN_TYPED_REF ? scr_dyn_typed_ref_materialize(value)
                                                    : scr_dyn_retain((ScrDyn *)value);
    if (!view) return NULL;
    ScrDyn *result = scr_dyn_obj_read(view, key, strlen(key));
    scr_dyn_release(view);
    if (!result && !scr_exc_pending()) result = scr_dyn_retain(scr_dyn_undefined());
    if (!result) return NULL;
    ScrDyn *unboxed = pk_unbox(result);
    scr_dyn_release(result);
    return unboxed;
  }
  return scr_dyn_retain(scr_dyn_undefined());
}

static ScrDyn *pk_buffer_dyn(const unsigned char *data, size_t len) {
  ScrBytes *bytes = scr_bytes_new(SCR_BYTES_U8, (double)len);
  if (len) memcpy(bytes->data, data, len);
  bytes->is_buffer = true;
  ScrDyn *result = scr_dyn_new_bytes(bytes);
  scr_bytes_release(bytes);
  return result;
}

static ScrDyn *pk_str_dyn(const char *data, size_t len) {
  ScrStr *text = scr_str_new(data, len);
  ScrDyn *result = scr_dyn_new_str(text);
  scr_str_release(text);
  return result;
}

static ScrDyn *pk_cstr_dyn(const char *text) {
  return pk_str_dyn(text, strlen(text));
}

/* ── byte buffers and DER ─────────────────────────────────────────────── */

typedef struct {
  unsigned char *data;
  size_t len, cap;
} PkBuf;

static void pk_buf_reserve(PkBuf *b, size_t extra) {
  if (b->len + extra <= b->cap) return;
  size_t cap = b->cap ? b->cap * 2 : 256;
  while (cap < b->len + extra) cap *= 2;
  unsigned char *data = realloc(b->data, cap);
  if (!data) scr_trap("scriptc: out of memory\n");
  b->data = data;
  b->cap = cap;
}

static void pk_buf_put(PkBuf *b, const void *data, size_t len) {
  pk_buf_reserve(b, len);
  if (len) memcpy(b->data + b->len, data, len);
  b->len += len;
}

static void pk_buf_byte(PkBuf *b, unsigned char c) {
  pk_buf_put(b, &c, 1);
}

static void pk_buf_free(PkBuf *b) {
  if (b->data) mbedtls_platform_zeroize(b->data, b->cap);
  free(b->data);
  b->data = NULL;
  b->len = b->cap = 0;
}

static void pk_der_header(PkBuf *b, unsigned char tag, size_t len) {
  pk_buf_byte(b, tag);
  if (len < 0x80) {
    pk_buf_byte(b, (unsigned char)len);
    return;
  }
  unsigned char bytes[sizeof(size_t)];
  size_t n = 0;
  for (size_t v = len; v; v >>= 8) bytes[n++] = (unsigned char)(v & 0xff);
  pk_buf_byte(b, (unsigned char)(0x80 | n));
  while (n) pk_buf_byte(b, bytes[--n]);
}

static void pk_der_put(PkBuf *b, unsigned char tag, const unsigned char *content, size_t len) {
  pk_der_header(b, tag, len);
  pk_buf_put(b, content, len);
}

/* A DER INTEGER from unsigned big-endian bytes: leading zeros dropped,
 * a zero prepended when the top bit is set. */
static void pk_der_uint(PkBuf *b, const unsigned char *data, size_t len) {
  while (len > 1 && data[0] == 0) {
    data++;
    len--;
  }
  bool pad = len == 0 || (data[0] & 0x80);
  pk_der_header(b, MBEDTLS_ASN1_INTEGER, len + (pad ? 1 : 0));
  if (pad) pk_buf_byte(b, 0);
  pk_buf_put(b, data, len);
}

/* ASN.1 object identifiers of the algorithms Node reports by name. */
static const unsigned char pk_oid_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const unsigned char pk_oid_ec[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};

static const struct {
  const char *name;
  unsigned char len;
  unsigned char oid[10];
} pk_other_algorithms[] = {
  {"Ed25519", 3, {0x2b, 0x65, 0x70}},
  {"Ed448", 3, {0x2b, 0x65, 0x71}},
  {"X25519", 3, {0x2b, 0x65, 0x6e}},
  {"X448", 3, {0x2b, 0x65, 0x6f}},
  {"RSA-PSS", 9, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a}},
  {"DSA", 7, {0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x01}},
  {"DH", 9, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x03, 0x01}},
  {"DH", 7, {0x2a, 0x86, 0x48, 0xce, 0x3e, 0x02, 0x01}},
};

/* The AlgorithmIdentifier OID of a PrivateKeyInfo (pkcs8) or a
 * SubjectPublicKeyInfo (spki), or false when the structure is neither. */
static bool pk_algorithm_oid(const unsigned char *der, size_t len, bool private_info,
                             const unsigned char **oid, size_t *oid_len) {
  unsigned char *p = (unsigned char *)der, *end = p + len;
  size_t n;
  if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) return false;
  end = p + n;
  if (private_info) {
    int version;
    if (mbedtls_asn1_get_int(&p, end, &version) != 0) return false;
  }
  if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) return false;
  unsigned char *alg_end = p + n;
  if (mbedtls_asn1_get_tag(&p, alg_end, &n, MBEDTLS_ASN1_OID) != 0) return false;
  *oid = p;
  *oid_len = n;
  return true;
}

/* An SC2020 refusal naming a key type mbedTLS does not implement, when
 * der is a PrivateKeyInfo or SubjectPublicKeyInfo of one. */
static bool pk_refuse_algorithm(const unsigned char *der, size_t len, bool private_info) {
  const unsigned char *oid;
  size_t oid_len;
  if (!pk_algorithm_oid(der, len, private_info, &oid, &oid_len)) return false;
  const char *name = NULL;
  for (size_t i = 0; i < sizeof pk_other_algorithms / sizeof pk_other_algorithms[0]; i++)
    if (pk_other_algorithms[i].len == oid_len && memcmp(pk_other_algorithms[i].oid, oid, oid_len) == 0)
      name = pk_other_algorithms[i].name;
  bool ec = oid_len == sizeof pk_oid_ec && memcmp(oid, pk_oid_ec, oid_len) == 0;
  if (!name && !ec) return false;
  char message[256];
  if (ec)
    snprintf(message, sizeof message,
             "EC keys on this curve have no native implementation; the supported curves are "
             "prime256v1, secp384r1, secp521r1, secp256k1, secp224r1, prime192v1, secp192k1, "
             "secp224k1, brainpoolP256r1, brainpoolP384r1 and brainpoolP512r1");
  else
    snprintf(message, sizeof message, "%s keys have no native implementation; RSA and EC keys are supported", name);
  pk_refuse(message);
  return true;
}

/* ── PEM ──────────────────────────────────────────────────────────────── */

typedef struct {
  char label[48];
  const unsigned char *start; /* "-----BEGIN" */
  const unsigned char *end;   /* past the END line's dashes */
  const unsigned char *body;
  size_t body_len;
  bool encrypted; /* a Proc-Type: 4,ENCRYPTED header */
} PkPem;

static const unsigned char *pk_find(const unsigned char *from, const unsigned char *end, const char *text) {
  size_t n = strlen(text);
  for (const unsigned char *p = from; p + n <= end; p++)
    if (memcmp(p, text, n) == 0) return p;
  return NULL;
}

/* The next PEM block at or after `from`. */
static bool pk_pem_next(const unsigned char *from, const unsigned char *end, PkPem *pem) {
  for (const unsigned char *p = from; (p = pk_find(p, end, "-----BEGIN ")) != NULL; p++) {
    const unsigned char *label = p + 11;
    const unsigned char *close = pk_find(label, end, "-----");
    if (!close || (size_t)(close - label) >= sizeof pem->label) continue;
    bool line = true;
    for (const unsigned char *c = label; c < close; c++)
      if (*c == '\n' || *c == '\r') line = false;
    if (!line) continue;
    size_t label_len = (size_t)(close - label);
    char footer[72];
    snprintf(footer, sizeof footer, "-----END %.*s-----", (int)label_len, (const char *)label);
    const unsigned char *body = close + 5;
    const unsigned char *stop = pk_find(body, end, footer);
    if (!stop) return false;
    memcpy(pem->label, label, label_len);
    pem->label[label_len] = '\0';
    pem->start = p;
    pem->end = stop + strlen(footer);
    pem->encrypted = false;
    /* RFC 1421 headers precede a blank line. */
    const unsigned char *headers = body;
    while (headers < stop && (*headers == '\r' || *headers == '\n')) headers++;
    const unsigned char *colon = NULL;
    for (const unsigned char *c = headers; c < stop && *c != '\n'; c++)
      if (*c == ':') colon = c;
    if (colon) {
      const unsigned char *proc = pk_find(headers, stop, "Proc-Type: 4,ENCRYPTED");
      pem->encrypted = proc != NULL;
      const unsigned char *blank = pk_find(headers, stop, "\n\n");
      const unsigned char *blank_crlf = pk_find(headers, stop, "\r\n\r\n");
      if (blank_crlf && (!blank || blank_crlf < blank)) blank = blank_crlf + 2;
      body = blank ? blank + 2 : stop;
    }
    pem->body = body;
    pem->body_len = (size_t)(stop - body);
    return true;
  }
  return false;
}

/* The base64 body of a PEM block as DER (whitespace skipped). */
static bool pk_pem_decode(const PkPem *pem, PkBuf *der) {
  PkBuf text = {0};
  for (size_t i = 0; i < pem->body_len; i++) {
    unsigned char c = pem->body[i];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') pk_buf_byte(&text, c);
  }
  size_t out_len = 0;
  pk_buf_reserve(der, text.len + 1);
  int ret = mbedtls_base64_decode(der->data, der->cap, &out_len, text.data ? text.data : (unsigned char *)"",
                                  text.len);
  pk_buf_free(&text);
  if (ret != 0 || out_len == 0) return false;
  der->len = out_len;
  return true;
}

static bool pk_label_is_private(const char *label) {
  size_t n = strlen(label);
  return n >= 11 && strcmp(label + n - 11, "PRIVATE KEY") == 0;
}

static bool pk_label_is(const char *label, const char *name) {
  return strcmp(label, name) == 0;
}

/* ── keys ─────────────────────────────────────────────────────────────── */

typedef struct PkKey {
  size_t rc;
  bool is_private;
  bool no_private; /* a public JWK passed where a private key goes */
  mbedtls_pk_context pk;
} PkKey;

static void *pk_key_retain(void *h) {
  ((PkKey *)h)->rc++;
  return h;
}

static void pk_key_release(void *h) {
  PkKey *key = h;
  if (!key || --key->rc) return;
  mbedtls_pk_free(&key->pk);
  mbedtls_platform_zeroize(key, sizeof *key);
  free(key);
}

static PkKey *pk_key_new(bool is_private) {
  PkKey *key = calloc(1, sizeof *key);
  if (!key) scr_trap("scriptc: out of memory\n");
  key->rc = 1;
  key->is_private = is_private;
  mbedtls_pk_init(&key->pk);
  return key;
}

static bool pk_is_rsa(const PkKey *key) {
  return mbedtls_pk_get_type(&key->pk) == MBEDTLS_PK_RSA;
}

static PkKey *pk_key_of(const ScrDyn *value) {
  if (value->kind != SCR_DYN_HANDLE) return NULL;
  if (value->v.handle.tag != SCR_DYNH_PRIVATE_KEY_OBJECT && value->v.handle.tag != SCR_DYNH_PUBLIC_KEY_OBJECT)
    return NULL;
  return value->v.handle.ptr;
}

/* OpenSSL's names for the curves mbedTLS implements. */
static const struct {
  mbedtls_ecp_group_id id;
  const char *name;
  const char *jwk;
} pk_curves[] = {
  {MBEDTLS_ECP_DP_SECP256R1, "prime256v1", "P-256"},
  {MBEDTLS_ECP_DP_SECP384R1, "secp384r1", "P-384"},
  {MBEDTLS_ECP_DP_SECP521R1, "secp521r1", "P-521"},
  {MBEDTLS_ECP_DP_SECP256K1, "secp256k1", "secp256k1"},
  {MBEDTLS_ECP_DP_SECP224R1, "secp224r1", NULL},
  {MBEDTLS_ECP_DP_SECP192R1, "prime192v1", NULL},
  {MBEDTLS_ECP_DP_SECP192K1, "secp192k1", NULL},
  {MBEDTLS_ECP_DP_SECP224K1, "secp224k1", NULL},
  {MBEDTLS_ECP_DP_BP256R1, "brainpoolP256r1", NULL},
  {MBEDTLS_ECP_DP_BP384R1, "brainpoolP384r1", NULL},
  {MBEDTLS_ECP_DP_BP512R1, "brainpoolP512r1", NULL},
};

static size_t pk_curve_index(mbedtls_ecp_group_id id) {
  for (size_t i = 0; i < sizeof pk_curves / sizeof pk_curves[0]; i++)
    if (pk_curves[i].id == id) return i;
  return (size_t)-1;
}

static mbedtls_ecp_group_id pk_ec_group(const PkKey *key) {
  return mbedtls_ecp_keypair_get_group_id(mbedtls_pk_ec(key->pk));
}

/* The byte length of a curve's field elements and of its order. */
static size_t pk_ec_field_bytes(mbedtls_ecp_group_id id) {
  const mbedtls_ecp_curve_info *info = mbedtls_ecp_curve_info_from_grp_id(id);
  return info ? (info->bit_size + 7) / 8 : 0;
}

static size_t pk_ec_order_bytes(const PkKey *key) {
  mbedtls_ecp_group grp;
  mbedtls_ecp_group_init(&grp);
  size_t n = 0;
  if (mbedtls_ecp_group_load(&grp, pk_ec_group(key)) == 0) n = (mbedtls_mpi_bitlen(&grp.N) + 7) / 8;
  mbedtls_ecp_group_free(&grp);
  return n;
}

/* A public key holding only the public half of key. */
static PkKey *pk_public_of(const PkKey *key) {
  unsigned char der[2600];
  int n = mbedtls_pk_write_pubkey_der(&key->pk, der, sizeof der);
  if (n <= 0) {
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED", "Failed to encode public key");
    return NULL;
  }
  PkKey *pub = pk_key_new(false);
  if (mbedtls_pk_parse_public_key(&pub->pk, der + sizeof der - (size_t)n, (size_t)n) != 0) {
    pk_key_release(pub);
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED", "Failed to encode public key");
    return NULL;
  }
  return pub;
}

/* ── parsing ──────────────────────────────────────────────────────────── */

typedef enum { PK_TYPE_NONE, PK_TYPE_PKCS1, PK_TYPE_PKCS8, PK_TYPE_SPKI, PK_TYPE_SEC1 } PkEncodingType;

typedef struct {
  bool der; /* format 'der'; PEM otherwise */
  PkEncodingType type;
  const unsigned char *passphrase;
  size_t passphrase_len;
  bool has_passphrase;
} PkInput;

/* ncrypto's IsASN1Sequence. */
static bool pk_asn1_sequence(const unsigned char *data, size_t size, size_t *offset, size_t *len) {
  if (size < 2 || data[0] != 0x30) return false;
  if (data[1] & 0x80) {
    size_t n = data[1] & 0x7f;
    if (n + 2 > size || n > sizeof(size_t)) return false;
    size_t length = 0;
    for (size_t i = 0; i < n; i++) length = (length << 8) | data[i + 2];
    *offset = 2 + n;
    *len = size - 2 - n < length ? size - 2 - n : length;
  } else {
    *offset = 2;
    *len = size - 2 < data[1] ? size - 2 : data[1];
  }
  return true;
}

static bool pk_encrypted_info(const unsigned char *data, size_t len) {
  size_t offset, n;
  return pk_asn1_sequence(data, len, &offset, &n) && n >= 1 && data[offset] != 2;
}

static bool pk_rsa_private_structure(const unsigned char *data, size_t len) {
  size_t offset, n;
  return pk_asn1_sequence(data, len, &offset, &n) && n >= 3 && data[offset] == 2 && data[offset + 1] == 1 &&
         !(data[offset + 2] & 0xfe);
}

/* A PrivateKeyInfo begins INTEGER version, SEQUENCE algorithm. */
static bool pk_private_info_structure(const unsigned char *data, size_t len) {
  unsigned char *p = (unsigned char *)data, *end = p + len;
  size_t n;
  int version;
  if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) return false;
  end = p + n;
  if (mbedtls_asn1_get_int(&p, end, &version) != 0) return false;
  return p < end && *p == (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
}

/* d2i_AutoPrivateKey's errors when no decoder reads a SEQUENCE. */
static void pk_auto_failure(void) {
  static const char *const stack[] = {
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:068000A8:asn1 encoding routines::wrong tag",
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:0688010A:asn1 encoding routines::nested asn1 error",
    "error:068000A8:asn1 encoding routines::wrong tag",
    "error:1E08010C:DECODER routines::unsupported",
  };
  pk_throw_openssl("error:1E08010C:DECODER routines::unsupported", "DECODER routines", "unsupported",
                   "ERR_OSSL_UNSUPPORTED", stack, sizeof stack / sizeof stack[0]);
}

/* Reports a failed parse of a private key and answers NULL. */
static PkKey *pk_private_failure(int ret, const unsigned char *der, size_t len, PkSource source, const PkInput *in) {
  if (ret == MBEDTLS_ERR_PK_PASSWORD_REQUIRED) {
    pk_passphrase_unavailable(source);
    return NULL;
  }
  if (ret == MBEDTLS_ERR_PK_PASSWORD_MISMATCH) {
    if (in->has_passphrase) pk_bad_decrypt(source);
    else pk_passphrase_unavailable(source);
    return NULL;
  }
  if (ret == MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE) {
    pk_refuse("The key's encryption has no native implementation; encrypted keys with PBES2 over AES-CBC or DES are supported");
    return NULL;
  }
  if (der && pk_refuse_algorithm(der, len, true)) return NULL;
  if (source != PK_FROM_DER || !der) pk_decoder_unsupported();
  else if (der[0] == (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) && len >= 2) pk_auto_failure();
  else pk_der_failure(der, len);
  return NULL;
}

/* A private key from DER: PrivateKeyInfo, EncryptedPrivateKeyInfo
 * (decrypted with the passphrase), RSAPrivateKey or ECPrivateKey. */
static PkKey *pk_parse_private_der(const unsigned char *der, size_t len, const PkInput *in, PkSource source) {
  PkKey *key = pk_key_new(true);
  int ret;
  if (pk_encrypted_info(der, len)) {
    if (!in->has_passphrase) {
      pk_key_release(key);
      pk_passphrase_unavailable(source);
      return NULL;
    }
    /* mbedTLS skips decryption for an empty passphrase; the key then does
     * not decrypt, as OpenSSL's does not. */
    ret = in->passphrase_len ? mbedtls_pk_parse_key(&key->pk, der, len, in->passphrase, in->passphrase_len,
                                                    pk_rng, NULL)
                             : MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
  } else {
    ret = mbedtls_pk_parse_key(&key->pk, der, len, NULL, 0, pk_rng, NULL);
  }
  if (ret == 0 && mbedtls_pk_get_type(&key->pk) != MBEDTLS_PK_RSA && mbedtls_pk_get_type(&key->pk) != MBEDTLS_PK_ECKEY)
    ret = MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
  if (ret != 0) {
    pk_key_release(key);
    return pk_private_failure(ret, der, len, source, in);
  }
  return key;
}

/* PEM_read_bio_PrivateKey: the first block whose label names a private
 * key. A legacy encrypted block (Proc-Type: 4,ENCRYPTED) decrypts with
 * the passphrase through mbedTLS's PEM reader. */
static PkKey *pk_parse_private_pem(const unsigned char *data, size_t len, const PkInput *in) {
  const unsigned char *end = data + len;
  PkPem pem;
  for (const unsigned char *at = data; pk_pem_next(at, end, &pem); at = pem.end) {
    if (!pk_label_is_private(pem.label)) continue;
    if (pem.encrypted) {
      if (!in->has_passphrase) {
        pk_passphrase_unavailable(PK_FROM_PEM_LEGACY);
        return NULL;
      }
      size_t block_len = (size_t)(pem.end - pem.start);
      unsigned char *block = malloc(block_len + 1);
      if (!block) scr_trap("scriptc: out of memory\n");
      memcpy(block, pem.start, block_len);
      block[block_len] = '\0';
      PkKey *key = pk_key_new(true);
      int ret = in->passphrase_len ? mbedtls_pk_parse_key(&key->pk, block, block_len + 1, in->passphrase,
                                                          in->passphrase_len, pk_rng, NULL)
                                   : MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
      mbedtls_platform_zeroize(block, block_len + 1);
      free(block);
      if (ret != 0) {
        pk_key_release(key);
        return pk_private_failure(ret, NULL, 0, PK_FROM_PEM_LEGACY, in);
      }
      return key;
    }
    PkBuf der = {0};
    if (!pk_pem_decode(&pem, &der)) {
      pk_buf_free(&der);
      pk_decoder_unsupported();
      return NULL;
    }
    bool rsa_label = pk_label_is(pem.label, "RSA PRIVATE KEY");
    bool ec_label = pk_label_is(pem.label, "EC PRIVATE KEY");
    bool pkcs8_label = pk_label_is(pem.label, "PRIVATE KEY") || pk_label_is(pem.label, "ENCRYPTED PRIVATE KEY");
    PkKey *key = NULL;
    if (rsa_label || ec_label || pkcs8_label) {
      /* The label decides the structure, as OpenSSL's decoders do. */
      bool matches = pkcs8_label ? (pk_private_info_structure(der.data, der.len) || pk_encrypted_info(der.data, der.len))
                     : rsa_label ? pk_rsa_private_structure(der.data, der.len)
                                 : !pk_private_info_structure(der.data, der.len);
      if (matches) key = pk_parse_private_der(der.data, der.len, in, PK_FROM_PEM_PKCS8);
      else pk_decoder_unsupported();
      if (key && ((rsa_label && !pk_is_rsa(key)) || (ec_label && pk_is_rsa(key)))) {
        pk_key_release(key);
        key = NULL;
        pk_decoder_unsupported();
      }
    } else {
      char message[120];
      snprintf(message, sizeof message, "PEM blocks labeled %s have no native implementation; RSA and EC keys are supported",
               pem.label);
      pk_refuse(message);
    }
    pk_buf_free(&der);
    return key;
  }
  pk_decoder_unsupported();
  return NULL;
}

/* TryParsePrivateKey for an input encoding. */
static PkKey *pk_parse_private(const unsigned char *data, size_t len, const PkInput *in) {
  if (!in->der) return pk_parse_private_pem(data, len, in);
  if (in->type == PK_TYPE_PKCS8 && !pk_encrypted_info(data, len) && !pk_private_info_structure(data, len)) {
    pk_der_failure(data, len);
    return NULL;
  }
  if (in->type != PK_TYPE_PKCS8 && pk_encrypted_info(data, len)) {
    pk_auto_failure();
    return NULL;
  }
  return pk_parse_private_der(data, len, in, PK_FROM_DER);
}

static PkKey *pk_parse_public_der(const unsigned char *der, size_t len, bool pkcs1) {
  PkKey *key = pk_key_new(false);
  int ret = mbedtls_pk_parse_public_key(&key->pk, der, len);
  if (ret == 0 && pkcs1 && !pk_is_rsa(key)) ret = MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
  if (ret == 0 && !pkcs1) {
    /* mbedTLS also reads a bare RSAPublicKey; DER spki input must be a
     * SubjectPublicKeyInfo. */
    const unsigned char *oid;
    size_t oid_len;
    if (!pk_algorithm_oid(der, len, false, &oid, &oid_len)) ret = MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
  }
  if (ret != 0) {
    pk_key_release(key);
    if (!pkcs1 && pk_refuse_algorithm(der, len, false)) return NULL;
    pk_public_der_failure(pkcs1);
    return NULL;
  }
  return key;
}

/* The public key of a certificate's SubjectPublicKeyInfo. */
static bool pk_certificate_spki(const unsigned char *der, size_t len, const unsigned char **spki, size_t *spki_len) {
  unsigned char *p = (unsigned char *)der, *end = p + len;
  size_t n;
  const int seq = MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE;
  if (mbedtls_asn1_get_tag(&p, end, &n, seq) != 0) return false;
  end = p + n;
  if (mbedtls_asn1_get_tag(&p, end, &n, seq) != 0) return false; /* TBSCertificate */
  end = p + n;
  /* [0] version, serialNumber, signature, issuer, validity, subject. */
  if (p < end && *p == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0)) {
    if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0) != 0)
      return false;
    p += n;
  }
  if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_INTEGER) != 0) return false;
  p += n;
  for (int i = 0; i < 4; i++) {
    if (mbedtls_asn1_get_tag(&p, end, &n, seq) != 0) return false;
    p += n;
  }
  unsigned char *start = p;
  if (mbedtls_asn1_get_tag(&p, end, &n, seq) != 0) return false;
  *spki = start;
  *spki_len = (size_t)(p + n - start);
  return true;
}

/* TryParsePublicKeyPEM's labels, in its order. NOT_RECOGNIZED (no such
 * block) answers NULL without an exception. */
static PkKey *pk_parse_public_pem(const unsigned char *data, size_t len, bool *recognized) {
  static const char *const labels[] = {"PUBLIC KEY", "RSA PUBLIC KEY", "CERTIFICATE"};
  const unsigned char *end = data + len;
  *recognized = false;
  for (size_t i = 0; i < 3; i++) {
    PkPem pem;
    for (const unsigned char *at = data; pk_pem_next(at, end, &pem); at = pem.end) {
      if (!pk_label_is(pem.label, labels[i])) continue;
      *recognized = true;
      PkBuf der = {0};
      PkKey *key = NULL;
      if (!pk_pem_decode(&pem, &der)) {
        pk_decoder_unsupported();
      } else if (i == 2) {
        const unsigned char *spki;
        size_t spki_len;
        if (pk_certificate_spki(der.data, der.len, &spki, &spki_len)) key = pk_parse_public_der(spki, spki_len, false);
        else pk_openssl("068000A8", "asn1 encoding routines", "wrong tag", "ERR_OSSL_ASN1_WRONG_TAG");
      } else {
        key = pk_parse_public_der(der.data, der.len, i == 1);
      }
      pk_buf_free(&der);
      return key;
    }
  }
  return NULL;
}

/* GetPublicOrPrivateKeyFromJs for data in an input encoding: a public
 * key, or a private key read as public (is_private stays false). */
static PkKey *pk_parse_public_or_private(const unsigned char *data, size_t len, const PkInput *in) {
  if (!in->der) {
    bool recognized;
    PkKey *key = pk_parse_public_pem(data, len, &recognized);
    if (key || recognized) return key;
  } else if (in->type == PK_TYPE_SPKI || (in->type == PK_TYPE_PKCS1 && !pk_rsa_private_structure(data, len))) {
    return pk_parse_public_der(data, len, in->type == PK_TYPE_PKCS1);
  }
  PkKey *key = pk_parse_private(data, len, in);
  if (key) key->is_private = false;
  return key;
}

/* What a key argument is for (prepareAsymmetricKey's ctx). */
typedef enum { PK_CONSUME_PUBLIC, PK_CONSUME_PRIVATE, PK_CREATE_PUBLIC, PK_CREATE_PRIVATE } PkContext;

/* ── JWK ──────────────────────────────────────────────────────────────── */

/* A base64url member of a JWK (the decoder accepts base64 too). *present
 * says whether it is a string and *defined whether it is not undefined. */
static bool pk_jwk_member(const ScrDyn *jwk, const char *name, PkBuf *out, bool *present, bool *defined) {
  ScrDyn *value = pk_read(jwk, name);
  if (!value) return false;
  *present = value->kind == SCR_DYN_STR;
  if (defined) *defined = value->kind != SCR_DYN_UNDEF;
  if (*present) {
    ScrStr *enc = scr_str_new("base64url", 9);
    ScrBytes *bytes = scr_bytes_from_str(value->v.str, enc);
    scr_str_release(enc);
    if (bytes) {
      pk_buf_put(out, bytes->data, (size_t)scr_bytes_byte_len(bytes));
      scr_bytes_release(bytes);
    }
  }
  scr_dyn_release(value);
  return true;
}

static bool pk_jwk_bytes(const ScrDyn *jwk, const char *name, PkBuf *out, bool *present) {
  return pk_jwk_member(jwk, name, out, present, NULL);
}

/* ImportJWKRsaKey. createPrivateKey needs the private members; signing
 * takes a public JWK and fails when it signs (no_private). */
static PkKey *pk_import_jwk_rsa(const ScrDyn *jwk, PkContext ctx) {
  static const char *const names[] = {"n", "e", "d", "p", "q", "dp", "dq", "qi"};
  PkBuf parts[8] = {{0}};
  bool present[8] = {false};
  bool d_defined = false;
  PkKey *key = NULL;
  for (size_t i = 0; i < 8; i++)
    if (!pk_jwk_member(jwk, names[i], &parts[i], &present[i], i == 2 ? &d_defined : NULL)) goto done;
  if (!present[0] || !present[1]) {
    pk_invalid_jwk("Invalid JWK RSA key");
    goto done;
  }
  bool has_private = d_defined;
  if (has_private && !(present[2] && present[3] && present[4] && present[5] && present[6] && present[7])) {
    pk_invalid_jwk("Invalid JWK RSA key");
    goto done;
  }
  if (ctx == PK_CREATE_PRIVATE && !has_private) {
    pk_invalid_jwk("JWK does not contain private key material");
    goto done;
  }
  key = pk_key_new(has_private || ctx == PK_CONSUME_PRIVATE);
  key->no_private = !has_private && ctx == PK_CONSUME_PRIVATE;
  if (mbedtls_pk_setup(&key->pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) != 0) goto fail;
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key->pk);
  if (has_private) {
    if (mbedtls_rsa_import_raw(rsa, parts[0].data, parts[0].len, parts[3].data, parts[3].len, parts[4].data,
                               parts[4].len, parts[2].data, parts[2].len, parts[1].data, parts[1].len) != 0 ||
        mbedtls_rsa_complete(rsa) != 0 || mbedtls_rsa_check_privkey(rsa) != 0)
      goto fail;
  } else if (mbedtls_rsa_import_raw(rsa, parts[0].data, parts[0].len, NULL, 0, NULL, 0, NULL, 0, parts[1].data,
                                    parts[1].len) != 0 ||
             mbedtls_rsa_complete(rsa) != 0 || mbedtls_rsa_check_pubkey(rsa) != 0) {
    goto fail;
  }
  goto done;
fail:
  pk_key_release(key);
  key = NULL;
  pk_invalid_jwk("Invalid JWK RSA key");
done:
  for (size_t i = 0; i < 8; i++) pk_buf_free(&parts[i]);
  return key;
}

/* ImportJWKEcKey: crv, x and y are strings and d is a string when
 * present, then crv names a curve. */
static PkKey *pk_import_jwk_ec(const ScrDyn *jwk, PkContext ctx) {
  PkBuf x = {0}, y = {0}, d = {0};
  bool has_x, has_y, has_d, d_defined;
  PkKey *key = NULL;
  ScrDyn *crv = pk_read(jwk, "crv");
  if (!crv) return NULL;
  if (!pk_jwk_bytes(jwk, "x", &x, &has_x) || !pk_jwk_bytes(jwk, "y", &y, &has_y) ||
      !pk_jwk_member(jwk, "d", &d, &has_d, &d_defined))
    goto done;
  if (crv->kind != SCR_DYN_STR || !has_x || !has_y || (d_defined && !has_d)) {
    pk_invalid_jwk("Invalid JWK EC key");
    goto done;
  }
  size_t curve = (size_t)-1;
  for (size_t i = 0; i < sizeof pk_curves / sizeof pk_curves[0]; i++)
    if (pk_curves[i].jwk && pk_str_is(crv, pk_curves[i].jwk)) curve = i;
  if (curve == (size_t)-1) {
    pk_throw(SCR_ERR_TYPE, "ERR_CRYPTO_INVALID_CURVE", "Invalid EC curve name");
    goto done;
  }
  if (ctx == PK_CREATE_PRIVATE && !has_d) {
    pk_invalid_jwk("JWK does not contain private key material");
    goto done;
  }
  mbedtls_ecp_group_id id = pk_curves[curve].id;
  size_t field = pk_ec_field_bytes(id);
  if (x.len != field || y.len != field || (has_d && d.len != field)) {
    pk_invalid_jwk("Invalid JWK EC key");
    goto done;
  }
  key = pk_key_new(has_d || ctx == PK_CONSUME_PRIVATE);
  key->no_private = !has_d && ctx == PK_CONSUME_PRIVATE;
  if (mbedtls_pk_setup(&key->pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) goto fail;
  mbedtls_ecp_keypair *ec = mbedtls_pk_ec(key->pk);
  PkBuf point = {0};
  pk_buf_byte(&point, 0x04);
  pk_buf_put(&point, x.data, x.len);
  pk_buf_put(&point, y.data, y.len);
  mbedtls_ecp_group grp;
  mbedtls_ecp_point q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_ecp_point_init(&q);
  int ret = mbedtls_ecp_group_load(&grp, id);
  if (ret == 0) ret = mbedtls_ecp_point_read_binary(&grp, &q, point.data, point.len);
  if (ret == 0) ret = mbedtls_ecp_check_pubkey(&grp, &q);
  if (ret == 0 && has_d) ret = mbedtls_ecp_read_key(id, ec, d.data, d.len);
  if (ret == 0) ret = mbedtls_ecp_set_public_key(id, ec, &q);
  if (ret == 0 && has_d) ret = mbedtls_ecp_check_pub_priv(ec, ec, pk_rng, NULL);
  mbedtls_ecp_point_free(&q);
  mbedtls_ecp_group_free(&grp);
  pk_buf_free(&point);
  if (ret == 0) goto done;
fail:
  pk_key_release(key);
  key = NULL;
  pk_invalid_jwk("Invalid JWK EC key");
done:
  scr_dyn_release(crv);
  pk_buf_free(&x);
  pk_buf_free(&y);
  pk_buf_free(&d);
  return key;
}

/* ImportJWKAsymmetricKey: kty RSA or EC. */
static PkKey *pk_import_jwk(const ScrDyn *jwk, PkContext ctx) {
  ScrDyn *kty = pk_read(jwk, "kty");
  if (!kty) return NULL;
  PkKey *key = NULL;
  if (kty->kind != SCR_DYN_STR) {
    pk_invalid_jwk("Invalid JWK format");
  } else if (pk_str_is(kty, "RSA")) {
    key = pk_import_jwk_rsa(jwk, ctx);
  } else if (pk_str_is(kty, "EC")) {
    key = pk_import_jwk_ec(jwk, ctx);
  } else if (pk_str_is(kty, "OKP")) {
    pk_refuse("OKP JWKs (Ed25519, Ed448, X25519, X448) have no native implementation; RSA and EC keys are supported");
  } else {
    ScrJsonBuf b;
    scr_jb_init(&b);
    scr_jb_put_str(&b, kty->v.str);
    scr_jb_puts(&b, " is not a supported JWK key type");
    ScrStr *message = scr_jb_finish(&b);
    scr_throw_error_msg_code(SCR_ERR_TYPE, message->data, message->len, "ERR_CRYPTO_INVALID_JWK");
    scr_str_release(message);
  }
  scr_dyn_release(kty);
  return key;
}

/* ── key input (lib/internal/crypto/keys.js) ──────────────────────────── */

/* getKeyTypes: the "of type string or an instance of ..." clause. */
static const char *pk_key_types(PkContext ctx) {
  switch (ctx) {
    case PK_CREATE_PRIVATE:
      return "of type string or an instance of ArrayBuffer, Buffer, TypedArray, DataView, or URL";
    case PK_CREATE_PUBLIC:
    case PK_CONSUME_PUBLIC:
      return "of type string or an instance of ArrayBuffer, Buffer, TypedArray, DataView, KeyObject, or CryptoKey";
    case PK_CONSUME_PRIVATE:
      return "of type string or an instance of ArrayBuffer, Buffer, TypedArray, DataView, KeyObject, CryptoKey, "
             "or URL";
  }
  return "";
}

static bool pk_private_input(PkContext ctx) {
  return ctx == PK_CONSUME_PRIVATE || ctx == PK_CREATE_PRIVATE;
}

static bool pk_is_crypto_key(const ScrDyn *value) {
  return value->kind == SCR_DYN_HANDLE && value->v.handle.tag == SCR_DYNH_CRYPTO_KEY;
}

static bool pk_is_url(const ScrDyn *value) {
  return scr_dyn_native_url_is(value);
}

/* validateAsymmetricKeyType for a KeyObject. */
static PkKey *pk_key_object_input(const ScrDyn *value, PkContext ctx, const char *name) {
  PkKey *key = pk_key_of(value);
  const char *type = key ? (key->is_private ? "private" : "public") : "secret";
  if (ctx == PK_CREATE_PRIVATE) {
    scr_dyn_arg_type_fail(name, pk_key_types(ctx), value);
    return NULL;
  }
  if (!key || !key->is_private) {
    if (ctx == PK_CONSUME_PRIVATE || ctx == PK_CREATE_PUBLIC) {
      pk_key_object_type(type, "private");
      return NULL;
    }
    if (!key) {
      pk_key_object_type(type, "private or public");
      return NULL;
    }
  }
  return pk_key_retain(key);
}

/* parseKeyFormatAndType and parseKeyType for an input encoding. */
static bool pk_parse_input_encoding(const ScrDyn *options, PkContext ctx, const char *name, PkInput *in) {
  char option[48];
  ScrDyn *format = pk_read(options, "format");
  if (!format) return false;
  bool ok = true;
  if (format->kind == SCR_DYN_UNDEF || pk_str_is(format, "pem")) in->der = false;
  else if (pk_str_is(format, "der")) in->der = true;
  else {
    snprintf(option, sizeof option, "%s.format", name);
    pk_invalid_property(option, format, NULL);
    ok = false;
  }
  scr_dyn_release(format);
  if (!ok) return false;
  ScrDyn *type = pk_read(options, "type");
  if (!type) return false;
  bool is_public_false = pk_private_input(ctx);
  in->type = PK_TYPE_NONE;
  if (type->kind == SCR_DYN_UNDEF && !in->der) in->type = PK_TYPE_NONE;
  else if (pk_str_is(type, "pkcs1")) in->type = PK_TYPE_PKCS1;
  else if (pk_str_is(type, "spki") && !is_public_false) in->type = PK_TYPE_SPKI;
  else if (pk_str_is(type, "pkcs8")) in->type = PK_TYPE_PKCS8;
  else if (pk_str_is(type, "sec1")) in->type = PK_TYPE_SEC1;
  else {
    snprintf(option, sizeof option, "%s.type", name);
    pk_invalid_property(option, type, NULL);
    ok = false;
  }
  scr_dyn_release(type);
  return ok;
}

/* The passphrase as bytes (+1 into *out), or NULL for undefined. */
static bool pk_passphrase(const ScrDyn *options, const ScrDyn *encoding, const char *name, ScrBytes **out) {
  *out = NULL;
  ScrDyn *passphrase = pk_read(options, "passphrase");
  if (!passphrase) return false;
  bool ok = true;
  if (passphrase->kind == SCR_DYN_STR) {
    *out = pk_string_bytes(passphrase->v.str, encoding);
    ok = *out != NULL;
  } else if (pk_is_buffer_source(passphrase)) {
    *out = scr_bytes_buffer_source(passphrase);
  } else if (passphrase->kind != SCR_DYN_UNDEF) {
    char option[48];
    snprintf(option, sizeof option, "%s.passphrase", name);
    pk_invalid_property(option, passphrase, NULL);
    ok = false;
  }
  scr_dyn_release(passphrase);
  return ok;
}

static PkKey *pk_from_bytes(const ScrBytes *bytes, PkContext ctx, const PkInput *in) {
  const unsigned char *data = bytes->data;
  size_t len = (size_t)scr_bytes_byte_len(bytes);
  if (ctx == PK_CONSUME_PRIVATE || ctx == PK_CREATE_PRIVATE) return pk_parse_private(data, len, in);
  return pk_parse_public_or_private(data, len, in);
}

/* prepareAsymmetricKeyOptions and the native init for an object key. */
static PkKey *pk_key_from_options(const ScrDyn *options, PkContext ctx, const char *name) {
  char option[48];
  ScrDyn *data = pk_read(options, "key");
  if (!data) return NULL;
  PkKey *key = NULL;
  ScrDyn *encoding = NULL, *format = NULL;
  ScrBytes *bytes = NULL, *passphrase = NULL;
  bool is_data = data->kind == SCR_DYN_STR || pk_is_view(data);
  if (!is_data && (pk_key_of(data) || scr_key_object_bytes(data, &(size_t){0}))) {
    key = pk_key_object_input(data, ctx, name);
    goto done;
  }
  if (!is_data && pk_is_crypto_key(data)) {
    pk_refuse("CryptoKey values as asymmetric keys have no native implementation; pass a KeyObject");
    goto done;
  }
  is_data = is_data || scr_buffer_storage_is(data);
  if (!is_data && pk_private_input(ctx) && pk_is_url(data)) {
    pk_refuse("Private keys referenced by URL (OpenSSL STORE loaders) have no native implementation");
    goto done;
  }
  if (!(encoding = pk_read(options, "encoding"))) goto done;
  if (!(format = pk_read(options, "format"))) goto done;
  if (pk_str_is(format, "jwk")) {
    if (data->kind != SCR_DYN_OBJ && data->kind != SCR_DYN_TYPED_REF && data->kind != SCR_DYN_PROXY) {
      snprintf(option, sizeof option, "%s.key", name);
      scr_dyn_prop_type_fail(option, "of type object", data);
      goto done;
    }
    key = pk_import_jwk(data, ctx);
    goto done;
  }
  if (pk_str_is(format, "raw-public") || pk_str_is(format, "raw-private") || pk_str_is(format, "raw-seed")) {
    if (pk_private_input(ctx) && pk_str_is(format, "raw-public")) {
      snprintf(option, sizeof option, "%s.format", name);
      pk_invalid_property(option, format, NULL);
      goto done;
    }
    pk_refuse("Raw key import (raw-public, raw-private, raw-seed) has no native implementation; use pem, der or jwk");
    goto done;
  }
  if (!is_data) {
    snprintf(option, sizeof option, "%s.key", name);
    scr_dyn_prop_type_fail(option, pk_key_types(ctx), data);
    goto done;
  }
  bytes = data->kind == SCR_DYN_STR ? pk_string_bytes(data->v.str, encoding) : scr_bytes_buffer_source(data);
  if (!bytes) goto done;
  PkInput in = {0};
  if (!pk_parse_input_encoding(options, ctx, name, &in)) goto done;
  if (!pk_passphrase(options, encoding, name, &passphrase)) goto done;
  if (passphrase) {
    in.has_passphrase = true;
    in.passphrase = passphrase->data;
    in.passphrase_len = (size_t)scr_bytes_byte_len(passphrase);
  }
  key = pk_from_bytes(bytes, ctx, &in);
done:
  scr_dyn_release(data);
  if (encoding) scr_dyn_release(encoding);
  if (format) scr_dyn_release(format);
  scr_bytes_release(bytes);
  if (passphrase) {
    mbedtls_platform_zeroize(passphrase->data, (size_t)scr_bytes_byte_len(passphrase));
    scr_bytes_release(passphrase);
  }
  return key;
}

/* prepareAsymmetricKey(key, ctx, name) and the native init. +1 key or
 * NULL with Node's error pending. */
static PkKey *pk_prepare(const ScrDyn *value, PkContext ctx, const char *name) {
  PkInput pem = {0};
  if (value->kind == SCR_DYN_STR || pk_is_view(value)) {
    ScrBytes *bytes = value->kind == SCR_DYN_STR ? pk_string_bytes(value->v.str, NULL) : scr_bytes_buffer_source(value);
    if (!bytes) return NULL;
    PkKey *key = pk_from_bytes(bytes, ctx, &pem);
    scr_bytes_release(bytes);
    return key;
  }
  if (pk_key_of(value) || scr_key_object_bytes(value, &(size_t){0})) return pk_key_object_input(value, ctx, name);
  if (pk_is_crypto_key(value)) {
    pk_refuse("CryptoKey values as asymmetric keys have no native implementation; pass a KeyObject");
    return NULL;
  }
  if (pk_private_input(ctx) && pk_is_url(value)) {
    pk_refuse("Private keys referenced by URL (OpenSSL STORE loaders) have no native implementation");
    return NULL;
  }
  if (scr_buffer_storage_is(value)) {
    ScrBytes *bytes = scr_bytes_buffer_source(value);
    if (!bytes) return NULL;
    PkKey *key = pk_from_bytes(bytes, ctx, &pem);
    scr_bytes_release(bytes);
    return key;
  }
  if (value->kind == SCR_DYN_NULL) {
    char message[96];
    snprintf(message, sizeof message, "Cannot destructure property 'key' of '%s' as it is null.", name);
    pk_throw(SCR_ERR_TYPE, NULL, message);
    return NULL;
  }
  if (value->kind == SCR_DYN_OBJ || value->kind == SCR_DYN_TYPED_REF || value->kind == SCR_DYN_PROXY ||
      value->kind == SCR_DYN_ARR || value->kind == SCR_DYN_HANDLE)
    return pk_key_from_options(value, ctx, name);
  scr_dyn_arg_type_fail(name, pk_key_types(ctx), value);
  return NULL;
}

/* ── KeyObject handles ────────────────────────────────────────────────── */

static ScrDyn *pk_key_invoke(void *h, ScrDyn *self, const char *method, ScrDyn *const *args, size_t argc,
                             const char *what);
static ScrDyn *pk_key_get(void *h, const char *name, size_t len);
static bool pk_key_set(void *h, const char *name, size_t len, const ScrDyn *value);

static ScrDyn *pk_key_inspect_view(void *h) {
  (void)h;
  return scr_dyn_new_obj();
}

static const ScrDynHandleOps pk_private_ops = {
    .cls = "PrivateKeyObject",
    .retain = pk_key_retain,
    .release = pk_key_release,
    .invoke = pk_key_invoke,
    .get = pk_key_get,
    .set = pk_key_set,
    .inspect_view = pk_key_inspect_view,
    .json_empty_object = true,
    .to_string_tag = "KeyObject",
    .inspect_view_empty = true,
};

static const ScrDynHandleOps pk_public_ops = {
    .cls = "PublicKeyObject",
    .retain = pk_key_retain,
    .release = pk_key_release,
    .invoke = pk_key_invoke,
    .get = pk_key_get,
    .set = pk_key_set,
    .inspect_view = pk_key_inspect_view,
    .json_empty_object = true,
    .to_string_tag = "KeyObject",
    .inspect_view_empty = true,
};

/* scr_crypto_asymmetric_key_type_fail: an asymmetric KeyObject given
 * where a secret one is expected. */
static bool pk_secret_expected(const ScrDyn *value, const char *expected) {
  PkKey *key = pk_key_of(value);
  if (!key) return false;
  pk_key_object_type(key->is_private ? "private" : "public", expected);
  return true;
}

/* Consumes key. */
static ScrDyn *pk_key_box(PkKey *key) {
  static SCR_TL bool installed;
  if (!installed) {
    installed = true;
    scr_crypto_asymmetric_key_type_fail = pk_secret_expected;
    scr_dyn_handle_install(SCR_DYNH_PRIVATE_KEY_OBJECT, &pk_private_ops);
    scr_dyn_handle_install(SCR_DYNH_PUBLIC_KEY_OBJECT, &pk_public_ops);
  }
  ScrDyn *box = scr_dyn_new_handle(key, key->is_private ? SCR_DYNH_PRIVATE_KEY_OBJECT : SCR_DYNH_PUBLIC_KEY_OBJECT);
  pk_key_release(key);
  return box;
}

/* The public exponent as a BigInt (+1). */
static ScrDyn *pk_exponent(const mbedtls_mpi *e) {
  unsigned char bytes[64];
  size_t n = mbedtls_mpi_size(e);
  if (n <= 8) {
    uint64_t value = 0;
    mbedtls_mpi_write_binary(e, bytes, n);
    for (size_t i = 0; i < n; i++) value = (value << 8) | bytes[i];
    ScrBigInt *big = scr_bigint_from_u64(value);
    ScrDyn *result = scr_dyn_new_bigint(big);
    scr_bigint_release(big);
    return result;
  }
  char hex[2 + 2 * sizeof bytes + 1];
  size_t written;
  hex[0] = '0';
  hex[1] = 'x';
  if (n > sizeof bytes || mbedtls_mpi_write_string(e, 16, hex + 2, sizeof hex - 2, &written) != 0)
    return scr_dyn_retain(scr_dyn_undefined());
  ScrStr *text = scr_str_new(hex, strlen(hex));
  ScrBigInt *big = scr_bigint_parse(text);
  scr_str_release(text);
  if (!big) return NULL;
  ScrDyn *result = scr_dyn_new_bigint(big);
  scr_bigint_release(big);
  return result;
}

static ScrDyn *pk_key_details(const PkKey *key) {
  ScrDyn *out = scr_dyn_new_obj();
  if (pk_is_rsa(key)) {
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key->pk);
    mbedtls_mpi e;
    mbedtls_mpi_init(&e);
    mbedtls_rsa_export(rsa, NULL, NULL, NULL, NULL, &e);
    scr_dyn_obj_set(out, "modulusLength", 13, scr_dyn_new_num((double)mbedtls_pk_get_bitlen(&key->pk)));
    ScrDyn *exponent = pk_exponent(&e);
    mbedtls_mpi_free(&e);
    if (!exponent) {
      scr_dyn_release(out);
      return NULL;
    }
    scr_dyn_obj_set(out, "publicExponent", 14, exponent);
  } else {
    size_t curve = pk_curve_index(pk_ec_group(key));
    scr_dyn_obj_set(out, "namedCurve", 10, pk_cstr_dyn(curve == (size_t)-1 ? "" : pk_curves[curve].name));
  }
  return out;
}

static ScrDyn *pk_key_get(void *h, const char *name, size_t len) {
  const PkKey *key = h;
  if (len == 4 && memcmp(name, "type", 4) == 0) return pk_cstr_dyn(key->is_private ? "private" : "public");
  if (len == 17 && memcmp(name, "asymmetricKeyType", 17) == 0) return pk_cstr_dyn(pk_is_rsa(key) ? "rsa" : "ec");
  if (len == 20 && memcmp(name, "asymmetricKeyDetails", 20) == 0) return pk_key_details(key);
  return NULL;
}

/* The getters have no setters; Node's strict-mode TypeError names the
 * prototype that defines each one. */
static bool pk_key_set(void *h, const char *name, size_t len, const ScrDyn *value) {
  (void)h;
  (void)value;
  const char *holder = len == 4 && memcmp(name, "type", 4) == 0 ? "KeyObject"
                       : (len == 17 && memcmp(name, "asymmetricKeyType", 17) == 0) ||
                               (len == 20 && memcmp(name, "asymmetricKeyDetails", 20) == 0)
                           ? "AsymmetricKeyObject"
                           : NULL;
  if (!holder) return false;
  char message[160];
  int n = snprintf(message, sizeof message, "Cannot set property %.*s of #<%s> which has only a getter",
                   (int)len, name, holder);
  scr_throw_error_msg(SCR_ERR_TYPE, message, (size_t)n);
  return true;
}

/* ── export ───────────────────────────────────────────────────────────── */

/* mbedTLS writes DER at the end of the buffer; this moves it into out. */
static bool pk_write_der(int (*write)(const mbedtls_pk_context *, unsigned char *, size_t), const PkKey *key,
                         PkBuf *out) {
  size_t cap = 8192;
  unsigned char *buf = malloc(cap);
  if (!buf) scr_trap("scriptc: out of memory\n");
  int n = write(&key->pk, buf, cap);
  if (n > 0) pk_buf_put(out, buf + cap - (size_t)n, (size_t)n);
  mbedtls_platform_zeroize(buf, cap);
  free(buf);
  return n > 0;
}

/* RSAPublicKey (pkcs1) for an RSA key. */
static bool pk_write_rsa_public(const PkKey *key, PkBuf *out) {
  unsigned char buf[2600];
  unsigned char *p = buf + sizeof buf;
  int n = mbedtls_pk_write_pubkey(&p, buf, &key->pk);
  if (n <= 0) return false;
  pk_buf_put(out, p, (size_t)n);
  return true;
}

/* PrivateKeyInfo around the traditional encoding. An EC key's inner
 * ECPrivateKey leaves out the parameters, as OpenSSL writes it. */
static bool pk_write_pkcs8(const PkKey *key, PkBuf *out) {
  PkBuf traditional = {0}, inner = {0}, alg = {0}, body = {0};
  bool ok = pk_write_der(mbedtls_pk_write_key_der, key, &traditional);
  if (ok && pk_is_rsa(key)) {
    pk_der_put(&alg, MBEDTLS_ASN1_OID, pk_oid_rsa, sizeof pk_oid_rsa);
    pk_der_put(&alg, MBEDTLS_ASN1_NULL, NULL, 0);
    pk_buf_put(&inner, traditional.data, traditional.len);
  } else if (ok) {
    /* ECPrivateKey: version, privateKey, [0] parameters, [1] publicKey. */
    unsigned char *p = traditional.data, *end = p + traditional.len;
    size_t n;
    ok = mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) == 0;
    PkBuf fields = {0};
    unsigned char *curve_oid = NULL;
    size_t curve_len = 0;
    while (ok && p < end) {
      unsigned char *start = p;
      unsigned char tag = *p++;
      size_t len;
      if (mbedtls_asn1_get_len(&p, end, &len) != 0) {
        ok = false;
        break;
      }
      if (tag == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0)) {
        unsigned char *q = p;
        size_t oid_len;
        if (mbedtls_asn1_get_tag(&q, p + len, &oid_len, MBEDTLS_ASN1_OID) == 0) {
          curve_oid = q;
          curve_len = oid_len;
        }
      } else {
        pk_buf_put(&fields, start, (size_t)(p + len - start));
      }
      p += len;
    }
    ok = ok && curve_oid != NULL;
    if (ok) {
      pk_der_put(&inner, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE, fields.data, fields.len);
      pk_der_put(&alg, MBEDTLS_ASN1_OID, pk_oid_ec, sizeof pk_oid_ec);
      pk_der_put(&alg, MBEDTLS_ASN1_OID, curve_oid, curve_len);
    }
    pk_buf_free(&fields);
  }
  if (ok) {
    static const unsigned char version[] = {MBEDTLS_ASN1_INTEGER, 1, 0};
    pk_buf_put(&body, version, sizeof version);
    pk_der_put(&body, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE, alg.data, alg.len);
    pk_der_put(&body, MBEDTLS_ASN1_OCTET_STRING, inner.data, inner.len);
    pk_der_put(out, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE, body.data, body.len);
  }
  pk_buf_free(&traditional);
  pk_buf_free(&inner);
  pk_buf_free(&alg);
  pk_buf_free(&body);
  return ok;
}

/* PEM with 64-column base64 lines, as OpenSSL writes it. */
static ScrDyn *pk_pem_dyn(const char *label, const PkBuf *der) {
  size_t b64_len = 0;
  mbedtls_base64_encode(NULL, 0, &b64_len, der->data, der->len);
  unsigned char *b64 = malloc(b64_len + 1);
  if (!b64) scr_trap("scriptc: out of memory\n");
  mbedtls_base64_encode(b64, b64_len + 1, &b64_len, der->data, der->len);
  PkBuf text = {0};
  char line[80];
  int n = snprintf(line, sizeof line, "-----BEGIN %s-----\n", label);
  pk_buf_put(&text, line, (size_t)n);
  for (size_t i = 0; i < b64_len; i += 64) {
    size_t chunk = b64_len - i < 64 ? b64_len - i : 64;
    pk_buf_put(&text, b64 + i, chunk);
    pk_buf_byte(&text, '\n');
  }
  n = snprintf(line, sizeof line, "-----END %s-----\n", label);
  pk_buf_put(&text, line, (size_t)n);
  mbedtls_platform_zeroize(b64, b64_len + 1);
  free(b64);
  ScrDyn *result = pk_str_dyn((const char *)text.data, text.len);
  pk_buf_free(&text);
  return result;
}

static ScrDyn *pk_base64url_dyn(const unsigned char *data, size_t len) {
  ScrBytes view = {.rc = 1, .len = (double)len, .elem = SCR_BYTES_U8, .data = (unsigned char *)data};
  ScrStr *enc = scr_str_new("base64url", 9);
  ScrStr *text = scr_bytes_to_str(&view, enc);
  scr_str_release(enc);
  ScrDyn *result = scr_dyn_new_str(text);
  scr_str_release(text);
  return result;
}

static void pk_jwk_mpi(ScrDyn *out, const char *name, const mbedtls_mpi *value) {
  size_t n = mbedtls_mpi_size(value);
  unsigned char *bytes = malloc(n ? n : 1);
  if (!bytes) scr_trap("scriptc: out of memory\n");
  mbedtls_mpi_write_binary(value, bytes, n);
  scr_dyn_obj_set(out, name, strlen(name), pk_base64url_dyn(bytes, n));
  mbedtls_platform_zeroize(bytes, n);
  free(bytes);
}

/* The uncompressed public point, or the private scalar, of an EC key at
 * the curve's field width. */
static bool pk_ec_point(const PkKey *key, int format, PkBuf *out) {
  unsigned char buf[2 * 66 + 1];
  size_t n;
  if (mbedtls_ecp_write_public_key(mbedtls_pk_ec(key->pk), format, &n, buf, sizeof buf) != 0) return false;
  pk_buf_put(out, buf, n);
  return true;
}

static bool pk_ec_scalar(const PkKey *key, PkBuf *out) {
  unsigned char buf[66];
  size_t n;
  if (mbedtls_ecp_write_key_ext(mbedtls_pk_ec(key->pk), &n, buf, sizeof buf) != 0) return false;
  size_t field = pk_ec_field_bytes(pk_ec_group(key));
  for (size_t i = n; i < field; i++) pk_buf_byte(out, 0);
  pk_buf_put(out, buf, n);
  mbedtls_platform_zeroize(buf, sizeof buf);
  return true;
}

static ScrDyn *pk_export_jwk(const PkKey *key, bool include_private) {
  ScrDyn *out = scr_dyn_new_obj();
  if (pk_is_rsa(key)) {
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key->pk);
    mbedtls_mpi n, p, q, d, e, dp, dq, qi;
    mbedtls_mpi *all[] = {&n, &p, &q, &d, &e, &dp, &dq, &qi};
    for (size_t i = 0; i < 8; i++) mbedtls_mpi_init(all[i]);
    scr_dyn_obj_set(out, "kty", 3, pk_cstr_dyn("RSA"));
    if (include_private) {
      mbedtls_rsa_export(rsa, &n, &p, &q, &d, &e);
      mbedtls_rsa_export_crt(rsa, &dp, &dq, &qi);
    } else {
      mbedtls_rsa_export(rsa, &n, NULL, NULL, NULL, &e);
    }
    pk_jwk_mpi(out, "n", &n);
    pk_jwk_mpi(out, "e", &e);
    if (include_private) {
      pk_jwk_mpi(out, "d", &d);
      pk_jwk_mpi(out, "p", &p);
      pk_jwk_mpi(out, "q", &q);
      pk_jwk_mpi(out, "dp", &dp);
      pk_jwk_mpi(out, "dq", &dq);
      pk_jwk_mpi(out, "qi", &qi);
    }
    for (size_t i = 0; i < 8; i++) mbedtls_mpi_free(all[i]);
    return out;
  }
  size_t curve = pk_curve_index(pk_ec_group(key));
  if (curve == (size_t)-1 || !pk_curves[curve].jwk) {
    scr_dyn_release(out);
    char message[96];
    snprintf(message, sizeof message, "Unsupported JWK EC curve: %s.",
             curve == (size_t)-1 ? "" : pk_curves[curve].name);
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_JWK_UNSUPPORTED_CURVE", message);
    return NULL;
  }
  PkBuf point = {0};
  pk_ec_point(key, MBEDTLS_ECP_PF_UNCOMPRESSED, &point);
  size_t field = (point.len - 1) / 2;
  scr_dyn_obj_set(out, "kty", 3, pk_cstr_dyn("EC"));
  scr_dyn_obj_set(out, "x", 1, pk_base64url_dyn(point.data + 1, field));
  scr_dyn_obj_set(out, "y", 1, pk_base64url_dyn(point.data + 1 + field, field));
  scr_dyn_obj_set(out, "crv", 3, pk_cstr_dyn(pk_curves[curve].jwk));
  pk_buf_free(&point);
  if (include_private) {
    PkBuf d = {0};
    pk_ec_scalar(key, &d);
    scr_dyn_obj_set(out, "d", 1, pk_base64url_dyn(d.data, d.len));
    pk_buf_free(&d);
  }
  return out;
}

static const char *pk_encoding_name(PkEncodingType type) {
  switch (type) {
    case PK_TYPE_PKCS1: return "pkcs1";
    case PK_TYPE_PKCS8: return "pkcs8";
    case PK_TYPE_SPKI: return "spki";
    case PK_TYPE_SEC1: return "sec1";
    default: return "";
  }
}

/* parseKeyType for an output encoding of this key. */
static bool pk_output_type(const PkKey *key, const ScrDyn *type, PkEncodingType *out) {
  bool is_public = !key->is_private;
  if (pk_str_is(type, "pkcs1")) {
    if (!pk_is_rsa(key)) {
      pk_incompatible("pkcs1", "can only be used for RSA keys");
      return false;
    }
    *out = PK_TYPE_PKCS1;
  } else if (pk_str_is(type, "spki") && is_public) {
    *out = PK_TYPE_SPKI;
  } else if (pk_str_is(type, "pkcs8") && !is_public) {
    *out = PK_TYPE_PKCS8;
  } else if (pk_str_is(type, "sec1") && !is_public) {
    if (pk_is_rsa(key)) {
      pk_incompatible("sec1", "can only be used for EC keys");
      return false;
    }
    *out = PK_TYPE_SEC1;
  } else {
    pk_invalid_property("options.type", type, NULL);
    return false;
  }
  return true;
}

/* PublicKeyObject.export and PrivateKeyObject.export. */
static ScrDyn *pk_export(const PkKey *key, const ScrDyn *options) {
  ScrDyn *format = NULL, *type = NULL, *cipher = NULL, *passphrase = NULL;
  ScrDyn *result = NULL;
  bool nullish = pk_nullish(options);
  if (!nullish) {
    if (!(format = pk_read(options, "format"))) goto done;
    if (!(type = pk_read(options, "type"))) goto done;
    if (!(cipher = pk_read(options, "cipher"))) goto done;
    if (!(passphrase = pk_read(options, "passphrase"))) goto done;
  }
  if (key->is_private && passphrase && passphrase->kind != SCR_DYN_UNDEF && !pk_str_is(format, "pem") &&
      !pk_str_is(format, "der")) {
    ScrStr *text = format->kind == SCR_DYN_STR ? scr_str_retain(format->v.str) : scr_dyn_string_coerce_js(format);
    if (text) {
      char name[64];
      snprintf(name, sizeof name, "%.*s", (int)(text->len < 40 ? text->len : 40), text->data);
      scr_str_release(text);
      pk_incompatible(name, "does not support encryption");
    }
    goto done;
  }
  if (!nullish && pk_str_is(format, "jwk")) {
    result = pk_export_jwk(key, key->is_private);
    goto done;
  }
  if (!nullish && (pk_str_is(format, key->is_private ? "raw-private" : "raw-public"))) {
    PkBuf raw = {0};
    if (pk_is_rsa(key)) {
      pk_incompatible(NULL, NULL);
    } else if (key->is_private) {
      pk_ec_scalar(key, &raw);
      result = pk_buffer_dyn(raw.data, raw.len);
    } else {
      bool compressed = pk_str_is(type, "compressed");
      if (type->kind != SCR_DYN_UNDEF && !compressed && !pk_str_is(type, "uncompressed")) {
        static const char allowed[] = "'compressed', 'uncompressed'";
        char rendered[160];
        const char *text = scr_dyn_inspect_lite(type, rendered, sizeof rendered);
        char message[256];
        snprintf(message, sizeof message, "The property 'options.type' must be one of: %s. Received %s", allowed, text);
        pk_throw(SCR_ERR_TYPE, "ERR_INVALID_ARG_VALUE", message);
      } else {
        pk_ec_point(key, compressed ? MBEDTLS_ECP_PF_COMPRESSED : MBEDTLS_ECP_PF_UNCOMPRESSED, &raw);
        result = pk_buffer_dyn(raw.data, raw.len);
      }
    }
    pk_buf_free(&raw);
    goto done;
  }
  /* parseKeyEncoding(options, keyType, isPublic). */
  if (nullish || !pk_is_object(options)) {
    scr_dyn_arg_type_fail("options", "of type object", options);
    goto done;
  }
  bool pem = pk_str_is(format, "pem");
  if (!pem && !pk_str_is(format, "der")) {
    pk_invalid_property("options.format", format, NULL);
    goto done;
  }
  PkEncodingType encoding;
  if (!pk_output_type(key, type, &encoding)) goto done;
  if (key->is_private) {
    if (!pk_nullish(cipher)) {
      if (cipher->kind != SCR_DYN_STR) {
        pk_invalid_property("options.cipher", cipher, NULL);
        goto done;
      }
      if (!pem && (encoding == PK_TYPE_PKCS1 || encoding == PK_TYPE_SEC1)) {
        pk_incompatible(pk_encoding_name(encoding), "does not support encryption");
        goto done;
      }
      if (!pk_is_buffer_source(passphrase) && passphrase->kind != SCR_DYN_STR) {
        pk_invalid_property("options.passphrase", passphrase, NULL);
        goto done;
      }
      pk_refuse("Encrypted key export (cipher and passphrase) has no native implementation");
      goto done;
    }
    if (passphrase->kind != SCR_DYN_UNDEF) {
      pk_invalid_property("options.cipher", cipher, "is required when a passphrase is specified");
      goto done;
    }
  }
  PkBuf der = {0};
  bool ok;
  const char *label;
  if (encoding == PK_TYPE_SPKI) {
    ok = pk_write_der(mbedtls_pk_write_pubkey_der, key, &der);
    label = "PUBLIC KEY";
  } else if (encoding == PK_TYPE_PKCS1 && !key->is_private) {
    ok = pk_write_rsa_public(key, &der);
    label = "RSA PUBLIC KEY";
  } else if (encoding == PK_TYPE_PKCS8) {
    ok = pk_write_pkcs8(key, &der);
    label = "PRIVATE KEY";
  } else {
    ok = pk_write_der(mbedtls_pk_write_key_der, key, &der);
    label = encoding == PK_TYPE_PKCS1 ? "RSA PRIVATE KEY" : "EC PRIVATE KEY";
  }
  if (!ok) pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED",
                    key->is_private ? "Failed to encode private key" : "Failed to encode public key");
  else result = pem ? pk_pem_dyn(label, &der) : pk_buffer_dyn(der.data, der.len);
  pk_buf_free(&der);
done:
  if (format) scr_dyn_release(format);
  if (type) scr_dyn_release(type);
  if (cipher) scr_dyn_release(cipher);
  if (passphrase) scr_dyn_release(passphrase);
  return result;
}

/* EVP_PKEY_eq over the public halves: the same type and public key. */
static bool pk_equal(const PkKey *a, const PkKey *b) {
  if (mbedtls_pk_get_type(&a->pk) != mbedtls_pk_get_type(&b->pk)) return false;
  PkBuf x = {0}, y = {0};
  bool equal = pk_write_der(mbedtls_pk_write_pubkey_der, a, &x) && pk_write_der(mbedtls_pk_write_pubkey_der, b, &y) &&
               x.len == y.len && memcmp(x.data, y.data, x.len) == 0;
  pk_buf_free(&x);
  pk_buf_free(&y);
  return equal;
}

static ScrDyn *pk_key_invoke(void *h, ScrDyn *self, const char *method, ScrDyn *const *args, size_t argc,
                             const char *what) {
  (void)self;
  PkKey *key = h;
  ScrDyn *arg = argc ? pk_unbox(args[0]) : scr_dyn_retain(scr_dyn_undefined());
  if (!arg) return NULL;
  ScrDyn *result = NULL;
  if (strcmp(method, "export") == 0) {
    result = pk_export(key, arg);
  } else if (strcmp(method, "equals") == 0) {
    PkKey *other = pk_key_of(arg);
    size_t len;
    if (!other && !scr_key_object_bytes(arg, &len)) {
      scr_dyn_arg_type_fail("otherKeyObject", "an instance of KeyObject", arg);
    } else {
      result = scr_dyn_new_bool(other && other->is_private == key->is_private && pk_equal(key, other));
    }
  } else if (strcmp(method, "toCryptoKey") == 0) {
    pk_refuse("KeyObject.toCryptoKey() needs WebCrypto CryptoKey values, which have no native lowering");
  } else {
    pk_not_a_function(what);
  }
  scr_dyn_release(arg);
  return result;
}

/* createPrivateKey(key) and createPublicKey(key). +1 KeyObject or NULL
 * with Node's error pending. */
ScrDyn *scr_crypto_create_private_key(ScrDyn *key) {
  ScrDyn *value = pk_unbox(key);
  if (!value) return NULL;
  PkKey *result = pk_prepare(value, PK_CREATE_PRIVATE, "key");
  scr_dyn_release(value);
  return result ? pk_key_box(result) : NULL;
}

ScrDyn *scr_crypto_create_public_key(ScrDyn *key) {
  ScrDyn *value = pk_unbox(key);
  if (!value) return NULL;
  PkKey *result = pk_prepare(value, PK_CREATE_PUBLIC, "key");
  scr_dyn_release(value);
  if (!result) return NULL;
  /* A public KeyObject keeps the public half only. */
  PkKey *pub = pk_public_of(result);
  pk_key_release(result);
  return pub ? pk_key_box(pub) : NULL;
}

/* ── signatures ───────────────────────────────────────────────────────── */

enum { PK_PADDING_PKCS1 = 1, PK_PADDING_NONE = 3, PK_PADDING_X931 = 5, PK_PADDING_PSS = 6 };
enum { PK_SALT_DIGEST = -1, PK_SALT_AUTO = -2, PK_SALT_MAX = -3, PK_SALT_AUTO_DIGEST_MAX = -4 };

typedef struct {
  bool has_padding, has_salt;
  int padding;
  int salt;
  bool p1363;
} PkSigOptions;

/* getIntOption(name, options): value === value >> 0. */
static bool pk_int_option(const ScrDyn *options, const char *name, bool *present, int *out) {
  *present = false;
  ScrDyn *value = pk_read(options, name);
  if (!value) return false;
  bool ok = true;
  if (value->kind != SCR_DYN_UNDEF) {
    double v = value->kind == SCR_DYN_NUM ? value->v.num : NAN;
    if (value->kind == SCR_DYN_NUM && v >= -2147483648.0 && v <= 2147483647.0 && v == (double)(int32_t)v) {
      *present = true;
      *out = (int)v;
    } else {
      char option[48];
      snprintf(option, sizeof option, "options.%s", name);
      pk_invalid_property(option, value, NULL);
      ok = false;
    }
  }
  scr_dyn_release(value);
  return ok;
}

/* getPadding, getSaltLength and getDSASignatureEncoding over the key
 * argument, in sig.js's order. */
static bool pk_sig_options(const ScrDyn *key, PkSigOptions *out) {
  memset(out, 0, sizeof *out);
  if (!pk_int_option(key, "padding", &out->has_padding, &out->padding)) return false;
  if (!pk_int_option(key, "saltLength", &out->has_salt, &out->salt)) return false;
  if (out->has_padding && out->padding == PK_PADDING_PSS && !out->has_salt) {
    out->has_salt = true;
    out->salt = PK_SALT_AUTO;
  }
  if (key->kind == SCR_DYN_OBJ || key->kind == SCR_DYN_TYPED_REF || key->kind == SCR_DYN_PROXY ||
      key->kind == SCR_DYN_ARR || key->kind == SCR_DYN_HANDLE || key->kind == SCR_DYN_NULL || pk_is_view(key)) {
    if (key->kind == SCR_DYN_NULL) return true;
    ScrDyn *encoding = pk_read(key, "dsaEncoding");
    if (!encoding) return false;
    bool ok = true;
    if (encoding->kind == SCR_DYN_UNDEF || pk_str_is(encoding, "der")) out->p1363 = false;
    else if (pk_str_is(encoding, "ieee-p1363")) out->p1363 = true;
    else {
      pk_invalid_property("options.dsaEncoding", encoding, NULL);
      ok = false;
    }
    scr_dyn_release(encoding);
    return ok;
  }
  return true;
}

/* DigestInfo prefixes for EMSA-PKCS1-v1_5 (RFC 8017 section 9.2). */
static const struct {
  ScrDigestAlg alg;
  unsigned char len;
  unsigned char prefix[19];
} pk_digest_info[] = {
  {SCR_DIGEST_MD5, 18, {0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05, 0x05, 0x00, 0x04, 0x10}},
  {SCR_DIGEST_SHA1, 15, {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14}},
  {SCR_DIGEST_SHA224, 19, {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c}},
  {SCR_DIGEST_SHA256, 19, {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20}},
  {SCR_DIGEST_SHA384, 19, {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30}},
  {SCR_DIGEST_SHA512, 19, {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40}},
  {SCR_DIGEST_SHA512_224, 19, {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x05, 0x05, 0x00, 0x04, 0x1c}},
  {SCR_DIGEST_SHA512_256, 19, {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x06, 0x05, 0x00, 0x04, 0x20}},
  {SCR_DIGEST_SHA3_224, 19, {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x07, 0x05, 0x00, 0x04, 0x1c}},
  {SCR_DIGEST_SHA3_256, 19, {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x08, 0x05, 0x00, 0x04, 0x20}},
  {SCR_DIGEST_SHA3_384, 19, {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x09, 0x05, 0x00, 0x04, 0x30}},
  {SCR_DIGEST_SHA3_512, 19, {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0a, 0x05, 0x00, 0x04, 0x40}},
  {SCR_DIGEST_RIPEMD160, 15, {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x24, 0x03, 0x02, 0x01, 0x05, 0x00, 0x04, 0x14}},
  /* OpenSSL's SM3 DigestInfo names 1.2.156.10197.1.504. */
  {SCR_DIGEST_SM3, 18, {0x30, 0x30, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x83, 0x78, 0x05, 0x00, 0x04, 0x20}},
};

/* The digests OpenSSL's RSA signature provider accepts. */
static bool pk_rsa_digest(ScrDigestAlg alg) {
  if (alg == SCR_DIGEST_MD5_SHA1) return true;
  for (size_t i = 0; i < sizeof pk_digest_info / sizeof pk_digest_info[0]; i++)
    if (pk_digest_info[i].alg == alg) return true;
  return false;
}

/* The digest checks of the RSA and ECDSA providers. */
static bool pk_check_digest(const PkKey *key, ScrDigestAlg alg) {
  if (pk_is_rsa(key)) {
    if (!pk_rsa_digest(alg)) {
      pk_provider("1C8000AE", "digest not allowed", "ERR_OSSL_DIGEST_NOT_ALLOWED");
      return false;
    }
    return true;
  }
  if (alg == SCR_DIGEST_SHAKE128 || alg == SCR_DIGEST_SHAKE256) {
    pk_provider("1C80007A", "invalid digest", "ERR_OSSL_INVALID_DIGEST");
    return false;
  }
  if (scr_crypto_digest_is_xof(alg)) {
    pk_provider("1C8000B7", "xof digests not allowed", "ERR_OSSL_XOF_DIGESTS_NOT_ALLOWED");
    return false;
  }
  return true;
}

/* The padding checks of OpenSSL's RSA signature provider: PKCS#1 v1.5,
 * PSS and X9.31 sign; X9.31 has no implementation here. */
static bool pk_check_padding(const PkKey *key, const PkSigOptions *opts, int *padding) {
  *padding = PK_PADDING_PKCS1;
  if (!pk_is_rsa(key) || !opts->has_padding) return true;
  int p = opts->padding;
  if (p == PK_PADDING_PKCS1 || p == PK_PADDING_PSS) {
    *padding = p;
    return true;
  }
  if (p == -1) return true;
  if (p == PK_PADDING_X931) {
    pk_refuse("RSA_X931_PADDING has no native implementation; RSA_PKCS1_PADDING and RSA_PKCS1_PSS_PADDING are supported");
    return false;
  }
  if (p == PK_PADDING_NONE) pk_provider("1C8000A8", "invalid padding mode", "ERR_OSSL_INVALID_PADDING_MODE");
  else pk_provider("1C8000A5", "illegal or unsupported padding mode", "ERR_OSSL_ILLEGAL_OR_UNSUPPORTED_PADDING_MODE");
  return false;
}

/* MGF1 (RFC 8017 appendix B.2.1) over the digest, XORed into mask. */
static void pk_mgf1_xor(ScrDigestAlg alg, const unsigned char *seed, size_t seed_len, unsigned char *mask,
                        size_t len) {
  unsigned char block[64];
  size_t hlen = scr_crypto_digest_size(alg);
  for (uint32_t counter = 0, done = 0; done < len; counter++) {
    unsigned char c[4] = {(unsigned char)(counter >> 24), (unsigned char)(counter >> 16), (unsigned char)(counter >> 8),
                          (unsigned char)counter};
    ScrCryptoDigest *d = scr_crypto_digest_new(alg);
    scr_crypto_digest_feed(d, seed, seed_len);
    scr_crypto_digest_feed(d, c, 4);
    scr_crypto_digest_finish(d, block);
    scr_crypto_digest_free(d);
    for (size_t i = 0; i < hlen && done < len; i++) mask[done++] ^= block[i];
  }
  mbedtls_platform_zeroize(block, sizeof block);
}

/* M' = 8 zero bytes || mHash || salt, hashed. */
static void pk_pss_hash(ScrDigestAlg alg, const unsigned char *mhash, size_t hlen, const unsigned char *salt,
                        size_t salt_len, unsigned char out[64]) {
  static const unsigned char zeros[8] = {0};
  ScrCryptoDigest *d = scr_crypto_digest_new(alg);
  scr_crypto_digest_feed(d, zeros, 8);
  scr_crypto_digest_feed(d, mhash, hlen);
  if (salt_len) scr_crypto_digest_feed(d, salt, salt_len);
  scr_crypto_digest_finish(d, out);
  scr_crypto_digest_free(d);
}

/* RSA_padding_add_PKCS1_PSS_mgf1's salt length rules. */
static bool pk_pss_salt_for_sign(int salt, size_t hlen, size_t em_len, size_t *out) {
  if (salt == PK_SALT_DIGEST) *out = hlen;
  else if (salt == PK_SALT_AUTO || salt == PK_SALT_MAX) *out = em_len >= hlen + 2 ? em_len - hlen - 2 : 0;
  else if (salt == PK_SALT_AUTO_DIGEST_MAX) {
    size_t max = em_len >= hlen + 2 ? em_len - hlen - 2 : 0;
    *out = hlen < max ? hlen : max;
  } else if (salt < PK_SALT_AUTO_DIGEST_MAX) {
    pk_provider("1C800070", "invalid salt length", "ERR_OSSL_INVALID_SALT_LENGTH");
    return false;
  } else *out = (size_t)salt;
  return true;
}

/* An RSA provider failure. crypto.sign's job reports the provider error
 * and the error itself twice; Sign.prototype.sign reports the provider
 * entries only. */
static void pk_rsa_failure(const char *packed, const char *reason, const char *code, bool oneshot) {
  char message[128], entry[128];
  snprintf(message, sizeof message, "error:%s:rsa routines::%s", packed, reason);
  snprintf(entry, sizeof entry, "%s", message);
  const char *const stack[] = {
    "error:030000EA:digital envelope routines::provider signature failure",
    "error:1C880004:Provider routines::RSA lib",
    entry,
    "error:1C880004:Provider routines::RSA lib",
  };
  pk_throw_openssl(message, "rsa routines", reason, code, stack, oneshot ? 4 : 2);
}

static void pk_too_large(bool oneshot) {
  pk_rsa_failure("0200006E", "data too large for key size", "ERR_OSSL_RSA_DATA_TOO_LARGE_FOR_KEY_SIZE", oneshot);
}

/* Signing with the public half of a JWK. */
static void pk_missing_private(const PkKey *key, bool oneshot) {
  if (pk_is_rsa(key)) {
    pk_rsa_failure("020000B3", "missing private key", "ERR_OSSL_RSA_MISSING_PRIVATE_KEY", oneshot);
    return;
  }
  const char *const stack[] = {
    "error:030000EA:digital envelope routines::provider signature failure",
    "error:0800007D:elliptic curve routines::missing private key",
  };
  pk_throw_openssl("error:0800007D:elliptic curve routines::missing private key", "elliptic curve routines",
                   "missing private key", "ERR_OSSL_EC_MISSING_PRIVATE_KEY", stack, oneshot ? 2 : 1);
}

/* The encoded message for RSA signing: EMSA-PKCS1-v1_5 or EMSA-PSS, at
 * the modulus length. */
static bool pk_rsa_encode(const PkKey *key, ScrDigestAlg alg, const unsigned char *hash, size_t hlen, int padding,
                          const PkSigOptions *opts, unsigned char *em, size_t k, bool oneshot) {
  size_t mod_bits = mbedtls_pk_get_bitlen(&key->pk);
  if (padding == PK_PADDING_PKCS1) {
    const unsigned char *prefix = NULL;
    size_t prefix_len = 0;
    for (size_t i = 0; i < sizeof pk_digest_info / sizeof pk_digest_info[0]; i++)
      if (pk_digest_info[i].alg == alg) {
        prefix = pk_digest_info[i].prefix;
        prefix_len = pk_digest_info[i].len;
      }
    size_t t_len = prefix_len + hlen;
    if (k < t_len + 11) {
      pk_too_large(oneshot);
      return false;
    }
    em[0] = 0x00;
    em[1] = 0x01;
    memset(em + 2, 0xff, k - t_len - 3);
    em[k - t_len - 1] = 0x00;
    if (prefix_len) memcpy(em + k - t_len, prefix, prefix_len);
    memcpy(em + k - hlen, hash, hlen);
    return true;
  }
  size_t em_bits = mod_bits - 1;
  size_t em_len = (em_bits + 7) / 8;
  size_t salt_len;
  if (!pk_pss_salt_for_sign(opts->has_salt ? opts->salt : PK_SALT_AUTO, hlen, em_len, &salt_len)) return false;
  if (em_len < hlen + 2 || salt_len > em_len - hlen - 2) {
    pk_too_large(oneshot);
    return false;
  }
  unsigned char salt[1024];
  if (salt_len > sizeof salt) {
    pk_too_large(oneshot);
    return false;
  }
  arc4random_buf(salt, salt_len);
  unsigned char *out = em + (k - em_len);
  memset(em, 0, k - em_len);
  size_t db_len = em_len - hlen - 1;
  unsigned char h[64];
  pk_pss_hash(alg, hash, hlen, salt, salt_len, h);
  memset(out, 0, db_len);
  out[db_len - salt_len - 1] = 0x01;
  memcpy(out + db_len - salt_len, salt, salt_len);
  pk_mgf1_xor(alg, h, hlen, out, db_len);
  out[0] &= (unsigned char)(0xff >> (8 * em_len - em_bits));
  memcpy(out + db_len, h, hlen);
  out[em_len - 1] = 0xbc;
  mbedtls_platform_zeroize(salt, salt_len);
  return true;
}

static bool pk_rsa_verify_pkcs1(ScrDigestAlg alg, const unsigned char *hash, size_t hlen, const unsigned char *em,
                                size_t k) {
  unsigned char *expected = malloc(k);
  if (!expected) scr_trap("scriptc: out of memory\n");
  const unsigned char *prefix = NULL;
  size_t prefix_len = 0;
  for (size_t i = 0; i < sizeof pk_digest_info / sizeof pk_digest_info[0]; i++)
    if (pk_digest_info[i].alg == alg) {
      prefix = pk_digest_info[i].prefix;
      prefix_len = pk_digest_info[i].len;
    }
  size_t t_len = prefix_len + hlen;
  bool ok = k >= t_len + 11;
  if (ok) {
    expected[0] = 0x00;
    expected[1] = 0x01;
    memset(expected + 2, 0xff, k - t_len - 3);
    expected[k - t_len - 1] = 0x00;
    if (prefix_len) memcpy(expected + k - t_len, prefix, prefix_len);
    memcpy(expected + k - hlen, hash, hlen);
    ok = memcmp(expected, em, k) == 0;
  }
  free(expected);
  return ok;
}

/* RSA_verify_PKCS1_PSS_mgf1 with OpenSSL's salt length rules. */
static int pk_rsa_verify_pss(ScrDigestAlg alg, const unsigned char *hash, size_t hlen, const unsigned char *em_full,
                             size_t k, size_t mod_bits, int salt) {
  if (salt < PK_SALT_AUTO_DIGEST_MAX) {
    pk_provider("1C800070", "invalid salt length", "ERR_OSSL_INVALID_SALT_LENGTH");
    return -1;
  }
  size_t em_bits = mod_bits - 1;
  size_t em_len = (em_bits + 7) / 8;
  const unsigned char *em = em_full;
  if (em_len < k) {
    if (em_full[0] != 0) return 0;
    em = em_full + 1;
  }
  if (em_len < hlen + 2 || em[em_len - 1] != 0xbc) return 0;
  if (em[0] & (unsigned char)(0xff << (8 - (8 * em_len - em_bits))) & 0xff && (8 * em_len - em_bits) != 0) return 0;
  size_t db_len = em_len - hlen - 1;
  unsigned char *db = malloc(db_len);
  if (!db) scr_trap("scriptc: out of memory\n");
  memcpy(db, em, db_len);
  const unsigned char *h = em + db_len;
  pk_mgf1_xor(alg, h, hlen, db, db_len);
  db[0] &= (unsigned char)(0xff >> (8 * em_len - em_bits));
  size_t i = 0;
  while (i < db_len - 1 && db[i] == 0) i++;
  int result = 0;
  if (db[i] == 0x01) {
    size_t salt_len = db_len - i - 1;
    size_t max = em_len - hlen - 2;
    /* RSA_PSS_SALTLEN_AUTO and RSA_PSS_SALTLEN_AUTO_DIGEST_MAX recover the
     * salt length from the encoding. */
    bool salt_ok = salt == PK_SALT_AUTO || salt == PK_SALT_AUTO_DIGEST_MAX ? true
                   : salt == PK_SALT_DIGEST ? salt_len == hlen
                   : salt == PK_SALT_MAX ? salt_len == max
                                         : salt_len == (size_t)salt;
    if (salt_ok) {
      unsigned char check[64];
      pk_pss_hash(alg, hash, hlen, db + i + 1, salt_len, check);
      result = memcmp(check, h, hlen) == 0 ? 1 : 0;
    }
  }
  free(db);
  return result;
}

/* ECDSA-Sig-Value { r INTEGER, s INTEGER } in minimal DER. */
static void pk_ecdsa_der(const mbedtls_mpi *r, const mbedtls_mpi *s, PkBuf *out) {
  PkBuf body = {0};
  const mbedtls_mpi *values[] = {r, s};
  for (size_t i = 0; i < 2; i++) {
    unsigned char bytes[80];
    size_t n = mbedtls_mpi_size(values[i]);
    mbedtls_mpi_write_binary(values[i], bytes, n);
    pk_der_uint(&body, bytes, n);
  }
  pk_der_put(out, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE, body.data, body.len);
  pk_buf_free(&body);
}

/* d2i_ECDSA_SIG then i2d, compared with the input: a signature that is
 * not canonical DER does not verify. */
static bool pk_ecdsa_read_der(const unsigned char *sig, size_t len, mbedtls_mpi *r, mbedtls_mpi *s) {
  unsigned char *p = (unsigned char *)sig, *end = p + len;
  size_t n;
  if (mbedtls_asn1_get_tag(&p, end, &n, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) return false;
  if (p + n != end) return false;
  if (mbedtls_asn1_get_mpi(&p, end, r) != 0 || mbedtls_asn1_get_mpi(&p, end, s) != 0 || p != end) return false;
  PkBuf canonical = {0};
  pk_ecdsa_der(r, s, &canonical);
  bool same = canonical.len == len && memcmp(canonical.data, sig, len) == 0;
  pk_buf_free(&canonical);
  return same;
}

/* The signature of digest by key, or NULL with Node's error pending. */
static ScrBytes *pk_sign_digest(const PkKey *key, ScrDigestAlg alg, const unsigned char *hash, size_t hlen,
                                const PkSigOptions *opts, bool oneshot) {
  if (!pk_check_digest(key, alg)) return NULL;
  if (pk_is_rsa(key)) {
    int padding;
    if (!pk_check_padding(key, opts, &padding)) return NULL;
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key->pk);
    size_t k = mbedtls_rsa_get_len(rsa);
    unsigned char *em = calloc(1, k);
    if (!em) scr_trap("scriptc: out of memory\n");
    ScrBytes *out = NULL;
    if (pk_rsa_encode(key, alg, hash, hlen, padding, opts, em, k, oneshot)) {
      if (key->no_private) {
        pk_missing_private(key, oneshot);
        mbedtls_platform_zeroize(em, k);
        free(em);
        return NULL;
      }
      out = scr_bytes_new(SCR_BYTES_U8, (double)k);
      out->is_buffer = true;
      if (mbedtls_rsa_private(rsa, pk_rng, NULL, em, out->data) != 0) {
        scr_bytes_release(out);
        out = NULL;
        pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED", "PEM_read_bio_PrivateKey failed");
      }
    }
    mbedtls_platform_zeroize(em, k);
    free(em);
    return out;
  }
  mbedtls_ecp_group grp;
  mbedtls_mpi d, r, s;
  mbedtls_ecp_point q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);
  mbedtls_ecp_point_init(&q);
  ScrBytes *out = NULL;
  if (key->no_private) {
    pk_missing_private(key, oneshot);
    return NULL;
  }
  int ret = mbedtls_ecp_export(mbedtls_pk_ec(key->pk), &grp, &d, &q);
  if (ret == 0) ret = mbedtls_ecdsa_sign(&grp, &r, &s, &d, hash, hlen, pk_rng, NULL);
  if (ret == 0) {
    PkBuf sig = {0};
    if (opts->p1363) {
      size_t n = (mbedtls_mpi_bitlen(&grp.N) + 7) / 8;
      pk_buf_reserve(&sig, 2 * n);
      sig.len = 2 * n;
      mbedtls_mpi_write_binary(&r, sig.data, n);
      mbedtls_mpi_write_binary(&s, sig.data + n, n);
    } else {
      pk_ecdsa_der(&r, &s, &sig);
    }
    out = scr_bytes_new(SCR_BYTES_U8, (double)sig.len);
    out->is_buffer = true;
    memcpy(out->data, sig.data, sig.len);
    pk_buf_free(&sig);
  } else {
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED", "PEM_read_bio_PrivateKey failed");
  }
  mbedtls_ecp_group_free(&grp);
  mbedtls_mpi_free(&d);
  mbedtls_mpi_free(&r);
  mbedtls_mpi_free(&s);
  mbedtls_ecp_point_free(&q);
  return out;
}

/* Whether the provider accepts this digest, padding and salt length for
 * verification. Verify.prototype.verify answers false where crypto.verify
 * throws. */
static bool pk_verify_options_ok(const PkKey *key, ScrDigestAlg alg, const PkSigOptions *opts) {
  if (pk_is_rsa(key)) {
    if (!pk_rsa_digest(alg)) return false;
    if (opts->has_padding && opts->padding != PK_PADDING_PKCS1 && opts->padding != PK_PADDING_PSS && opts->padding != -1)
      return false;
    if (opts->has_padding && opts->padding == PK_PADDING_PSS && opts->has_salt && opts->salt < PK_SALT_AUTO_DIGEST_MAX)
      return false;
    return true;
  }
  return !scr_crypto_digest_is_xof(alg);
}

/* 1 verified, 0 not, -1 with an error pending. */
static int pk_verify_digest(const PkKey *key, ScrDigestAlg alg, const unsigned char *hash, size_t hlen,
                            const unsigned char *sig, size_t sig_len, const PkSigOptions *opts) {
  if (!pk_check_digest(key, alg)) return -1;
  if (pk_is_rsa(key)) {
    int padding;
    if (!pk_check_padding(key, opts, &padding)) return -1;
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key->pk);
    size_t k = mbedtls_rsa_get_len(rsa);
    if (padding == PK_PADDING_PSS && opts->has_salt && opts->salt < PK_SALT_AUTO_DIGEST_MAX) {
      pk_provider("1C800070", "invalid salt length", "ERR_OSSL_INVALID_SALT_LENGTH");
      return -1;
    }
    if (sig_len != k) return 0;
    unsigned char *em = malloc(k);
    if (!em) scr_trap("scriptc: out of memory\n");
    int result = 0;
    if (mbedtls_rsa_public(rsa, sig, em) == 0) {
      if (padding == PK_PADDING_PKCS1) result = pk_rsa_verify_pkcs1(alg, hash, hlen, em, k) ? 1 : 0;
      else
        result = pk_rsa_verify_pss(alg, hash, hlen, em, k, mbedtls_pk_get_bitlen(&key->pk),
                                   opts->has_salt ? opts->salt : PK_SALT_AUTO);
    }
    free(em);
    return result;
  }
  mbedtls_ecp_group grp;
  mbedtls_mpi d, r, s;
  mbedtls_ecp_point q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);
  mbedtls_ecp_point_init(&q);
  int result = 0;
  if (mbedtls_ecp_export(mbedtls_pk_ec(key->pk), &grp, &d, &q) == 0) {
    bool parsed;
    if (opts->p1363) {
      size_t n = (mbedtls_mpi_bitlen(&grp.N) + 7) / 8;
      parsed = sig_len == 2 * n && mbedtls_mpi_read_binary(&r, sig, n) == 0 &&
               mbedtls_mpi_read_binary(&s, sig + n, n) == 0;
    } else {
      parsed = pk_ecdsa_read_der(sig, sig_len, &r, &s);
    }
    if (parsed && mbedtls_ecdsa_verify(&grp, hash, hlen, &q, &r, &s) == 0) result = 1;
  }
  mbedtls_ecp_group_free(&grp);
  mbedtls_mpi_free(&d);
  mbedtls_mpi_free(&r);
  mbedtls_mpi_free(&s);
  mbedtls_ecp_point_free(&q);
  return result;
}

/* ncrypto::getDigestByName for a non-null algorithm; a null or undefined
 * algorithm takes the key type's default digest, SHA-256. */
static bool pk_digest_of(const ScrDyn *algorithm, ScrDigestAlg *alg, bool named_error) {
  if (pk_nullish(algorithm)) {
    *alg = SCR_DIGEST_SHA256;
    return true;
  }
  const ScrStr *name = algorithm->v.str;
  int found = scr_crypto_digest_lookup(name->data, name->len);
  if (found < 0) {
    if (!named_error) {
      pk_throw(SCR_ERR_TYPE, "ERR_CRYPTO_INVALID_DIGEST", "Invalid digest");
      return false;
    }
    ScrJsonBuf b;
    scr_jb_init(&b);
    scr_jb_puts(&b, "Invalid digest: ");
    scr_jb_put_str(&b, name);
    ScrStr *message = scr_jb_finish(&b);
    scr_throw_error_msg_code(SCR_ERR_TYPE, message->data, message->len, "ERR_CRYPTO_INVALID_DIGEST");
    scr_str_release(message);
    return false;
  }
  *alg = (ScrDigestAlg)found;
  return true;
}

static void pk_hash(ScrDigestAlg alg, const ScrBytes *data, unsigned char out[64], size_t *len) {
  ScrCryptoDigest *d = scr_crypto_digest_new(alg);
  scr_crypto_digest_feed(d, data->data, (size_t)scr_bytes_byte_len(data));
  *len = scr_crypto_digest_finish(d, out);
  scr_crypto_digest_free(d);
}

static bool pk_callback_check(const ScrDyn *callback) {
  if (!callback || callback->kind == SCR_DYN_UNDEF) return true;
  if (callback->kind != SCR_DYN_FUNC) {
    scr_dyn_arg_type_fail("callback", "of type function", callback);
    return false;
  }
  pk_refuse("crypto.sign and crypto.verify with a callback have no native implementation; call them without one");
  return false;
}

/* crypto.sign(algorithm, data, key): signOneShot. +1 Buffer or NULL. */
ScrDyn *scr_crypto_sign_oneshot(ScrDyn *algorithm, ScrDyn *data, ScrDyn *key, ScrDyn *callback) {
  ScrDyn *alg_value = pk_unbox(algorithm);
  ScrDyn *data_value = alg_value ? pk_unbox(data) : NULL;
  ScrDyn *key_value = data_value ? pk_unbox(key) : NULL;
  ScrBytes *bytes = NULL;
  PkKey *pkey = NULL;
  ScrDyn *result = NULL;
  if (!key_value) goto done;
  if (!pk_nullish(alg_value) && alg_value->kind != SCR_DYN_STR) {
    scr_dyn_arg_type_fail("algorithm", "of type string", alg_value);
    goto done;
  }
  if (!pk_callback_check(callback)) goto done;
  if (!(bytes = pk_buffer_or_view(data_value, "data", NULL))) goto done;
  if (!scr_dyn_truthy(key_value)) {
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_SIGN_KEY_REQUIRED", "No key provided to sign");
    goto done;
  }
  PkSigOptions opts;
  if (!pk_sig_options(key_value, &opts)) goto done;
  if (!(pkey = pk_prepare(key_value, PK_CONSUME_PRIVATE, "key"))) goto done;
  ScrDigestAlg alg;
  if (!pk_digest_of(alg_value, &alg, true)) goto done;
  unsigned char hash[64];
  size_t hlen;
  pk_hash(alg, bytes, hash, &hlen);
  ScrBytes *sig = pk_sign_digest(pkey, alg, hash, hlen, &opts, true);
  if (sig) {
    result = scr_dyn_new_bytes(sig);
    scr_bytes_release(sig);
  }
done:
  if (alg_value) scr_dyn_release(alg_value);
  if (data_value) scr_dyn_release(data_value);
  if (key_value) scr_dyn_release(key_value);
  scr_bytes_release(bytes);
  if (pkey) pk_key_release(pkey);
  return result;
}

/* crypto.verify(algorithm, data, key, signature): verifyOneShot. +1
 * boolean or NULL. */
ScrDyn *scr_crypto_verify_oneshot(ScrDyn *algorithm, ScrDyn *data, ScrDyn *key, ScrDyn *signature,
                                  ScrDyn *callback) {
  ScrDyn *alg_value = pk_unbox(algorithm);
  ScrDyn *data_value = alg_value ? pk_unbox(data) : NULL;
  ScrDyn *key_value = data_value ? pk_unbox(key) : NULL;
  ScrDyn *sig_value = key_value ? pk_unbox(signature) : NULL;
  ScrBytes *bytes = NULL, *sig = NULL;
  PkKey *pkey = NULL;
  ScrDyn *result = NULL;
  if (!sig_value) goto done;
  if (!pk_nullish(alg_value) && alg_value->kind != SCR_DYN_STR) {
    scr_dyn_arg_type_fail("algorithm", "of type string", alg_value);
    goto done;
  }
  if (!pk_callback_check(callback)) goto done;
  if (!(bytes = pk_buffer_or_view(data_value, "data", NULL))) goto done;
  if (pk_nullish(key_value)) {
    pk_nullish_read(key_value, "padding");
    goto done;
  }
  PkSigOptions opts;
  if (!pk_sig_options(key_value, &opts)) goto done;
  if (!(sig = pk_buffer_or_view(sig_value, "signature", NULL))) goto done;
  if (!(pkey = pk_prepare(key_value, PK_CONSUME_PUBLIC, "key"))) goto done;
  ScrDigestAlg alg;
  if (!pk_digest_of(alg_value, &alg, true)) goto done;
  unsigned char hash[64];
  size_t hlen;
  pk_hash(alg, bytes, hash, &hlen);
  int verified = pk_verify_digest(pkey, alg, hash, hlen, sig->data, (size_t)scr_bytes_byte_len(sig), &opts);
  if (verified >= 0) result = scr_dyn_new_bool(verified == 1);
done:
  if (alg_value) scr_dyn_release(alg_value);
  if (data_value) scr_dyn_release(data_value);
  if (key_value) scr_dyn_release(key_value);
  if (sig_value) scr_dyn_release(sig_value);
  scr_bytes_release(bytes);
  scr_bytes_release(sig);
  if (pkey) pk_key_release(pkey);
  return result;
}

/* ── Sign and Verify ──────────────────────────────────────────────────── */

typedef struct PkSigner {
  size_t rc;
  bool verify;
  ScrDigestAlg alg;
  ScrCryptoDigest *digest; /* NULL after sign() or verify() */
} PkSigner;

static void *pk_signer_retain(void *h) {
  ((PkSigner *)h)->rc++;
  return h;
}

static void pk_signer_release(void *h) {
  PkSigner *s = h;
  if (!s || --s->rc) return;
  scr_crypto_digest_free(s->digest);
  free(s);
}

static ScrDyn *pk_arg(ScrDyn *const *args, size_t argc, size_t index) {
  return index < argc ? args[index] : scr_dyn_undefined();
}

static void pk_not_initialised(void) {
  pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_INVALID_STATE", "Not initialised");
}

/* Sign.prototype.update and Verify.prototype.update. */
static ScrDyn *pk_signer_update(PkSigner *s, ScrDyn *self, ScrDyn *data, ScrDyn *encoding) {
  ScrBytes *in = NULL;
  if (data->kind == SCR_DYN_STR) {
    /* validateEncoding: hex input must have an even length. */
    const char *name = encoding->kind == SCR_DYN_STR ? pk_normalize_encoding(encoding->v.str) : NULL;
    if (name && strcmp(name, "hex") == 0 && (unsigned long)scr_str_utf16_len(data->v.str) % 2 != 0) {
      char rendered[160];
      const char *text = scr_dyn_inspect_lite(encoding, rendered, sizeof rendered);
      char message[256];
      snprintf(message, sizeof message, "The argument 'encoding' is invalid for data of length %lu. Received %s",
               (unsigned long)scr_str_utf16_len(data->v.str), text);
      pk_throw(SCR_ERR_TYPE, "ERR_INVALID_ARG_VALUE", message);
      return NULL;
    }
    /* C++ ParseEncoding: unknown names read as utf8. */
    ScrStr *canonical = scr_str_new(name ? name : "utf8", strlen(name ? name : "utf8"));
    in = scr_bytes_from_str(data->v.str, canonical);
    scr_str_release(canonical);
  } else if (pk_is_view(data)) {
    in = scr_bytes_buffer_source(data);
  } else {
    scr_dyn_arg_type_fail("data", "of type string or an instance of Buffer, TypedArray, or DataView", data);
    return NULL;
  }
  if (!in) return NULL;
  if (!s->digest) {
    scr_bytes_release(in);
    pk_not_initialised();
    return NULL;
  }
  scr_crypto_digest_feed(s->digest, in->data, (size_t)scr_bytes_byte_len(in));
  scr_bytes_release(in);
  return scr_dyn_retain(self);
}

/* The digest so far; the signer is spent afterwards. */
static bool pk_signer_finish(PkSigner *s, unsigned char out[64], size_t *len) {
  if (!s->digest) {
    pk_not_initialised();
    return false;
  }
  if (s->alg == SCR_DIGEST_SHAKE128 || s->alg == SCR_DIGEST_SHAKE256) {
    scr_crypto_digest_free(s->digest);
    s->digest = NULL;
    pk_xof_final();
    return false;
  }
  *len = scr_crypto_digest_finish(s->digest, out);
  scr_crypto_digest_free(s->digest);
  s->digest = NULL;
  return true;
}

/* Buffer.prototype.toString(encoding) for the result. */
static ScrDyn *pk_encode_output(ScrBytes *bytes, const ScrDyn *encoding) {
  if (!scr_dyn_truthy(encoding) || pk_str_is(encoding, "buffer")) {
    ScrDyn *result = scr_dyn_new_bytes(bytes);
    scr_bytes_release(bytes);
    return result;
  }
  ScrStr *name = encoding->kind == SCR_DYN_STR ? scr_str_retain(encoding->v.str) : scr_dyn_string_coerce_js(encoding);
  ScrStr *text = NULL;
  if (name) {
    const char *canonical = pk_normalize_encoding(name);
    if (!canonical) {
      ScrDyn *as_dyn = scr_dyn_new_str(name);
      pk_unknown_encoding(as_dyn);
      scr_dyn_release(as_dyn);
    } else {
      ScrStr *enc = scr_str_new(canonical, strlen(canonical));
      text = scr_bytes_to_str(bytes, enc);
      scr_str_release(enc);
    }
    scr_str_release(name);
  }
  scr_bytes_release(bytes);
  if (!text) return NULL;
  ScrDyn *result = scr_dyn_new_str(text);
  scr_str_release(text);
  return result;
}

/* Sign.prototype.sign(privateKey[, outputEncoding]). */
static ScrDyn *pk_signer_sign(PkSigner *s, ScrDyn *key, ScrDyn *encoding) {
  if (!scr_dyn_truthy(key)) {
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_SIGN_KEY_REQUIRED", "No key provided to sign");
    return NULL;
  }
  PkKey *pkey = pk_prepare(key, PK_CONSUME_PRIVATE, "privateKey");
  if (!pkey) return NULL;
  PkSigOptions opts;
  ScrDyn *result = NULL;
  unsigned char hash[64];
  size_t hlen;
  if (pk_sig_options(key, &opts) && pk_signer_finish(s, hash, &hlen)) {
    ScrBytes *sig = pk_sign_digest(pkey, s->alg, hash, hlen, &opts, false);
    if (sig) result = pk_encode_output(sig, encoding);
  }
  pk_key_release(pkey);
  return result;
}

/* Verify.prototype.verify(key, signature[, signatureEncoding]). */
static ScrDyn *pk_signer_verify(PkSigner *s, ScrDyn *key, ScrDyn *signature, ScrDyn *encoding) {
  PkKey *pkey = pk_prepare(key, PK_CONSUME_PUBLIC, "key");
  if (!pkey) return NULL;
  ScrDyn *result = NULL;
  PkSigOptions opts;
  ScrBytes *sig = NULL;
  if (!pk_sig_options(key, &opts)) goto done;
  if (!(sig = pk_buffer_or_view(signature, "signature", encoding))) goto done;
  size_t sig_len = (size_t)scr_bytes_byte_len(sig);
  if (opts.p1363 && !pk_is_rsa(pkey) && sig_len != 2 * pk_ec_order_bytes(pkey)) {
    pk_throw(SCR_ERR_ERROR, "ERR_CRYPTO_OPERATION_FAILED", "Malformed signature");
    goto done;
  }
  unsigned char hash[64];
  size_t hlen;
  if (!pk_signer_finish(s, hash, &hlen)) goto done;
  if (!pk_verify_options_ok(pkey, s->alg, &opts)) {
    result = scr_dyn_new_bool(false);
    goto done;
  }
  int verified = pk_verify_digest(pkey, s->alg, hash, hlen, sig->data, sig_len, &opts);
  if (verified >= 0) result = scr_dyn_new_bool(verified == 1);
done:
  scr_bytes_release(sig);
  pk_key_release(pkey);
  return result;
}

/* stream.Writable members a Sign or Verify inherits. */
static bool pk_stream_member(const char *name) {
  static const char *const members[] = {
    "write", "end", "on", "once", "off", "addListener", "removeListener", "removeAllListeners", "emit",
    "destroy", "cork", "uncork", "setDefaultEncoding", "prependListener", "prependOnceListener", "listeners",
    "listenerCount", "eventNames", "_write", "pipe",
  };
  for (size_t i = 0; i < sizeof members / sizeof members[0]; i++)
    if (strcmp(name, members[i]) == 0) return true;
  return false;
}

static ScrDyn *pk_signer_invoke(void *h, ScrDyn *self, const char *method, ScrDyn *const *args, size_t argc,
                                const char *what) {
  PkSigner *s = h;
  ScrDyn *unboxed[3] = {NULL, NULL, NULL};
  size_t count = argc < 3 ? argc : 3;
  ScrDyn *result = NULL;
  for (size_t i = 0; i < count; i++)
    if (!(unboxed[i] = pk_unbox(args[i]))) goto done;
  if (strcmp(method, "update") == 0) {
    result = pk_signer_update(s, self, pk_arg(unboxed, count, 0), pk_arg(unboxed, count, 1));
  } else if (!s->verify && strcmp(method, "sign") == 0) {
    result = pk_signer_sign(s, pk_arg(unboxed, count, 0), pk_arg(unboxed, count, 1));
  } else if (s->verify && strcmp(method, "verify") == 0) {
    result = pk_signer_verify(s, pk_arg(unboxed, count, 0), pk_arg(unboxed, count, 1), pk_arg(unboxed, count, 2));
  } else if (pk_stream_member(method)) {
    pk_refuse(s->verify ? "Verify stream members have no lowering; call update() and verify()"
                        : "Sign stream members have no lowering; call update() and sign()");
  } else {
    pk_not_a_function(what);
  }
done:
  for (size_t i = 0; i < count; i++) scr_dyn_release(unboxed[i]);
  return result;
}

static ScrDyn *pk_signer_get(void *h, const char *key, size_t key_len) {
  (void)key;
  (void)key_len;
  pk_refuse(((PkSigner *)h)->verify ? "Verify property reads have no lowering; call update() and verify()"
                                    : "Sign property reads have no lowering; call update() and sign()");
  return NULL;
}

static bool pk_signer_set(void *h, const char *key, size_t key_len, const ScrDyn *value) {
  (void)key;
  (void)key_len;
  (void)value;
  pk_refuse(((PkSigner *)h)->verify ? "Writing Verify properties has no lowering" : "Writing Sign properties has no lowering");
  return true;
}

static const ScrDynHandleOps pk_sign_ops = {
    .cls = "Sign",
    .retain = pk_signer_retain,
    .release = pk_signer_release,
    .invoke = pk_signer_invoke,
    .get = pk_signer_get,
    .set = pk_signer_set,
};

static const ScrDynHandleOps pk_verify_ops = {
    .cls = "Verify",
    .retain = pk_signer_retain,
    .release = pk_signer_release,
    .invoke = pk_signer_invoke,
    .get = pk_signer_get,
    .set = pk_signer_set,
};

/* createSign(algorithm[, options]) (verify false) and
 * createVerify(algorithm[, options]) (verify true). The options are
 * stream.Writable's; they are read by the stream members, which have no
 * lowering. +1 handle or NULL. */
ScrDyn *scr_crypto_signer_new(bool verify, ScrDyn *algorithm, ScrDyn *options) {
  (void)options;
  ScrDyn *value = pk_unbox(algorithm);
  if (!value) return NULL;
  ScrDyn *result = NULL;
  if (value->kind != SCR_DYN_STR) {
    scr_dyn_arg_type_fail("algorithm", "of type string", value);
  } else {
    ScrDigestAlg alg;
    if (pk_digest_of(value, &alg, false)) {
      static SCR_TL bool installed;
      if (!installed) {
        installed = true;
        scr_dyn_handle_install(SCR_DYNH_SIGN, &pk_sign_ops);
        scr_dyn_handle_install(SCR_DYNH_VERIFY, &pk_verify_ops);
      }
      PkSigner *s = calloc(1, sizeof *s);
      if (!s) scr_trap("scriptc: out of memory\n");
      s->rc = 1;
      s->verify = verify;
      s->alg = alg;
      s->digest = scr_crypto_digest_new(alg);
      result = scr_dyn_new_handle(s, verify ? SCR_DYNH_VERIFY : SCR_DYNH_SIGN);
      pk_signer_release(s);
    }
  }
  scr_dyn_release(value);
  return result;
}
