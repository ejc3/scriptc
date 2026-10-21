/* tsc wraps every CommonJS import in its interop helpers:
 *
 *   const events_1 = __importDefault(require("events"));   // events_1.default.X
 *   const path = __importStar(require("path"));             // path.basename
 *
 * For program modules cjs-tsc-interop.ts reads such a binding as a plain
 * require binding, and the helper declarations (`var __importDefault =
 * (this && this.__importDefault) || …`) go dead. A builtin module is not a
 * program module, so its binding kept the helpers live, and their probe of
 * the module's top-level `this` has no lowering. A builtin carries no
 * `__esModule`: `events_1.default` is the module and the namespace copy
 * reads the module's members. When every reference has that form
 * (`x.default…` for the default helper, `x.<member>` other than `default`
 * for the namespace helper), the binding becomes the plain `require()` of
 * the builtin and `x.default` becomes `x`. */
import * as ts from "./ts7/syntax.js";
import { applyEdits, unparen, type Edit } from "./npm-static-bundled-chunks.js";
import { canonicalBuiltinModule } from "./builtin-modules.js";

/** True when the text may hold such a binding (a cheap probe). */
export function mayHoldTscBuiltinImport(source: string): boolean {
  return /__import(?:Default|Star)\(require\(/.test(source);
}

/** `__importDefault(require("<builtin>"))` / `__importStar(…)`: the helper
 * and builtin spec, or null. */
function builtinInterop(init: ts.Expression): { helper: string; spec: string } | null {
  const e = unparen(init);
  if (!ts.isCallExpression(e) || !ts.isIdentifier(e.expression) || e.arguments.nodes.length !== 1)
    return null;
  const helper = e.expression.text;
  if (helper !== "__importDefault" && helper !== "__importStar") return null;
  const req = unparen(e.arguments.nodes[0]!);
  if (
    !ts.isCallExpression(req) ||
    !ts.isIdentifier(req.expression) ||
    req.expression.text !== "require" ||
    req.arguments.nodes.length !== 1 ||
    !ts.isStringLiteral(req.arguments.nodes[0]!)
  )
    return null;
  const spec = (req.arguments.nodes[0] as ts.StringLiteral).text;
  return canonicalBuiltinModule(spec) === null ? null : { helper, spec };
}

/** The `.default` accesses to remove when every reference to `name` (other
 * than its declaration) has the helper's form, or null. */
function interopReferences(
  sf: ts.SourceFile,
  decl: ts.VariableDeclaration,
  name: string,
  helper: string,
): ts.PropertyAccessExpression[] | null {
  const defaults: ts.PropertyAccessExpression[] = [];
  let ok = true;
  ts.walkPreorder(sf, (n) => {
    if (!ok) return "stop";
    if (!ts.isIdentifier(n) || n.text !== name || n === decl.name) return undefined;
    const parent = n.parent;
    // A write through the wrapper (`x.default = …`, `ns.member = …`)
    // changes the helper's object, not the module.
    if (
      parent === undefined ||
      !ts.isPropertyAccessExpression(parent) ||
      parent.expression !== n ||
      parent.questionDotToken !== undefined ||
      isWriteTarget(parent)
    ) {
      ok = false;
      return "stop";
    }
    const member = parent.name.text;
    if (helper === "__importDefault") {
      if (member !== "default") ok = false;
      else defaults.push(parent);
    } else if (member === "default") {
      ok = false;
    }
    return undefined;
  });
  return ok ? defaults : null;
}

/** Whether an access is assigned (directly or as a destructuring or
 * for-in/of target), updated or deleted. */
function isWriteTarget(access: ts.Expression): boolean {
  let node: ts.Node = access;
  while (node.parent !== undefined && ts.isParenthesizedExpression(node.parent)) node = node.parent;
  const parent = node.parent;
  if (parent === undefined) return false;
  if (ts.isPrefixUnaryExpression(parent) || ts.isPostfixUnaryExpression(parent))
    return (
      parent.operator === ts.SyntaxKind.PlusPlusToken ||
      parent.operator === ts.SyntaxKind.MinusMinusToken
    );
  if (ts.isDeleteExpression(parent)) return true;
  let target: ts.Node = node;
  for (;;) {
    const up = target.parent;
    if (up === undefined) return false;
    if (ts.isBinaryExpression(up))
      return (
        up.left === target &&
        up.operatorToken.kind >= ts.SyntaxKind.FirstAssignment &&
        up.operatorToken.kind <= ts.SyntaxKind.LastAssignment
      );
    if (
      ((ts.isPropertyAssignment(up) && up.initializer === target) ||
        ts.isSpreadElement(up) ||
        ts.isSpreadAssignment(up)) &&
      up.parent !== undefined
    )
      target = up.parent;
    else if (ts.isArrayLiteralExpression(up) || ts.isObjectLiteralExpression(up)) target = up;
    else return (ts.isForInStatement(up) || ts.isForOfStatement(up)) && up.initializer === target;
  }
}

/** The module text with its builtin interop bindings unwrapped, or null. */
export function unwrapTscBuiltinImports(sf: ts.SourceFile): string | null {
  if (ts.isExternalModule(sf) && !sf.fileName.endsWith(".cjs")) return null;
  const edits: Edit[] = [];
  const stmts = sf.statements.nodes;
  // Builtin modules through the interop helpers.
  for (const stmt of stmts) {
    if (!ts.isVariableStatement(stmt)) continue;
    for (const d of stmt.declarationList.declarations.nodes) {
      if (!ts.isIdentifier(d.name) || d.initializer === undefined) continue;
      const interop = builtinInterop(d.initializer);
      if (interop === null) continue;
      const defaults = interopReferences(sf, d, d.name.text, interop.helper);
      if (defaults === null) continue;
      edits.push({
        start: d.initializer.getStart(sf),
        end: d.initializer.getEnd(),
        text: `require(${JSON.stringify(interop.spec)})`,
      });
      for (const access of defaults)
        edits.push({ start: access.getStart(sf), end: access.getEnd(), text: d.name.text });
    }
  }
  if (edits.length === 0) return null;
  return applyEdits(sf.text, 0, sf.text.length, edits);
}
