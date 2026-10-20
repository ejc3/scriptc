// Brotli one-shot forms with dynamically typed inputs: strings, Buffers,
// TypedArrays of other widths, DataViews and ArrayBuffers contribute their
// bytes, other values throw Node's ERR_INVALID_ARG_TYPE ("buffer" for the
// sync forms, "chunk" for the callback forms) after the options are checked,
// and options and dictionaries arrive as dynamic values too.
const zlib = require("node:zlib");

function packSync(data, options) {
  return zlib.brotliCompressSync(data, options);
}

function unpackSync(data, options) {
  return zlib.brotliDecompressSync(data, options);
}

function pack(data, options, callback) {
  zlib.brotliCompress(data, options, callback);
}

function show(label, run) {
  try {
    const result = run();
    console.log(label, "ok", result.length, result.toString("hex").slice(0, 40));
  } catch (error) {
    console.log(label, error.name, error.code, JSON.stringify(error.message));
  }
}

const roundtrip = (input) => () => zlib.brotliDecompressSync(zlib.brotliCompressSync(input));

show("string", roundtrip("dynamic string ☃"));
show("buffer", roundtrip(Buffer.from("dynamic buffer")));
show("uint8", roundtrip(new Uint8Array([1, 2, 3, 4])));
show("uint16", roundtrip(new Uint16Array([1, 2, 0xfffe])));
show("int32", roundtrip(new Int32Array([-1, 7])));
show("float64", roundtrip(new Float64Array([1.5, -0])));
show("dataview", roundtrip(new DataView(new Uint8Array([9, 8, 7, 6]).buffer, 1, 2)));
show("arraybuffer", roundtrip(new Uint8Array([4, 5, 6]).buffer));
show("subarray", roundtrip(new Uint8Array([10, 11, 12, 13, 14]).subarray(1, 4)));
show("empty-string", roundtrip(""));

show("number", () => packSync(5));
show("undefined", () => packSync(undefined));
show("null", () => packSync(null));
show("object", () => packSync({ length: 3 }));
show("array", () => packSync([1, 2, 3]));
show("boolean", () => unpackSync(true));
show("bigint", () => unpackSync(3n));
show("options-first", () => packSync(5, { chunkSize: 1 }));
show("options-first-decode", () => unpackSync(null, { params: { 99: 1 } }));
show("dyn-roundtrip", () => unpackSync(packSync(new Uint16Array([7, 8]), { chunkSize: 64 })));

const inputs = ["text", Buffer.from("buffer"), new Uint16Array([300, 400])];
for (const input of inputs) {
  const packed = packSync(input);
  console.log("loop", packed.toString("hex"), unpackSync(packed).toString("hex"));
}

const quality = zlib.constants.BROTLI_PARAM_QUALITY;
const optionSets = [undefined, {}, { params: { [quality]: 1 } }, { chunkSize: 64 }, { params: [9] }];
optionSets.forEach((options, index) => {
  show("options-" + index, () => packSync("option sets ".repeat(20), options));
});

const words = () => new Uint8Array(Buffer.from("dictionary words"));
const dictionaries = [
  ["Buffer", Buffer.from("dictionary words")],
  ["Uint8Array", words()],
  ["ArrayBuffer", words().buffer],
  ["DataView", new DataView(words().buffer)],
  ["Uint16Array", new Uint16Array(words().buffer)],
];
for (const [name, dictionary] of dictionaries) {
  const packed = packSync("words in the dictionary words", { dictionary });
  const plain = unpackSync(packed, { dictionary });
  console.log("dictionary", name, packed.toString("hex"), plain.toString());
}

function attemptPack(label, input, options) {
  try {
    pack(input, options, () => console.log(label, "not reached"));
  } catch (error) {
    console.log(label, error.name, error.code, error.message);
  }
}
attemptPack("callback-number", 42, {});
attemptPack("callback-options-first", 42, { flush: 9 });

// One job at a time: worker jobs finish in any order.
pack(new Uint16Array([1, 2, 3]), undefined, (error, value) => {
  console.log("callback-uint16", error, value.toString("hex"));
  const bytes = value.buffer.slice(value.byteOffset, value.byteOffset + value.length);
  zlib.brotliDecompress(bytes, (again, plain) => {
    console.log("callback-arraybuffer", again, plain.toString("hex"));
    pack(null, undefined, (third, empty) => {
      console.log("callback-null", third, empty.length);
      pack(undefined, {}, (fourth, none) => console.log("callback-undefined", fourth, none.length));
    });
  });
});
