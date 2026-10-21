// A for-of over a number[] the compiler cannot prove free of holes binds
// a number that may be undefined. That number indexes typed-array writes,
// compound element assignments, element assignments used as values and
// element targets of destructuring assignment, as it indexes reads. (An
// index that is undefined at run time is a named fence: errors.test.ts.)
class Slots {
  private items: number[] = [];
  add(value: number): void {
    const i = this.items.length;
    this.items.push(value);
    this.items[i] = value;
  }
  all(): number[] {
    return this.items;
  }
}
const slots = new Slots();
for (const value of [2, 0, 1]) slots.add(value);

const cost = new Float64Array(4).fill(Infinity);
const counts = new Int32Array(4);
const seen: number[] = [0, 0, 0, 0];
let last = 0;
for (const i of slots.all()) {
  cost[i] = i * 1.5;
  counts[i] += 2;
  counts[i]++;
  last = cost[i] = i + 0.25;
  [seen[i]] = [i * 10];
}
console.log(Array.from(cost).join(","), Array.from(counts).join(","), seen.join(","), last);
