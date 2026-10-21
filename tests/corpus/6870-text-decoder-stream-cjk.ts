// TextDecoder streams the multi-byte legacy encodings (Shift_JIS, EUC-JP,
// GBK, gb18030, Big5 and EUC-KR): each chunking of a text gives the one-shot
// decode, with a character split across chunks held until its last byte
// arrives, and malformed bytes inside one chunk decoding as Node decodes them.
const samples: [string, string][] = [
  ["shift_jis", "93fa967b8cea82cc8365834c835883678141b6c0b6c582c64142438142"],
  ["euc-jp", "c6fccbdcb8eca4cea5c6a5ada5b9a5c8a1a28eb68ec08eb68ec5a4c8414243a1a3"],
  ["gbk", "d6d0cec4cec4b1bea3acb2e2cad4414243a1a3"],
  ["gb18030", "d6d0cec4cec4b1bea3acb2e2cad4414243a1a3a2e39439fc36"],
  ["big5", "c163c5e9a4a4a4e5b4fab8d5414243a143"],
  ["euc-kr", "c7d1b1b9beee20c5d8bdbac6ae20414243a1a3"],
];

function bytesOf(hex: string): Uint8Array {
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

function streamed(label: string, bytes: Uint8Array, size: number): string {
  const decoder = new TextDecoder(label);
  let text = "";
  for (let i = 0; i < bytes.length; i += size) {
    text += decoder.decode(bytes.subarray(i, i + size), { stream: true });
  }
  return text + decoder.decode();
}

for (const [label, hex] of samples) {
  const bytes = bytesOf(hex);
  const whole = new TextDecoder(label).decode(bytes);
  const sizes: string[] = [];
  for (let size = 1; size <= 5; size++) sizes.push(streamed(label, bytes, size) === whole ? "=" : "!");
  console.log(label, JSON.stringify(whole), sizes.join(""));
}

// A lead byte at the end of a chunk waits; a flush ends it with U+FFFD.
const jis = new TextDecoder("shift_jis");
console.log(JSON.stringify(jis.decode(new Uint8Array([0x41, 0x93]), { stream: true })));
console.log(JSON.stringify(jis.decode(new Uint8Array([0xfa, 0x93]), { stream: true })));
console.log(JSON.stringify(jis.decode()));
// A gb18030 four-byte sequence across three chunks.
const gb = new TextDecoder("gb18030");
const parts = [[0x41, 0x94], [0x39], [0xfc, 0x36, 0x42]];
console.log(parts.map((part) => JSON.stringify(gb.decode(new Uint8Array(part), { stream: true }))).join(" "), JSON.stringify(gb.decode()));
// Malformed bytes within one chunk, and a malformed sequence that starts
// in the chunk that ends it.
for (const label of ["gbk", "euc-jp", "big5", "euc-kr", "shift_jis"]) {
  const decoder = new TextDecoder(label);
  const first = decoder.decode(new Uint8Array([0x81, 0x20, 0x41]), { stream: true });
  const second = decoder.decode(new Uint8Array([0x42, 0x81, 0x30, 0x30]), { stream: true });
  console.log(label, JSON.stringify(first), JSON.stringify(second), JSON.stringify(decoder.decode()));
}
// fatal decoders throw on a malformed byte in a chunk, not on a held lead.
const fatal = new TextDecoder("big5", { fatal: true });
console.log(JSON.stringify(fatal.decode(new Uint8Array([0x41, 0xa4]), { stream: true })));
try {
  fatal.decode();
} catch (e) {
  console.log((e as Error).name, (e as { code?: string }).code);
}
