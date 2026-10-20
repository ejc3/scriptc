// Brotli with a custom dictionary: Buffer and Uint8Array dictionaries, the
// same dictionary on both sides, an empty dictionary, and dictionaries with
// quality and window params.
import { brotliCompress, brotliCompressSync, brotliDecompressSync, constants } from "node:zlib";

const hex = (bytes: Uint8Array): string => Buffer.from(bytes).toString("hex");
const dictionary = Buffer.from(
  "the dictionary holds phrases the input repeats: content-type, application/json, charset=utf-8",
);
const text = '{"content-type":"application/json; charset=utf-8","body":"phrases the input repeats"}';

const plainPacked = brotliCompressSync(text);
const packed = brotliCompressSync(text, { dictionary });
console.log("sizes", plainPacked.length, packed.length, packed.length < plainPacked.length);
console.log("bytes", hex(packed));
console.log("roundtrip", brotliDecompressSync(packed, { dictionary }).toString() === text);

const u8 = new Uint8Array(dictionary);
console.log("uint8", hex(brotliCompressSync(text, { dictionary: u8 })) === hex(packed));
console.log("uint8-decode", brotliDecompressSync(packed, { dictionary: u8 }).toString() === text);

const sub = dictionary.subarray(4, 40);
const subPacked = brotliCompressSync(text, { dictionary: sub });
console.log("subarray", hex(subPacked).slice(0, 32), brotliDecompressSync(subPacked, { dictionary: Buffer.from(sub) }).toString() === text);

const empty = brotliCompressSync(text, { dictionary: Buffer.alloc(0) });
console.log("empty", hex(empty) === hex(plainPacked), brotliDecompressSync(empty).toString() === text);

for (const quality of [0, 2, 5, 9, 11]) {
  const out = brotliCompressSync(text, {
    dictionary,
    params: { [constants.BROTLI_PARAM_QUALITY]: quality },
  });
  console.log("quality", quality, out.length, brotliDecompressSync(out, { dictionary }).toString() === text);
}
const windowed = brotliCompressSync(text, {
  dictionary,
  params: { [constants.BROTLI_PARAM_LGWIN]: 12 },
});
console.log("lgwin", windowed.length, brotliDecompressSync(windowed, { dictionary }).toString() === text);

brotliCompress(text, { dictionary }, (error, value) => {
  console.log("callback", error, value.equals(packed));
});
