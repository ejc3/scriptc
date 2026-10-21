// An 'error' event ends events.on iteration by rejecting next(); a close
// event ends it as done after the queued events.
import { EventEmitter, on } from "node:events";

async function failing(): Promise<void> {
  const ee = new EventEmitter();
  setTimeout(() => {
    ee.emit("data", "first");
    ee.emit("error", new Error("broken pipe"));
    ee.emit("data", "late");
  }, 1);
  try {
    for await (const [value] of on(ee, "data")) console.log("data", value);
    console.log("not reached");
  } catch (e) {
    console.log("caught", (e as Error).message, ee.listenerCount("data"), ee.listenerCount("error"));
  }
}

async function closing(): Promise<void> {
  const ee = new EventEmitter();
  setTimeout(() => {
    ee.emit("line", "one");
    ee.emit("line", "two");
    ee.emit("end");
    ee.emit("line", "three");
  }, 1);
  const lines: string[] = [];
  for await (const [line] of on(ee, "line", { close: ["end"] })) lines.push(String(line));
  console.log("lines", lines.join(","), ee.listenerCount("line"), ee.listenerCount("end"));
}

async function errorWhileBusy(): Promise<void> {
  const ee = new EventEmitter();
  setTimeout(() => ee.emit("job", 1), 1);
  try {
    for await (const [id] of on(ee, "job")) {
      console.log("job", id);
      // Queued behind the error: the error is stored, and the next call
      // rejects.
      ee.emit("error", new Error("stop " + id));
    }
  } catch (e) {
    console.log("busy caught", (e as Error).message);
  }
}

failing()
  .then(closing)
  .then(errorWhileBusy)
  .then(() => console.log("done"));
