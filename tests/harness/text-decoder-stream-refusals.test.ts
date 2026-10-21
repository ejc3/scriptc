/* Streaming TextDecoder cases scriptc refuses at run time with SC2020, where
 * Node's ICU converter doesn't give the one-shot decode: an ISO-2022-JP
 * decoder with { stream: true }, and a gb18030/GBK or EUC-JP sequence that
 * starts in an earlier chunk and turns out malformed, which Node can throw
 * ERR_ENCODING_INVALID_ENCODED_DATA for. Sequences that stay well-formed
 * across chunks decode (corpus 6870). */
import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { promisify } from "node:util";
import { expect, test } from "vitest";
import { compile } from "@scriptc/compiler";

const execFileAsync = promisify(execFile);
const repoRoot = join(import.meta.dirname, "../..");
const cacheDir = join(repoRoot, "node_modules/.cache/scriptc-tests");
const sanitize = process.env["SCRIPTC_SAN"] === "1";

const source = `
const attempt = (label: string, chunks: number[][], encoding: string): void => {
  const decoder = new TextDecoder(encoding);
  try {
    let text = "";
    for (const chunk of chunks) text += decoder.decode(new Uint8Array(chunk), { stream: true });
    text += decoder.decode();
    console.log(label, Array.from(text, (c) => c.codePointAt(0)!.toString(16)).join(" "));
  } catch (e) {
    console.log(label, (e as { code?: string }).code, (e as Error).message);
  }
};
attempt("iso-2022-jp", [[0x1b, 0x24, 0x42, 0x30], [0x21]], "iso-2022-jp");
attempt("gbk carried", [[0x81, 0x30], [0x30]], "gbk");
attempt("gb18030 carried", [[0x81, 0x30, 0x81], [0x20]], "gb18030");
attempt("euc-jp carried", [[0x8f, 0xa1], [0x20]], "euc-jp");
attempt("gbk in one chunk", [[0x81, 0x30, 0x30]], "gbk");
attempt("gbk lead carried", [[0x81], [0x30, 0x30]], "gbk");
attempt("gbk valid", [[0x81, 0x30], [0x81, 0x30]], "gbk");
`;

test("streaming decodes refuse what Node's converter does not decode as one shot", async () => {
  const key = createHash("sha256")
    .update(source)
    .update(sanitize ? "san" : "plain")
    .digest("hex")
    .slice(0, 16);
  const outDir = join(cacheDir, `text-decoder-stream-${key}`);
  mkdirSync(outDir, { recursive: true });
  const file = join(outDir, "stream.ts");
  writeFileSync(file, source);
  const result = await compile(file, { outPath: join(outDir, "stream"), outDir, sanitize });
  if (!result.ok)
    throw new Error(result.diagnostics.map((d) => `${d.code}: ${d.message}`).join("\n"));
  const { stdout } = await execFileAsync(result.binaryPath, [], { encoding: "utf8" });
  expect(stdout.split("\n")).toEqual([
    "iso-2022-jp SC2020 Streaming TextDecoder for ISO-2022-JP has no native lowering",
    "gbk carried SC2020 Streaming TextDecoder over a malformed sequence that starts in an earlier chunk has no native lowering",
    "gb18030 carried SC2020 Streaming TextDecoder over a malformed sequence that starts in an earlier chunk has no native lowering",
    "euc-jp carried SC2020 Streaming TextDecoder over a malformed sequence that starts in an earlier chunk has no native lowering",
    "gbk in one chunk fffd 30 30",
    "gbk lead carried fffd 30 30",
    "gbk valid 80",
    "",
  ]);
}, 300_000);
