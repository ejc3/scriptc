import * as ts from "../../ts7/adapter.js";
import { type Lowerer } from "../lowerer.js";
import { locOf } from "../../program.js";
import { dynUndefinedExpr } from "../../../ir/build.js";
import { BOOL, DYN, F64, type IrExpr } from "../../../ir/ir.js";
import { builtinImportOf } from "./module-bindings.js";

/* node:util's MIMEType and MIMEParams are native checked-dynamic handles
 * (scr_mime.c): both types map to dyn, the getters and setters are handle
 * property reads and writes, and the methods dispatch through the handle.
 * These helpers lower the uses a dyn value cannot answer by itself:
 * construction, MIMEType.parse() and instanceof. */

type MimeClass = "MIMEType" | "MIMEParams";

/** The MIMEType or MIMEParams class in any node:util spelling. */
export function mimeClassOf(lowerer: Lowerer, node: ts.Expression): MimeClass | null {
  const builtin = ts.isIdentifier(node)
    ? builtinImportOf(lowerer, node)
    : ts.isPropertyAccessExpression(node) && !node.questionDotToken
      ? lowerer.builtinMemberOf(node)
      : null;
  if (builtin?.module !== "util") return null;
  return builtin.member === "MIMEType" || builtin.member === "MIMEParams" ? builtin.member : null;
}

/** The class of a stdlib MIMEType- or MIMEParams-typed value. */
function mimeValueClass(lowerer: Lowerer, node: ts.Expression): MimeClass | null {
  const type = lowerer.typeOf(node);
  const sym = type.getAliasSymbol() ?? type.getSymbol();
  if (!sym) return null;
  const name = ts.symbolName(sym);
  if (name !== "MIMEType" && name !== "MIMEParams") return null;
  return lowerer.isStdlibSymbol(sym) ? name : null;
}

/** `new MIMEType(input)` and `new MIMEParams()`. MIMEType's argument
 * vector goes to the runtime, which converts the input with ToString;
 * MIMEParams takes no arguments, but any given still evaluate first. */
export function lowerMimeNew(lowerer: Lowerer, expr: ts.NewExpression): IrExpr | null {
  const cls = mimeClassOf(lowerer, expr.expression);
  if (cls === null) return null;
  const args = expr.arguments?.nodes ?? [];
  if (args.some(ts.isSpreadElement)) return null;
  const loc = locOf(expr);
  const values = args.map((arg) => lowerer.lowerExprExpecting(arg, DYN));
  if (cls === "MIMEType") {
    return {
      kind: "libCall",
      fn: "util.mimeTypeNew",
      args: [{ kind: "dynArrLit", elems: values, type: DYN, loc }],
      type: DYN,
      loc,
    };
  }
  const created: IrExpr = { kind: "libCall", fn: "util.mimeParamsNew", args: [], type: DYN, loc };
  if (values.length === 0) return created;
  return {
    kind: "seqExpr",
    stmts: values.map((value) => ({ kind: "exprStmt" as const, expr: value, loc: value.loc })),
    result: created,
    type: DYN,
    loc,
  };
}

const INSTANCE_METHODS: Record<MimeClass, ReadonlySet<string>> = {
  MIMEType: new Set(["toString", "toJSON"]),
  MIMEParams: new Set([
    "get",
    "set",
    "has",
    "delete",
    "entries",
    "keys",
    "values",
    "toString",
    "toJSON",
  ]),
};

/** MIMEType.parse(), and the instance methods, which the handle answers
 * as methods, so they dispatch through dynInvoke rather than as a member
 * read and a call. */
export function lowerMimeCall(lowerer: Lowerer, call: ts.CallExpression): IrExpr | null {
  if (call.questionDotToken || !ts.isPropertyAccessExpression(call.expression)) return null;
  const access = call.expression;
  if (access.questionDotToken || call.arguments.nodes.some(ts.isSpreadElement)) return null;
  const name = access.name.text;
  const loc = locOf(call);
  if (name === "parse" && mimeClassOf(lowerer, access.expression) === "MIMEType") {
    // parse() takes one value; any others still evaluate, in order.
    const args = call.arguments.nodes.map((arg) => lowerer.lowerExprExpecting(arg, DYN));
    const result: IrExpr = {
      kind: "libCall",
      fn: "util.mimeTypeParse",
      args: [args[0] ?? dynUndefinedExpr(loc)],
      type: DYN,
      loc,
    };
    if (args.length <= 1) return result;
    const local = lowerer.declareHiddenLocal("%mimeParsed", DYN);
    return {
      kind: "seqExpr",
      stmts: [
        { kind: "varDecl", localId: local.id, init: result, loc },
        ...args.slice(1).map((arg) => ({ kind: "exprStmt" as const, expr: arg, loc: arg.loc })),
      ],
      result: { kind: "varRef", localId: local.id, type: DYN, loc },
      type: DYN,
      loc,
    };
  }
  const cls = mimeValueClass(lowerer, access.expression);
  if (cls === null || !INSTANCE_METHODS[cls].has(name)) return null;
  return {
    kind: "dynInvoke",
    recv: lowerer.lowerExprExpecting(access.expression, DYN),
    method: name,
    calleeName: ts.getTextOfNode(access),
    args: call.arguments.nodes.map((arg) => lowerer.lowerExprExpecting(arg, DYN)),
    type: DYN,
    loc,
  };
}

/** `v instanceof MIMEType` / `v instanceof MIMEParams`. */
export function lowerMimeInstanceof(lowerer: Lowerer, expr: ts.BinaryExpression): IrExpr | null {
  const cls = mimeClassOf(lowerer, expr.right);
  if (cls === null) return null;
  const loc = locOf(expr);
  const value = lowerer.coerceInto(expr.left, lowerer.lowerExpr(expr.left), DYN);
  return {
    kind: "libCall",
    fn: "util.isMime",
    args: [value, { kind: "numLit", value: cls === "MIMEType" ? 0 : 1, type: F64, loc }],
    type: BOOL,
    loc,
  };
}

/** `params`, `params.entries()`, `params.keys()` or `params.values()` on a
 * MIMEParams: a native iterable whose drained values convert to a typed
 * literal's elements ([name, value] tuples or strings). */
export function isMimeParamsIterable(lowerer: Lowerer, node: ts.Expression): boolean {
  while (ts.isParenthesizedExpression(node)) node = node.expression;
  if (mimeValueClass(lowerer, node) === "MIMEParams") return true;
  if (
    !ts.isCallExpression(node) ||
    node.arguments.nodes.length !== 0 ||
    !ts.isPropertyAccessExpression(node.expression)
  )
    return false;
  const name = node.expression.name.text;
  return (
    (name === "keys" || name === "values" || name === "entries") &&
    mimeValueClass(lowerer, node.expression.expression) === "MIMEParams"
  );
}
