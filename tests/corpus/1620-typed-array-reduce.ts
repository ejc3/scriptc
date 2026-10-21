// reduce and reduceRight on typed arrays: with and without an initial
// value, over several element kinds, with callbacks that read the index and
// the array, accumulators of another type, elements written during the
// walk (each step reads the element live), and the empty-array TypeError.
const values = new Float64Array([1, 2, 3.5]);
console.log(values.reduce((sum, x) => sum + x, 0), values.reduce((sum, x) => sum + x));
console.log(values.reduceRight((acc, x) => acc * 10 + x, 0), values.reduceRight((acc, x) => acc - x));

const ints = new Int32Array([5, -3, 7, 2]);
console.log(ints.reduce((best, x) => (x > best ? x : best)), ints.reduce((acc, x, i) => acc + x * i, 0));

const bytes = new Uint8Array([250, 4, 9]);
console.log(bytes.reduce((acc, x) => acc + x, 0), bytes.reduce((acc, x) => (acc + x) & 0xff));

const halves = new Float32Array([0.5, 0.25, 0.125]);
console.log(halves.reduce((acc, x, i, all) => acc + x * all.length + i, 0));

const text = ints.reduce((acc, x, i) => `${acc}${i ? "," : ""}${x}`, "");
const stats = values.reduce(
  (acc, x) => ({ count: acc.count + 1, max: Math.max(acc.max, x) }),
  { count: 0, max: -Infinity },
);
console.log(text, stats.count, stats.max);

// A later element written by the callback is the one the walk then reads.
const live = new Int16Array([1, 1, 1, 1]);
console.log(
  live.reduce((acc, x, i, all) => {
    if (i + 1 < all.length) all[i + 1] = acc + x;
    return acc + x;
  }, 0),
  Array.from(live).join(","),
);
const back = new Uint16Array([1, 2, 3]);
console.log(
  back.reduceRight((acc, x, i, all) => {
    if (i > 0) all[i - 1] = x * 10;
    return acc + x;
  }, 0),
  Array.from(back).join(","),
);

const one = new Float64Array([42]);
console.log(one.reduce((acc, x) => acc + x), one.reduceRight((acc, x) => acc + x, 1));

function emptyFloats(empty: Float64Array): void {
  try {
    console.log(empty.reduce((acc, x) => acc + x));
  } catch (error) {
    console.log((error as Error).name, (error as Error).message);
  }
  console.log(empty.reduce((acc, x) => acc + x, 7));
}
function emptyBytes(empty: Uint8Array): void {
  try {
    console.log(empty.reduceRight((acc, x) => acc + x));
  } catch (error) {
    console.log((error as Error).name, (error as Error).message);
  }
  console.log(empty.reduceRight((acc, x) => acc + x, 8));
}
emptyFloats(new Float64Array(0));
emptyBytes(new Uint8Array(0));

// A callback that returns a missing value makes the accumulator undefined,
// which later steps receive and the call returns, in both directions.
const missing: string[] = [];
const noNumbers: number[] = [];
const lost = new Uint8Array([1]).reduce((_acc, _value) => missing[0], "");
const lostBack = new Uint8Array([1]).reduceRight((_acc, _value) => missing[0], "");
console.log(typeof lost, lost, typeof lostBack, lostBack);
const carried = new Uint8Array([1, 2]).reduce((acc, x) => (x === 1 ? missing[0] : acc + "!"), "");
const seeded = new Float64Array([1, 2, 3]).reduce((acc, x) => (x === 2 ? noNumbers[9] : acc + x));
const seededBack = new Int32Array([1, 2, 3]).reduceRight((acc, x) => (x === 2 ? noNumbers[9] : acc + x));
console.log(carried, seeded, seededBack);
