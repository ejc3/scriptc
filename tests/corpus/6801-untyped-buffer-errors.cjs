// Errors of Buffer members on untyped values match Node's: argument
// types, offsets out of range, unknown encodings, values out of range for
// the numeric writes, buffer sizes for swap, and fill and write argument
// errors.
"use strict";

function attempt(label, run) {
  try {
    const result = run();
    console.log(label, "ok", typeof result === "bigint" ? `${result}n` : Buffer.isBuffer(result) ? result.toString("hex") : result);
  } catch (e) {
    console.log(label, e.name, e.code, e.message);
  }
}

function errors(b, big) {
  attempt("equals number", () => b.equals(5));
  attempt("equals string", () => b.equals("ab"));
  attempt("equals array", () => b.equals([1, 2]));
  attempt("equals Uint16Array", () => b.equals(new Uint16Array(2)));
  attempt("compare object", () => b.compare({}));
  attempt("compare string offset", () => b.compare(b, "1"));
  attempt("compare negative", () => b.compare(b, -1));
  attempt("compare end past", () => b.compare(b, 0, 99));
  attempt("compare fractional", () => b.compare(b, 0.5));
  attempt("copy target", () => b.copy("x"));
  attempt("copy negative target", () => b.copy(Buffer.alloc(2), -1));
  attempt("copy source past", () => b.copy(Buffer.alloc(2), 0, 99));
  attempt("toString bogus", () => b.toString("bogus"));
  attempt("toString number enc", () => b.toString(5));
  attempt("toString empty bogus", () => Buffer.alloc(0).toString("bogus"));
  attempt("write number", () => b.write(5));
  attempt("write offset string", () => b.write("a", "1", 1));
  attempt("write offset range", () => b.write("a", 99));
  attempt("write length range", () => b.write("a", 0, 99));
  attempt("write bogus", () => b.write("a", 0, 1, "bogus"));
  attempt("write encoding bogus", () => b.write("a", "bogus"));
  attempt("fill bogus", () => b.fill("a", "bogus"));
  attempt("fill encoding number", () => b.fill("a", 0, 1, 5));
  attempt("fill offset string type", () => b.fill(1, "1"));
  attempt("fill offset range", () => b.fill(1, -1));
  attempt("fill end range", () => b.fill(1, 0, 99));
  attempt("fill empty buffer", () => b.fill(Buffer.alloc(0)));
  attempt("fill bad hex", () => b.fill("zz", "hex"));
  attempt("fill object", () => b.fill({}));
  attempt("indexOf object", () => b.indexOf({}));
  attempt("indexOf bogus", () => b.indexOf("a", 0, "bogus"));
  attempt("includes bogus", () => b.includes("a", "bogus"));
  attempt("swap16 odd", () => Buffer.alloc(3).swap16());
  attempt("swap32 odd", () => Buffer.alloc(6).swap32());
  attempt("swap64 odd", () => Buffer.alloc(4).swap64());
  attempt("readUInt8 string", () => b.readUInt8("0"));
  attempt("readUInt8 range", () => b.readUInt8(99));
  attempt("readUInt8 fractional", () => b.readUInt8(0.5));
  attempt("readUInt8 negative", () => b.readUInt8(-1));
  attempt("readInt32LE short", () => Buffer.alloc(3).readInt32LE());
  attempt("readUIntLE byteLength", () => b.readUIntLE(0, 7));
  attempt("readUIntLE missing", () => b.readUIntLE(0));
  attempt("readBigInt64LE short", () => b.readBigInt64LE(0));
  attempt("writeUInt8 range", () => b.writeUInt8(256, 0));
  attempt("writeInt8 range", () => b.writeInt8(-129, 0));
  attempt("writeUInt16LE range", () => b.writeUInt16LE(-1, 0));
  attempt("writeUInt8 offset", () => b.writeUInt8(1, 99));
  attempt("writeUInt8 offset type", () => b.writeUInt8(1, "0"));
  attempt("writeFloatLE short", () => b.writeFloatLE(1, 0));
  attempt("writeBigInt64LE number", () => big.writeBigInt64LE(1));
  attempt("writeBigInt64LE range", () => big.writeBigInt64LE(2n ** 63n));
  attempt("writeBigUInt64BE negative number", () => big.writeBigUInt64BE(-1));
  attempt("writeBigInt64LE offset type", () => big.writeBigInt64LE(1n, "0"));
  attempt("writeBigInt64LE range before offset type", () => big.writeBigInt64LE(2n ** 64n, "0"));
  attempt("writeUInt16LE range before offset type", () => b.writeUInt16LE(-1, "0"));
  attempt("writeUInt8 offset type before range", () => b.writeUInt8(-1, "0"));
  attempt("readUIntLE offset type", () => b.readUIntLE("0", 1));
  attempt("readUIntLE byteLength before offset type", () => b.readUIntLE("0", 9));
  attempt("writeIntBE offset missing", () => b.writeIntBE(1, undefined, 2));
  attempt("writeIntLE range", () => b.writeIntLE(0x8000, 0, 2));
  attempt("writeUIntBE byteLength", () => b.writeUIntBE(1, 0, 0));
  attempt("readUInt8 NaN", () => b.readUInt8(NaN));
  attempt("writeUInt8 NaN value", () => b.writeUInt8(NaN, 0));
}

errors(Buffer.from([1, 2]), Buffer.alloc(8));
