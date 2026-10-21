import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterAll, expect, test } from "vitest";
import { nodeFrontendServices } from "./services-node.js";

const dirs: string[] = [];
afterAll(() => {
  for (const dir of dirs) rmSync(dir, { recursive: true, force: true });
});

function rewrite(source: string): string | null {
  return nodeFrontendServices().exportsThis(source, "/pkg/index.js");
}

function expectSameBehavior(source: string, use: string): string {
  const result = rewrite(source);
  expect(typeof result).toBe("string");
  const dir = mkdtempSync(join(tmpdir(), "scriptc-exports-this-"));
  dirs.push(dir);
  writeFileSync(join(dir, "original.cjs"), source);
  writeFileSync(join(dir, "rewritten.cjs"), result!);
  const run = (file: string) => {
    writeFileSync(join(dir, "use.cjs"), use.replace("LIB", `./${file}`));
    return spawnSync(process.execPath, [join(dir, "use.cjs")], { encoding: "utf8" });
  };
  const oracle = run("original.cjs");
  expect(oracle.status).toBe(0);
  expect(run("rewritten.cjs").stdout).toBe(oracle.stdout);
  return result!;
}

test("exported functions reach sibling exports through `this`", () => {
  const out = expectSameBehavior(
    `'use strict';
exports.operation = function (o) { return exports.timeouts(o).length; };
exports.timeouts = function (o) {
  var t = [];
  for (var i = 0; i < o.retries; i++) t.push(this.createTimeout(i));
  [1].forEach(() => t.push(this.createTimeout(9)));
  return t;
};
exports.createTimeout = function (i) { return 10 * Math.pow(2, i); };
`,
    `const lib = require("LIB"); console.log(lib.timeouts({ retries: 3 }).join(","), lib.operation({ retries: 2 }));`,
  );
  expect(out).toContain("exports.createTimeout(i)");
});

test("other uses of `this`, replaced exports and detached functions keep their text", () => {
  // `this` as a value.
  expect(rewrite(`exports.f = function () { return this; };\nexports.g = 1;\n`)).toBeNull();
  // A member the module does not export.
  expect(rewrite(`exports.f = function () { return this.missing; };\n`)).toBeNull();
  // The exports object replaced.
  expect(
    rewrite(
      `exports.f = function () { return this.g(); };\nexports.g = function () { return 1; };\nmodule.exports = { f: exports.f };\n`,
    ),
  ).toBeNull();
  // The function handed elsewhere inside the module.
  expect(
    rewrite(
      `exports.f = function () { return this.g(); };\nexports.g = function () { return 1; };\nsetTimeout(exports.f, 0);\n`,
    ),
  ).toBeNull();
});

test("a function another export detaches through `this` keeps its text", () => {
  // `a` reads `this.b` as a value and calls it without a receiver, so `b`
  // runs with the global `this`, where `c` is missing.
  const out = expectSameBehavior(
    `exports.a = function () {
  var g = this.b;
  try { return g(); } catch (e) { return e.constructor.name; }
};
exports.b = function () { return this.c(); };
exports.c = function () { return "c"; };
`,
    `const lib = require("LIB"); console.log(lib.a(), lib.b());`,
  );
  expect(out).toContain("var g = exports.b;");
  expect(out).toContain("return this.c();");
});

test("a function that declares its own `exports` binding keeps its text", () => {
  // A parameter, a var, an arrow parameter, the function name and a catch
  // binding each shadow the module's exports object.
  for (const fn of [
    `function (exports) { return this.g(); }`,
    `function () { var exports = { g: () => "local" }; return this.g(); }`,
    `function () { return [1].map((exports) => this.g())[0]; }`,
    `function exports() { return this.g(); }`,
    `function () { try { throw 0; } catch (exports) { return this.g(); } }`,
  ]) {
    expect(
      rewrite(`exports.f = ${fn};\nexports.g = function () { return "g"; };\n`),
      fn,
    ).toBeNull();
  }
  // A module-scope binding named exports is not the exports object.
  expect(
    rewrite(
      `var exports = module.exports;\nexports.f = function () { return this.g(); };\nexports.g = function () { return "g"; };\n`,
    ),
  ).toBeNull();
  // The shadowed shapes run as Node runs them.
  expectSameBehavior(
    `exports.f = function (exports) { return this.g(); };
exports.g = function () { return "g"; };
exports.h = function () { return this.g(); };
`,
    `const lib = require("LIB"); console.log(lib.f({ g: () => "shadow" }), lib.h());`,
  );
});
