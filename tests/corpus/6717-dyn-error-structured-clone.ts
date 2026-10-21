// structuredClone of errors held as `any` or `unknown` matches Node's: the
// clone's prototype follows `name` when it names a standard error
// constructor (Error otherwise), and it has the source's own `message`,
// its `stack` and its own `cause` (cloned), all non-enumerable. Other own
// properties are not copied, and `name` is not an own property of the
// clone. Errors inside objects and arrays clone the same way.

function dyn(value: unknown): any {
  return value;
}

class Custom extends Error { extra = 1; constructor(m: string) { super(m); this.name = "Custom"; } }
const head = (s: unknown) => (typeof s === "string" ? JSON.stringify(s.split("\n")[0]) : String(s));
function show(label: string, value: any): void {
  const c = dyn(structuredClone(value));
  console.log(label, "|", c.name, c.message, String(c), "|", Object.keys(c).join(","), "|", Object.getOwnPropertyNames(c).join(","), "|",
    c instanceof Error, c instanceof TypeError, c instanceof RangeError, "|", head(c.stack), "|", c === value);
}
show("error", dyn(new Error("plain")));
show("type", dyn(new TypeError("bad")));
show("range", dyn(new RangeError("far")));
show("syntax", dyn(new SyntaxError("syn")));
show("uri", dyn(new URIError("uri")));
show("custom", dyn(new Custom("mine")));
show("cause", dyn(new Error("outer", { cause: { inner: 1 } })));
const extra = dyn(new Error("extra")); extra.code = "E_X"; extra.detail = [1];
show("extra props", extra);
const renamed = dyn(new Error("renamed")); renamed.name = "TypeError";
show("renamed to TypeError", renamed);
const odd = dyn(new TypeError("odd")); odd.name = "Weird";
show("renamed weird", odd);
const nomsg = dyn(new Error());
show("no message", nomsg);
const st = dyn(new Error("st")); st.stack = "custom stack";
show("assigned stack", st);
const nested = dyn({ list: [new RangeError("in list")], err: new Error("in obj") });
const nc = dyn(structuredClone(nested));
console.log("nested", nc.list[0] instanceof RangeError, nc.list[0].message, nc.err instanceof Error, Object.keys(nc.err).join(","));
show("typed arg", new TypeError("typed"));
const cm = dyn(structuredClone(dyn(new Error("cm", { cause: new TypeError("tc") }))));
console.log("cause error", cm.cause instanceof TypeError, cm.cause.message, Object.getOwnPropertyDescriptor(cm, "cause")!.enumerable);
console.log("message desc", JSON.stringify(Object.getOwnPropertyDescriptor(dyn(structuredClone(dyn(new Error("d")))), "message")));
const unknownError: unknown = new EvalError("held as unknown");
show("unknown", unknownError);
show("reference", dyn(new ReferenceError("missing")));
