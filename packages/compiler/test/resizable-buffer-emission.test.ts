import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, expect, test } from "vitest";
import { compile } from "../src/index.js";

const directories: string[] = [];
afterEach(async () => {
  await Promise.all(directories.splice(0).map((dir) => rm(dir, { recursive: true, force: true })));
});

async function emit(source: string): Promise<string> {
  const outDir = await mkdtemp(join(tmpdir(), "scriptc-resizable-"));
  directories.push(outDir);
  const entry = join(outDir, "main.ts");
  await writeFile(entry, source);
  const outPath = join(outDir, "program.ll");
  const result = await compile(entry, { entry, outDir, outPath, outputKind: "llvm" });
  expect(result.ok, JSON.stringify(result)).toBe(true);
  return readFile(outPath, "utf8");
}

const calls = (llvm: string, symbol: string): number =>
  llvm.split("\n").filter((line) => line.includes("call ") && line.includes(`@${symbol}(`)).length;

const views = `
const buf = new ArrayBuffer(8, { maxByteLength: 16 });
const view = new Uint8Array(buf, 2);
console.log(view.join(), view.byteOffset, [...view].length);
for (const x of view) console.log(x);
`;

test("programs that never resize a buffer check no views", async () => {
  const llvm = await emit(`${views}
class Image {
  resize(width: number): number {
    return width * 2;
  }
}
console.log(new Image().resize(2));
`);
  expect(calls(llvm, "scr_bytes_validate")).toBe(0);
  expect(calls(llvm, "scr_bytes_validate_c")).toBe(0);
  expect(calls(llvm, "scr_bytes_view_byte_offset")).toBe(0);
  expect(calls(llvm, "scr_bytes_byte_offset")).toBe(1);
});

test("programs that resize a buffer check views before typed-array operations", async () => {
  const llvm = await emit(`${views}
buf.resize(4);
`);
  // join, the start of the for...of and each of its steps check through
  // bytesIntrinsic validate; the spread checks inside toArray.
  expect(calls(llvm, "scr_bytes_validate")).toBe(3);
  expect(calls(llvm, "scr_bytes_validate_c")).toBe(1);
  expect(calls(llvm, "scr_bytes_view_byte_offset")).toBe(1);
  expect(calls(llvm, "scr_bytes_byte_offset")).toBe(0);
});

test("subarray with an undefined end takes the form that can track the receiver's length", async () => {
  const llvm = await emit(`${views}
const end = (n?: number): Uint8Array => view.subarray(1, n);
console.log(view.subarray(1).length, view.subarray(1, undefined).length);
console.log(view.subarray(1, Infinity).length, end().length, end(3).length);
buf.resize(4);
`);
  // subarray(1), subarray(1, undefined) and the undefined arm of `n`.
  expect(calls(llvm, "scr_bytes_subarray_tail")).toBe(3);
  // subarray(1, Infinity) and the number arm of `n`.
  expect(calls(llvm, "scr_bytes_subarray")).toBe(2);
});

test("a computed key that can be resize counts as resizing a buffer", async () => {
  const resizing = await emit(`${views}
const key: string = process.argv.length > 99 ? "slice" : "resize";
((buf as unknown as Record<string, unknown>)[key] as (this: ArrayBuffer, n: number) => void).call(buf, 4);
`);
  expect(calls(resizing, "scr_bytes_validate")).toBe(3);
  const other = await emit(`${views}
const key = process.argv.length > 99 ? "slice" : "transfer";
console.log(typeof (buf as unknown as Record<string, unknown>)[key]);
`);
  expect(calls(other, "scr_bytes_validate")).toBe(0);
});
