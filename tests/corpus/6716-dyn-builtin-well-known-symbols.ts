// Well-known symbols on builtin values held as `any` answer as in Node.
// `in` finds Symbol.toStringTag on Map, Set, typed arrays, Promise,
// ArrayBuffer, DataView, iterators, generators, EventTarget and
// AbortController, Symbol.toPrimitive on Dates and Symbol.hasInstance on
// functions, all inherited from their prototypes, and reading
// Symbol.toStringTag answers the tag's name. Async, generator and async
// generator functions carry their kind's tag. A DataView is not iterable,
// and Object.prototype.toString uses an object's own or inherited string
// Symbol.toStringTag.

function dyn(value: unknown): any {
  return value;
}

async function* agen() { yield 1; }
function* gen() { yield 1; }
const values: [string, any][] = [
  ["array", dyn([1])], ["string", dyn("s")], ["map", dyn(new Map())], ["set", dyn(new Set())],
  ["u8", dyn(new Uint8Array(1))], ["promise", dyn(Promise.resolve(1))], ["gen", dyn(gen())],
  ["agen", dyn(agen())], ["object", dyn({ a: 1 })], ["date", dyn(new Date(0))], ["regex", dyn(/x/)],
  ["fn", dyn(() => 1)], ["error", dyn(new Error("e"))],
];
const symbols: [string, symbol][] = [
  ["iterator", Symbol.iterator], ["asyncIterator", Symbol.asyncIterator], ["toStringTag", Symbol.toStringTag],
  ["toPrimitive", Symbol.toPrimitive], ["hasInstance", Symbol.hasInstance],
];
for (const [label, v] of values) {
  const row: string[] = [];
  for (const [name, s] of symbols) {
    const value: any = v;
    row.push(`${name}:${typeof value === "object" || typeof value === "function" ? s in value : "-"}`);
  }
  console.log(label, row.join(" "));
}

class Plain { x = 1; }
const more: [string, any][] = [
  ["dataview", dyn(new DataView(new ArrayBuffer(2)))], ["arraybuffer", dyn(new ArrayBuffer(2))],
  ["map iterator", dyn(new Map([[1, 2]]).keys())], ["array iterator", dyn([1].values())],
  ["eventtarget", dyn(new EventTarget())], ["abort", dyn(new AbortController())], ["weakmap", dyn(new WeakMap())],
  ["class", dyn(new Plain())], ["parsed", dyn(JSON.parse("{}"))],
  ["own tag", dyn({ [Symbol.toStringTag]: "Mine" })], ["buffer", dyn(Buffer.from("a"))],
];
for (const [label, v] of more) {
  console.log(label, Symbol.toStringTag in v, Symbol.iterator in v, Symbol.toPrimitive in v, Object.prototype.toString.call(v));
  console.log(label, "tag", String(v[Symbol.toStringTag]));
}
for (const [label, v] of values) console.log(label, "tag", String(v[Symbol.toStringTag]));

async function asyncNamed(): Promise<number> {
  return 1;
}
const functions: [string, any][] = [
  ["async arrow", dyn(async () => 1)], ["async function", dyn(asyncNamed)], ["generator function", dyn(gen)],
  ["async generator function", dyn(agen)], ["arrow", dyn(() => 1)],
];
for (const [label, f] of functions) {
  console.log(label, Symbol.toStringTag in f, Object.prototype.toString.call(f), String(f[Symbol.toStringTag]), Object.hasOwn(f, Symbol.toStringTag));
}

const view = dyn(new DataView(new ArrayBuffer(2)));
try {
  for (const x of view) console.log(x);
} catch (error) {
  console.log("for-of", String(error));
}
console.log([...dyn(new Uint8Array([1, 2]))].join(","));
