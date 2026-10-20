/** The pinned QuickJS, mbedTLS, zlib, and Brotli inputs used by native recipes. */
export const QJS_COMMIT = "3c8f3d68953955950074c41c6e4d999562ae82a7";
export const MBEDTLS_VERSION = "3.6.7";
export const ZLIB_VERSION = "1.3.1";
export const BROTLI_VERSION = "1.2.0";
/** google/brotli's library translation units (CMakeLists.txt's c/common,
 * c/dec and c/enc globs), relative to packages/runtime/vendor/brotli. */
export const BROTLI_SOURCES = [
  "common/constants.c",
  "common/context.c",
  "common/dictionary.c",
  "common/platform.c",
  "common/shared_dictionary.c",
  "common/transform.c",
  "dec/bit_reader.c",
  "dec/decode.c",
  "dec/huffman.c",
  "dec/prefix.c",
  "dec/state.c",
  "dec/static_init.c",
  "enc/backward_references.c",
  "enc/backward_references_hq.c",
  "enc/bit_cost.c",
  "enc/block_splitter.c",
  "enc/brotli_bit_stream.c",
  "enc/cluster.c",
  "enc/command.c",
  "enc/compound_dictionary.c",
  "enc/compress_fragment.c",
  "enc/compress_fragment_two_pass.c",
  "enc/dictionary_hash.c",
  "enc/encode.c",
  "enc/encoder_dict.c",
  "enc/entropy_encode.c",
  "enc/fast_log.c",
  "enc/histogram.c",
  "enc/literal_cost.c",
  "enc/memory.c",
  "enc/metablock.c",
  "enc/static_dict.c",
  "enc/static_dict_lut.c",
  "enc/static_init.c",
  "enc/utf8_util.c",
] as const;
/** Exact translation-unit membership shared by cache builds and external
 * source-pack recipes. */
export const LRE_SOURCES = ["libregexp.c", "libunicode.c"] as const;
export const ZLIB_SOURCES = [
  "adler32.c",
  "compress.c",
  "crc32.c",
  "deflate.c",
  "infback.c",
  "inffast.c",
  "inflate.c",
  "inftrees.c",
  "trees.c",
  "uncompr.c",
  "zutil.c",
] as const;
