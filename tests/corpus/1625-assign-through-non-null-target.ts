// Assignments, compound assignments and updates whose element or member
// target is wrapped in a non-null assertion or parentheses write that
// element or member, exactly as the unwrapped target would.
const totals = { sums: [1, 2, 3] };
const k = 1;
totals.sums[k]! += 10;
totals.sums[0]! = 7;
(totals.sums[2]) *= 2;
totals.sums[k]!++;
--totals.sums[0]!;
console.log(totals.sums.join(","));

const values = new Float64Array([0.5, 1.5]);
values[1]! -= 1;
(values[0])! += 2;
values[0]!++;
console.log(Array.from(values).join(","));

const counts: Record<string, number> = { a: 1 };
counts["a"]! += 4;
counts["b"] = 0;
counts["b"]! += 2;
console.log(counts["a"], counts["b"]);

interface Box {
  n?: number;
  label: string;
}
const box: Box = { n: 1, label: "x" };
box.n! += 5;
box.label! += "y";
(box.n)! *= 3;
box.n!--;
console.log(box.n, box.label);

const grid: number[][] = [
  [1, 2],
  [3, 4],
];
grid[1]![0]! += 10;
grid[0]![1]!++;
console.log(JSON.stringify(grid));

// A function's expando member is a module global.
function counter(): number {
  return 0;
}
counter.hits = 1;
counter.hits! *= 3;
console.log(counter.hits, counter());

// A missing element reads as undefined, so the compound stores NaN.
const short: number[] = [1];
short[3]! += 1;
console.log(short.length, short[3], 2 in short);

// The reference is evaluated once, before the right-hand side.
let reads = 0;
const at = (): number => {
  reads++;
  return 1;
};
const list = [10, 20, 30];
list[at()]! += reads * 100;
console.log(list.join(","), reads);
