// util.MIMEType and MIMEParams validation: ERR_INVALID_MIME_SYNTAX names the
// part, the input and the UTF-16 position of the first invalid character.
import { MIMEType } from "node:util";

function attempt(label: string, run: () => unknown): void {
  try {
    const value = run();
    console.log(label, "->", String(value));
  } catch (error) {
    const e = error as Error & { code?: string };
    console.log(label, "!", e instanceof TypeError, e.name, e.code, e.message);
  }
}

for (const input of [
  "",
  "   ",
  "text",
  "text/",
  "/html",
  " /html",
  "te xt/html",
  "text/ht ml",
  "text/ html",
  "text/htmlé",
  "t\u{1F600}t/html",
  "text/ html",
  "text\t/html",
  "(text)/html",
]) {
  attempt(JSON.stringify(input), () => new MIMEType(input));
  console.log("  parse:", MIMEType.parse(input));
}

const mime = new MIMEType("text/plain; charset=utf-8");
attempt("type = ''", () => (mime.type = ""));
attempt("type = 'a b'", () => (mime.type = "a b"));
attempt("type = 'x/y'", () => (mime.type = "x/y"));
attempt("subtype = 'é'", () => (mime.subtype = "é"));
attempt("subtype = 'Ok-1'", () => (mime.subtype = "Ok-1"));
console.log(String(mime));

attempt("params.set('', 'v')", () => mime.params.set("", "v"));
attempt("params.set('a b', 'v')", () => mime.params.set("a b", "v"));
attempt("params.set('k', '\\u0100')", () => mime.params.set("k", "Ā"));
attempt("params.set('k', 'tab\\there')", () => mime.params.set("k", "tab\there"));
attempt("params.set('k', 'nl\\nhere')", () => mime.params.set("k", "nl\nhere"));
attempt("params.set('Name', '\\u00ff')", () => mime.params.set("Name", "ÿ"));
console.log(String(mime));

// The getters have no setters; an untyped write reaches them.
const untyped: any = mime;
attempt("essence =", () => (untyped.essence = "a/b"));
attempt("params =", () => (untyped.params = null));
attempt("symbol input", () => new MIMEType(Symbol("x") as unknown as string));
