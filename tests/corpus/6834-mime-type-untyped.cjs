// util.MIMEType and MIMEParams through require() and untyped values: the
// namespace and destructured spellings, dynamic member reads and writes, and
// method calls on values whose type is not known.
const util = require("node:util");
const { MIMEType, MIMEParams } = require("util");

const fromNamespace = new util.MIMEType("Application/LD+JSON; Profile=\"https://example.com/p\"");
console.log(fromNamespace.essence, fromNamespace.params.get("profile"), String(fromNamespace));

function describe(value) {
  return [value.type, value.subtype, value.essence, value.params.toString(), value.toString()].join(" | ");
}
console.log(describe(new MIMEType("text/csv; header=present")));

const box = { mime: new MIMEType("text/plain") };
box.mime.type = "TEXT";
box.mime.subtype = "markdown";
box.mime.params.set("variant", "GFM");
console.log(String(box.mime), box.mime.params.has("variant"));

const keys = ["type", "subtype", "essence"];
for (const key of keys) console.log(key, box.mime[key]);
console.log(box.mime.missing);

const params = new MIMEParams();
params.set("a", 1);
params.set("b", true);
params.set("c", null);
console.log(String(params), JSON.stringify(Array.from(params)));

try {
  box.mime.subtype = "";
} catch (error) {
  console.log(error.code, error.message);
}
try {
  box.mime.nope();
} catch (error) {
  console.log(error instanceof TypeError, error.message);
}
console.log(MIMEType.parse("image/webp").essence, MIMEType.parse("bad"));
