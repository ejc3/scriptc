// Brotli encoder and decoder params: quality, window, mode, size hint, large
// window and the decoder's flags, with Node's conversion of param values
// (booleans, fractions, values above 2^32, NaN, and -1 for unset).
import { brotliCompressSync, brotliDecompressSync, constants } from "node:zlib";

const hex = (bytes: Uint8Array): string => Buffer.from(bytes).toString("hex");
const text = "Brotli params: the quick brown fox jumps over the lazy dog. ".repeat(50);

function show(label: string, params: Record<number, number | boolean>): void {
  try {
    const packed = brotliCompressSync(text, { params });
    const plain = brotliDecompressSync(packed);
    console.log(label, packed.length, hex(packed.subarray(0, 16)), plain.toString() === text);
  } catch (error) {
    const e = error as NodeJS.ErrnoException;
    console.log(label, e.name, e.code, e.message);
  }
}

for (let quality = 0; quality <= 11; quality++) {
  show("quality-" + quality, { [constants.BROTLI_PARAM_QUALITY]: quality });
}
show("lgwin-10", { [constants.BROTLI_PARAM_LGWIN]: 10 });
show("lgwin-24", { [constants.BROTLI_PARAM_LGWIN]: 24 });
show("lgblock-16", { [constants.BROTLI_PARAM_LGBLOCK]: 16 });
show("mode-text", { [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_TEXT });
show("mode-font", { [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_FONT });
show("mode-generic", { [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_GENERIC });
show("size-hint", { [constants.BROTLI_PARAM_SIZE_HINT]: text.length });
show("literal-context-modeling", {
  [constants.BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING]: 1,
});
show("npostfix", { [constants.BROTLI_PARAM_NPOSTFIX]: 2, [constants.BROTLI_PARAM_NDIRECT]: 8 });
show("several", {
  [constants.BROTLI_PARAM_QUALITY]: 6,
  [constants.BROTLI_PARAM_LGWIN]: 18,
  [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_TEXT,
});

show("unset", { [constants.BROTLI_PARAM_QUALITY]: -1 });
show("fraction", { [constants.BROTLI_PARAM_QUALITY]: 3.7 });
show("true", { [constants.BROTLI_PARAM_QUALITY]: true });
show("false", { [constants.BROTLI_PARAM_QUALITY]: false });
show("above-2^32", { [constants.BROTLI_PARAM_QUALITY]: 2 ** 33 + 3 });
show("nan", { [constants.BROTLI_PARAM_QUALITY]: NaN });
show("negative-fraction", { [constants.BROTLI_PARAM_QUALITY]: -0.5 });
show("quality-12", { [constants.BROTLI_PARAM_QUALITY]: 12 });
show("lgwin-9", { [constants.BROTLI_PARAM_LGWIN]: 9 });
show("lgwin-25", { [constants.BROTLI_PARAM_LGWIN]: 25 });
show("mode-3", { [constants.BROTLI_PARAM_MODE]: 3 });

const large = brotliCompressSync(text, {
  params: { [constants.BROTLI_PARAM_LARGE_WINDOW]: 1, [constants.BROTLI_PARAM_LGWIN]: 30 },
});
console.log("large-window", large.length, hex(large.subarray(0, 8)));
try {
  brotliDecompressSync(large);
} catch (error) {
  const e = error as NodeJS.ErrnoException;
  console.log("large-window-plain-decoder", e.code, e.message, e.errno);
}
const opened = brotliDecompressSync(large, {
  params: { [constants.BROTLI_DECODER_PARAM_LARGE_WINDOW]: 1 },
});
console.log("large-window-decoder", opened.toString() === text);
const ring = brotliDecompressSync(brotliCompressSync(text), {
  params: { [constants.BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION]: true },
});
console.log("ring-buffer", ring.toString() === text);
