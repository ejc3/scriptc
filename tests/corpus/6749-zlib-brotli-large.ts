// Larger Brotli inputs: a megabyte of generated text and a block of
// pseudo-random bytes, compressed at several qualities, decoded with small
// and default chunks, and run through several callbacks at once.
import { brotliCompress, brotliCompressSync, brotliDecompress, brotliDecompressSync, constants } from "node:zlib";
import { createHash } from "node:crypto";

const digest = (bytes: Uint8Array): string => createHash("sha256").update(bytes).digest("hex").slice(0, 16);

const words = ["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta"];
let seed = 12345;
const next = (): number => {
  seed = (seed * 1103515245 + 12345) % 2147483648;
  return seed;
};
const parts: string[] = [];
let size = 0;
while (size < 1 << 20) {
  const word = words[next() % words.length]!;
  parts.push(word);
  size += word.length + 1;
}
const text = Buffer.from(parts.join(" "));
const noise = Buffer.alloc(200000);
for (let i = 0; i < noise.length; i++) noise[i] = next() & 0xff;
console.log("inputs", text.length, digest(text), noise.length, digest(noise));

for (const quality of [1, 4, 9, 11]) {
  const packed = brotliCompressSync(text, { params: { [constants.BROTLI_PARAM_QUALITY]: quality } });
  console.log("text", quality, packed.length, digest(packed), brotliDecompressSync(packed).equals(text));
}
const hinted = brotliCompressSync(text, {
  params: {
    [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_TEXT,
    [constants.BROTLI_PARAM_SIZE_HINT]: text.length,
  },
});
console.log("hinted", hinted.length, digest(hinted));
const noisy = brotliCompressSync(noise);
console.log("noise", noisy.length, digest(noisy), brotliDecompressSync(noisy).equals(noise));
const packed = brotliCompressSync(text);
console.log("chunk-64", brotliDecompressSync(packed, { chunkSize: 64 }).equals(text));
console.log("chunk-1m", brotliDecompressSync(packed, { chunkSize: 1 << 20 }).equals(text));

// Workers finish in any order, so results print once every job is done.
const jobs = [text, noise, text.subarray(1000, 50000), noise.subarray(5, 9)];
const lines: string[] = [];
let finished = 0;
jobs.forEach((job, index) => {
  brotliCompress(job, (error, value) => {
    const compressed = `compressed ${index} ${error === null} ${value.length} ${digest(value)}`;
    brotliDecompress(value, (again, plain) => {
      lines[index] = `${compressed} decompressed ${again === null} ${plain.equals(job)}`;
      finished++;
      if (finished === jobs.length) for (const line of lines) console.log(line);
    });
  });
});
