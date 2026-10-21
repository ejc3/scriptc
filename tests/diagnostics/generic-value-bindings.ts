// Generic function VALUE fences, named: every shape here keeps the generic
// signature alive where static monomorphization cannot follow.

// A reassigned binding cannot resolve statically — a later write could hold
// a structurally identical function with a different body.
let flip = <T>(x: T): T => x;
flip = <T>(x: T): T => x;
console.log(flip(1));

// An ALIAS binding registers its target (calls through it monomorphize),
// but a USE with no pinning context still keeps the generic signature —
// the value fences at the reference, not the binding.
const id = <T>(x: T): T => x;
const stored = id;
console.log(typeof stored);

// An alias of a REASSIGNED binding never registers: the target's own
// fence reports, and the alias falls back to the ordinary binding story.
const flipAlias = flip;
console.log(flipAlias(5));

// Declared inside a function and reading its frame: instances are module
// functions and cannot capture the enclosing frame.
function outer(): number {
  const base = 1;
  const inner = <T>(x: T): [T, number] => [x, base];
  return inner(3)[1];
}
console.log(outer());

// One that reads nothing from its frame compiles for direct calls, but a
// value of it is a distinct function each time the declaration runs.
function factory(): (x: number) => number {
  const id = <T>(x: T): T => x;
  return id<number>;
}
console.log(factory()(2));

// An ambient binding of generic function type now compiles to Node's exact
// ReferenceError at the access (the declare-erasure stance — no diagnostic
// here; the optional chain cannot guard the root's own throw).
declare const ambient: undefined | (<T>(f: (a: T) => T) => T);
console.log(ambient?.((n: number) => n) === undefined);

// A call whose RESULT is itself a generic function: the returned signature
// keeps its type parameters and would need the producing call's frame.
const curried = <T>(x: T) => <U>(y: U): string => `${x}|${y}`;
console.log(curried(1)("a"));

// Ambient `declare class` generic methods are signature-only. Through an
// AMBIENT-ROOTED receiver the call now compiles to Node's ReferenceError
// at the root (the declare-const chain stance — no diagnostic here); a
// receiver that exists at runtime keeps the fence.
declare class Amb {
  m<T>(x: T): T;
}
declare const amb: Amb;
console.log(amb.m(4));
function throughParam(a: Amb): void {
  console.log(a.m(4));
}
throughParam(amb);

// A binding inside a function whose initializer might not run before a
// use: one a closure above it can read, and a var assigned in a branch.
function readAbove(): (x: number) => number {
  const read = (): ((x: number) => number) => early;
  const early: (x: number) => number = <T>(x: T): T => x;
  return read();
}
console.log(readAbove()(7));
function inBranch(flag: boolean): number {
  if (flag) {
    var maybe = <T>(x: T): T => x;
  }
  const invoke = (): number => maybe(1);
  return invoke();
}
console.log(inBranch(true));
function hoisted(flag: boolean): number {
  if (flag) return helper();
  const later: (x: number) => number = <T>(x: T): T => x;
  return helper();
  function helper(): number {
    return later(1);
  }
}
console.log(hoisted(false));
class HeritageBase {}
function heritage(n: number): typeof HeritageBase {
  console.log(n);
  return HeritageBase;
}
function inHeritage(): void {
  const before = (): void => {
    class Derived extends heritage(first(1)) {}
    new Derived();
  };
  const first: (x: number) => number = <T>(x: T): T => x;
  before();
}
inHeritage();
