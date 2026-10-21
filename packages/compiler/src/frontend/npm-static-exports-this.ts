/* A CommonJS module whose exported functions call each other through
 * `this` (retry 0.13: `exports.timeouts = function (options) { …
 * this.createTimeout(i, opts) … }`). A call `exports.timeouts()` or
 * `retry.timeouts()` passes the exports object as `this`, which compiles to
 * a static table with no runtime object to pass. When every `this` in such
 * a function names another export the module writes (`this.X` where the
 * module has `exports.X = …`), the exports object is never replaced, and
 * the module itself calls the function only through `exports.f(…)`, `this.X`
 * reads `exports.X`: the same member Node reads for those calls. Only a
 * caller that detaches the function or rebinds its `this` would observe
 * the difference. */
import * as ts from "./ts7/syntax.js";
import { applyEdits, unparen, type Edit } from "./npm-static-bundled-chunks.js";

/** True when the text may hold such a function (a cheap probe). */
export function mayHoldExportsThis(source: string): boolean {
  return source.includes("this.") && source.includes("exports.");
}

function exportsMember(e: ts.Expression): string | null {
  const u = unparen(e);
  if (!ts.isPropertyAccessExpression(u) || !ts.isIdentifier(u.name)) return null;
  const obj = u.expression;
  if (ts.isIdentifier(obj) && obj.text === "exports") return u.name.text;
  if (
    ts.isPropertyAccessExpression(obj) &&
    ts.isIdentifier(obj.expression) &&
    obj.expression.text === "module" &&
    obj.name.text === "exports"
  )
    return u.name.text;
  return null;
}

/** The module text with `this.X` respelled `exports.X` in qualifying
 * exported functions, or null. */
export function rewriteExportsThis(sf: ts.SourceFile): string | null {
  if (ts.isExternalModule(sf) && !sf.fileName.endsWith(".cjs")) return null;
  // Names the module exports by member assignment, and whether it ever
  // replaces or rebinds the exports object.
  const exported = new Set<string>();
  const functions = new Map<string, ts.FunctionExpression>();
  let replaced = false;
  for (const stmt of sf.statements.nodes) {
    if (!ts.isExpressionStatement(stmt)) continue;
    const e = unparen(stmt.expression);
    if (!ts.isBinaryExpression(e) || e.operatorToken.kind !== ts.SyntaxKind.EqualsToken) continue;
    const name = exportsMember(e.left);
    if (name !== null) {
      if (exported.has(name)) functions.delete(name);
      exported.add(name);
      const value = unparen(e.right);
      if (ts.isFunctionExpression(value) && !functions.has(name)) functions.set(name, value);
    }
  }
  ts.walkPreorder(sf, (n) => {
    if (replaced) return "stop";
    if (
      ts.isBinaryExpression(n) &&
      n.operatorToken.kind >= ts.SyntaxKind.FirstAssignment &&
      n.operatorToken.kind <= ts.SyntaxKind.LastAssignment
    ) {
      const left = unparen(n.left);
      if (ts.isIdentifier(left) && left.text === "exports") replaced = true;
      if (
        ts.isPropertyAccessExpression(left) &&
        ts.isIdentifier(left.expression) &&
        left.expression.text === "module" &&
        left.name.text === "exports"
      )
        replaced = true;
    }
    return undefined;
  });
  if (replaced) return null;
  const edits: Edit[] = [];
  for (const [name, fn] of functions) {
    // `this` uses directly in the body (arrow functions share it).
    const uses: ts.ThisExpression[] = [];
    let ok = true;
    const walk = (n: ts.Node): void => {
      if (!ok) return;
      if (n !== fn && ts.isFunctionLike(n) && !ts.isArrowFunction(n)) return;
      if (ts.isClassDeclaration(n) || ts.isClassExpression(n)) return;
      if (n.kind === ts.SyntaxKind.ThisKeyword) {
        const parent = n.parent;
        if (
          parent === undefined ||
          !ts.isPropertyAccessExpression(parent) ||
          parent.expression !== n ||
          !ts.isIdentifier(parent.name) ||
          !exported.has(parent.name.text)
        ) {
          ok = false;
          return;
        }
        uses.push(n as ts.ThisExpression);
        return;
      }
      n.forEachChild(walk);
    };
    fn.forEachChild(walk);
    if (!ok || uses.length === 0) continue;
    // Inside the module the function is only called through the exports.
    let detached = false;
    ts.walkPreorder(sf, (n) => {
      if (detached) return "stop";
      if (!ts.isPropertyAccessExpression(n) || !ts.isIdentifier(n.name) || n.name.text !== name)
        return undefined;
      if (exportsMember(n) !== name) return undefined;
      const parent = n.parent;
      const isStore =
        parent !== undefined &&
        ts.isBinaryExpression(parent) &&
        parent.left === n &&
        parent.operatorToken.kind === ts.SyntaxKind.EqualsToken;
      const isCall = parent !== undefined && ts.isCallExpression(parent) && parent.expression === n;
      if (!isStore && !isCall) detached = true;
      return undefined;
    });
    if (detached) continue;
    for (const use of uses)
      edits.push({ start: use.getStart(sf), end: use.getEnd(), text: "exports" });
  }
  if (edits.length === 0) return null;
  return applyEdits(sf.text, 0, sf.text.length, edits);
}
