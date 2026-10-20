/* scriptc runtime — zlib's Brotli one-shot forms (scr_brotli.c). Included by
 * scr_runtime.h, which says where new declarations go. */
#ifndef SCR_BROTLI_H
#define SCR_BROTLI_H

#include "scr_zlib.h"

/* ── Brotli (scr_brotli.c — compiled, with the vendored google/brotli
 * archive, only when the program uses the Brotli functions; it also needs
 * scr_zlib.c's option helpers and callback adapter) ──────────────────────
 * brotliCompressSync / brotliDecompressSync with an options value
 * (undefined for none): Node's Brotli() and ZlibBase() option ladder, then
 * processChunkSync over the Brotli encoder or decoder, throwing Node's
 * errors ("Decompression failed" with the decoder's code and errno,
 * "unexpected end of file", ERR_BUFFER_TOO_LARGE,
 * ERR_ZLIB_INITIALIZATION_FAILED). Each direction has its own entry points,
 * so a program links only the codecs it calls. Borrow their inputs;
 * results are +1 Buffers. */
ScrBytes *scr_brotli_compress_sync(const ScrBytes *data, ScrDyn *opts);
ScrBytes *scr_brotli_decompress_sync(const ScrBytes *data, ScrDyn *opts);
/* The checked-dynamic input twins: strings encode as UTF-8, TypedArrays,
 * DataViews and ArrayBuffers contribute their bytes, and other values throw
 * Node's ERR_INVALID_ARG_TYPE for "buffer". */
ScrBytes *scr_brotli_compress_sync_dyn(ScrDyn *data, ScrDyn *opts);
ScrBytes *scr_brotli_decompress_sync_dyn(ScrDyn *data, ScrDyn *opts);
/* brotliCompress / brotliDecompress with a callback: the options and the
 * encoder or decoder are set up synchronously (their errors throw), the
 * codec runs on the worker pool, and fn calls cb with the Error or the
 * Buffer. The dynamic twins take null and undefined as empty input, as
 * Node's end(data) does. Move cb. */
void scr_brotli_compress_async(const ScrBytes *data, ScrDyn *opts, ScrClosure *cb /* moves */,
                               ScrZlibBytesFn fn);
void scr_brotli_decompress_async(const ScrBytes *data, ScrDyn *opts, ScrClosure *cb /* moves */,
                                 ScrZlibBytesFn fn);
void scr_brotli_compress_async_dyn(ScrDyn *data, ScrDyn *opts, ScrClosure *cb /* moves */,
                                   ScrZlibBytesFn fn);
void scr_brotli_decompress_async_dyn(ScrDyn *data, ScrDyn *opts, ScrClosure *cb /* moves */,
                                     ScrZlibBytesFn fn);

#endif
