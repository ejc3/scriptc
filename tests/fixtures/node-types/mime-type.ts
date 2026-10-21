// Under @types/node: util.MIMEType and util.MIMEParams.
import { MIMEParams, MIMEType } from "node:util";
import * as util from "node:util";

const mime = new MIMEType('Text/HTML; Charset="UTF-8"; q=0.9');
console.log(mime.type, mime.subtype, mime.essence, mime.params.get("charset"), String(mime));
mime.subtype = "plain";
mime.params.set("format", "flowed");
mime.params.delete("q");
console.log(mime.toString(), mime.params.has("format"), JSON.stringify(mime));
console.log(JSON.stringify([...mime.params]), [...mime.params.keys()].join(","), [...mime.params.values()].join(","));
for (const [name, value] of mime.params.entries()) console.log(name, value);

const params = new MIMEParams();
params.set("a", "1");
console.log(String(params), params.get("b"), params instanceof MIMEParams, mime instanceof util.MIMEType);
try {
  new util.MIMEType("bad");
} catch (err) {
  console.log((err as NodeJS.ErrnoException).code, (err as Error).message);
}
console.log(util.inspect(mime));
