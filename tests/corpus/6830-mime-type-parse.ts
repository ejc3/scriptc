// util.MIMEType parsing and serialization: type, subtype and parameter names
// fold to lowercase, values keep their case, quoted values unescape, and the
// first occurrence of a parameter wins.
import { MIMEType } from "node:util";

const inputs = [
  "text/html",
  "Text/HTML; Charset=UTF-8",
  "  application/json  ;  charset=utf-8  ",
  'text/plain; charset="utf-8"; format=flowed',
  'multipart/form-data; boundary="----=_Part_0"',
  'text/plain; note="a \\"quoted\\" word"; path="C:\\\\dir"',
  "text/plain; a=1; A=2; b=3",
  "text/plain; =x; y=; z; ok=1",
  "text/plain;;; k=v;;",
  'text/plain; q="unterminated',
  'text/plain; q="trailing\\',
  "image/svg+xml; charset=utf-8",
  "application/vnd.api+json",
  "text/html;charset=gbk",
  'text/plain; empty=""; space=" "',
];
for (const input of inputs) {
  const mime = new MIMEType(input);
  console.log(JSON.stringify(input));
  console.log("  type:", mime.type, "subtype:", mime.subtype, "essence:", mime.essence);
  console.log("  string:", String(mime));
  console.log("  template:", `${mime}`);
  console.log("  json:", JSON.stringify(mime));
  console.log("  toString:", mime.toString(), mime.toJSON() === mime.toString());
}

const parsed = MIMEType.parse("Text/Plain; Charset=ASCII");
console.log(parsed === null ? "null" : parsed.essence, parsed?.params.get("charset"));
console.log(MIMEType.parse("not a mime"), MIMEType.parse("text/"), MIMEType.parse(""));

const mime = new MIMEType("text/html; charset=utf-8");
mime.type = "APPLICATION";
mime.subtype = "XHTML+XML";
console.log(mime.essence, String(mime));
const fromObject = new MIMEType({ toString: () => "text/css; charset=ascii" });
console.log(fromObject.essence, fromObject.params.get("charset"));
