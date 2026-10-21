// Errors held as any share one prototype per error class, as in Node: two
// errors of one class have the same prototype, which holds the class's name
// and an empty message and chains to Error's, and assert.deepStrictEqual of
// two equal errors passes.
import assert from "node:assert";

function dyn(value: unknown): any {
  return value;
}

const first = dyn(new TypeError("same"));
const second = dyn(new TypeError("same"));
const plain = dyn(new Error("plain"));
const prototype = Object.getPrototypeOf(first);
console.log("same class", prototype === Object.getPrototypeOf(second));
console.log("other class", prototype === Object.getPrototypeOf(plain));
console.log("chain", Object.getPrototypeOf(prototype) === Object.getPrototypeOf(plain));
console.log("base", Object.getPrototypeOf(Object.getPrototypeOf(plain)) === Object.prototype);
console.log("fields", prototype.name, JSON.stringify(prototype.message), Object.keys(prototype).length);
console.log("own", Object.getOwnPropertyNames(first).join(","), first.name, first.message);

for (const [label, a, b] of [
  ["equal", first, second],
  ["other message", first, dyn(new TypeError("other"))],
  ["other class", first, dyn(new RangeError("same"))],
] as [string, any, any][]) {
  try {
    assert.deepStrictEqual(a, b);
    console.log(label, "deepStrictEqual passes");
  } catch (error) {
    console.log(label, "deepStrictEqual throws", (error as Error).name);
  }
}

const empty = dyn(new RangeError());
console.log("no message", Object.getPrototypeOf(empty) === Object.getPrototypeOf(dyn(new RangeError("x"))), JSON.stringify(empty.message), "message" in empty, Object.hasOwn(empty, "message"));

const renamed = new Error("renamed");
renamed.name = "Custom";
const held = dyn(renamed);
console.log("own name", Object.getPrototypeOf(held) === Object.getPrototypeOf(plain), held.name, String(held), Object.getPrototypeOf(held).name);

try {
  JSON.parse("{");
} catch (error) {
  const caught = dyn(error);
  console.log("caught", Object.getPrototypeOf(caught) === Object.getPrototypeOf(dyn(new SyntaxError("s"))), caught.name);
}
