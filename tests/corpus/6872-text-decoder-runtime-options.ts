// TextDecoder options the compiler can't see: options from a variable, a
// parameter or a call, with truthy and falsy values; null and undefined; the
// TypeError for a primitive; arrays and functions accepted as Node accepts
// them; decode()'s stream option from a variable; and the order in which
// Node reads the properties: fatal, ignoreBOM, ignoreBOM, fatal for the
// constructor, and stream for decode() except on single-byte decoders.
function options(fatal: boolean, ignoreBOM: boolean): { fatal: boolean; ignoreBOM: boolean } {
  return { fatal, ignoreBOM };
}
function describe(decoder: TextDecoder): string {
  return `${decoder.encoding} ${decoder.fatal} ${decoder.ignoreBOM}`;
}

const both = options(true, true);
console.log(describe(new TextDecoder("utf-16le", both)));
console.log(describe(new TextDecoder("utf-8", options(false, true))));
console.log(describe(new TextDecoder(undefined, options(true, false))));
const loose: unknown = { fatal: 1, ignoreBOM: "" };
console.log(describe(new TextDecoder("latin1", loose as any)));
console.log(describe(new TextDecoder("utf-8", null as any)));
console.log(describe(new TextDecoder("utf-8", undefined as any)));
console.log(describe(new TextDecoder("utf-8", [] as any)));


for (const bad of [5, "fatal", true]) {
  try {
    new TextDecoder("utf-8", bad as any);
  } catch (e) {
    console.log((e as Error).name, (e as { code?: string }).code, (e as Error).message);
  }
}
// An unsupported label and invalid options: Node validates the options
// first.
try {
  new TextDecoder("no-such-encoding", 5 as any);
} catch (e) {
  console.log((e as { code?: string }).code);
}

const reads: string[] = [];
const watched = {
  get fatal(): boolean {
    reads.push("fatal");
    return false;
  },
  get ignoreBOM(): boolean {
    reads.push("ignoreBOM");
    return true;
  },
  get stream(): boolean {
    reads.push("stream");
    return true;
  },
};
const watchedDecoder = new TextDecoder("utf-8", watched as any);
console.log(reads.join(","), describe(watchedDecoder));
reads.length = 0;
const head = new Uint8Array([0xe2, 0x82]);
const tail = new Uint8Array([0xac]);
let text = watchedDecoder.decode(head, watched as any);
text += watchedDecoder.decode(tail);
console.log(reads.join(","), JSON.stringify(text));
reads.length = 0;
const single = new TextDecoder("windows-1252");
console.log(JSON.stringify(single.decode(new Uint8Array([0x80]), watched as any)), reads.join(","));

const streaming: { stream?: boolean } = { stream: true };
const decoder = new TextDecoder();
console.log(JSON.stringify(decoder.decode(new Uint8Array([0xf0, 0x9f]), streaming) + decoder.decode(new Uint8Array([0x98, 0x80]))));
for (const bad of [1, "x"]) {
  try {
    decoder.decode(new Uint8Array([0x41]), bad as any);
  } catch (e) {
    console.log((e as { code?: string }).code, (e as Error).message);
  }
}
console.log(JSON.stringify(decoder.decode(new Uint8Array([0x41]), null as any)));
