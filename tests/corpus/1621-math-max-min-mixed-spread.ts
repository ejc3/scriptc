// Math.max and Math.min with spread and positional arguments mixed: every
// argument evaluates left to right, then one fold over all of them, with
// NaN, -0 and empty spreads as in Node.
const xs = [3, 1, 4];
const empty: number[] = [];
const log: string[] = [];
const at = (s: string, v: number): number => {
  log.push(s);
  return v;
};
const list = (s: string, v: number[]): number[] => {
  log.push(s);
  return v;
};
console.log(Math.max(...xs.map((x) => x * 2), 5), Math.min(...xs, 2), Math.min(0, ...xs, -1, ...empty));
console.log(Math.max(...empty, -0), Object.is(Math.max(-0, ...[0]), 0), Object.is(Math.min(...[0], -0), -0));
console.log(Math.max(...xs, NaN), Math.min(...[NaN], 1), Math.max(...empty, ...empty));
console.log(Math.max(at("a", 1), ...list("b", [7]), at("c", 2)), log.join(""));

// The shape of a floor over a filtered list.
const points = [
  { minute: 470, ok: 0.97 },
  { minute: 475, ok: 0.99 },
  { minute: 480, ok: 0.9 },
];
const floor = 460;
const latest = Math.max(...points.filter((p) => p.ok >= 0.95).map((p) => p.minute), floor);
const none = Math.max(...points.filter((p) => p.ok > 1).map((p) => p.minute), floor);
console.log(latest, none);
