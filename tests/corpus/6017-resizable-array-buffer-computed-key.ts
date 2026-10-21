// A resizable ArrayBuffer resized through a string-typed member key.
const storage = new ArrayBuffer(8, { maxByteLength: 16 });
const fixedView = new Uint8Array(storage, 0, 8);
fixedView.fill(3);
const methods: Record<string, unknown> = storage as unknown as Record<string, unknown>;
const key: string = process.argv.length > 99 ? "slice" : ["re", "size"].join("");
(methods[key] as (this: ArrayBuffer, n: number) => void).call(storage, 4);
let total = 0;
for (let i = 0; i < fixedView.length; i++) total += fixedView[i]!;
console.log(storage.byteLength, fixedView.length, total);
try {
  fixedView.fill(1);
  console.log("filled");
} catch (e) {
  console.log((e as Error).name, (e as Error).message);
}
