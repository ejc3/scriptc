// %TypedArray%.prototype members on untyped Buffers and Uint8Arrays:
// the callback methods (with thisArg), map and filter keeping the
// receiver's kind, includes/indexOf/lastIndexOf on a plain typed array,
// reverse, sort with and without a comparator, toReversed, toSorted and
// with (which answer a Uint8Array), keys/values/entries iterators, join,
// toString and toLocaleString, and the members of other element kinds.
"use strict";

const fmt = (v) =>
  Buffer.isBuffer(v) ? `Buffer[${Array.from(v).join(",")}]`
    : ArrayBuffer.isView(v) ? `${v.constructor.name}[${Array.from(v).join(",")}]`
    : Array.isArray(v) ? `[${v.map(fmt).join(",")}]` : String(v);

function callbacks(a, empty) {
  const seen = [];
  a.forEach(function (v, i, arr) { seen.push(`${v}@${i}:${arr === a}:${this.tag}`); }, { tag: "t" });
  console.log("forEach", seen.join(" "));
  console.log("map", fmt(a.map((v, i) => v * 2 + i)));
  console.log("map wraps", fmt(a.map((v) => v + 250)));
  console.log("filter", fmt(a.filter((v) => v % 2 === 1)));
  console.log("some/every", a.some((v) => v > 4), a.every((v) => v > 0), a.every((v) => v > 1));
  console.log("find", a.find((v) => v > 2), a.findIndex((v) => v > 2), a.findLast((v) => v < 4), a.findLastIndex((v) => v < 4));
  console.log("find missing", a.find((v) => v > 99), a.findIndex((v) => v > 99));
  console.log("reduce", a.reduce((s, v) => s + v), a.reduce((s, v) => s + v, 100), a.reduceRight((s, v) => s + "," + v, ""));
  try { a.forEach(5); } catch (e) { console.log("forEach not callable", e.name, e.message); }
  try { empty.reduce((s, v) => s + v); } catch (e) { console.log("reduce empty", e.name, e.message); }
}

function searches(a) {
  console.log("includes", a.includes(3), a.includes(9), a.includes(3, 3));
  console.log("indexOf", a.indexOf(3), a.indexOf(3, 3), a.indexOf(9), a.lastIndexOf(1), a.lastIndexOf(1, 1));
  console.log("at", a.at(0), a.at(-1), a.at(99));
}

function reorder(a) {
  console.log("toReversed", fmt(a.toReversed()), fmt(a));
  console.log("toSorted", fmt(a.toSorted()), fmt(a.toSorted((x, y) => y - x)));
  console.log("with", fmt(a.with(0, 42)), fmt(a.with(-1, 300)));
  try { a.with(99, 1); } catch (e) { console.log("with range", e.name, e.message); }
  console.log("reverse", fmt(a.reverse()), fmt(a));
  console.log("sort", fmt(a.sort()), fmt(a.sort((x, y) => y - x)));
}

function iterate(a) {
  console.log("keys", [...a.keys()].join(","), "values", [...a.values()].join(","));
  console.log("entries", [...a.entries()].map(([i, v]) => `${i}=${v}`).join(","));
  console.log("spread", [...a].join(","));
  console.log("join", a.join(), a.join("-"), "toString", a.toString(), "toLocaleString", a.toLocaleString());
  console.log("typeof", typeof a.map, typeof a.entries, typeof a.toSorted, typeof a.equals);
}

const buf = Buffer.from([3, 1, 4, 1, 5]);
callbacks(buf, Buffer.alloc(0));
searches(buf);
iterate(Buffer.from([55, 56]));
reorder(Buffer.from([3, 1, 2]));

const u8 = new Uint8Array([3, 1, 4, 1, 5]);
callbacks(u8, new Uint8Array(0));
searches(u8);
iterate(new Uint8Array([7, 8]));
reorder(new Uint8Array([3, 1, 2]));

function otherKinds(i16, f64) {
  console.log("Int16 map", fmt(i16.map((v) => v * 1000)), "filter", fmt(i16.filter((v) => v > 0)));
  console.log("Int16 sort", fmt(i16.toSorted()), "includes", i16.includes(-3), "indexOf", i16.indexOf(2));
  console.log("Float64 includes NaN", f64.includes(NaN), "indexOf NaN", f64.indexOf(NaN), "sort", fmt(f64.toSorted()));
  console.log("Float64 toLocaleString", f64.toLocaleString(), "join", f64.join(" "));
}

otherKinds(new Int16Array([-3, 10, 2]), new Float64Array([1.5, NaN, -0, 1234.5678]));
