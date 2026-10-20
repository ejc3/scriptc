// Buffer and typed-array members on untyped values, edge cases: method
// identity across Buffer and Uint8Array, names and lengths, the parent and
// offset getters, inspect, slice aliasing, method values called on the
// other flavor, wider copy targets and fill patterns, the end argument of
// indexOf/lastIndexOf, entries during mutation, comparator sorts, and
// typed-array members on other element kinds.
"use strict";

function t(label, run) {
  try {
    const r = run();
    const text = typeof r === "bigint" ? `${r}n`
      : Buffer.isBuffer(r) ? `Buffer<${r.toString("hex")}>`
      : ArrayBuffer.isView(r) ? `${r.constructor.name}[${Array.from(r).join(",")}]`
      : typeof r === "object" && r !== null ? JSON.stringify(r) : String(r);
    console.log(label, text);
  } catch (e) {
    console.log(label, "threw", e.name, e.code, e.message);
  }
}

function identity(b, u) {
  t("same map", () => b.map === u.map);
  t("same at", () => b.at === u.at);
  t("own toString", () => b.toString === u.toString);
  t("own fill", () => b.fill === u.fill);
  t("own slice", () => b.slice === u.slice);
  t("lengths", () => [b.equals.length, b.write.length, u.map.length, b.readUIntLE.length, b.writeIntBE.length, b.toString.length].join());
  t("names", () => [b.equals.name, b.readFloatLE.name, b.writeUint8.name, b.readBigUint64LE.name, u.toSorted.name, b.toLocaleString.name].join());
  t("parent", () => b.parent === b.buffer);
  t("offset", () => typeof b.offset);
  t("u8 parent", () => typeof u.parent);
  t("u8 equals", () => typeof u.equals);
}

function views(b, u, long, empty) {
  t("map keeps Buffer", () => Buffer.isBuffer(b.map((x) => x)));
  t("toSorted answers Uint8Array", () => Buffer.isBuffer(b.toSorted()));
  t("slice keeps Buffer", () => Buffer.isBuffer(b.slice(1)));
  t("Buffer slice aliases", () => { const s = b.slice(0, 2); s[0] = 99; const r = b[0]; s[0] = 1; return r; });
  t("u8 slice copies", () => { const s = u.slice(0, 2); s[0] = 99; return u[0]; });
  t("inspect", () => b.inspect());
  t("inspect long", () => long.inspect());
  t("toJSON empty", () => empty.toJSON());
  t("toString null encoding", () => b.toString(null));
  t("toString object start", () => b.toString("hex", { valueOf() { return 1; } }));
}

function calls(b, u) {
  const map = b.map;
  t("TypedArray method on Uint8Array", () => map.call(u, (x) => x + 1));
  t("TypedArray method on Buffer", () => map.call(b, (x) => x + 1));
  t("Buffer toString on Uint8Array", () => b.toString.call(u, "hex"));
  t("Buffer equals on Uint8Array", () => b.equals.call(u, b));
  t("join through apply", () => u.join.apply(u, ["-"]));
  t("method on a number", () => map.call(5, (x) => x));
}

function copies(b, u16, four, u8empty, eight) {
  t("copy into Uint16Array", () => { b.copy(u16); return Array.from(u16).join(); });
  t("copy string index", () => b.copy(four, "1"));
  t("copy Infinity", () => b.copy(four, Infinity));
  t("compare targetStart only", () => b.compare(b, 1));
  t("compare undefined ranges", () => b.compare(b, undefined, undefined, undefined, undefined));
  t("compare sourceStart past", () => b.compare(b, 0, 1, 9));
  t("fill Uint16Array pattern", () => four.fill(u16));
  t("fill empty Uint8Array", () => four.fill(u8empty));
  t("fill bigint", () => four.fill(1n));
  t("fill NaN", () => four.fill(NaN));
  t("fill -1", () => four.fill(-1));
  t("fill string offset end", () => four.fill("ab", 1, 3));
  t("fill string end encoding", () => four.fill("61", 1, "hex"));
  t("write hex invalid", () => eight.write("zz11", "hex"));
  t("write false encoding", () => eight.write("A", 0, 1, false));
  t("write null encoding", () => eight.write("A", 0, 1, null));
}

function searches(aaa, abab, b) {
  t("indexOf end past", () => aaa.indexOf("a", 0, 99));
  t("indexOf end negative", () => aaa.indexOf("a", 0, -1));
  t("indexOf fractional end", () => abab.indexOf("b", 0, 1.5));
  t("indexOf number end", () => abab.indexOf(0x62, 0, 1));
  t("lastIndexOf end", () => abab.lastIndexOf("b", 3, 3));
  t("lastIndexOf number end", () => abab.lastIndexOf(0x62, undefined, 2));
  t("lastIndexOf buffer", () => abab.lastIndexOf(b.subarray(0, 0)));
  t("lastIndexOf negative offset", () => abab.lastIndexOf("a", -2));
  t("lastIndexOf empty", () => abab.lastIndexOf(""));
  t("lastIndexOf empty offset", () => abab.lastIndexOf("", 1));
  t("indexOf hex", () => abab.indexOf("62", "hex"));
  t("indexOf base64", () => abab.indexOf("Yg==", 0, "base64"));
  t("indexOf boolean", () => abab.indexOf(true));
  t("indexOf null encoding", () => abab.indexOf("a", 0, undefined, null));
}

function numbers(big, f32) {
  t("readDoubleBE", () => big.readDoubleBE());
  t("readFloatLE", () => f32.readFloatLE(0));
  t("writeDoubleLE", () => big.writeDoubleLE(1.25));
  t("writeFloatBE NaN", () => big.writeFloatBE(NaN, 4));
  t("writeUInt32BE range", () => big.writeUInt32BE(2 ** 32, 0));
  t("writeUIntLE 6", () => big.writeUIntLE(2 ** 48 - 1, 0, 6));
  t("writeIntLE 6 range", () => big.writeIntLE(2 ** 47, 0, 6));
  t("readIntBE 6", () => big.readIntBE(0, 6));
  t("writeBigUInt64LE negative", () => big.writeBigUInt64LE(-1n));
  t("readBigInt64LE", () => big.readBigInt64LE());
  t("writeInt32LE string", () => big.writeInt32LE("7", 0));
  t("writeInt8 bigint", () => big.writeInt8(1n, 0));
  t("writeUInt8 object", () => big.writeUInt8({ valueOf() { return 7; } }, 0));
  t("readUInt16LE null", () => big.readUInt16LE(null));
  t("readUInt8 undefined", () => big.readUInt8(undefined));
}

function typed(u, c312, i8, f32s, u16) {
  t("entries during mutation", () => { const out = []; for (const [i, v] of c312.entries()) { out.push(v); if (i === 0) c312[1] = 9; } c312[1] = 1; return out.join(); });
  t("values iterator", () => { const it = u.values(); it.next(); return it.next().value; });
  t("sort comparator throws", () => { try { c312.sort((x) => { if (x) throw new Error("x"); return 0; }); } catch (e) { return `${e.message}:${Array.from(c312).join(",")}`; } });
  t("sort comparator not callable", () => u.sort("x"));
  t("sort comparator order", () => Array.from(i8.sort((a, b) => a - b)).join());
  t("sort float default", () => Array.from(f32s.sort()).join());
  t("toSorted comparator", () => u.toSorted((a, b) => b - a));
  t("with float", () => f32s.with(0, 0.1)[0]);
  t("with negative", () => u.with(-1, 7));
  t("set", () => { c312.set([5, 6], 1); return Array.from(c312).join(); });
  t("set out of bounds", () => c312.set([1, 2], 2));
  t("at string", () => u.at("1"));
  t("copyWithin", () => c312.copyWithin(0, 2));
  t("fill", () => c312.fill(7, 1));
  t("join undefined", () => u.join(undefined));
  t("find thisArg", () => u.find(function (v) { return v === this.x; }, { x: 2 }));
  t("every this", () => u.every(function () { return this === undefined; }));
  t("reduce initial", () => u.reduce((a, v, i) => a + v * i, 0));
  t("reduceRight", () => u.reduceRight((a, v) => a + "," + v));
  t("map coercion", () => u.map((v) => String(v * 3)));
  t("filter none", () => u.filter(() => false));
  t("Uint16 map", () => u16.map((x) => x * 2));
  t("Uint16 toString", () => u16.toString());
  t("Uint16 toLocaleString", () => u16.toLocaleString());
  t("Uint16 lastIndexOf", () => u16.lastIndexOf(2));
  t("u8 includes negative", () => u.includes(3, -1));
  t("u8 lastIndexOf before start", () => u.lastIndexOf(1, -10));
  t("u8 indexOf string", () => u.indexOf("1"));
}

const b = Buffer.from([1, 2, 3]);
const u = new Uint8Array([1, 2, 3]);
identity(b, u);
views(b, u, Buffer.alloc(52), Buffer.alloc(0));
calls(b, u);
copies(b, new Uint16Array(2), Buffer.alloc(5), new Uint8Array(0), Buffer.alloc(8));
searches(Buffer.from("aaa"), Buffer.from("abab"), b);
numbers(Buffer.alloc(8), Buffer.from([0, 0, 0xc0, 0x3f]));
typed(u, new Uint8Array([3, 1, 2]), new Int8Array([5, -1, 3, -7]), new Float32Array([2, -0, 0, NaN, -1]), new Uint16Array([1, 2, 300]));
