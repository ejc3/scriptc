// Brotli params Node refuses: keys outside the encoder's or decoder's
// params, keys that aren't integers, keys given twice, values that aren't
// numbers or booleans, params that aren't objects, and values the encoder
// or decoder rejects at initialization.
const zlib = require("node:zlib");

function show(label, run) {
  try {
    const result = run();
    console.log(label, "ok", result.length, result.toString("hex").slice(0, 24));
  } catch (error) {
    console.log(label, error.name, error.code, JSON.stringify(error.message), "errno" in error ? error.errno : "-");
  }
}

const compress = (params) => () => zlib.brotliCompressSync("hello hello hello", { params });
const decompress = (params) => () =>
  zlib.brotliDecompressSync(zlib.brotliCompressSync("hello"), { params });

show("key-8", compress({ 8: 0 }));
show("key-9", compress({ 9: 0 }));
show("key-999", compress({ 999: 1 }));
show("key-minus-1", compress({ "-1": 0 }));
show("key-fraction", compress({ "1.5": 0 }));
show("key-word", compress({ abc: 0 }));
show("key-space", compress({ " 1": 4 }));
show("key-hex", compress({ "0x1": 4 }));
show("key-empty", compress({ "": 4 }));
show("key-twice", compress({ 1: 4, "01": 5 }));
show("value-string", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: "a" }));
show("value-numeric-string", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: "4" }));
show("value-null", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: null }));
show("value-undefined", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: undefined }));
show("value-object", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: {} }));
show("value-bigint", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: 4n }));
show("params-null", compress(null));
show("params-undefined", compress(undefined));
show("params-number", compress(5));
show("params-string", compress("ab"));
show("params-string-3", compress("abcd"));
show("params-array", compress([4]));
show("params-array-long", compress([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]));
show("params-boolean", compress(true));
show("params-function", compress(() => 1));
show("params-empty", compress({}));
show("quality-12", compress({ [zlib.constants.BROTLI_PARAM_QUALITY]: 12 }));
show("lgwin-9", compress({ [zlib.constants.BROTLI_PARAM_LGWIN]: 9 }));
show("lgblock-25", compress({ [zlib.constants.BROTLI_PARAM_LGBLOCK]: 25 }));
show("ndirect-odd", compress({ [zlib.constants.BROTLI_PARAM_NDIRECT]: 3 }));
show("decoder-key-0", decompress({ 0: 1 }));
show("decoder-key-1", decompress({ 1: 1 }));
show("decoder-key-2", decompress({ 2: 1 }));
show("decoder-key-9", decompress({ 9: 1 }));
show("decoder-value-string", decompress({ 0: "x" }));
