// Generic arrow and function-expression bindings declared inside
// functions, methods and arrows compile like module-scope ones when they
// need nothing from the enclosing frame: their own parameters and locals,
// module-scope bindings, globals and their own name.
type Point = { x: number; y: number };
const SEPARATOR = "|";
type Pick1 = <T>(xs: T[]) => T;

function lists(): void {
  const put = <T>(xs: T[], x: T): T[] => [...xs, x];
  console.log(put([1, 2], 3).join(","), put(["a"], "b").join(","));
  const points = put<Point>([{ x: 1, y: 2 }], { x: 3, y: 4 });
  console.log(points.map((p) => `${p.x}:${p.y}`).join(SEPARATOR));
}

function nested(n: number): string {
  function inner(): string {
    const twice = function <T>(x: T): T[] {
      return [x, x];
    };
    return twice(n).join("+") + " " + twice("s").length;
  }
  return inner();
}

function recursive(): void {
  const depth = <T>(value: T, n: number): T[] => (n === 0 ? [] : [value, ...depth(value, n - 1)]);
  console.log(depth(7, 3).join(","), depth("q", 2).join(""));
}

function contextual(): void {
  const last: Pick1 = (xs) => xs[xs.length - 1]!;
  const tag = <T extends { x: number }>(p: T): string => `x=${p.x}${SEPARATOR}${Math.max(p.x, 0)}`;
  console.log(last([1, 2, 3]), last(["u", "v"]), tag({ x: -2, y: 5 }));
}

class Shelf {
  label = "shelf";
  describe(): string {
    const wrap = <T>(value: T): { value: T } => ({ value });
    return `${this.label} ${wrap(4).value} ${wrap("w").value}`;
  }
}

const arrowHost = (): string => {
  const pairOf = <A, B>(a: A, b: B): [A, B] => [a, b];
  const [k, v] = pairOf("k", 9);
  return `${k}=${v}`;
};

// Two functions declare the same local name, and a local shadows a
// module-scope generic binding of that name: each is its own function.
const same = <T>(x: T): T[] => [x, x];
function first(): string {
  const same = <T>(x: T): T => x;
  return `${same(1)} ${same("a")}`;
}
function second(): string {
  const same = <T>(x: T): string => `<${String(x)}>`;
  return `${same(2)} ${same(true)}`;
}

lists();
console.log(first(), second(), same(3).join(","), same("b").length);
console.log(nested(5));
recursive();
contextual();
console.log(new Shelf().describe(), arrowHost());

// A type alias of the same name is not a use of the binding.
function aliased(): number {
  type same = number;
  const same = <T>(x: T): T => x;
  const n: same = same(5);
  return n;
}
console.log(aliased());
