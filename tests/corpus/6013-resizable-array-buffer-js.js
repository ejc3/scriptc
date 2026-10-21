// Resizable ArrayBuffers in JavaScript: views reached through untyped values,
// their properties, methods, iteration and DataView accessors when out of
// bounds, and resize called through an untyped receiver.
const report = (label, f) => {
  try {
    console.log(label, f());
  } catch (e) {
    console.log(label, e.name, e.message);
  }
};
const b = new ArrayBuffer(8, { maxByteLength: 16 });
const view = new Uint8Array(b, 4, 4);
const tracking = new Uint16Array(b);
const dv = new DataView(b, 2);
function same(x) {
  return x;
}
const v = same(view);
const d = same(dv);
b.resize(2);
report("sizes", () => [v.length, v.byteOffset, tracking.length, b.byteLength, b.maxByteLength, b.resizable]);
report("join", () => v.join());
report("fill", () => v.fill(1));
report("slice", () => v.slice());
report("at", () => v.at(0));
report("for-of", () => {
  for (const x of v) console.log(x);
  return "done";
});
report("spread", () => [...v]);
report("json", () => JSON.stringify(v));
report("DataView", () => [d.byteLength, d.byteOffset]);
report("getInt16", () => d.getInt16(0));
report("setFloat32", () => d.setFloat32(0, 1));
b.resize(12);
report("back", () => [v.length, v.join(), tracking.length, [...tracking].length, d.byteLength, d.getUint8(0)]);
const r = new ArrayBuffer(2, { maxByteLength: 4 });
same(r).resize(3);
report("resized", () => [r.byteLength, new Uint8Array(r).length]);
report("fixed", () => same(new ArrayBuffer(2)).resize(1));
report("options", () => new ArrayBuffer(1, { get maxByteLength() { return 4; } }).maxByteLength);
const s = new ArrayBuffer(4, { maxByteLength: 8 });
const stepView = same(new Uint8Array(s, 0, 4));
report("step", () => {
  for (const x of stepView) {
    console.log("step", x);
    s.resize(2);
  }
});
s.resize(4);
let shrinkSteps = 0;
for (const _ of same(new Uint8Array(s))) {
  shrinkSteps++;
  if (shrinkSteps === 2) s.resize(3);
}
report("shrink", () => shrinkSteps);
const g = new ArrayBuffer(2, { maxByteLength: 8 });
const tracked = same(new Uint8Array(g)).subarray(1);
g.resize(6);
report("tracking subarray", () => [tracked.length, same(new Uint8Array(g)).subarray(0, 2).length]);
const cut = same(new Uint8Array(g, 4, 2));
g.resize(2);
report("subarray out of bounds", () => cut.subarray(0));
const e = new ArrayBuffer(4, { maxByteLength: 16 });
const ev = same(new Uint8Array(e));
const endViews = [ev.subarray(1), ev.subarray(1, undefined), ev.subarray(1, Infinity), ev.subarray(1, same(undefined))];
e.resize(8);
report("subarray ends", () => endViews.map((x) => x.length));
const eb = same(Buffer.from(e));
const bufferViews = [eb.subarray(2), eb.slice(2), eb.subarray(), eb.subarray(1, undefined)];
e.resize(12);
report("buffer subarray", () => [eb.length, bufferViews.map((x) => x.length)]);
const st = new ArrayBuffer(8, { maxByteLength: 16 });
const src = same(new Uint8Array(st, 4, 4));
const target = same(new Uint8Array(8));
st.resize(4);
report("set source out of bounds", () => target.set(src));
report("set negative offset", () => src.set(src, -1));
report("set target out of bounds", () => src.set([1]));
report("set tracking source", () => {
  target.set(same(new Uint8Array(st)), 4);
  return target.join();
});
