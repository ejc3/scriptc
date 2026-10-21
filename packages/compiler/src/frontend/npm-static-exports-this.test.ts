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
    rewrite(`exports.f = function () { return this.g(); };\nexports.g = function () { return 1; };\nmodule.exports = { f: exports.f };\n`),
  ).toBeNull();
  // The function handed elsewhere inside the module.
  expect(
    rewrite(`exports.f = function () { return this.g(); };\nexports.g = function () { return 1; };\nsetTimeout(exports.f, 0);\n`),
  ).toBeNull();
});
