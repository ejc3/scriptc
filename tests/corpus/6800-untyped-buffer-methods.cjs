// Buffer members on untyped values (unannotated CommonJS parameters):
// typeof and identity of the method values, equals/compare/copy,
// toString over encodings and ranges, toJSON, write with its argument
// forms, fill with numbers, strings, buffers and encodings,
// indexOf/lastIndexOf/includes with offsets, ends and encodings,
// swap16/32/64, the numeric read and write families, and method values
// called through .call.
"use strict";

const hex = (b) => (Buffer.isBuffer(b) ? `<${b.toString("hex")}>` : b instanceof Uint8Array ? `[${Array.from(b).join(",")}]` : String(b));
function show(label, ...values) {
  console.log([label, ...values.map((value) => (typeof value === "bigint" ? `${value}n` : hex(value)))].join(" "));
}

function members(x) {
  return [
    `equals:${typeof x.equals}`, `compare:${typeof x.compare}`, `copy:${typeof x.copy}`,
    `toString:${typeof x.toString}`, `toJSON:${typeof x.toJSON}`, `write:${typeof x.write}`,
    `fill:${typeof x.fill}`, `indexOf:${typeof x.indexOf}`, `lastIndexOf:${typeof x.lastIndexOf}`,
    `includes:${typeof x.includes}`, `swap16:${typeof x.swap16}`, `swap32:${typeof x.swap32}`,
    `swap64:${typeof x.swap64}`, `readUInt8:${typeof x.readUInt8}`, `readInt32LE:${typeof x.readInt32LE}`,
    `readBigUInt64BE:${typeof x.readBigUInt64BE}`, `readDoubleLE:${typeof x.readDoubleLE}`,
    `writeUInt16BE:${typeof x.writeUInt16BE}`, `writeIntLE:${typeof x.writeIntLE}`,
    `writeBigInt64LE:${typeof x.writeBigInt64LE}`, `toLocaleString:${typeof x.toLocaleString}`,
    `slice:${typeof x.slice}`, `subarray:${typeof x.subarray}`, `at:${typeof x.at}`, `map:${typeof x.map}`,
  ].join(" ");
}

function equalsAndCompare(a, b, c) {
  show("equals", a.equals(b));
  show("equals other", a.equals(c));
  show("compare", a.compare(c));
  show("compare reverse", c.compare(a));
  show("compare range", a.compare(c, 1, 3, 1, 3));
  show("compare equal ranges", a.compare(b, 0, 2, 0, 2));
  show("compare empty", a.compare(c, 2, 2));
  show("same method", a.equals === b.equals, a.readUint8 === a.readUInt8, a.toLocaleString === a.toString);
}

function copyInto(src, dst) {
  show("copy count", src.copy(dst));
  show("copy target", dst);
  show("copy offsets", src.copy(dst, 4, 1, 3));
  show("copy target after", dst);
  show("copy fractional", src.copy(dst, 1.7, 0.2, 2.9));
  show("copy past end", src.copy(dst, 100));
  show("copy into Uint8Array", src.copy(new Uint8Array(3)));
}

function strings(b) {
  show("toString", b.toString());
  for (const enc of ["hex", "base64", "base64url", "latin1", "binary", "ascii", "utf16le", "ucs2", "UTF-8", "Hex"])
    show(`toString ${enc}`, b.toString(enc));
  show("toString range", b.toString("utf8", 1, 4));
  show("toString negative start", b.toString("utf8", -3, 2));
  show("toString start past end", JSON.stringify(b.toString("utf8", 99)));
  show("toString end before start", JSON.stringify(b.toString("utf8", 3, 1)));
  show("toString fractional", b.toString("utf8", 1.9, 3.2));
  show("toString string range", b.toString("utf8", "1", "3"));
  show("toString undefined enc", b.toString(undefined, 2));
  show("toLocaleString", b.toLocaleString("hex"));
  console.log("toJSON", JSON.stringify(b.toJSON()), JSON.stringify(b));
}

function writes(b) {
  show("write", b.write("hi"));
  show("after write", b);
  show("write encoding", b.write("aabb", "hex"));
  show("write offset", b.write("xyz", 4));
  show("write offset encoding", b.write("ff", 6, "hex"));
  show("write offset length", b.write("abcdef", 2, 3));
  show("write all args", b.write("0102", 1, 1, "hex"));
  show("write truncates utf8", b.write("é", 7));
  show("write utf16le", b.write("ab", 0, "utf16le"));
  show("after writes", b);
}

function fills(b) {
  show("fill number", b.fill(1));
  show("fill offset", b.fill(2, 3));
  show("fill range", b.fill(3, 1, 2));
  show("fill string", b.fill("ab"));
  show("fill string encoding", b.fill("0aff", "hex"));
  show("fill string offset encoding", b.fill("7a", 2, "hex"));
  show("fill string range encoding", b.fill("41", 0, 2, "hex"));
  show("fill empty string", b.fill(""));
  show("fill buffer", b.fill(Buffer.from([9, 8])));
  show("fill bool", b.fill(true));
  show("fill 257", b.fill(257));
  show("fill chains", b.fill(0) === b);
}

function searches(b) {
  show("indexOf string", b.indexOf("lo"));
  show("indexOf number", b.indexOf(0x6c));
  show("indexOf buffer", b.indexOf(Buffer.from("o w")));
  show("indexOf offset", b.indexOf("o", 5));
  show("indexOf negative offset", b.indexOf("o", -4));
  show("indexOf string offset", b.indexOf("6f", "hex"));
  show("indexOf end", b.indexOf("o", 0, 4));
  show("indexOf end encoding", b.indexOf("6f", 0, "hex"));
  show("indexOf missing", b.indexOf("zz"));
  show("indexOf empty", b.indexOf(""));
  show("indexOf empty past end", b.indexOf("", 99));
  show("indexOf 256+", b.indexOf(0x16c));
  show("lastIndexOf", b.lastIndexOf("o"));
  show("lastIndexOf offset", b.lastIndexOf("o", 5));
  show("lastIndexOf number", b.lastIndexOf(0x6c));
  show("lastIndexOf negative", b.lastIndexOf("l", -9));
  show("lastIndexOf end", b.lastIndexOf("l", undefined, 4));
  show("includes", b.includes("world"));
  show("includes offset", b.includes("hello", 1));
  show("includes number", b.includes(100));
  show("indexOf utf16le", Buffer.from("abcb", "utf16le").indexOf("b", 0, "utf16le"));
  show("indexOf NaN offset", b.indexOf("l", NaN));
  show("indexOf null offset", b.indexOf("l", null));
}

function swaps(b) {
  show("swap16", Buffer.from(b).swap16());
  show("swap32", Buffer.from(b).swap32());
  show("swap64", Buffer.from(b).swap64());
}

function numbers(b) {
  b.fill(0);
  show("writeUInt8", b.writeUInt8(255, 0));
  show("writeInt8", b.writeInt8(-2, 1));
  show("writeUInt16LE", b.writeUInt16LE(0x1234, 2));
  show("writeUInt16BE", b.writeUInt16BE(0x1234, 4));
  show("writeInt16LE", b.writeInt16LE(-300, 6));
  show("writeUInt32BE", b.writeUInt32BE(0xdeadbeef, 8));
  show("writeInt32LE", b.writeInt32LE(-5, 12));
  show("after integer writes", b);
  show("readUInt8", b.readUInt8(0));
  show("readUint8", b.readUint8());
  show("readInt8", b.readInt8(1));
  show("readUInt16LE", b.readUInt16LE(2));
  show("readUInt16BE", b.readUInt16BE(4));
  show("readInt16LE", b.readInt16LE(6));
  show("readInt16BE", b.readInt16BE(6));
  show("readUInt32BE", b.readUInt32BE(8));
  show("readUInt32LE", b.readUInt32LE(8));
  show("readInt32LE", b.readInt32LE(12));
  show("readInt32BE", b.readInt32BE(12));
  show("writeFloatLE", b.writeFloatLE(1.5, 0));
  show("readFloatLE", b.readFloatLE(0));
  show("writeFloatBE", b.writeFloatBE(-0.25, 4));
  show("readFloatBE", b.readFloatBE(4));
  show("writeDoubleLE", b.writeDoubleLE(Math.PI, 8));
  show("readDoubleLE", b.readDoubleLE(8));
  show("writeDoubleBE", b.writeDoubleBE(-1e300, 0));
  show("readDoubleBE", b.readDoubleBE(0));
  show("writeBigUInt64LE", b.writeBigUInt64LE(2n ** 63n + 5n, 0));
  show("readBigUInt64LE", b.readBigUInt64LE(0));
  show("readBigUint64LE", b.readBigUint64LE());
  show("writeBigInt64BE", b.writeBigInt64BE(-12345678901234n, 8));
  show("readBigInt64BE", b.readBigInt64BE(8));
  show("readBigInt64LE", b.readBigInt64LE(8));
  show("writeBigUint64BE", b.writeBigUint64BE(1n, 0));
  show("readBigUInt64BE", b.readBigUInt64BE(0));
  show("writeUIntLE", b.writeUIntLE(0x123456, 0, 3));
  show("readUIntLE", b.readUIntLE(0, 3));
  show("writeUIntBE", b.writeUIntBE(0x123456789a, 3, 5));
  show("readUIntBE", b.readUIntBE(3, 5));
  show("writeIntLE", b.writeIntLE(-0x1234, 8, 2));
  show("readIntLE", b.readIntLE(8, 2));
  show("writeIntBE", b.writeIntBE(-0x123456789ab, 10, 6));
  show("readIntBE", b.readIntBE(10, 6));
  show("readUintBE", b.readUintBE(10, 6));
  show("writeUint32LE", b.writeUint32LE(7));
  show("readUint32LE", b.readUint32LE());
  show("value coercion", b.writeUInt8("12", 0), b.readUInt8(0));
}

function methodValues(b, other) {
  const eq = b.equals;
  show("call equals", eq.call(b, other));
  const read = b.readUInt16BE;
  show("call read", read.call(b, 0));
  const ts = b.toString;
  show("call toString", ts.call(b, "hex"));
  show("method name", eq.name, read.name);
  show("apply write", b.write.apply(b, ["zz", 1]));
}

console.log(members(Buffer.from("hello")));
equalsAndCompare(Buffer.from([1, 2, 3]), Buffer.from([1, 2, 3]), Buffer.from([1, 3, 2, 0]));
copyInto(Buffer.from([1, 2, 3, 4]), Buffer.alloc(6));
strings(Buffer.from("héllo wörld"));
writes(Buffer.alloc(8));
fills(Buffer.alloc(5));
searches(Buffer.from("hello world"));
swaps(Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]));
numbers(Buffer.alloc(16));
methodValues(Buffer.from([0xab, 0xcd]), Buffer.from([0xab, 0xcd]));
