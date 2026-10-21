/* A CommonJS export of a variable copies its value at the export
 * statement. The lowering reads the variable itself instead, which is
 * exact only when the value is settled above the export and never written
 * again (tests/corpus/6484-cjs-settled-exports runs that shape against
 * Node). A write that can run later keeps the located fence: here a
 * function's write, and top-level code below the export.
 */
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterAll, expect, test } from "vitest";
import { analyze } from "../src/index.js";

const scratch = mkdtempSync(join(tmpdir(), "scriptc-cjs-settled-"));
afterAll(() => rmSync(scratch, { recursive: true, force: true }));

function fences(files: Record<string, string>, match = "by reference"): string[] {
  const dir = mkdtempSync(join(scratch, "case-"));
  writeFileSync(join(dir, "package.json"), '{ "private": true }\n');
  for (const [name, text] of Object.entries(files)) writeFileSync(join(dir, name), text);
  const { coverage } = analyze(join(dir, "main.cjs"));
  expect(coverage.preflightFailed).toBe(false);
  return [...coverage.diagnostics, ...(coverage.runtimeFences ?? [])]
    .filter((d) => d.message.includes(match))
    .map((d) => `${d.loc.file.slice(dir.length + 1)}: ${d.message.split(" (")[0]}`);
}

test("exports of variables written later keep their fence", () => {
  expect(
    fences({
      "main.cjs": `"use strict";
const counter = require("./counter.cjs");
const handler = require("./handler.cjs");
console.log(counter.count, handler());
`,
      "counter.cjs": `"use strict";
let count = 0;
function bump() {
  count++;
}
module.exports = { count, bump };
`,
      "handler.cjs": `"use strict";
var handler = function () {
  return 1;
};
module.exports = handler;
handler = function () {
  return 2;
};
`,
    }),
  ).toEqual([
    "counter.cjs: exporting the mutable 'let' binding 'count' by reference",
    "handler.cjs: exporting the mutable 'let' binding 'handler' by reference",
  ]);
});

test("exports of variables settled above the export compile", () => {
  expect(
    fences({
      "main.cjs": `"use strict";
const table = require("./table.cjs");
console.log(table.total, table.label);
`,
      "table.cjs": `"use strict";
let total = 0;
for (const n of [1, 2, 3]) total += n;
var label = "sum";
if (total > 5) label = "big sum";
module.exports = { total, label };
`,
    }),
  ).toEqual([]);
});

test("a call through a whole export's member that may read 'this' fences", () => {
  expect(
    fences(
      {
        "main.cjs": `"use strict";
const Counter = require("./counter.cjs");
console.log(Counter.bump(2), Counter.n);
`,
        "counter.cjs": `"use strict";
function Counter() {
  this.n = 0;
}
function bump(by) {
  this.n = (this.n || 0) + by;
  return this.n;
}
module.exports = Counter;
module.exports.bump = bump;
`,
      },
      "may read 'this'",
    ),
  ).toEqual([
    "main.cjs: calling 'bump' through a module's whole export when the function may read 'this'",
  ]);
});

test("a builtin call with a callback above a function export's table keeps the table optional", () => {
  // Array.from runs the callback, which can call the exported function
  // before `var t` is assigned, so `t` stays runtime-optional and its
  // computed write keeps the fence semver's shape had before.
  expect(
    fences(
      {
        "main.cjs": `"use strict";
const version = require("./version.cjs");
console.log(version());
`,
        "version.cjs": `"use strict";
exports = module.exports = Version;
var early = Array.from([1], () => module.exports());
var t = {};
var R = 0;
function tok(n) {
  t[n] = R++;
}
tok("A");
function Version() {
  return typeof t;
}
`,
      },
      "runtime-optional",
    ),
  ).toEqual(["version.cjs: a computed read from a runtime-optional global"]);
});

test("a class static initializer above a function export's table keeps the table optional", () => {
  // Static field initializers and static blocks run where the class is
  // defined, so either can call the exported function before `var t` is
  // assigned.
  for (const member of ["static early = module.exports();", "static { module.exports(); }"]) {
    expect(
      fences(
        {
          "main.cjs": `"use strict";
const version = require("./version.cjs");
console.log(version());
`,
          "version.cjs": `"use strict";
exports = module.exports = Version;
class Init {
  ${member}
}
var t = {};
var R = 0;
function tok(n) {
  t[n] = R++;
}
tok("A");
function Version() {
  return typeof t;
}
`,
        },
        "runtime-optional",
      ),
    ).toEqual(["version.cjs: a computed read from a runtime-optional global"]);
  }
});
