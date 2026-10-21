// util.inspect, console.log, util.format and JSON.stringify over TextEncoder
// and TextDecoder: Node's custom inspect forms (`{ encoding: 'utf-8' }` for
// an encoder, `TextDecoder { encoding, fatal, ignoreBOM }` for a decoder), the
// `[TextEncoder]` and `[TextDecoder]` forms past the depth limit, the compact
// and breakLength options, `{}` from JSON.stringify, and
// Object.prototype.toString's tags.
import { format, inspect } from "node:util";

const enc = new TextEncoder();
const dec = new TextDecoder("utf-16le", { fatal: true, ignoreBOM: true });
const plain = new TextDecoder();
const show = (label: string, text: string): void => console.log(label, JSON.stringify(text));

show("encoder", inspect(enc));
show("decoder", inspect(dec));
show("default decoder", inspect(plain));
show("legacy decoder", inspect(new TextDecoder("shift_jis")));
show("nested", inspect({ a: { b: { c: enc, d: dec } } }));
show("deep", inspect({ a: { b: { c: { e: enc, d: dec } } } }));
show("depth 0", inspect({ e: enc, d: dec }, { depth: 0 }));
show("array", inspect([enc, dec]));
show("map", inspect(new Map([["k", dec]])));
show("compact", inspect({ e: enc, d: dec }, { compact: false }));
show("breakLength", inspect([dec, dec, dec], { breakLength: 40 }));
show("format", format("%O %s %j", enc, dec, dec));
show("strings", String(dec) + " " + `${enc}` + " " + enc.toString() + " " + dec.toString());
show("json", JSON.stringify([enc, dec, { enc, list: [plain] }]));
show("json pretty", JSON.stringify({ enc, dec }, null, 2));
console.log(enc);
console.log([dec], { dec });
