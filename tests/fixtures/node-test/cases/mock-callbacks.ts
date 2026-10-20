// A mock passed where a function is expected is called as the function,
// as in Node: timer, immediate, nextTick and queueMicrotask callbacks,
// EventEmitter listeners, array callbacks, promise handlers, and callback
// parameters and function-typed variables of the program's own functions.
// Each call is recorded on the mock. A mock.fn() mock without an original
// receives every argument the caller passes.
import assert from "node:assert";
import { EventEmitter } from "node:events";
import { mock, test } from "node:test";

const tick = (): Promise<void> => new Promise((resolve) => setTimeout(resolve, 5));

test("scheduled callbacks", async () => {
  const timer = mock.fn((): void => {});
  const immediate = mock.fn((): void => {});
  const next = mock.fn((): void => {});
  const micro = mock.fn((): void => {});
  setTimeout(timer, 1);
  setImmediate(immediate);
  process.nextTick(next);
  queueMicrotask(micro);
  await tick();
  assert.strictEqual(timer.mock.callCount(), 1);
  assert.strictEqual(immediate.mock.callCount(), 1);
  assert.strictEqual(next.mock.callCount(), 1);
  assert.strictEqual(micro.mock.callCount(), 1);
});

test("EventEmitter listeners", () => {
  const listener = mock.fn((name: string, n: number): void => {});
  const e = new EventEmitter();
  e.on("ping", listener);
  e.emit("ping", "a", 1);
  e.emit("ping", "b", 2);
  e.off("ping", listener);
  e.emit("ping", "c", 3);
  assert.strictEqual(listener.mock.callCount(), 2);
  assert.strictEqual(listener.mock.calls[1]!.arguments[0], "b");
  assert.strictEqual(listener.mock.calls[1]!.arguments[1], 2);
});

test("array callbacks", () => {
  const each = mock.fn((n: number): void => {});
  [10, 20, 30].forEach(each);
  assert.strictEqual(each.mock.callCount(), 3);
  assert.strictEqual(each.mock.calls[2]!.arguments[0], 30);
  const double = mock.fn((n: number): number => n * 2);
  assert.deepStrictEqual([1, 2].map(double), [2, 4]);
  const even = mock.fn((n: number): boolean => n % 2 === 0);
  assert.deepStrictEqual([1, 2, 3, 4].filter(even), [2, 4]);
  assert.strictEqual(even.mock.callCount(), 4);
});

test("promise handlers", async () => {
  const onValue = mock.fn((v: number): number => v + 1);
  const result = await Promise.resolve(41).then(onValue);
  assert.strictEqual(result, 42);
  assert.strictEqual(onValue.mock.calls[0]!.arguments[0], 41);
});

function run(cb: (x: string) => number, value: string): number {
  return cb(value);
}

function maybe(cb?: () => void): void {
  if (cb) cb();
}

test("callback parameters and function-typed variables", () => {
  const len = mock.fn((s: string): number => s.length);
  assert.strictEqual(run(len, "four"), 4);
  const hook = mock.fn((): void => {});
  maybe(hook);
  maybe();
  const stored: (x: string) => number = len;
  assert.strictEqual(stored("ab"), 2);
  assert.strictEqual(len.mock.callCount(), 2);
  assert.strictEqual(hook.mock.callCount(), 1);
});

test("mock.fn() without an original as a callback", async () => {
  const listener = mock.fn();
  const e = new EventEmitter();
  e.on("ping", listener);
  e.emit("ping", "a", 2);
  assert.deepStrictEqual(listener.mock.calls[0]!.arguments, ["a", 2]);
  const timer = mock.fn();
  setTimeout(timer, 1);
  await tick();
  assert.strictEqual(timer.mock.callCount(), 1);
  const each = mock.fn();
  [10, 20].forEach(each);
  assert.deepStrictEqual(each.mock.calls[1]!.arguments, [20, 1, [10, 20]]);
  const pred = mock.fn();
  assert.ok([1, 2].find(pred) === undefined);
  assert.strictEqual(pred.mock.callCount(), 2);
  const onValue = mock.fn();
  await Promise.resolve(7).then(onValue);
  assert.deepStrictEqual(onValue.mock.calls[0]!.arguments, [7]);
  const boom = mock.fn();
  boom.mock.mockImplementation(() => {
    throw new Error("boom");
  });
  assert.throws(boom, { message: "boom" });
  assert.strictEqual(boom.mock.callCount(), 1);
});
