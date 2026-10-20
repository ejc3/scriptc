// brotliDecompressSync errors: invalid streams carry the decoder's error
// code and errno, truncated streams report "unexpected end of file", and a
// complete stream ignores bytes after its end.
import { brotliCompressSync, brotliDecompressSync, constants } from "node:zlib";

function show(label: string, run: () => Buffer): void {
  try {
    const result = run();
    console.log(label, "ok", result.length, JSON.stringify(result.toString()));
  } catch (error) {
    const e = error as NodeJS.ErrnoException;
    console.log(label, e.name, e.code, JSON.stringify(e.message), e.errno, error instanceof Error);
  }
}

const packed = brotliCompressSync("hello world hello world hello world");

show("text", () => brotliDecompressSync(Buffer.from("not brotli data at all")));
show("zeros", () => brotliDecompressSync(Buffer.alloc(16)));
show("ones", () => brotliDecompressSync(Buffer.alloc(16, 0xff)));
show("one-byte", () => brotliDecompressSync(Buffer.from([0x21])));
show("empty", () => brotliDecompressSync(Buffer.alloc(0)));
show("empty-string", () => brotliDecompressSync(""));
for (const cut of [1, 3, 5, 8, packed.length - 1]) {
  show("truncated-" + cut, () => brotliDecompressSync(packed.subarray(0, cut)));
}
show("trailing", () =>
  brotliDecompressSync(Buffer.concat([packed, Buffer.from("trailing bytes")])),
);
show("two-streams", () => brotliDecompressSync(Buffer.concat([packed, packed])));
const corrupt = Buffer.from(packed);
corrupt[corrupt.length >> 1] ^= 0x5a;
show("corrupt", () => brotliDecompressSync(corrupt));

show("finish-flush", () =>
  brotliDecompressSync(packed.subarray(0, 6), { finishFlush: constants.BROTLI_OPERATION_FLUSH }),
);
show("finish-process", () =>
  brotliDecompressSync(packed.subarray(0, 6), { finishFlush: constants.BROTLI_OPERATION_PROCESS }),
);
show("finish-flush-whole", () =>
  brotliDecompressSync(packed, { finishFlush: constants.BROTLI_OPERATION_FLUSH }),
);

const dictionary = Buffer.from("hello world, a dictionary for hello world");
const withDictionary = brotliCompressSync("hello world hello world", { dictionary });
show("dictionary-missing", () => brotliDecompressSync(withDictionary));
show("dictionary-wrong", () =>
  brotliDecompressSync(withDictionary, { dictionary: Buffer.from("something else entirely") }),
);
show("dictionary-right", () => brotliDecompressSync(withDictionary, { dictionary }));

try {
  brotliDecompressSync(Buffer.from("xyz"));
} catch (error) {
  const e = error as NodeJS.ErrnoException;
  console.log("stack", typeof e.stack, e instanceof Error, e.name);
}
