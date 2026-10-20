/* Buffer.prototype and %TypedArray%.prototype members on checked-dynamic
 * typed arrays (an unannotated CommonJS parameter holding a Buffer, an
 * `any` Uint8Array). Linked only when the IR calls or reads one of these
 * member names on a dyn value (the dynBytes feature), so other binaries
 * keep their exact bytes.
 *
 * Calls on a typed-array receiver dispatch here directly
 * (scr_dyn_bytes_method_invoke); other receivers continue to
 * scr_dyn_invoke_prepared. Reads answer one interned function per member,
 * so `typeof b.equals` is "function", `a.equals === b.equals` holds and
 * aliases (readUint8, toLocaleString on a Buffer) are the same function.
 * The argument ladders follow Node 24's lib/buffer.js,
 * lib/internal/buffer.js and the src/node_buffer.cc search and fill
 * bindings; the typed-array members follow ECMA-262 and V8's messages. */
#include "scr_runtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  DBM_NONE,
  /* %TypedArray%.prototype */
  DBM_AT, DBM_COPY_WITHIN, DBM_ENTRIES, DBM_EVERY, DBM_FILL, DBM_FILTER, DBM_FIND, DBM_FIND_INDEX,
  DBM_FIND_LAST, DBM_FIND_LAST_INDEX, DBM_FOR_EACH, DBM_INCLUDES, DBM_INDEX_OF, DBM_JOIN, DBM_KEYS,
  DBM_LAST_INDEX_OF, DBM_MAP, DBM_REVERSE, DBM_REDUCE, DBM_REDUCE_RIGHT, DBM_SET, DBM_SLICE,
  DBM_SOME, DBM_SORT, DBM_SUBARRAY, DBM_TO_LOCALE_STRING, DBM_TO_REVERSED, DBM_TO_SORTED,
  DBM_TO_STRING, DBM_VALUES, DBM_WITH,
  /* Buffer.prototype */
  DBM_B_COMPARE, DBM_B_COPY, DBM_B_EQUALS, DBM_B_FILL, DBM_B_INCLUDES, DBM_B_INDEX_OF,
  DBM_B_INSPECT, DBM_B_LAST_INDEX_OF, DBM_B_SLICE, DBM_B_SUBARRAY, DBM_B_SWAP16, DBM_B_SWAP32,
  DBM_B_SWAP64, DBM_B_TO_JSON, DBM_B_TO_STRING, DBM_B_WRITE,
  DBM_B_READ_U8, DBM_B_READ_U16LE, DBM_B_READ_U16BE, DBM_B_READ_U32LE, DBM_B_READ_U32BE,
  DBM_B_READ_I8, DBM_B_READ_I16LE, DBM_B_READ_I16BE, DBM_B_READ_I32LE, DBM_B_READ_I32BE,
  DBM_B_READ_F32LE, DBM_B_READ_F32BE, DBM_B_READ_F64LE, DBM_B_READ_F64BE,
  DBM_B_READ_BIG_U64LE, DBM_B_READ_BIG_U64BE, DBM_B_READ_BIG_I64LE, DBM_B_READ_BIG_I64BE,
  DBM_B_READ_UINT_LE, DBM_B_READ_UINT_BE, DBM_B_READ_INT_LE, DBM_B_READ_INT_BE,
  DBM_B_WRITE_U8, DBM_B_WRITE_U16LE, DBM_B_WRITE_U16BE, DBM_B_WRITE_U32LE, DBM_B_WRITE_U32BE,
  DBM_B_WRITE_I8, DBM_B_WRITE_I16LE, DBM_B_WRITE_I16BE, DBM_B_WRITE_I32LE, DBM_B_WRITE_I32BE,
  DBM_B_WRITE_F32LE, DBM_B_WRITE_F32BE, DBM_B_WRITE_F64LE, DBM_B_WRITE_F64BE,
  DBM_B_WRITE_BIG_U64LE, DBM_B_WRITE_BIG_U64BE, DBM_B_WRITE_BIG_I64LE, DBM_B_WRITE_BIG_I64BE,
  DBM_B_WRITE_UINT_LE, DBM_B_WRITE_UINT_BE, DBM_B_WRITE_INT_LE, DBM_B_WRITE_INT_BE,
  DBM_B_ASCII_SLICE, DBM_B_BASE64_SLICE, DBM_B_BASE64URL_SLICE, DBM_B_LATIN1_SLICE,
  DBM_B_HEX_SLICE, DBM_B_UCS2_SLICE, DBM_B_UTF8_SLICE,
  DBM_B_ASCII_WRITE, DBM_B_BASE64_WRITE, DBM_B_BASE64URL_WRITE, DBM_B_LATIN1_WRITE,
  DBM_B_HEX_WRITE, DBM_B_UCS2_WRITE, DBM_B_UTF8_WRITE,
  DBM_COUNT
} DbmId;

/* The numeric read/write families: the operation and its element. */
typedef enum { DBM_NUM_NONE, DBM_NUM_READ, DBM_NUM_WRITE, DBM_VAR_READ, DBM_VAR_WRITE,
               DBM_BIG_READ, DBM_BIG_WRITE } DbmNumOp;

typedef struct {
  const char *name;   /* the function's own name ("" for Node's anonymous ones) */
  const char *member; /* the member name in errors */
  uint8_t length;
  uint8_t num_op;     /* DbmNumOp */
  uint8_t num_kind;   /* ScrBytesNumKind, or the signedness of the var/big families */
  bool le;
} DbmInfo;

#define DBM_TA(id, name, length) [id] = { name, name, length, DBM_NUM_NONE, 0, false }
#define DBM_BUF(id, name, length) [id] = { name, name, length, DBM_NUM_NONE, 0, false }
#define DBM_RW(id, name, member, length, op, kind, le) [id] = { name, member, length, op, kind, le }

static const DbmInfo dbm_info[DBM_COUNT] = {
  DBM_TA(DBM_AT, "at", 1), DBM_TA(DBM_COPY_WITHIN, "copyWithin", 2),
  DBM_TA(DBM_ENTRIES, "entries", 0), DBM_TA(DBM_EVERY, "every", 1), DBM_TA(DBM_FILL, "fill", 1),
  DBM_TA(DBM_FILTER, "filter", 1), DBM_TA(DBM_FIND, "find", 1),
  DBM_TA(DBM_FIND_INDEX, "findIndex", 1), DBM_TA(DBM_FIND_LAST, "findLast", 1),
  DBM_TA(DBM_FIND_LAST_INDEX, "findLastIndex", 1), DBM_TA(DBM_FOR_EACH, "forEach", 1),
  DBM_TA(DBM_INCLUDES, "includes", 1), DBM_TA(DBM_INDEX_OF, "indexOf", 1),
  DBM_TA(DBM_JOIN, "join", 1), DBM_TA(DBM_KEYS, "keys", 0),
  DBM_TA(DBM_LAST_INDEX_OF, "lastIndexOf", 1), DBM_TA(DBM_MAP, "map", 1),
  DBM_TA(DBM_REVERSE, "reverse", 0), DBM_TA(DBM_REDUCE, "reduce", 1),
  DBM_TA(DBM_REDUCE_RIGHT, "reduceRight", 1), DBM_TA(DBM_SET, "set", 1),
  DBM_TA(DBM_SLICE, "slice", 2), DBM_TA(DBM_SOME, "some", 1), DBM_TA(DBM_SORT, "sort", 1),
  DBM_TA(DBM_SUBARRAY, "subarray", 2), DBM_TA(DBM_TO_LOCALE_STRING, "toLocaleString", 0),
  DBM_TA(DBM_TO_REVERSED, "toReversed", 0), DBM_TA(DBM_TO_SORTED, "toSorted", 1),
  DBM_TA(DBM_TO_STRING, "toString", 0), DBM_TA(DBM_VALUES, "values", 0),
  DBM_TA(DBM_WITH, "with", 2),
  DBM_BUF(DBM_B_COMPARE, "compare", 5), DBM_BUF(DBM_B_COPY, "copy", 4),
  DBM_BUF(DBM_B_EQUALS, "equals", 1), DBM_BUF(DBM_B_FILL, "fill", 4),
  DBM_BUF(DBM_B_INCLUDES, "includes", 4), DBM_BUF(DBM_B_INDEX_OF, "indexOf", 4),
  DBM_BUF(DBM_B_INSPECT, "inspect", 2), DBM_BUF(DBM_B_LAST_INDEX_OF, "lastIndexOf", 4),
  DBM_BUF(DBM_B_SLICE, "slice", 2), DBM_BUF(DBM_B_SUBARRAY, "subarray", 2),
  DBM_BUF(DBM_B_SWAP16, "swap16", 0), DBM_BUF(DBM_B_SWAP32, "swap32", 0),
  DBM_BUF(DBM_B_SWAP64, "swap64", 0), DBM_BUF(DBM_B_TO_JSON, "toJSON", 0),
  DBM_BUF(DBM_B_TO_STRING, "toString", 3), DBM_BUF(DBM_B_WRITE, "write", 4),
  DBM_RW(DBM_B_READ_U8, "", "readUInt8", 1, DBM_NUM_READ, SCR_BN_U8, false),
  DBM_RW(DBM_B_READ_U16LE, "", "readUInt16LE", 1, DBM_NUM_READ, SCR_BN_U16, true),
  DBM_RW(DBM_B_READ_U16BE, "", "readUInt16BE", 1, DBM_NUM_READ, SCR_BN_U16, false),
  DBM_RW(DBM_B_READ_U32LE, "", "readUInt32LE", 1, DBM_NUM_READ, SCR_BN_U32, true),
  DBM_RW(DBM_B_READ_U32BE, "", "readUInt32BE", 1, DBM_NUM_READ, SCR_BN_U32, false),
  DBM_RW(DBM_B_READ_I8, "", "readInt8", 1, DBM_NUM_READ, SCR_BN_I8, false),
  DBM_RW(DBM_B_READ_I16LE, "", "readInt16LE", 1, DBM_NUM_READ, SCR_BN_I16, true),
  DBM_RW(DBM_B_READ_I16BE, "", "readInt16BE", 1, DBM_NUM_READ, SCR_BN_I16, false),
  DBM_RW(DBM_B_READ_I32LE, "", "readInt32LE", 1, DBM_NUM_READ, SCR_BN_I32, true),
  DBM_RW(DBM_B_READ_I32BE, "", "readInt32BE", 1, DBM_NUM_READ, SCR_BN_I32, false),
  /* Node assigns the float accessors from its Forwards/Backwards functions
   * by host byte order; every supported target is little-endian. */
  DBM_RW(DBM_B_READ_F32LE, "readFloatForwards", "readFloatLE", 0, DBM_NUM_READ, SCR_BN_F32, true),
  DBM_RW(DBM_B_READ_F32BE, "readFloatBackwards", "readFloatBE", 0, DBM_NUM_READ, SCR_BN_F32, false),
  DBM_RW(DBM_B_READ_F64LE, "readDoubleForwards", "readDoubleLE", 0, DBM_NUM_READ, SCR_BN_F64, true),
  DBM_RW(DBM_B_READ_F64BE, "readDoubleBackwards", "readDoubleBE", 0, DBM_NUM_READ, SCR_BN_F64, false),
  DBM_RW(DBM_B_READ_BIG_U64LE, "readBigUInt64LE", "readBigUInt64LE", 0, DBM_BIG_READ, 0, true),
  DBM_RW(DBM_B_READ_BIG_U64BE, "readBigUInt64BE", "readBigUInt64BE", 0, DBM_BIG_READ, 0, false),
  DBM_RW(DBM_B_READ_BIG_I64LE, "readBigInt64LE", "readBigInt64LE", 0, DBM_BIG_READ, 1, true),
  DBM_RW(DBM_B_READ_BIG_I64BE, "readBigInt64BE", "readBigInt64BE", 0, DBM_BIG_READ, 1, false),
  DBM_RW(DBM_B_READ_UINT_LE, "readUIntLE", "readUIntLE", 2, DBM_VAR_READ, 0, true),
  DBM_RW(DBM_B_READ_UINT_BE, "readUIntBE", "readUIntBE", 2, DBM_VAR_READ, 0, false),
  DBM_RW(DBM_B_READ_INT_LE, "readIntLE", "readIntLE", 2, DBM_VAR_READ, 1, true),
  DBM_RW(DBM_B_READ_INT_BE, "readIntBE", "readIntBE", 2, DBM_VAR_READ, 1, false),
  DBM_RW(DBM_B_WRITE_U8, "writeUInt8", "writeUInt8", 1, DBM_NUM_WRITE, SCR_BN_U8, false),
  DBM_RW(DBM_B_WRITE_U16LE, "writeUInt16LE", "writeUInt16LE", 1, DBM_NUM_WRITE, SCR_BN_U16, true),
  DBM_RW(DBM_B_WRITE_U16BE, "writeUInt16BE", "writeUInt16BE", 1, DBM_NUM_WRITE, SCR_BN_U16, false),
  DBM_RW(DBM_B_WRITE_U32LE, "writeUInt32LE", "writeUInt32LE", 1, DBM_NUM_WRITE, SCR_BN_U32, true),
  DBM_RW(DBM_B_WRITE_U32BE, "writeUInt32BE", "writeUInt32BE", 1, DBM_NUM_WRITE, SCR_BN_U32, false),
  DBM_RW(DBM_B_WRITE_I8, "writeInt8", "writeInt8", 1, DBM_NUM_WRITE, SCR_BN_I8, false),
  DBM_RW(DBM_B_WRITE_I16LE, "writeInt16LE", "writeInt16LE", 1, DBM_NUM_WRITE, SCR_BN_I16, true),
  DBM_RW(DBM_B_WRITE_I16BE, "writeInt16BE", "writeInt16BE", 1, DBM_NUM_WRITE, SCR_BN_I16, false),
  DBM_RW(DBM_B_WRITE_I32LE, "writeInt32LE", "writeInt32LE", 1, DBM_NUM_WRITE, SCR_BN_I32, true),
  DBM_RW(DBM_B_WRITE_I32BE, "writeInt32BE", "writeInt32BE", 1, DBM_NUM_WRITE, SCR_BN_I32, false),
  DBM_RW(DBM_B_WRITE_F32LE, "writeFloatForwards", "writeFloatLE", 1, DBM_NUM_WRITE, SCR_BN_F32, true),
  DBM_RW(DBM_B_WRITE_F32BE, "writeFloatBackwards", "writeFloatBE", 1, DBM_NUM_WRITE, SCR_BN_F32, false),
  DBM_RW(DBM_B_WRITE_F64LE, "writeDoubleForwards", "writeDoubleLE", 1, DBM_NUM_WRITE, SCR_BN_F64, true),
  DBM_RW(DBM_B_WRITE_F64BE, "writeDoubleBackwards", "writeDoubleBE", 1, DBM_NUM_WRITE, SCR_BN_F64, false),
  DBM_RW(DBM_B_WRITE_BIG_U64LE, "writeBigUInt64LE", "writeBigUInt64LE", 1, DBM_BIG_WRITE, 0, true),
  DBM_RW(DBM_B_WRITE_BIG_U64BE, "writeBigUInt64BE", "writeBigUInt64BE", 1, DBM_BIG_WRITE, 0, false),
  DBM_RW(DBM_B_WRITE_BIG_I64LE, "writeBigInt64LE", "writeBigInt64LE", 1, DBM_BIG_WRITE, 1, true),
  DBM_RW(DBM_B_WRITE_BIG_I64BE, "writeBigInt64BE", "writeBigInt64BE", 1, DBM_BIG_WRITE, 1, false),
  DBM_RW(DBM_B_WRITE_UINT_LE, "writeUIntLE", "writeUIntLE", 3, DBM_VAR_WRITE, 0, true),
  DBM_RW(DBM_B_WRITE_UINT_BE, "writeUIntBE", "writeUIntBE", 3, DBM_VAR_WRITE, 0, false),
  DBM_RW(DBM_B_WRITE_INT_LE, "writeIntLE", "writeIntLE", 3, DBM_VAR_WRITE, 1, true),
  DBM_RW(DBM_B_WRITE_INT_BE, "writeIntBE", "writeIntBE", 3, DBM_VAR_WRITE, 1, false),
  DBM_RW(DBM_B_ASCII_SLICE, "", "asciiSlice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_BASE64_SLICE, "", "base64Slice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_BASE64URL_SLICE, "", "base64urlSlice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_LATIN1_SLICE, "", "latin1Slice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_HEX_SLICE, "", "hexSlice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_UCS2_SLICE, "", "ucs2Slice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_UTF8_SLICE, "", "utf8Slice", 2, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_ASCII_WRITE, "", "asciiWrite", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_BASE64_WRITE, "", "base64Write", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_BASE64URL_WRITE, "", "base64urlWrite", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_LATIN1_WRITE, "", "latin1Write", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_HEX_WRITE, "", "hexWrite", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_UCS2_WRITE, "", "ucs2Write", 3, DBM_NUM_NONE, 0, false),
  DBM_RW(DBM_B_UTF8_WRITE, "", "utf8Write", 3, DBM_NUM_NONE, 0, false),
};

#undef DBM_TA
#undef DBM_BUF
#undef DBM_RW

/* Member names, sorted for bsearch: the %TypedArray%.prototype member and
 * the Buffer.prototype member each name answers (DBM_NONE when absent). A
 * Buffer reads its own member first. */
typedef struct {
  const char *name;
  uint8_t typed;
  uint8_t buffer;
} DbmName;

static const DbmName dbm_names[] = {
  { "asciiSlice", DBM_NONE, DBM_B_ASCII_SLICE },
  { "asciiWrite", DBM_NONE, DBM_B_ASCII_WRITE },
  { "at", DBM_AT, DBM_NONE },
  { "base64Slice", DBM_NONE, DBM_B_BASE64_SLICE },
  { "base64Write", DBM_NONE, DBM_B_BASE64_WRITE },
  { "base64urlSlice", DBM_NONE, DBM_B_BASE64URL_SLICE },
  { "base64urlWrite", DBM_NONE, DBM_B_BASE64URL_WRITE },
  { "compare", DBM_NONE, DBM_B_COMPARE },
  { "copy", DBM_NONE, DBM_B_COPY },
  { "copyWithin", DBM_COPY_WITHIN, DBM_NONE },
  { "entries", DBM_ENTRIES, DBM_NONE },
  { "equals", DBM_NONE, DBM_B_EQUALS },
  { "every", DBM_EVERY, DBM_NONE },
  { "fill", DBM_FILL, DBM_B_FILL },
  { "filter", DBM_FILTER, DBM_NONE },
  { "find", DBM_FIND, DBM_NONE },
  { "findIndex", DBM_FIND_INDEX, DBM_NONE },
  { "findLast", DBM_FIND_LAST, DBM_NONE },
  { "findLastIndex", DBM_FIND_LAST_INDEX, DBM_NONE },
  { "forEach", DBM_FOR_EACH, DBM_NONE },
  { "hexSlice", DBM_NONE, DBM_B_HEX_SLICE },
  { "hexWrite", DBM_NONE, DBM_B_HEX_WRITE },
  { "includes", DBM_INCLUDES, DBM_B_INCLUDES },
  { "indexOf", DBM_INDEX_OF, DBM_B_INDEX_OF },
  { "inspect", DBM_NONE, DBM_B_INSPECT },
  { "join", DBM_JOIN, DBM_NONE },
  { "keys", DBM_KEYS, DBM_NONE },
  { "lastIndexOf", DBM_LAST_INDEX_OF, DBM_B_LAST_INDEX_OF },
  { "latin1Slice", DBM_NONE, DBM_B_LATIN1_SLICE },
  { "latin1Write", DBM_NONE, DBM_B_LATIN1_WRITE },
  { "map", DBM_MAP, DBM_NONE },
  { "readBigInt64BE", DBM_NONE, DBM_B_READ_BIG_I64BE },
  { "readBigInt64LE", DBM_NONE, DBM_B_READ_BIG_I64LE },
  { "readBigUInt64BE", DBM_NONE, DBM_B_READ_BIG_U64BE },
  { "readBigUInt64LE", DBM_NONE, DBM_B_READ_BIG_U64LE },
  { "readBigUint64BE", DBM_NONE, DBM_B_READ_BIG_U64BE },
  { "readBigUint64LE", DBM_NONE, DBM_B_READ_BIG_U64LE },
  { "readDoubleBE", DBM_NONE, DBM_B_READ_F64BE },
  { "readDoubleLE", DBM_NONE, DBM_B_READ_F64LE },
  { "readFloatBE", DBM_NONE, DBM_B_READ_F32BE },
  { "readFloatLE", DBM_NONE, DBM_B_READ_F32LE },
  { "readInt16BE", DBM_NONE, DBM_B_READ_I16BE },
  { "readInt16LE", DBM_NONE, DBM_B_READ_I16LE },
  { "readInt32BE", DBM_NONE, DBM_B_READ_I32BE },
  { "readInt32LE", DBM_NONE, DBM_B_READ_I32LE },
  { "readInt8", DBM_NONE, DBM_B_READ_I8 },
  { "readIntBE", DBM_NONE, DBM_B_READ_INT_BE },
  { "readIntLE", DBM_NONE, DBM_B_READ_INT_LE },
  { "readUInt16BE", DBM_NONE, DBM_B_READ_U16BE },
  { "readUInt16LE", DBM_NONE, DBM_B_READ_U16LE },
  { "readUInt32BE", DBM_NONE, DBM_B_READ_U32BE },
  { "readUInt32LE", DBM_NONE, DBM_B_READ_U32LE },
  { "readUInt8", DBM_NONE, DBM_B_READ_U8 },
  { "readUIntBE", DBM_NONE, DBM_B_READ_UINT_BE },
  { "readUIntLE", DBM_NONE, DBM_B_READ_UINT_LE },
  { "readUint16BE", DBM_NONE, DBM_B_READ_U16BE },
  { "readUint16LE", DBM_NONE, DBM_B_READ_U16LE },
  { "readUint32BE", DBM_NONE, DBM_B_READ_U32BE },
  { "readUint32LE", DBM_NONE, DBM_B_READ_U32LE },
  { "readUint8", DBM_NONE, DBM_B_READ_U8 },
  { "readUintBE", DBM_NONE, DBM_B_READ_UINT_BE },
  { "readUintLE", DBM_NONE, DBM_B_READ_UINT_LE },
  { "reduce", DBM_REDUCE, DBM_NONE },
  { "reduceRight", DBM_REDUCE_RIGHT, DBM_NONE },
  { "reverse", DBM_REVERSE, DBM_NONE },
  { "set", DBM_SET, DBM_NONE },
  { "slice", DBM_SLICE, DBM_B_SLICE },
  { "some", DBM_SOME, DBM_NONE },
  { "sort", DBM_SORT, DBM_NONE },
  { "subarray", DBM_SUBARRAY, DBM_B_SUBARRAY },
  { "swap16", DBM_NONE, DBM_B_SWAP16 },
  { "swap32", DBM_NONE, DBM_B_SWAP32 },
  { "swap64", DBM_NONE, DBM_B_SWAP64 },
  { "toJSON", DBM_NONE, DBM_B_TO_JSON },
  /* Buffer.prototype.toLocaleString is Buffer.prototype.toString. */
  { "toLocaleString", DBM_TO_LOCALE_STRING, DBM_B_TO_STRING },
  { "toReversed", DBM_TO_REVERSED, DBM_NONE },
  { "toSorted", DBM_TO_SORTED, DBM_NONE },
  { "toString", DBM_TO_STRING, DBM_B_TO_STRING },
  { "ucs2Slice", DBM_NONE, DBM_B_UCS2_SLICE },
  { "ucs2Write", DBM_NONE, DBM_B_UCS2_WRITE },
  { "utf8Slice", DBM_NONE, DBM_B_UTF8_SLICE },
  { "utf8Write", DBM_NONE, DBM_B_UTF8_WRITE },
  { "values", DBM_VALUES, DBM_NONE },
  { "with", DBM_WITH, DBM_NONE },
  { "write", DBM_NONE, DBM_B_WRITE },
  { "writeBigInt64BE", DBM_NONE, DBM_B_WRITE_BIG_I64BE },
  { "writeBigInt64LE", DBM_NONE, DBM_B_WRITE_BIG_I64LE },
  { "writeBigUInt64BE", DBM_NONE, DBM_B_WRITE_BIG_U64BE },
  { "writeBigUInt64LE", DBM_NONE, DBM_B_WRITE_BIG_U64LE },
  { "writeBigUint64BE", DBM_NONE, DBM_B_WRITE_BIG_U64BE },
  { "writeBigUint64LE", DBM_NONE, DBM_B_WRITE_BIG_U64LE },
  { "writeDoubleBE", DBM_NONE, DBM_B_WRITE_F64BE },
  { "writeDoubleLE", DBM_NONE, DBM_B_WRITE_F64LE },
  { "writeFloatBE", DBM_NONE, DBM_B_WRITE_F32BE },
  { "writeFloatLE", DBM_NONE, DBM_B_WRITE_F32LE },
  { "writeInt16BE", DBM_NONE, DBM_B_WRITE_I16BE },
  { "writeInt16LE", DBM_NONE, DBM_B_WRITE_I16LE },
  { "writeInt32BE", DBM_NONE, DBM_B_WRITE_I32BE },
  { "writeInt32LE", DBM_NONE, DBM_B_WRITE_I32LE },
  { "writeInt8", DBM_NONE, DBM_B_WRITE_I8 },
  { "writeIntBE", DBM_NONE, DBM_B_WRITE_INT_BE },
  { "writeIntLE", DBM_NONE, DBM_B_WRITE_INT_LE },
  { "writeUInt16BE", DBM_NONE, DBM_B_WRITE_U16BE },
  { "writeUInt16LE", DBM_NONE, DBM_B_WRITE_U16LE },
  { "writeUInt32BE", DBM_NONE, DBM_B_WRITE_U32BE },
  { "writeUInt32LE", DBM_NONE, DBM_B_WRITE_U32LE },
  { "writeUInt8", DBM_NONE, DBM_B_WRITE_U8 },
  { "writeUIntBE", DBM_NONE, DBM_B_WRITE_UINT_BE },
  { "writeUIntLE", DBM_NONE, DBM_B_WRITE_UINT_LE },
  { "writeUint16BE", DBM_NONE, DBM_B_WRITE_U16BE },
  { "writeUint16LE", DBM_NONE, DBM_B_WRITE_U16LE },
  { "writeUint32BE", DBM_NONE, DBM_B_WRITE_U32BE },
  { "writeUint32LE", DBM_NONE, DBM_B_WRITE_U32LE },
  { "writeUint8", DBM_NONE, DBM_B_WRITE_U8 },
  { "writeUintBE", DBM_NONE, DBM_B_WRITE_UINT_BE },
  { "writeUintLE", DBM_NONE, DBM_B_WRITE_UINT_LE },
};

static int dbm_name_cmp(const void *key, const void *entry) {
  return strcmp((const char *)key, ((const DbmName *)entry)->name);
}

/* The member `name` answers on a typed array (a Buffer when `buffer`), or
 * DBM_NONE. */
static DbmId dbm_lookup(const char *name, bool buffer) {
  const DbmName *hit = bsearch(name, dbm_names, sizeof dbm_names / sizeof dbm_names[0],
                               sizeof dbm_names[0], dbm_name_cmp);
  if (!hit) return DBM_NONE;
  return (DbmId)(buffer && hit->buffer != DBM_NONE ? hit->buffer : hit->typed);
}

static bool dbm_is_buffer_method(DbmId id) { return id >= DBM_B_COMPARE; }

/* ── argument helpers ─────────────────────────────────────────────────── */

static ScrDyn *dbm_arg(ScrDyn *const *args, size_t argc, size_t i) {
  return i < argc ? args[i] : scr_dyn_undefined();
}

/* ToIntegerOrInfinity over an optional index argument; missing and
 * undefined take the operation's default. */
static bool dbm_index_arg(ScrDyn *const *args, size_t argc, size_t i, double fallback, double *out) {
  if (i >= argc || args[i]->kind == SCR_DYN_UNDEF) {
    *out = fallback;
    return true;
  }
  double value;
  if (!scr_dyn_number_coerce_js(args[i], &value)) return false;
  *out = isnan(value) ? 0 : trunc(value);
  return true;
}

static void dbm_throw_unsupported(const char *proto, const char *member) {
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_putc(&b, '\'');
  scr_jb_puts(&b, proto);
  scr_jb_puts(&b, ".prototype.");
  scr_jb_puts(&b, member);
  scr_jb_puts(&b, "' on a dynamic value is not supported yet");
  scr_throw_error(SCR_ERR_ERROR, scr_jb_finish(&b));
}

/* V8's "<value> is not a function" for a typed-array callback: the type
 * name, and the value for numbers, strings, booleans and null. */
static void dbm_throw_not_callable(const ScrDyn *v) {
  ScrJsonBuf b;
  scr_jb_init(&b);
  switch (v->kind) {
  case SCR_DYN_UNDEF: scr_jb_puts(&b, "undefined"); break;
  case SCR_DYN_NULL: scr_jb_puts(&b, "object null"); break;
  case SCR_DYN_BOOL: scr_jb_puts(&b, v->v.b ? "boolean true" : "boolean false"); break;
  case SCR_DYN_NUM: {
    ScrStr *n = scr_f64_to_scrstr(v->v.num);
    scr_jb_puts(&b, "number ");
    scr_jb_put_str(&b, n);
    scr_str_release(n);
    break;
  }
  case SCR_DYN_STR:
    scr_jb_puts(&b, "string \"");
    scr_jb_put_str(&b, v->v.str);
    scr_jb_putc(&b, '"');
    break;
  case SCR_DYN_BIGINT: scr_jb_puts(&b, "bigint"); break;
  case SCR_DYN_SYMBOL: scr_jb_puts(&b, "symbol"); break;
  default: scr_jb_puts(&b, "object"); break;
  }
  scr_jb_puts(&b, " is not a function");
  scr_throw_error(SCR_ERR_TYPE, scr_jb_finish(&b));
}

static bool dbm_callable(const ScrDyn *cb) {
  if (scr_dyn_is_callable(cb)) return true;
  dbm_throw_not_callable(cb);
  return false;
}

/* Node's validateNumber: ERR_INVALID_ARG_TYPE for non-numbers. */
static bool dbm_number_arg(const ScrDyn *v, const char *name) {
  if (v->kind == SCR_DYN_NUM) return true;
  scr_dyn_arg_type_fail(name, "of type number", v);
  return false;
}

/* Node's validateOffset(value, name, 0, max) after its type gate. */
static bool dbm_offset_arg(const ScrDyn *v, const char *name, double max, double *out) {
  if (!dbm_number_arg(v, name)) return false;
  *out = v->v.num;
  return scr_bytes_validate_off(name, *out, max);
}

/* isUint8Array: a Uint8Array or Buffer, not another view. */
static bool dbm_is_u8(const ScrDyn *v) {
  return v->kind == SCR_DYN_BYTES && v->v.bytes->elem == SCR_BYTES_U8 && !v->v.bytes->is_data_view;
}

static ScrDyn *dbm_bytes_result(ScrBytes *out, bool buffer) {
  if (!out) return NULL;
  ScrDyn *d = buffer ? scr_dyn_new_buffer(out) : scr_dyn_new_bytes(out);
  scr_bytes_release(out);
  return d;
}

/* A fresh typed array of the receiver's kind: a Buffer for a Buffer (Node's
 * species constructor), else the same element kind. */
static ScrBytes *dbm_species_new(const ScrDyn *recv, size_t len) {
  if (recv->buffer) return scr_buffer_alloc((double)len);
  return scr_bytes_new(recv->v.bytes->elem, (double)len);
}

/* A fresh same-element copy with no Buffer brand (TypedArrayCreateSameType:
 * toReversed, toSorted and with answer a Uint8Array for a Buffer). */
static ScrBytes *dbm_plain_copy(const ScrBytes *b) {
  ScrBytes *out = scr_bytes_new(b->elem, (double)b->len);
  if (!out) return NULL;
  size_t width = scr_bytes_elem_size(b->elem);
  scr_bytes_read(b, 0, out->data, b->len * width);
  return out;
}

/* ── encodings ────────────────────────────────────────────────────────── */

/* Node's encoding names (case-insensitive) to the canonical names the
 * typed helpers take, or NULL. */
static const char *dbm_canonical_encoding(const ScrStr *raw) {
  static const struct { const char *from; const char *to; } map[] = {
    {"utf8", "utf8"}, {"utf-8", "utf8"}, {"hex", "hex"}, {"base64", "base64"},
    {"base64url", "base64url"}, {"latin1", "latin1"}, {"binary", "latin1"},
    {"ascii", "ascii"}, {"utf16le", "utf16le"}, {"utf-16le", "utf16le"},
    {"ucs2", "utf16le"}, {"ucs-2", "utf16le"},
  };
  for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
    size_t n = strlen(map[i].from);
    if (raw->len != n) continue;
    size_t k = 0;
    for (; k < n; k++) {
      char c = raw->data[k];
      if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
      if (c != map[i].from[k]) break;
    }
    if (k == n) return map[i].to;
  }
  return NULL;
}

/* ERR_UNKNOWN_ENCODING for `enc` (formatted with %s). */
static void dbm_unknown_encoding(const ScrDyn *enc) {
  ScrStr *text;
  if (enc->kind == SCR_DYN_STR) text = scr_str_retain(enc->v.str);
  else if (enc->kind == SCR_DYN_NUM) text = scr_f64_to_scrstr(enc->v.num);
  else if (enc->kind == SCR_DYN_BOOL || enc->kind == SCR_DYN_NULL || enc->kind == SCR_DYN_UNDEF)
    text = scr_dyn_string_coerce(enc);
  else {
    static const char message[] = "Buffer methods with an encoding that is not a string, number, boolean, null or undefined are not supported yet";
    scr_throw_error_msg_code(SCR_ERR_ERROR, message, sizeof message - 1, "SC2020");
    return;
  }
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, "Unknown encoding: ");
  scr_jb_put_str(&b, text);
  scr_str_release(text);
  ScrStr *message = scr_jb_finish(&b);
  scr_throw_error_msg_code(SCR_ERR_TYPE, message->data, message->len, "ERR_UNKNOWN_ENCODING");
  scr_str_release(message);
}

/* getEncodingOps(encoding): `encoding + ''`, then the case-insensitive
 * name. Sets *canonical to the name or NULL; false when the string
 * conversion threw. */
static bool dbm_encoding_ops(const ScrDyn *enc, const char **canonical) {
  ScrStr *text = scr_dyn_string_coerce_js(enc);
  if (!text) return false;
  *canonical = dbm_canonical_encoding(text);
  scr_str_release(text);
  return true;
}

/* getEncodingOps with ERR_UNKNOWN_ENCODING for unknown names: +1 canonical
 * name, or NULL with the exception pending. */
static ScrStr *dbm_encoding_checked(const ScrDyn *enc) {
  const char *canonical;
  if (!dbm_encoding_ops(enc, &canonical)) return NULL;
  if (!canonical) {
    dbm_unknown_encoding(enc);
    return NULL;
  }
  return scr_str_new(canonical, strlen(canonical));
}

static ScrStr *dbm_str(const char *s) { return scr_str_new(s, strlen(s)); }

static ScrDyn *dbm_empty_string(void) {
  ScrStr *empty = scr_str_new("", 0);
  ScrDyn *out = scr_dyn_new_str(empty);
  scr_str_release(empty);
  return out;
}

/* ── Buffer.prototype ─────────────────────────────────────────────────── */

/* Buffer.prototype.toString(encoding, start, end): the range clamps as
 * Node's does (start <= 0 is 0, start past the end answers '', end past
 * the length is the length), then the encoding resolves. */
static ScrDyn *dbm_buffer_to_string(ScrBytes *bytes, ScrDyn *const *args, size_t argc) {
  double len = (double)bytes->len;
  double start = 0, end = len;
  if (argc > 1 && args[1]->kind != SCR_DYN_UNDEF) {
    double v;
    if (!scr_dyn_number_coerce_js(args[1], &v)) return NULL;
    if (v <= 0) start = 0;
    else if (v >= len) return dbm_empty_string();
    else start = isnan(v) ? 0 : trunc(v);
  }
  if (argc > 2 && args[2]->kind != SCR_DYN_UNDEF) {
    double v;
    if (!scr_dyn_number_coerce_js(args[2], &v)) return NULL;
    end = v > len ? len : (isnan(v) ? 0 : trunc(v));
  }
  if (end <= start) return dbm_empty_string();
  ScrStr *enc = argc == 0 || args[0]->kind == SCR_DYN_UNDEF ? dbm_str("utf8") : dbm_encoding_checked(args[0]);
  if (!enc) return NULL;
  ScrStr *text = scr_bytes_to_str_range(bytes, enc, start, end);
  scr_str_release(enc);
  ScrDyn *out = scr_dyn_new_str(text);
  scr_str_release(text);
  return out;
}

/* Buffer.prototype.write(string, offset, length, encoding) with Node's
 * argument shuffling; the binding then requires a string. */
static ScrDyn *dbm_buffer_write(ScrBytes *bytes, ScrDyn *const *args, size_t argc) {
  ScrDyn *string = dbm_arg(args, argc, 0);
  ScrDyn *offset_arg = dbm_arg(args, argc, 1);
  ScrDyn *length_arg = dbm_arg(args, argc, 2);
  ScrDyn *encoding = dbm_arg(args, argc, 3);
  double len = (double)bytes->len;
  double offset = 0, length = len;
  /* write(string) with no offset is utf8Write over the whole buffer. */
  if (offset_arg->kind == SCR_DYN_UNDEF) encoding = scr_dyn_undefined();
  else {
    if (length_arg->kind == SCR_DYN_UNDEF && offset_arg->kind == SCR_DYN_STR) {
      encoding = offset_arg;
    } else {
      if (!dbm_offset_arg(offset_arg, "offset", len, &offset)) return NULL;
      double remaining = len - offset;
      if (length_arg->kind == SCR_DYN_UNDEF) length = remaining;
      else if (length_arg->kind == SCR_DYN_STR) {
        encoding = length_arg;
        length = remaining;
      } else {
        if (!dbm_offset_arg(length_arg, "length", len, &length)) return NULL;
        if (length > remaining) length = remaining;
      }
    }
  }
  ScrStr *enc;
  if (!scr_dyn_truthy(encoding)) enc = dbm_str("utf8");
  else enc = dbm_encoding_checked(encoding);
  if (!enc) return NULL;
  if (string->kind != SCR_DYN_STR) {
    scr_str_release(enc);
    static const char message[] = "argument must be a string";
    scr_throw_error_msg_code(SCR_ERR_TYPE, message, sizeof message - 1, "ERR_INVALID_ARG_TYPE");
    return NULL;
  }
  double written = scr_bytes_write_str(bytes, string->v.str, enc, offset, length, true);
  scr_str_release(enc);
  return scr_exc_pending() ? NULL : scr_dyn_new_num(written);
}

/* The canonical encoding normalizeEncoding answers for fill: undefined,
 * null and '' are utf8; other non-strings answer nothing. */
static const char *dbm_normalize_encoding(const ScrDyn *enc) {
  if (enc->kind == SCR_DYN_UNDEF || enc->kind == SCR_DYN_NULL) return "utf8";
  if (enc->kind != SCR_DYN_STR) return NULL;
  if (enc->v.str->len == 0) return "utf8";
  return dbm_canonical_encoding(enc->v.str);
}

/* Buffer.prototype.fill(value, offset, end, encoding). */
static ScrDyn *dbm_buffer_fill(ScrDyn *recv, ScrDyn *const *args, size_t argc) {
  ScrBytes *bytes = recv->v.bytes;
  ScrDyn *value = dbm_arg(args, argc, 0);
  ScrDyn *offset_arg = dbm_arg(args, argc, 1);
  ScrDyn *end_arg = dbm_arg(args, argc, 2);
  ScrDyn *encoding = dbm_arg(args, argc, 3);
  double len = (double)bytes->len;
  const char *canonical = NULL;
  bool whole = offset_arg->kind == SCR_DYN_UNDEF;
  if (value->kind == SCR_DYN_STR) {
    if (offset_arg->kind == SCR_DYN_UNDEF || offset_arg->kind == SCR_DYN_STR) {
      encoding = offset_arg;
      whole = true;
    } else if (end_arg->kind == SCR_DYN_STR) {
      encoding = end_arg;
      end_arg = scr_dyn_undefined();
    }
    canonical = dbm_normalize_encoding(encoding);
    if (!canonical) {
      if (encoding->kind != SCR_DYN_STR) scr_dyn_arg_type_fail("encoding", "of type string", encoding);
      else dbm_unknown_encoding(encoding);
      return NULL;
    }
  }
  double offset = 0, end = len;
  if (!whole) {
    if (!dbm_offset_arg(offset_arg, "offset", 9007199254740991.0, &offset)) return NULL;
    if (end_arg->kind != SCR_DYN_UNDEF && !dbm_offset_arg(end_arg, "end", len, &end)) return NULL;
    if (offset >= end) return scr_dyn_retain(recv);
  }
  ScrBytes *filled;
  if (value->kind == SCR_DYN_STR && value->v.str->len > 0) {
    ScrStr *enc = dbm_str(canonical);
    ScrBytes *pattern = scr_bytes_from_str(value->v.str, enc);
    scr_str_release(enc);
    if (pattern->len == 0) {
      /* The binding wrote no bytes: the fill value is invalid. */
      scr_bytes_release(pattern);
      scr_dyn_arg_value_fail("value", NULL, value);
      return NULL;
    }
    filled = scr_bytes_fill(bytes, pattern, 2, offset, end);
    scr_bytes_release(pattern);
  } else if (value->kind == SCR_DYN_STR) {
    /* '' fills zeros. */
    filled = scr_bytes_fill_num(bytes, 0, 2, offset, end);
  } else if (value->kind == SCR_DYN_BYTES) {
    /* Any view fills with its bytes; an empty one is invalid. */
    ScrBytes *pattern = scr_bytes_raw_view(value->v.bytes);
    if (pattern->len == 0 && !value->buffer) {
      /* util.inspect of an empty typed array: "Uint8Array(0) []". */
      scr_bytes_release(pattern);
      if (value->v.bytes->is_data_view) {
        scr_dyn_arg_value_fail("value", NULL, value);
        return NULL;
      }
      char message[96];
      int n = snprintf(message, sizeof message, "The argument 'value' is invalid. Received %s(0) []",
                       scr_bytes_elem_name(value->v.bytes->elem));
      scr_throw_error_msg_code(SCR_ERR_TYPE, message, (size_t)n, "ERR_INVALID_ARG_VALUE");
      return NULL;
    }
    filled = scr_bytes_fill(bytes, pattern, 2, offset, end);
    scr_bytes_release(pattern);
  } else {
    /* Numbers fill through TypedArray.prototype.fill and other values
     * through the binding's ToUint32; both store the low byte. */
    double number;
    if (!scr_dyn_number_coerce_js(value, &number)) return NULL;
    filled = scr_bytes_fill_num(bytes, number, 2, offset, end);
  }
  if (!filled) return NULL;
  scr_bytes_release(filled);
  return scr_dyn_retain(recv);
}

/* src/node_buffer.cc's IndexOfOffset. */
static int64_t dbm_index_of_offset(int64_t length, int64_t offset, int64_t needle_length, bool forward) {
  if (offset < 0) {
    if (offset + length >= 0) return length + offset;
    if (forward || needle_length == 0) return 0;
    return -1;
  }
  if (offset + needle_length <= length) return offset;
  if (needle_length == 0) return length;
  if (forward) return -1;
  return length - 1;
}

/* nbytes::SearchString over `unit`-byte code units: the first match at or
 * after `start` (forward) or the last at or before it; `length` when none. */
static size_t dbm_search(const uint8_t *hay, size_t length, const uint8_t *needle, size_t needle_length,
                         size_t start, bool forward, size_t unit) {
  if (length < needle_length) return length;
  size_t diff = length - needle_length;
  size_t width = needle_length * unit;
  if (forward) {
    for (size_t i = start; i <= diff; i++) {
      if (memcmp(hay + i * unit, needle, width) == 0) return i;
    }
    return length;
  }
  size_t i = start < diff ? start : diff;
  for (;;) {
    if (memcmp(hay + i * unit, needle, width) == 0) return i;
    if (i == 0) return length;
    i--;
  }
}

/* V8's Integer::Value on the binding's `end` number: the truncation, or
 * INT64_MIN when it does not fit (NaN and the infinities included). */
static int64_t dbm_int64(double v) {
  if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) return INT64_MIN;
  return (int64_t)v;
}

/* IndexOfString / IndexOfBuffer with the byte search or, for utf16le, the
 * code-unit search; `string` selects the string binding's haystack
 * rounding. */
static double dbm_index_of_bytes(const ScrBytes *b, const ScrBytes *needle, int64_t offset_i64,
                                 int64_t end_i64, bool ucs2, bool string, bool forward) {
  size_t haystack_length = string && ucs2 ? b->len & ~(size_t)1 : b->len;
  size_t needle_length = needle->len;
  int64_t end_clamped = end_i64 < 0 ? 0 : end_i64;
  size_t search_end = (uint64_t)end_clamped < haystack_length ? (size_t)end_clamped : haystack_length;
  if (ucs2) search_end &= ~(size_t)1;
  int64_t opt = dbm_index_of_offset((int64_t)haystack_length, offset_i64, (int64_t)needle_length, forward);
  if (needle_length == 0) return (double)(opt < (int64_t)search_end ? opt : (int64_t)search_end);
  if (haystack_length == 0 || opt <= -1) return -1;
  size_t offset = (size_t)opt;
  if (!forward && offset >= search_end) {
    if (search_end == 0) return -1;
    offset = search_end - 1;
  } else if (forward && offset >= search_end) {
    return -1;
  }
  if ((forward && needle_length + offset > search_end) || needle_length > search_end) return -1;
  size_t result;
  if (ucs2) {
    if (search_end < 2 || needle_length < 2) return -1;
    result = dbm_search(b->data, search_end / 2, needle->data, needle_length / 2, offset / 2, forward, 2) * 2;
  } else {
    result = dbm_search(b->data, search_end, needle->data, needle_length, offset, forward, 1);
  }
  return result >= search_end ? -1 : (double)result;
}

/* IndexOfNumberImpl: the low byte, memchr forward or memrchr backward. */
static double dbm_index_of_byte(const ScrBytes *b, uint8_t needle, int64_t offset_i64, int64_t end_i64,
                                bool forward) {
  size_t len = b->len;
  int64_t opt = dbm_index_of_offset((int64_t)len, offset_i64, 1, forward);
  if (opt <= -1 || len == 0) return -1;
  size_t offset = (size_t)opt;
  int64_t end_clamped = end_i64 < 0 ? 0 : end_i64;
  size_t search_end = (uint64_t)end_clamped < len ? (size_t)end_clamped : len;
  if (forward) {
    for (size_t i = offset; i < search_end; i++) {
      if (b->data[i] == needle) return (double)i;
    }
    return -1;
  }
  size_t backward_end = offset + 1 < search_end ? offset + 1 : search_end;
  for (size_t i = backward_end; i > 0; i--) {
    if (b->data[i - 1] == needle) return (double)(i - 1);
  }
  return -1;
}

static uint32_t dbm_to_uint32(double v) {
  if (!isfinite(v)) return 0;
  double t = fmod(trunc(v), 4294967296.0);
  if (t < 0) t += 4294967296.0;
  return (uint32_t)t;
}

/* Buffer.prototype.indexOf / lastIndexOf / includes(val, offset, end,
 * encoding): bidirectionalIndexOf's shuffling and coercions over the
 * binding searches. NaN with the exception pending. */
static double dbm_buffer_index_of(ScrBytes *bytes, ScrDyn *const *args, size_t argc, bool forward) {
  SCR_BYTES_SNAPSHOT(bytes);
  ScrDyn *val = dbm_arg(args, argc, 0);
  ScrDyn *offset_arg = dbm_arg(args, argc, 1);
  ScrDyn *end_arg = dbm_arg(args, argc, 2);
  ScrDyn *encoding = dbm_arg(args, argc, 3);
  double len = (double)bytes->len;
  double end = len;
  if (end_arg->kind == SCR_DYN_STR) encoding = end_arg;
  else if (end_arg->kind == SCR_DYN_NUM) end = end_arg->v.num;
  else if (end_arg->kind != SCR_DYN_UNDEF) {
    static const char message[] = "Buffer indexOf with an end that is not a number or string is not supported yet";
    scr_throw_error_msg_code(SCR_ERR_ERROR, message, sizeof message - 1, "SC2020");
    return NAN;
  }
  double offset = NAN;
  if (offset_arg->kind == SCR_DYN_STR) {
    encoding = offset_arg;
  } else if (offset_arg->kind != SCR_DYN_UNDEF) {
    if (!scr_dyn_number_coerce_js(offset_arg, &offset)) return NAN;
    if (offset > 2147483647.0) offset = 2147483647.0;
    else if (offset < -2147483648.0) offset = -2147483648.0;
  }
  if (isnan(offset)) offset = forward ? 0 : len;
  int64_t offset_i64 = (int64_t)offset;
  int64_t end_i64 = dbm_int64(end);
  if (val->kind == SCR_DYN_NUM)
    return dbm_index_of_byte(bytes, (uint8_t)dbm_to_uint32(val->v.num), offset_i64, end_i64, forward);
  const char *canonical = "utf8";
  if (encoding->kind != SCR_DYN_UNDEF && !dbm_encoding_ops(encoding, &canonical)) return NAN;
  if (val->kind == SCR_DYN_STR) {
    if (!canonical) {
      dbm_unknown_encoding(encoding);
      return NAN;
    }
    ScrStr *enc = dbm_str(canonical);
    ScrBytes *needle = scr_bytes_from_str(val->v.str, enc);
    scr_str_release(enc);
    bool ucs2 = strcmp(canonical, "utf16le") == 0;
    /* The string binding serves utf8, utf16le and latin1; the others
     * search their decoded bytes through the buffer binding. */
    bool string = ucs2 || strcmp(canonical, "utf8") == 0 || strcmp(canonical, "latin1") == 0;
    double result = dbm_index_of_bytes(bytes, needle, offset_i64, end_i64, ucs2, string, forward);
    scr_bytes_release(needle);
    return result;
  }
  if (dbm_is_u8(val)) {
    ScrBytes *needle = val->v.bytes;
    SCR_BYTES_SNAPSHOT(needle);
    bool ucs2 = canonical && strcmp(canonical, "utf16le") == 0;
    return dbm_index_of_bytes(bytes, needle, offset_i64, end_i64, ucs2, false, forward);
  }
  scr_dyn_arg_type_fail("value", "one of type number or string or an instance of Buffer or Uint8Array", val);
  return NAN;
}

/* copyImpl's index coercion: integers stand, other values run toInteger
 * (NaN and values past the safe range answer 0, the rest floor). */
static bool dbm_copy_index(const ScrDyn *v, double *out) {
  if (v->kind == SCR_DYN_NUM && isfinite(v->v.num) && trunc(v->v.num) == v->v.num) {
    *out = v->v.num;
    return true;
  }
  double n;
  if (!scr_dyn_number_coerce_js(v, &n)) return false;
  *out = !isnan(n) && n >= -9007199254740991.0 && n <= 9007199254740991.0 ? floor(n) : 0;
  return true;
}

static void dbm_copy_range_error(const char *name, double value, double max) {
  char received[48];
  scr_num_received(value, received);
  char message[192];
  int n;
  if (max < 0)
    n = snprintf(message, sizeof message, "The value of \"%s\" is out of range. It must be >= 0. Received %s", name, received);
  else {
    char maxb[32];
    scr_f64_to_str(max, maxb);
    n = snprintf(message, sizeof message, "The value of \"%s\" is out of range. It must be >= 0 && <= %s. Received %s",
                 name, maxb, received);
  }
  scr_throw_error_msg_code(SCR_ERR_RANGE, message, (size_t)n, "ERR_OUT_OF_RANGE");
}

/* Buffer.prototype.copy(target, targetStart, sourceStart, sourceEnd). */
static ScrDyn *dbm_buffer_copy(ScrBytes *bytes, ScrDyn *const *args, size_t argc) {
  ScrDyn *target = dbm_arg(args, argc, 0);
  if (target->kind != SCR_DYN_BYTES) {
    scr_dyn_arg_type_fail("target", "an instance of Buffer or Uint8Array", target);
    return NULL;
  }
  double source_len = (double)bytes->len;
  double ts = 0, ss = 0, se = source_len;
  ScrDyn *ts_arg = dbm_arg(args, argc, 1), *ss_arg = dbm_arg(args, argc, 2), *se_arg = dbm_arg(args, argc, 3);
  if (ts_arg->kind != SCR_DYN_UNDEF) {
    if (!dbm_copy_index(ts_arg, &ts)) return NULL;
    if (ts < 0) { dbm_copy_range_error("targetStart", ts, -1); return NULL; }
  }
  if (ss_arg->kind != SCR_DYN_UNDEF) {
    if (!dbm_copy_index(ss_arg, &ss)) return NULL;
    if (ss < 0 || ss > source_len) { dbm_copy_range_error("sourceStart", ss, source_len); return NULL; }
  }
  if (se_arg->kind != SCR_DYN_UNDEF) {
    if (!dbm_copy_index(se_arg, &se)) return NULL;
    if (se < 0) { dbm_copy_range_error("sourceEnd", se, -1); return NULL; }
  }
  /* The copy counts bytes: a wider target copies into its raw bytes. */
  ScrBytes *dst = scr_bytes_raw_view(target->v.bytes);
  double copied = ts >= (double)dst->len || ss >= se ? 0 : scr_bytes_copy_into(bytes, dst, 3, ts, ss, se);
  scr_bytes_release(dst);
  return scr_exc_pending() ? NULL : scr_dyn_new_num(copied);
}

/* Buffer.prototype.toJSON: { type: 'Buffer', data: [...] }. */
static ScrDyn *dbm_buffer_to_json(const ScrBytes *bytes) {
  ScrDyn *data = scr_dyn_new_arr();
  for (size_t i = 0; i < bytes->len; i++) scr_dyn_arr_push(data, scr_dyn_new_num(scr_bytes_get(bytes, (double)i)));
  ScrDyn *out = scr_dyn_new_obj();
  ScrStr *type = scr_str_new("Buffer", 6);
  scr_dyn_obj_set(out, "type", 4, scr_dyn_new_str(type));
  scr_str_release(type);
  scr_dyn_obj_set(out, "data", 4, data);
  return out;
}

/* Buffer.prototype.inspect: "<Buffer 01 02>", at most 50 bytes. */
static ScrDyn *dbm_buffer_inspect(const ScrBytes *bytes) {
  static const char digits[] = "0123456789abcdef";
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, "<Buffer ");
  size_t shown = bytes->len < 50 ? bytes->len : 50;
  for (size_t i = 0; i < shown; i++) {
    uint8_t byte = (uint8_t)scr_bytes_get(bytes, (double)i);
    if (i) scr_jb_putc(&b, ' ');
    scr_jb_putc(&b, digits[byte >> 4]);
    scr_jb_putc(&b, digits[byte & 15]);
  }
  if (bytes->len > 50) {
    char more[64];
    size_t remaining = bytes->len - 50;
    snprintf(more, sizeof more, " ... %zu more byte%s", remaining, remaining > 1 ? "s" : "");
    scr_jb_puts(&b, more);
  }
  scr_jb_putc(&b, '>');
  ScrStr *text = scr_jb_finish(&b);
  ScrDyn *out = scr_dyn_new_str(text);
  scr_str_release(text);
  return out;
}

/* ── the numeric read/write families ──────────────────────────────────── */

static const char dbm_mix_message[] = "Cannot mix BigInt and other types, use explicit conversions";

/* A scratch buffer the integer value checks run against when the offset
 * argument is not a number: Node checks the value range before the
 * offset's type for every width but one byte. */
static bool dbm_value_check(double value, size_t width, ScrBytesNumKind kind, bool var, bool sign) {
  uint8_t scratch[8] = {0};
  ScrBytes probe = { .rc = 1, .len = 8, .elem = SCR_BYTES_U8, .data = scratch, .backing = NULL };
  if (var) scr_bytes_write_var(&probe, value, 0, (double)width, sign, true);
  else scr_bytes_write_num(&probe, value, 0, kind, true);
  return !scr_exc_pending();
}

static size_t dbm_num_width(ScrBytesNumKind kind) {
  switch (kind) {
  case SCR_BN_U8: case SCR_BN_I8: return 1;
  case SCR_BN_U16: case SCR_BN_I16: return 2;
  case SCR_BN_F64: return 8;
  default: return 4;
  }
}

static ScrDyn *dbm_numeric(const DbmInfo *info, ScrBytes *bytes, ScrDyn *const *args, size_t argc) {
  switch ((DbmNumOp)info->num_op) {
  case DBM_NUM_READ: {
    ScrDyn *offset_arg = dbm_arg(args, argc, 0);
    double offset = 0;
    if (offset_arg->kind != SCR_DYN_UNDEF) {
      if (!dbm_number_arg(offset_arg, "offset")) return NULL;
      offset = offset_arg->v.num;
    }
    double value = scr_bytes_read_num(bytes, offset, (ScrBytesNumKind)info->num_kind, info->le);
    return scr_exc_pending() ? NULL : scr_dyn_new_num(value);
  }
  case DBM_NUM_WRITE: {
    ScrBytesNumKind kind = (ScrBytesNumKind)info->num_kind;
    double value;
    if (!scr_dyn_number_coerce_js(dbm_arg(args, argc, 0), &value)) return NULL;
    ScrDyn *offset_arg = dbm_arg(args, argc, 1);
    double offset = 0;
    if (offset_arg->kind != SCR_DYN_UNDEF) {
      if (offset_arg->kind != SCR_DYN_NUM) {
        bool integer = kind != SCR_BN_F32 && kind != SCR_BN_F64;
        if (integer && dbm_num_width(kind) > 1 && !dbm_value_check(value, 0, kind, false, false)) return NULL;
        dbm_number_arg(offset_arg, "offset");
        return NULL;
      }
      offset = offset_arg->v.num;
    }
    double next = scr_bytes_write_num(bytes, value, offset, kind, info->le);
    return scr_exc_pending() ? NULL : scr_dyn_new_num(next);
  }
  case DBM_VAR_READ: {
    ScrDyn *offset_arg = dbm_arg(args, argc, 0);
    ScrDyn *length_arg = dbm_arg(args, argc, 1);
    if (offset_arg->kind == SCR_DYN_UNDEF) {
      scr_dyn_arg_type_fail("offset", "of type number", offset_arg);
      return NULL;
    }
    if (!dbm_number_arg(length_arg, "byteLength")) return NULL;
    double byte_length = length_arg->v.num;
    if (offset_arg->kind != SCR_DYN_NUM) {
      /* An invalid byteLength reports first. */
      if (floor(byte_length) == byte_length && byte_length >= 1 && byte_length <= 6) dbm_number_arg(offset_arg, "offset");
      else scr_bytes_read_var(bytes, 0, byte_length, false, true);
      return NULL;
    }
    double value = scr_bytes_read_var(bytes, offset_arg->v.num, byte_length, info->num_kind != 0, info->le);
    return scr_exc_pending() ? NULL : scr_dyn_new_num(value);
  }
  case DBM_VAR_WRITE: {
    ScrDyn *length_arg = dbm_arg(args, argc, 2);
    if (!dbm_number_arg(length_arg, "byteLength")) return NULL;
    double byte_length = length_arg->v.num;
    bool sign = info->num_kind != 0;
    if (!(floor(byte_length) == byte_length && byte_length >= 1 && byte_length <= 6)) {
      scr_bytes_write_var(bytes, 0, 0, byte_length, sign, info->le);
      return NULL;
    }
    double value;
    if (!scr_dyn_number_coerce_js(dbm_arg(args, argc, 0), &value)) return NULL;
    ScrDyn *offset_arg = dbm_arg(args, argc, 1);
    if (offset_arg->kind != SCR_DYN_NUM) {
      if (byte_length > 1 && !dbm_value_check(value, (size_t)byte_length, SCR_BN_U8, true, sign)) return NULL;
      dbm_number_arg(offset_arg, "offset");
      return NULL;
    }
    double next = scr_bytes_write_var(bytes, value, offset_arg->v.num, byte_length, sign, info->le);
    return scr_exc_pending() ? NULL : scr_dyn_new_num(next);
  }
  case DBM_BIG_READ: {
    ScrDyn *offset_arg = dbm_arg(args, argc, 0);
    double offset = 0;
    if (offset_arg->kind != SCR_DYN_UNDEF) {
      if (!dbm_number_arg(offset_arg, "offset")) return NULL;
      offset = offset_arg->v.num;
    }
    ScrBigInt *value = scr_bigint_buffer_read(bytes, offset, info->num_kind != 0, info->le);
    if (!value) return NULL;
    ScrDyn *out = scr_dyn_new_bigint(value);
    scr_bigint_release(value);
    return out;
  }
  case DBM_BIG_WRITE: {
    bool sign = info->num_kind != 0;
    ScrDyn *value = dbm_arg(args, argc, 0);
    ScrDyn *offset_arg = dbm_arg(args, argc, 1);
    double offset = 0;
    bool offset_number = offset_arg->kind == SCR_DYN_NUM || offset_arg->kind == SCR_DYN_UNDEF;
    if (offset_arg->kind == SCR_DYN_NUM) offset = offset_arg->v.num;
    if (value->kind == SCR_DYN_BIGINT) {
      if (!offset_number) {
        /* The range check precedes the offset's type check. */
        uint8_t scratch[8] = {0};
        ScrBytes probe = { .rc = 1, .len = 8, .elem = SCR_BYTES_U8, .data = scratch, .backing = NULL };
        scr_bigint_buffer_write(&probe, value->v.bigint, 0, sign, info->le);
        if (scr_exc_pending()) return NULL;
        dbm_number_arg(offset_arg, "offset");
        return NULL;
      }
      double next = scr_bigint_buffer_write(bytes, value->v.bigint, offset, sign, info->le);
      return scr_exc_pending() ? NULL : scr_dyn_new_num(next);
    }
    /* checkInt compares a Number against the BigInt bounds, checks the
     * offset, and then the BigInt arithmetic throws. */
    if (value->kind == SCR_DYN_NUM) {
      double v = value->v.num;
      bool out_of_range = sign ? (v < -9223372036854775808.0 || v >= 9223372036854775808.0)
                               : (v < 0 || v >= 18446744073709551616.0);
      if (out_of_range) {
        char received[48];
        scr_num_received(v, received);
        char message[192];
        int n = snprintf(message, sizeof message, "The value of \"value\" is out of range. It must be %s. Received %s",
                         sign ? ">= -(2n ** 63n) and < 2n ** 63n" : ">= 0n and < 2n ** 64n", received);
        scr_throw_error_msg_code(SCR_ERR_RANGE, message, (size_t)n, "ERR_OUT_OF_RANGE");
        return NULL;
      }
    }
    if (!offset_number) {
      dbm_number_arg(offset_arg, "offset");
      return NULL;
    }
    scr_bytes_read_num(bytes, offset, SCR_BN_F64, true);
    if (scr_exc_pending()) return NULL;
    scr_throw_error_msg(SCR_ERR_TYPE, dbm_mix_message, sizeof dbm_mix_message - 1);
    return NULL;
  }
  case DBM_NUM_NONE:
    break;
  }
  return NULL;
}

/* ── %TypedArray%.prototype ───────────────────────────────────────────── */

static ScrDyn *dbm_elem(const ScrBytes *bytes, size_t i) { return scr_dyn_new_num(scr_bytes_get(bytes, (double)i)); }

/* callback(value, index, array) with `this_arg`; +1 or NULL. */
static ScrDyn *dbm_call3(ScrDyn *cb, const ScrDyn *this_arg, ScrDyn *value, size_t i, ScrDyn *recv) {
  ScrDyn *index = scr_dyn_new_num((double)i);
  ScrDyn *argv[3] = { value, index, recv };
  scr_dyn_this_push_dyn(this_arg);
  ScrDyn *result = scr_dyn_call(cb, argv, 3, "callback");
  scr_dyn_this_pop();
  scr_dyn_release(index);
  return result;
}

typedef enum { DBM_SCAN_EVERY, DBM_SCAN_SOME, DBM_SCAN_FIND, DBM_SCAN_FIND_INDEX, DBM_SCAN_FOR_EACH } DbmScan;

/* every / some / find / findIndex / findLast / findLastIndex / forEach. */
static ScrDyn *dbm_scan(ScrDyn *recv, ScrDyn *const *args, size_t argc, DbmScan scan, bool reverse) {
  ScrBytes *bytes = recv->v.bytes;
  ScrDyn *cb = dbm_arg(args, argc, 0);
  if (!dbm_callable(cb)) return NULL;
  const ScrDyn *this_arg = argc > 1 ? args[1] : NULL;
  size_t len = bytes->len;
  for (size_t step = 0; step < len; step++) {
    size_t i = reverse ? len - 1 - step : step;
    ScrDyn *value = dbm_elem(bytes, i);
    ScrDyn *r = dbm_call3(cb, this_arg, value, i, recv);
    if (!r) {
      scr_dyn_release(value);
      return NULL;
    }
    bool truthy = scr_dyn_truthy(r);
    scr_dyn_release(r);
    if (scan == DBM_SCAN_EVERY && !truthy) {
      scr_dyn_release(value);
      return scr_dyn_new_bool(false);
    }
    if (scan == DBM_SCAN_SOME && truthy) {
      scr_dyn_release(value);
      return scr_dyn_new_bool(true);
    }
    if (scan == DBM_SCAN_FIND && truthy) return value;
    scr_dyn_release(value);
    if (scan == DBM_SCAN_FIND_INDEX && truthy) return scr_dyn_new_num((double)i);
  }
  switch (scan) {
  case DBM_SCAN_EVERY: return scr_dyn_new_bool(true);
  case DBM_SCAN_SOME: return scr_dyn_new_bool(false);
  case DBM_SCAN_FIND_INDEX: return scr_dyn_new_num(-1);
  default: return scr_dyn_retain(scr_dyn_undefined());
  }
}

static ScrDyn *dbm_map(ScrDyn *recv, ScrDyn *const *args, size_t argc) {
  ScrBytes *bytes = recv->v.bytes;
  ScrDyn *cb = dbm_arg(args, argc, 0);
  if (!dbm_callable(cb)) return NULL;
  const ScrDyn *this_arg = argc > 1 ? args[1] : NULL;
  ScrBytes *out = dbm_species_new(recv, bytes->len);
  if (!out) return NULL;
  for (size_t i = 0; i < bytes->len; i++) {
    ScrDyn *value = dbm_elem(bytes, i);
    ScrDyn *mapped = dbm_call3(cb, this_arg, value, i, recv);
    scr_dyn_release(value);
    double number;
    bool ok = mapped && scr_dyn_number_coerce_js(mapped, &number);
    scr_dyn_release(mapped);
    if (!ok) {
      scr_bytes_release(out);
      return NULL;
    }
    scr_bytes_set(out, (double)i, number);
  }
  return dbm_bytes_result(out, recv->buffer);
}

static ScrDyn *dbm_filter(ScrDyn *recv, ScrDyn *const *args, size_t argc) {
  ScrBytes *bytes = recv->v.bytes;
  ScrDyn *cb = dbm_arg(args, argc, 0);
  if (!dbm_callable(cb)) return NULL;
  const ScrDyn *this_arg = argc > 1 ? args[1] : NULL;
  double *kept = malloc((bytes->len ? bytes->len : 1) * sizeof *kept);
  if (!kept) scr_trap("scriptc: out of memory\n");
  size_t count = 0;
  for (size_t i = 0; i < bytes->len; i++) {
    double element = scr_bytes_get(bytes, (double)i);
    ScrDyn *value = scr_dyn_new_num(element);
    ScrDyn *r = dbm_call3(cb, this_arg, value, i, recv);
    scr_dyn_release(value);
    if (!r) {
      free(kept);
      return NULL;
    }
    if (scr_dyn_truthy(r)) kept[count++] = element;
    scr_dyn_release(r);
  }
  ScrBytes *out = dbm_species_new(recv, count);
  if (out) {
    for (size_t i = 0; i < count; i++) scr_bytes_set(out, (double)i, kept[i]);
  }
  free(kept);
  return dbm_bytes_result(out, recv->buffer);
}

static ScrDyn *dbm_reduce(ScrDyn *recv, ScrDyn *const *args, size_t argc, bool right) {
  ScrBytes *bytes = recv->v.bytes;
  ScrDyn *cb = dbm_arg(args, argc, 0);
  if (!dbm_callable(cb)) return NULL;
  size_t len = bytes->len, step = 0;
  ScrDyn *acc;
  if (argc > 1) acc = scr_dyn_retain(args[1]);
  else {
    if (len == 0) {
      static const char message[] = "Reduce of empty array with no initial value";
      scr_throw_error_msg(SCR_ERR_TYPE, message, sizeof message - 1);
      return NULL;
    }
    acc = dbm_elem(bytes, right ? len - 1 : 0);
    step = 1;
  }
  for (; step < len; step++) {
    size_t i = right ? len - 1 - step : step;
    ScrDyn *value = dbm_elem(bytes, i);
    ScrDyn *index = scr_dyn_new_num((double)i);
    ScrDyn *argv[4] = { acc, value, index, recv };
    scr_dyn_this_push_dyn(NULL);
    ScrDyn *next = scr_dyn_call(cb, argv, 4, "callback");
    scr_dyn_this_pop();
    scr_dyn_release(value);
    scr_dyn_release(index);
    scr_dyn_release(acc);
    if (!next) return NULL;
    acc = next;
  }
  return acc;
}

/* includes / indexOf / lastIndexOf on a typed array: SameValueZero for
 * includes, strict equality for the index searches. */
static ScrDyn *dbm_typed_search(ScrBytes *bytes, ScrDyn *const *args, size_t argc, DbmId id) {
  size_t len = bytes->len;
  bool includes = id == DBM_INCLUDES;
  if (len == 0) return includes ? scr_dyn_new_bool(false) : scr_dyn_new_num(-1);
  ScrDyn *target = dbm_arg(args, argc, 0);
  double k;
  if (id == DBM_LAST_INDEX_OF) {
    double n = (double)len - 1;
    if (argc > 1) {
      double v;
      if (!scr_dyn_number_coerce_js(args[1], &v)) return NULL;
      n = isnan(v) ? 0 : trunc(v);
    }
    if (n == -INFINITY) return scr_dyn_new_num(-1);
    k = n >= 0 ? (n < (double)len - 1 ? n : (double)len - 1) : (double)len + n;
    if (target->kind == SCR_DYN_NUM) {
      for (; k >= 0; k--) {
        if (scr_bytes_get(bytes, k) == target->v.num) return scr_dyn_new_num(k);
      }
    }
    return scr_dyn_new_num(-1);
  }
  if (!dbm_index_arg(args, argc, 1, 0, &k)) return NULL;
  if (k == INFINITY) return includes ? scr_dyn_new_bool(false) : scr_dyn_new_num(-1);
  if (k < 0) {
    k += (double)len;
    if (k < 0) k = 0;
  }
  if (target->kind == SCR_DYN_NUM) {
    double needle = target->v.num;
    for (; k < (double)len; k++) {
      double element = scr_bytes_get(bytes, k);
      if (element == needle || (includes && isnan(element) && isnan(needle)))
        return includes ? scr_dyn_new_bool(true) : scr_dyn_new_num(k);
    }
  }
  return includes ? scr_dyn_new_bool(false) : scr_dyn_new_num(-1);
}

/* Reverses the elements of `b` in place, element by element. */
static void dbm_reverse_in_place(ScrBytes *b) {
  size_t width = scr_bytes_elem_size(b->elem);
  if (b->shared) {
    for (size_t i = 0, j = b->len; i + 1 < j; i++, j--) {
      double x = scr_bytes_get(b, (double)i);
      scr_bytes_set(b, (double)i, scr_bytes_get(b, (double)(j - 1)));
      scr_bytes_set(b, (double)(j - 1), x);
    }
    return;
  }
  uint8_t tmp[8];
  for (size_t i = 0, j = b->len; i + 1 < j; i++, j--) {
    memcpy(tmp, b->data + i * width, width);
    memcpy(b->data + i * width, b->data + (j - 1) * width, width);
    memcpy(b->data + (j - 1) * width, tmp, width);
  }
}

/* The default TypedArray order: ascending, -0 before +0, NaN last. */
static bool dbm_default_greater(double x, double y) {
  if (isnan(x)) return !isnan(y);
  if (isnan(y)) return false;
  if (x == 0 && y == 0) return !signbit(x) && signbit(y);
  return x > y;
}

/* V8's typed-array comparator merge sort (the schedule scr_array.c's
 * scr_bytes_sort_compare follows) with a checked-dynamic comparator, or
 * the default order when `cmp` is NULL. */
static bool dbm_merge_sort(double *source, size_t from, size_t to, double *target, ScrDyn *cmp) {
  size_t middle = from + ((to - from) >> 1);
  if (middle - from > 1 && !dbm_merge_sort(target, from, middle, source, cmp)) return false;
  if (to - middle > 1 && !dbm_merge_sort(target, middle, to, source, cmp)) return false;
  size_t left = from, right = middle;
  for (size_t k = from; k < to; k++) {
    if (left < middle && right >= to) {
      target[k] = source[left++];
    } else if (left < middle && !cmp) {
      if (dbm_default_greater(source[left], source[right])) target[k] = source[right++];
      else target[k] = source[left++];
    } else if (left < middle) {
      ScrDyn *x = scr_dyn_new_num(source[left]);
      ScrDyn *y = scr_dyn_new_num(source[right]);
      ScrDyn *argv[2] = { x, y };
      scr_dyn_this_push_dyn(NULL);
      ScrDyn *r = scr_dyn_call(cmp, argv, 2, "comparator");
      scr_dyn_this_pop();
      scr_dyn_release(x);
      scr_dyn_release(y);
      double order = 0;
      bool ok = r && scr_dyn_number_coerce_js(r, &order);
      scr_dyn_release(r);
      if (!ok) return false;
      if (order > 0) target[k] = source[right++];
      else target[k] = source[left++];
    } else {
      target[k] = source[right++];
    }
  }
  return true;
}

static bool dbm_comparator(const ScrDyn *cmp) {
  if (cmp->kind == SCR_DYN_UNDEF || scr_dyn_is_callable(cmp)) return true;
  ScrStr *text = scr_dyn_display(cmp);
  ScrJsonBuf b;
  scr_jb_init(&b);
  scr_jb_puts(&b, "The comparison function must be either a function or undefined: ");
  scr_jb_put_str(&b, text);
  scr_str_release(text);
  scr_throw_error(SCR_ERR_TYPE, scr_jb_finish(&b));
  return false;
}

/* Sorts `b` in place: the default numeric order, or through `cmp` with the
 * elements read once and written back after the last comparison. */
static bool dbm_sort_in_place(ScrBytes *b, ScrDyn *cmp) {
  if (cmp->kind == SCR_DYN_UNDEF) cmp = NULL;
  size_t n = b->len;
  if (n < 2) return true;
  double *work1 = malloc(n * sizeof *work1), *work2 = malloc(n * sizeof *work2);
  if (!work1 || !work2) scr_trap("scriptc: out of memory\n");
  for (size_t i = 0; i < n; i++) work1[i] = work2[i] = scr_bytes_get(b, (double)i);
  bool sorted = dbm_merge_sort(work2, 0, n, work1, cmp);
  if (sorted) {
    for (size_t i = 0; i < n; i++) scr_bytes_set(b, (double)i, work1[i]);
  }
  free(work1);
  free(work2);
  return sorted;
}

static ScrDyn *dbm_to_locale_string(const ScrBytes *bytes, ScrDyn *const *args, size_t argc) {
  for (size_t i = 0; i < argc && i < 2; i++) {
    if (args[i]->kind != SCR_DYN_UNDEF) {
      static const char message[] = "TypedArray toLocaleString with locales or options on a dynamic value is not supported yet";
      scr_throw_error_msg_code(SCR_ERR_ERROR, message, sizeof message - 1, "SC2020");
      return NULL;
    }
  }
  ScrJsonBuf b;
  scr_jb_init(&b);
  for (size_t i = 0; i < bytes->len; i++) {
    if (i) scr_jb_putc(&b, ',');
    ScrStr *text = scr_intl_num_format_en_us(scr_bytes_get(bytes, (double)i));
    scr_jb_put_str(&b, text);
    scr_str_release(text);
  }
  ScrStr *text = scr_jb_finish(&b);
  ScrDyn *out = scr_dyn_new_str(text);
  scr_str_release(text);
  return out;
}

static ScrDyn *dbm_join(const ScrBytes *bytes, ScrDyn *const *args, size_t argc, bool join) {
  ScrStr *separator = join && argc && args[0]->kind != SCR_DYN_UNDEF ? scr_dyn_string_coerce_js(args[0])
                                                                      : scr_str_new(",", 1);
  if (!separator) return NULL;
  ScrStr *joined = scr_bytes_join(bytes, separator);
  scr_str_release(separator);
  ScrDyn *out = scr_dyn_new_str(joined);
  scr_str_release(joined);
  return out;
}

/* keys(): the indices of a fixed-length view, through an array iterator. */
static ScrDyn *dbm_keys(const ScrBytes *bytes) {
  ScrDyn *indices = scr_dyn_new_arr();
  for (size_t i = 0; i < bytes->len; i++) scr_dyn_arr_push(indices, scr_dyn_new_num((double)i));
  ScrDyn *iterator = scr_dyn_native_handle_iterator(indices, 0);
  scr_dyn_release(indices);
  return iterator;
}

/* The entries() mapping: (value, counter) to [counter, value]. */
static ScrDyn *dbm_entry_pair(ScrClosure *closure, ScrDyn *const *args, size_t argc) {
  (void)closure;
  ScrDyn *pair = scr_dyn_new_arr();
  scr_dyn_arr_push(pair, scr_dyn_retain(argc > 1 ? args[1] : scr_dyn_undefined()));
  scr_dyn_arr_push(pair, scr_dyn_retain(argc > 0 ? args[0] : scr_dyn_undefined()));
  return pair;
}

/* entries(): the values iterator mapped to [index, value] pairs, so each
 * step reads the element's current value. */
static ScrDyn *dbm_entries(ScrDyn *recv) {
  ScrDyn *values = scr_dyn_native_handle_iterator(recv, 0);
  ScrStr *name = scr_str_new("map", 3);
  ScrDyn *map = scr_dyn_iterator_helper_method(name);
  scr_str_release(name);
  ScrDyn *pair = scr_dyn_new_func(scr_closure_new(NULL, 0), dbm_entry_pair, 2, "native:builtin", "");
  scr_dyn_this_push_dyn(values);
  ScrDyn *result = scr_dyn_call(map, &pair, 1, "entries");
  scr_dyn_this_pop();
  scr_dyn_release(pair);
  scr_dyn_release(map);
  scr_dyn_release(values);
  return result;
}

/* ── dispatch ─────────────────────────────────────────────────────────── */

static ScrDyn *dbm_dispatch(DbmId id, ScrDyn *recv, ScrDyn *const *args, size_t argc) {
  ScrBytes *bytes = recv->v.bytes;
  size_t len = bytes->len;
  const DbmInfo *info = &dbm_info[id];
  if (dbm_is_buffer_method(id) && scr_bytes_elem_size(bytes->elem) != 1) {
    dbm_throw_unsupported("Buffer", info->member);
    return NULL;
  }
  if (info->num_op != DBM_NUM_NONE) return dbm_numeric(info, bytes, args, argc);
  switch (id) {
  case DBM_AT: {
    double i;
    if (!dbm_index_arg(args, argc, 0, 0, &i)) return NULL;
    double index = i < 0 ? (double)len + i : i;
    if (index < 0 || index >= (double)len) return scr_dyn_retain(scr_dyn_undefined());
    return scr_dyn_new_num(scr_bytes_get(bytes, index));
  }
  case DBM_COPY_WITHIN: {
    double target, start, end;
    if (!dbm_index_arg(args, argc, 0, 0, &target) || !dbm_index_arg(args, argc, 1, 0, &start) ||
        !dbm_index_arg(args, argc, 2, (double)len, &end))
      return NULL;
    scr_bytes_release(scr_bytes_copy_within(bytes, target, start, end));
    return scr_dyn_retain(recv);
  }
  case DBM_ENTRIES: return dbm_entries(recv);
  case DBM_KEYS: return dbm_keys(bytes);
  case DBM_VALUES: return scr_dyn_native_handle_iterator(recv, 0);
  case DBM_EVERY: return dbm_scan(recv, args, argc, DBM_SCAN_EVERY, false);
  case DBM_SOME: return dbm_scan(recv, args, argc, DBM_SCAN_SOME, false);
  case DBM_FIND: return dbm_scan(recv, args, argc, DBM_SCAN_FIND, false);
  case DBM_FIND_INDEX: return dbm_scan(recv, args, argc, DBM_SCAN_FIND_INDEX, false);
  case DBM_FIND_LAST: return dbm_scan(recv, args, argc, DBM_SCAN_FIND, true);
  case DBM_FIND_LAST_INDEX: return dbm_scan(recv, args, argc, DBM_SCAN_FIND_INDEX, true);
  case DBM_FOR_EACH: return dbm_scan(recv, args, argc, DBM_SCAN_FOR_EACH, false);
  case DBM_FILL: {
    double value, start, end;
    if (!scr_dyn_number_coerce_js(dbm_arg(args, argc, 0), &value)) return NULL;
    if (!dbm_index_arg(args, argc, 1, 0, &start) || !dbm_index_arg(args, argc, 2, (double)len, &end)) return NULL;
    scr_bytes_release(scr_bytes_fill_elem(bytes, value, start, end));
    return scr_dyn_retain(recv);
  }
  case DBM_FILTER: return dbm_filter(recv, args, argc);
  case DBM_MAP: return dbm_map(recv, args, argc);
  case DBM_INCLUDES: case DBM_INDEX_OF: case DBM_LAST_INDEX_OF:
    return dbm_typed_search(bytes, args, argc, id);
  case DBM_JOIN: return dbm_join(bytes, args, argc, true);
  case DBM_TO_STRING: return dbm_join(bytes, args, argc, false);
  case DBM_TO_LOCALE_STRING: return dbm_to_locale_string(bytes, args, argc);
  case DBM_REVERSE:
    dbm_reverse_in_place(bytes);
    return scr_dyn_retain(recv);
  case DBM_REDUCE: return dbm_reduce(recv, args, argc, false);
  case DBM_REDUCE_RIGHT: return dbm_reduce(recv, args, argc, true);
  case DBM_SET: {
    double offset;
    if (!dbm_index_arg(args, argc, 1, 0, &offset)) return NULL;
    scr_bytes_set_from_dyn(bytes, dbm_arg(args, argc, 0), offset);
    return scr_exc_pending() ? NULL : scr_dyn_retain(scr_dyn_undefined());
  }
  case DBM_SLICE: case DBM_SUBARRAY: case DBM_B_SLICE: case DBM_B_SUBARRAY: {
    /* subarray and Buffer's slice alias the receiver; TypedArray slice
     * copies. Both keep a Buffer receiver's brand (Node's species). */
    double start, end;
    if (!dbm_index_arg(args, argc, 0, 0, &start) || !dbm_index_arg(args, argc, 1, (double)len, &end)) return NULL;
    ScrBytes *out = id == DBM_SLICE ? scr_bytes_slice(bytes, start, end) : scr_bytes_subarray(bytes, start, end);
    return dbm_bytes_result(out, recv->buffer || dbm_is_buffer_method(id));
  }
  case DBM_SORT:
    if (!dbm_comparator(dbm_arg(args, argc, 0))) return NULL;
    if (!dbm_sort_in_place(bytes, dbm_arg(args, argc, 0))) return NULL;
    return scr_dyn_retain(recv);
  case DBM_TO_SORTED: {
    if (!dbm_comparator(dbm_arg(args, argc, 0))) return NULL;
    ScrBytes *out = dbm_plain_copy(bytes);
    if (!out) return NULL;
    if (!dbm_sort_in_place(out, dbm_arg(args, argc, 0))) {
      scr_bytes_release(out);
      return NULL;
    }
    return dbm_bytes_result(out, false);
  }
  case DBM_TO_REVERSED: {
    ScrBytes *out = dbm_plain_copy(bytes);
    if (!out) return NULL;
    dbm_reverse_in_place(out);
    return dbm_bytes_result(out, false);
  }
  case DBM_WITH: {
    double rel, value;
    if (!dbm_index_arg(args, argc, 0, 0, &rel)) return NULL;
    if (!scr_dyn_number_coerce_js(dbm_arg(args, argc, 1), &value)) return NULL;
    double actual = rel >= 0 ? rel : (double)len + rel;
    if (!(actual >= 0) || actual >= (double)len) {
      static const char message[] = "Invalid typed array index";
      scr_throw_error_msg(SCR_ERR_RANGE, message, sizeof message - 1);
      return NULL;
    }
    ScrBytes *out = dbm_plain_copy(bytes);
    if (!out) return NULL;
    scr_bytes_set(out, actual, value);
    return dbm_bytes_result(out, false);
  }
  case DBM_B_COMPARE: {
    ScrDyn *target = dbm_arg(args, argc, 0);
    if (!dbm_is_u8(target)) {
      scr_dyn_arg_type_fail("target", "an instance of Buffer or Uint8Array", target);
      return NULL;
    }
    double order = argc <= 1 ? scr_bytes_compare(bytes, target->v.bytes, 0, 0, 0, 0, 0)
      : scr_bytes_compare_chk(bytes, target, dbm_arg(args, argc, 1), dbm_arg(args, argc, 2),
                              dbm_arg(args, argc, 3), dbm_arg(args, argc, 4));
    return scr_exc_pending() ? NULL : scr_dyn_new_num(order);
  }
  case DBM_B_COPY: return dbm_buffer_copy(bytes, args, argc);
  case DBM_B_EQUALS: {
    ScrDyn *other = dbm_arg(args, argc, 0);
    if (!dbm_is_u8(other)) {
      scr_dyn_arg_type_fail("otherBuffer", "an instance of Buffer or Uint8Array", other);
      return NULL;
    }
    return scr_dyn_new_bool(scr_bytes_equals(bytes, other->v.bytes));
  }
  case DBM_B_FILL: return dbm_buffer_fill(recv, args, argc);
  case DBM_B_INDEX_OF: case DBM_B_LAST_INDEX_OF: case DBM_B_INCLUDES: {
    double index = dbm_buffer_index_of(bytes, args, argc, id != DBM_B_LAST_INDEX_OF);
    if (scr_exc_pending()) return NULL;
    return id == DBM_B_INCLUDES ? scr_dyn_new_bool(index != -1) : scr_dyn_new_num(index);
  }
  case DBM_B_INSPECT: return dbm_buffer_inspect(bytes);
  case DBM_B_SWAP16: case DBM_B_SWAP32: case DBM_B_SWAP64: {
    ScrBytes *swapped = scr_bytes_swap(bytes, id == DBM_B_SWAP16 ? 2 : id == DBM_B_SWAP32 ? 4 : 8);
    if (!swapped) return NULL;
    scr_bytes_release(swapped);
    return scr_dyn_retain(recv);
  }
  case DBM_B_TO_JSON: return dbm_buffer_to_json(bytes);
  case DBM_B_TO_STRING: return dbm_buffer_to_string(bytes, args, argc);
  case DBM_B_WRITE: return dbm_buffer_write(bytes, args, argc);
  default:
    dbm_throw_unsupported("Buffer", info->member);
    return NULL;
  }
}

/* ── the interned method values ───────────────────────────────────────── */

static SCR_TL ScrDyn *dbm_values[DBM_COUNT];
static SCR_TL bool dbm_values_registered;

static void dbm_values_cleanup(void) {
  for (size_t i = 0; i < DBM_COUNT; i++) {
    scr_dyn_release(dbm_values[i]);
    dbm_values[i] = NULL;
  }
  dbm_values_registered = false;
}

static ScrDyn *dbm_value_call(ScrClosure *closure, ScrDyn *const *args, size_t argc) {
  DbmId id = (DbmId)scr_box_get_f64(closure->caps[0]);
  ScrDyn *recv = scr_dyn_this_get();
  ScrDyn *result = NULL;
  if (recv->kind == SCR_DYN_BYTES && !recv->v.bytes->is_data_view) {
    result = dbm_dispatch(id, recv, args, argc);
  } else if (dbm_is_buffer_method(id)) {
    dbm_throw_unsupported("Buffer", dbm_info[id].member);
  } else {
    static const char message[] = "this is not a typed array.";
    scr_throw_error_msg(SCR_ERR_TYPE, message, sizeof message - 1);
  }
  scr_dyn_release(recv);
  return result;
}

static ScrDyn *dbm_value(DbmId id) {
  if (!dbm_values[id]) {
    if (!dbm_values_registered) {
      dbm_values_registered = true;
      scr_atexit(dbm_values_cleanup);
    }
    ScrClosure *closure = scr_closure_new(NULL, 1);
    closure->caps[0] = scr_box_new(SCR_BOX_F64);
    scr_box_set_f64(closure->caps[0], (double)id);
    dbm_values[id] = scr_dyn_new_func(closure, dbm_value_call, dbm_info[id].length, "native:prototype",
                                      dbm_info[id].name);
  }
  return scr_dyn_retain(dbm_values[id]);
}

/* ── entry points ─────────────────────────────────────────────────────── */

ScrDyn *scr_dyn_bytes_method_invoke(ScrDyn *recv, ScrDyn *callee, const char *method,
                                    ScrDyn *const *args, size_t argc, const char *what) {
  if (!callee && recv->kind == SCR_DYN_BYTES && !recv->v.bytes->is_data_view) {
    DbmId id = dbm_lookup(method, recv->buffer);
    if (id != DBM_NONE) return dbm_dispatch(id, recv, args, argc);
  }
  return scr_dyn_invoke_prepared(recv, callee, method, args, argc, what);
}

ScrDyn *scr_dyn_bytes_method_read(const ScrDyn *d, const ScrStr *key, ScrDyn *read) {
  if (!read || read->kind != SCR_DYN_UNDEF || d->kind != SCR_DYN_BYTES || d->v.bytes->is_data_view) return read;
  if (d->buffer && key->len == 6 && memcmp(key->data, "parent", 6) == 0) {
    scr_dyn_release(read);
    return scr_array_buffer_from_bytes(d->v.bytes);
  }
  if (d->buffer && key->len == 6 && memcmp(key->data, "offset", 6) == 0) {
    scr_dyn_release(read);
    return scr_dyn_new_num(scr_bytes_byte_offset(d->v.bytes));
  }
  DbmId id = dbm_lookup(key->data, d->buffer);
  if (id == DBM_NONE) return read;
  scr_dyn_release(read);
  return dbm_value(id);
}
