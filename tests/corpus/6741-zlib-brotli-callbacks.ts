// brotliCompress / brotliDecompress with error-first callbacks, with and
// without options: the callback runs after the call returns, decoder errors
// reach the callback, and option errors throw synchronously.
import { brotliCompress, brotliCompressSync, brotliDecompress, constants } from "node:zlib";
import * as zlib from "node:zlib";

const text = "brotli callbacks ".repeat(64);

function compress(data: string, quality?: number): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    const done = (error: Error | null, value: Buffer): void => {
      if (error) reject(error);
      else resolve(value);
    };
    if (quality === undefined) brotliCompress(data, done);
    else brotliCompress(data, { params: { [constants.BROTLI_PARAM_QUALITY]: quality } }, done);
  });
}

function decompress(data: Buffer): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    brotliDecompress(data, (error, value) => {
      if (error) reject(error);
      else resolve(value);
    });
  });
}

async function run(): Promise<void> {
  let returned = false;
  const packed = await new Promise<Buffer>((resolve) => {
    brotliCompress(text, (error, value) => {
      console.log("compress", returned, error === null, Buffer.isBuffer(value));
      resolve(value);
    });
    returned = true;
    console.log("scheduled");
  });
  console.log("same-as-sync", packed.equals(brotliCompressSync(text)));
  const plain = await decompress(packed);
  console.log("roundtrip", plain.toString() === text);

  const q1 = await compress(text, 1);
  console.log("quality-1", q1.toString("hex").slice(0, 32), q1.length);
  console.log("quality-1-sync", q1.equals(brotliCompressSync(text, { params: { [constants.BROTLI_PARAM_QUALITY]: 1 } })));
  const fromBuffer = await new Promise<Buffer>((resolve) => {
    brotliCompress(Buffer.from(text), (error, value) => resolve(value));
  });
  console.log("buffer-input", fromBuffer.equals(packed));

  await new Promise<void>((resolve) => {
    zlib.brotliDecompress(Buffer.from("not a brotli stream"), (error) => {
      const e = error as NodeJS.ErrnoException | null;
      console.log("decode-error", e?.name, e?.code, e?.message, e?.errno);
      resolve();
    });
  });
  await new Promise<void>((resolve) => {
    brotliDecompress(packed.subarray(0, 4), (error) => {
      const e = error as NodeJS.ErrnoException | null;
      console.log("truncated", e?.code, e?.message, e?.errno);
      resolve();
    });
  });
  await new Promise<void>((resolve) => {
    brotliDecompress(packed, { maxOutputLength: 16 }, (error) => {
      console.log("too-large", error?.name, (error as NodeJS.ErrnoException | null)?.code, error?.message);
      resolve();
    });
  });

  try {
    brotliCompress(text, { chunkSize: 8 }, () => console.log("not reached"));
  } catch (error) {
    const e = error as NodeJS.ErrnoException;
    console.log("sync-throw", e.name, e.code, e.message);
  }
  try {
    brotliCompress(text, { params: { 99: 1 } }, () => console.log("not reached"));
  } catch (error) {
    const e = error as NodeJS.ErrnoException;
    console.log("sync-throw-param", e.name, e.code, e.message);
  }

  const order: string[] = [];
  const parts = ["one ", "two ", "three "].map((part) => part.repeat(200));
  const results = await Promise.all(
    parts.map(
      (part, index) =>
        new Promise<Buffer>((resolve, reject) => {
          brotliCompress(part, (error, value) => {
            order.push(String(index));
            if (error) reject(error);
            else resolve(value);
          });
        }),
    ),
  );
  console.log("parallel", order.length);
  for (let i = 0; i < results.length; i++) {
    const result = results[i];
    if (result === undefined) continue;
    console.log("parallel-roundtrip", i, (await decompress(result)).toString() === parts[i]);
  }
}

run().then(() => console.log("done"));
