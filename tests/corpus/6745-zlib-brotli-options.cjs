// Brotli's base options as Node checks them: chunkSize, flush, finishFlush,
// maxOutputLength and rejectGarbageAfterEnd ranges and types, options that
// aren't objects, and info, which the one-shot forms don't support here.
const zlib = require("node:zlib");

function show(label, run) {
  try {
    const result = run();
    console.log(label, "ok", result.length, result.toString("hex").slice(0, 24));
  } catch (error) {
    console.log(label, error.name, error.code, JSON.stringify(error.message));
  }
}

const text = "options options options ".repeat(30);
const packed = zlib.brotliCompressSync(text);
const compress = (options) => () => zlib.brotliCompressSync(text, options);
const decompress = (options) => () => zlib.brotliDecompressSync(packed, options);

show("none", compress(undefined));
show("null", compress(null));
show("empty", compress({}));
show("chunk-64", compress({ chunkSize: 64 }));
show("chunk-63", compress({ chunkSize: 63 }));
show("chunk-nan", compress({ chunkSize: NaN }));
show("chunk-infinity", compress({ chunkSize: Infinity }));
show("chunk-string", compress({ chunkSize: "64" }));
show("chunk-fraction", compress({ chunkSize: 100.5 }));
show("chunk-undefined", compress({ chunkSize: undefined }));
show("decode-chunk-64", decompress({ chunkSize: 64 }));
show("flush-0", compress({ flush: 0 }));
show("flush-3", compress({ flush: 3 }));
show("flush-4", compress({ flush: 4 }));
show("flush-negative", compress({ flush: -1 }));
show("flush-string", compress({ flush: "1" }));
show("finish-0", compress({ finishFlush: zlib.constants.BROTLI_OPERATION_PROCESS }));
show("finish-1", compress({ finishFlush: zlib.constants.BROTLI_OPERATION_FLUSH }));
show("finish-2", compress({ finishFlush: zlib.constants.BROTLI_OPERATION_FINISH }));
show("finish-3", compress({ finishFlush: zlib.constants.BROTLI_OPERATION_EMIT_METADATA }));
show("finish-4", compress({ finishFlush: 4 }));
show("max-1", compress({ maxOutputLength: 1 }));
show("max-0", compress({ maxOutputLength: 0 }));
show("max-huge", compress({ maxOutputLength: 2 ** 53 }));
show("max-safe", compress({ maxOutputLength: 2 ** 53 - 1 }));
show("max-string", compress({ maxOutputLength: "10" }));
show("garbage-true", decompress({ rejectGarbageAfterEnd: true }));
show("garbage-false", decompress({ rejectGarbageAfterEnd: false }));
show("garbage-number", decompress({ rejectGarbageAfterEnd: 1 }));
show("info-false", compress({ info: false }));
show("dictionary-undefined", compress({ dictionary: undefined }));
show("dictionary-string", compress({ dictionary: "abc" }));
show("dictionary-number", compress({ dictionary: 5 }));
show("params-and-flush", compress({ params: { [zlib.constants.BROTLI_PARAM_QUALITY]: 3 }, flush: 1 }));
show("unknown-option", compress({ level: 3, strategy: 1, windowBits: 9 }));
