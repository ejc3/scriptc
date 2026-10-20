// maxOutputLength and chunkSize together: output that fits exactly, output
// one byte over, small chunks that need many output rounds, and the same
// limits through the callback forms.
import { brotliCompressSync, brotliDecompress, brotliDecompressSync } from "node:zlib";

const data = Buffer.alloc(5000);
for (let i = 0; i < data.length; i++) data[i] = (i * 31 + (i >> 7)) & 0xff;
const packed = brotliCompressSync(data);
console.log("packed", packed.length);

function show(label: string, run: () => Buffer): void {
  try {
    const result = run();
    console.log(label, "ok", result.length, result.equals(data));
  } catch (error) {
    const e = error as NodeJS.ErrnoException;
    console.log(label, e.name, e.code, e.message);
  }
}

show("exact", () => brotliDecompressSync(packed, { maxOutputLength: data.length }));
show("one-under", () => brotliDecompressSync(packed, { maxOutputLength: data.length - 1 }));
show("small", () => brotliDecompressSync(packed, { maxOutputLength: 100 }));
show("chunk-64", () => brotliDecompressSync(packed, { chunkSize: 64 }));
show("chunk-64-exact", () =>
  brotliDecompressSync(packed, { chunkSize: 64, maxOutputLength: data.length }),
);
show("chunk-64-under", () =>
  brotliDecompressSync(packed, { chunkSize: 64, maxOutputLength: 4999 }),
);
show("chunk-1000-limit-1000", () =>
  brotliDecompressSync(packed, { chunkSize: 1000, maxOutputLength: 1000 }),
);
show("chunk-1000-limit-1001", () =>
  brotliDecompressSync(packed, { chunkSize: 1000, maxOutputLength: 1001 }),
);
show("compress-limit", () => brotliCompressSync(data, { maxOutputLength: 10 }));
show("compress-fits", () =>
  brotliDecompressSync(brotliCompressSync(data, { maxOutputLength: packed.length })),
);
show("compress-chunk-64", () => brotliDecompressSync(brotliCompressSync(data, { chunkSize: 64 })));
show("compress-chunk-64-same", () => {
  const small = brotliCompressSync(data, { chunkSize: 64 });
  console.log("compress-chunk-64-bytes", small.equals(packed));
  return brotliDecompressSync(small);
});

brotliDecompress(packed, { maxOutputLength: 4096 }, (error) => {
  const e = error as NodeJS.ErrnoException | null;
  console.log("callback-limit", e?.name, e?.code, e?.message);
  brotliDecompress(packed, { maxOutputLength: 5000, chunkSize: 128 }, (again, result) => {
    console.log("callback-fits", again, result.length, result.equals(data));
  });
});
