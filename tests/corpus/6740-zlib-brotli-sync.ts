// brotliCompressSync / brotliDecompressSync with default options: string,
// Buffer and Uint8Array inputs, empty input, and a
// fixed stream decoded from bytes. The vendored Brotli is the version Node
// bundles, so compressed bytes are printed as well as round-trips.
import { brotliCompressSync, brotliDecompressSync } from "node:zlib";
import * as zlib from "node:zlib";

const hex = (bytes: Uint8Array): string => Buffer.from(bytes).toString("hex");

const text = "the quick brown fox jumps over the lazy dog ☃ ".repeat(40);
const packed = brotliCompressSync(text);
console.log("string", packed.length, Buffer.isBuffer(packed), hex(packed.subarray(0, 24)));
const plain = brotliDecompressSync(packed);
console.log("roundtrip", Buffer.isBuffer(plain), plain.length, plain.toString() === text);

const raw = Buffer.from(text, "utf8");
console.log("buffer", hex(brotliCompressSync(raw)) === hex(packed));
console.log("namespace", hex(zlib.brotliCompressSync(raw)) === hex(packed));
console.log("namespace-decode", zlib.brotliDecompressSync(packed).equals(raw));

console.log("hello", hex(brotliCompressSync("hello")));
console.log("abc", hex(brotliCompressSync(Buffer.from("abc"))));

const empty = brotliCompressSync(Buffer.alloc(0));
console.log("empty", empty.length, hex(empty));
console.log("empty-decode", brotliDecompressSync(empty).length);
console.log("empty-string", hex(brotliCompressSync("")));

const bytes = new Uint8Array([1, 2, 3, 250, 251, 252]);
console.log("uint8", hex(brotliDecompressSync(brotliCompressSync(bytes))));
const sub = raw.subarray(4, 9);
console.log("subarray", brotliDecompressSync(brotliCompressSync(sub)).toString());

// "hello brotli\n" compressed at quality 5.
const fixed = Buffer.from("0b068068656c6c6f2062726f746c690a03", "hex");
console.log("fixed", JSON.stringify(brotliDecompressSync(fixed).toString()));
console.log("fixed-u8", brotliDecompressSync(new Uint8Array(fixed)).toString().trim());

const repeated = Buffer.alloc(100000, 0x61);
const small = brotliCompressSync(repeated);
console.log("repeated", small.length, brotliDecompressSync(small).equals(repeated));

const unicode = "naïve 日本語 🎉 ".repeat(30);
console.log("unicode", brotliDecompressSync(brotliCompressSync(unicode)).toString() === unicode);
