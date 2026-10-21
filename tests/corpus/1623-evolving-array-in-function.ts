// An unannotated `[]` declared inside a function and filled by pushes
// (TypeScript's evolving array) holds the element type its pushes build.
interface Node {
  id: number;
  tag: string | null;
}

function records(raw: number[]): { id: number; name: string }[] {
  const rows = [];
  for (let i = 0; i < raw.length; i++) rows.push({ id: raw[i]!, name: "n" + i });
  return rows;
}

function tagged(raw: number[]): Node[] {
  const nodes = [];
  for (const id of raw) {
    nodes.push({ id, tag: id > 1 ? "big" : null });
  }
  return nodes;
}

function shorthand(raw: number[]): { nodes: Node[]; count: number } {
  const nodes = [];
  for (const id of raw) nodes.push({ id, tag: null });
  const count = nodes.length;
  return { nodes, count };
}

function words(n: number): string {
  const parts = [];
  for (let i = 0; i < n; i++) parts.push("w" + i);
  return parts.join(" ");
}

function total(n: number): number {
  const xs = [];
  for (let i = 1; i <= n; i++) xs.push(i * i);
  let sum = 0;
  for (const x of xs) sum += x;
  return sum;
}

const pairs = (keys: string[]): string => {
  const out = [];
  for (const key of keys) out.push({ key, size: key.length });
  return out.map((p) => `${p.key}=${p.size}`).join(",");
};

console.log(
  records([4, 5])
    .map((r) => `${r.id}:${r.name}`)
    .join(","),
);
const ns = tagged([1, 2, 3]);
console.log(ns.length, ns[0]!.tag, ns[1]!.tag, ns[2]!.id);
const s = shorthand([7, 8]);
console.log(s.count, s.nodes[1]!.id, s.nodes[0]!.tag);
console.log(words(3), total(4), pairs(["a", "bcd"]));

// A let that is assigned another array keeps that array's identity.
function same(a: unknown, b: unknown): boolean {
  return Object.is(a, b);
}
function reassigned(): number {
  let xs = [];
  const wide = [{ x: 1, y: 2 }];
  console.log(same((xs = wide), wide));
  xs = [];
  xs.push({ x: 3 });
  return xs.length;
}
console.log(reassigned());
