// crypto.getCipherInfo answers from OpenSSL's cipher table as Node does:
// names (any case, aliases, object identifiers) and nids, the keyLength
// and ivLength options per mode, legacy ciphers that reject both options,
// undefined for an unknown cipher, and Node's argument errors. The result
// lists mode, name, nid, blockSize, ivLength and keyLength in Node's order
// and leaves out the fields a cipher does not have.
import { getCipherInfo } from "node:crypto";

function show(label: string, run: () => unknown): void {
  try {
    console.log(label, JSON.stringify(run()));
  } catch (error) {
    const e = error as Error & { code?: string };
    console.log(label, "throws", e.name, e.code, e.message);
  }
}

for (const name of ["aes-256-cbc", "AES-256-CBC", "aes256", "id-aes128-gcm", "aes-128-gcm", "chacha20-poly1305", "aes-128-ecb", "aes-128-xts", "id-aes128-wrap-pad", "aes-128-cbc-cts", "aes-256-siv", "des3", "bf", "rc4", "2.16.840.1.101.3.4.1.2", "idea", "IDEA-CBC", "idea-ofb", "1.3.6.1.4.1.188.7.1.1.2", "1.3.14.3.2.7", "1.2.410.200004.1.4", "nope", ""])
  show(`name ${name}`, () => getCipherInfo(name));
for (const nid of [427, 895, 5, 1198, 0, -1, 99999])
  show(`nid ${nid}`, () => getCipherInfo(nid));
for (const [name, ivLength] of [["aes-128-cbc", 16], ["aes-128-cbc", 12], ["aes-128-gcm", 8], ["aes-128-gcm", 0], ["aes-128-gcm", -1], ["aes-128-ccm", 7], ["aes-128-ccm", 14], ["aes-128-ocb", 15], ["aes-128-ocb", 16], ["aes-128-ecb", 0], ["aes-128-ecb", 16], ["rc4", 0]] as [string, number][])
  show(`iv ${name} ${ivLength}`, () => getCipherInfo(name, { ivLength }));
for (const [name, keyLength] of [["aes-128-cbc", 16], ["aes-128-cbc", 32], ["rc4", 16]] as [string, number][])
  show(`key ${name} ${keyLength}`, () => getCipherInfo(name, { keyLength }));
show("both", () => getCipherInfo("aes-256-gcm", { keyLength: 32, ivLength: 16 }));
show("empty options", () => getCipherInfo("rc4", {}));

const info = getCipherInfo("aes-192-ctr")!;
console.log("typed", info.mode, info.name, info.nid, info.blockSize, info.ivLength, info.keyLength);
console.log("keys", Object.keys(info).join(","));
const ecb = getCipherInfo("aes-128-ecb")!;
console.log("absent", ecb.ivLength, "ivLength" in ecb, getCipherInfo("chacha20-poly1305")!.blockSize);
console.log("missing", getCipherInfo("missing-cipher") === undefined);
const siv = getCipherInfo("aes-128-siv")!;
console.log("no nid", siv.mode, siv.nid, "nid" in siv, Object.keys(siv).join(","));

show("undefined name", () => getCipherInfo(undefined as any));
show("object name", () => getCipherInfo({} as any));
show("fraction nid", () => getCipherInfo(1.5));
show("large nid", () => getCipherInfo(2 ** 31));
show("NaN nid", () => getCipherInfo(NaN));
show("string options", () => getCipherInfo("aes-128-cbc", "x" as any));
show("null options", () => getCipherInfo("aes-128-cbc", null as any));
show("array options", () => getCipherInfo("aes-128-cbc", [] as any));
show("string keyLength", () => getCipherInfo("aes-128-cbc", { keyLength: "16" } as any));
show("fraction ivLength", () => getCipherInfo("aes-128-cbc", { ivLength: 1.5 }));
show("large keyLength", () => getCipherInfo("aes-128-cbc", { keyLength: 2 ** 31 }));
show("undefined keyLength", () => getCipherInfo("aes-128-cbc", { keyLength: undefined }));

const reads: string[] = [];
const watched = {
  get keyLength() { reads.push("keyLength"); return 16; },
  get ivLength() { reads.push("ivLength"); return 12; },
};
show("getter options", () => getCipherInfo("aes-128-gcm", watched));
console.log("reads", reads.join(","));
show("throwing getter", () => getCipherInfo("aes-128-cbc", { get keyLength(): number { throw new RangeError("from getter"); } }));
class Lengths { keyLength = 32; }
show("class options", () => getCipherInfo("aes-256-cbc", new Lengths()));
