// util.MIMEType and MIMEParams as values: util.inspect and console.log show
// the class name with no own properties, JSON.stringify uses toJSON(), and
// instanceof, typeof and structuredClone behave as in Node.
import { MIMEParams, MIMEType, inspect } from "node:util";

const mime = new MIMEType("text/html; charset=utf-8");
console.log(inspect(mime), inspect(mime.params), inspect(new MIMEParams()));
console.log(mime);
console.log([mime, mime.params]);
console.log({ mime });
console.log(JSON.stringify({ mime, params: mime.params, list: [mime] }));
console.log(typeof mime, typeof mime.params, Object.keys(mime).length);
console.log(mime instanceof MIMEType, mime instanceof MIMEParams, mime.params instanceof MIMEParams);
const plain: unknown = { essence: "text/html" };
console.log(plain instanceof MIMEType, plain instanceof MIMEParams);
console.log(mime.params === mime.params);
console.log(JSON.stringify(structuredClone(mime)));
console.log(`${mime}` === mime.toString(), mime + "" === String(mime));
const byEssence = new Map<string, MIMEType>();
for (const input of ["text/html", "TEXT/HTML; q=1", "image/png"]) {
  const parsed = new MIMEType(input);
  if (!byEssence.has(parsed.essence)) byEssence.set(parsed.essence, parsed);
}
console.log([...byEssence.keys()].join(" "), String(byEssence.get("text/html")));
