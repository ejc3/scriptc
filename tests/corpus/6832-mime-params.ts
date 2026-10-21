// util.MIMEParams: Map-ordered entries keyed by lowercased names, get()
// answering null for a missing name, live iteration, and serialization that
// quotes values that are empty or not tokens.
import { MIMEParams, MIMEType } from "node:util";

const params = new MIMEParams();
console.log(JSON.stringify(String(params)), params.has("x"), params.get("x"));
params.set("Charset", "UTF-8");
params.set("boundary", "a b");
params.set("q", 'say "hi"');
params.set("path", "C:\\tmp");
params.set("empty", "");
params.set("CHARSET", "latin1");
console.log(String(params));
console.log(params.get("charset"), params.get("CharSet"), params.has("BOUNDARY"), params.get("missing"));
console.log([...params.keys()].join(","), "|", [...params.values()].join(","));
console.log(JSON.stringify([...params.entries()]));
console.log(JSON.stringify([...params]));
for (const [name, value] of params) console.log("entry", name, JSON.stringify(value));

params.delete("q");
params.delete("not-there");
params.set("q", "back");
console.log(String(params));

// Deleting during iteration: the next entry is still visited.
const live = new MIMEParams();
for (const name of ["a", "b", "c", "d"]) live.set(name, name.toUpperCase());
const seen: string[] = [];
for (const [name] of live) {
  seen.push(name);
  if (name === "a") live.delete("a");
  if (name === "b") live.set("e", "E");
}
console.log(seen.join(","), String(live));

const mime = new MIMEType("text/plain; format=flowed; delsp=yes");
const shared = mime.params;
shared.set("charset", "utf-8");
shared.delete("delsp");
console.log(String(mime), mime.params.get("charset"), mime.params.has("delsp"));
console.log(JSON.stringify(mime.params), mime.params.toJSON());
const keys = mime.params.keys();
console.log(keys.next().value, keys.next().value, keys.next().done);
