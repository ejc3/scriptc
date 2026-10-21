// util.MIMEType over non-ASCII and converted input: Latin-1 values are kept
// (quoted when serialized), values with characters above U+00FF drop their
// parameter, and the input converts with ToString.
import { MIMEType } from "node:util";

const inputs = [
  'text/plain; title="café"; sign=±',
  "text/plain; name=ÿþ",
  "text/plain; bad=Ā; good=ok",
  'text/plain; emoji="\u{1F600}"; keep=1',
  "text/plain; tab=\"a\tb\"",
  "text/plain; k=v ",
  "TEXT/PLAIN; K=\"\\é\"",
];
for (const input of inputs) {
  const mime = new MIMEType(input);
  console.log(JSON.stringify(String(mime)), JSON.stringify([...mime.params]));
}

const original = new MIMEType("Image/PNG; dpi=300");
const copy = new MIMEType(original);
copy.params.set("dpi", "600");
console.log(String(original), String(copy));
const described = { toString: () => " application/xml ; charset=utf-8 " };
console.log(String(new MIMEType(described)));
console.log(String(new MIMEType(String(123) + "/" + "abc")));

const header = "multipart/mixed; boundary=\"simple boundary\"; charset=us-ascii";
const parsed = MIMEType.parse(header);
if (parsed !== null) {
  console.log(parsed.type, parsed.params.get("boundary"), parsed.params.get("charset"));
  parsed.params.set("boundary", "plain");
  console.log(String(parsed));
}
