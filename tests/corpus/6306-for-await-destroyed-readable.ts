// for await over a Readable that is destroyed before it ends: the loop
// reads nothing more once the stream is destroyed, and after 'close' it
// throws ERR_STREAM_PREMATURE_CLOSE, unless the destroy carried an error,
// which the loop throws instead. A stream that ended before the destroy
// finishes the loop normally.
import { Readable } from "node:stream";

async function drain(label: string, stream: Readable, order: string[]): Promise<void> {
  try {
    for await (const chunk of stream) order.push(`chunk ${String(chunk)}`);
    order.push("loop done");
  } catch (e) {
    const error = e as NodeJS.ErrnoException;
    order.push(`threw ${error.code ?? error.message}`);
  }
  console.log(label, order.join(", "));
}

async function main(): Promise<void> {
  // Destroyed after one chunk was read.
  const afterChunk = new Readable({ read() {} });
  const first: string[] = [];
  afterChunk.on("close", () => first.push("close"));
  afterChunk.push("seen");
  setTimeout(() => afterChunk.destroy(), 1);
  await drain("after a chunk:", afterChunk, first);

  // Destroyed before any data arrived.
  const empty = new Readable({ read() {} });
  const second: string[] = [];
  empty.on("close", () => second.push("close"));
  setTimeout(() => empty.destroy(), 1);
  await drain("before data:", empty, second);

  // Destroyed with an error: the loop throws that error.
  const failing = new Readable({ read() {} });
  const third: string[] = [];
  failing.on("error", () => third.push("error event"));
  failing.push("a");
  setTimeout(() => failing.destroy(new Error("boom")), 1);
  await drain("with an error:", failing, third);

  // Ended and fully read before the destroy: no premature close.
  const ended = Readable.from(["x", "y"]);
  const fourth: string[] = [];
  await drain("ended first:", ended, fourth);
  ended.destroy();

  // Destroyed while the loop body is still running.
  const busy = new Readable({ read() {} });
  const fifth: string[] = [];
  busy.push("1");
  busy.push("2");
  try {
    for await (const chunk of busy) {
      fifth.push(`chunk ${String(chunk)}`);
      busy.destroy();
    }
    fifth.push("loop done");
  } catch (e) {
    fifth.push(`threw ${(e as NodeJS.ErrnoException).code}`);
  }
  console.log("destroyed in the body:", fifth.join(", "));
}

void main();
