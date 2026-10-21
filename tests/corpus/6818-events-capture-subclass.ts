// The captured 'error' emit runs with capture off; a symbol handler sees
// the events.captureRejectionSymbol spelling; once listeners capture too.
import events, { EventEmitter } from "node:events";

const ee = new EventEmitter({ captureRejections: true });
let seen = 0;
ee.on("error", (err: Error) => {
  seen++;
  console.log("error", seen, err.message, ee.listenerCount("error"));
});
ee.once("ping", async (from: string): Promise<void> => {
  await null;
  throw new RangeError(`ping from ${from}`);
});
ee.emit("ping", "a");
console.log("once left", ee.listenerCount("ping"));
console.log(events.captureRejectionSymbol === Symbol.for("nodejs.rejection"));
console.log(events.captureRejections, EventEmitter.captureRejections);
events.captureRejections = true;
console.log(events.captureRejections, EventEmitter.captureRejections);
events.captureRejections = false;

class Tracker extends EventEmitter {
  calls: string[] = [];
  [events.captureRejectionSymbol](err: Error, event: string): void {
    this.calls.push(`${event}:${err.message}`);
    console.log("tracker", this.calls.join(","));
  }
}
const t = new Tracker({ captureRejections: true });
const fail = async (n: number): Promise<number> => {
  if (n % 2) throw new Error(`odd ${n}`);
  return n;
};
t.on("n", fail);
for (let i = 0; i < 4; i++) t.emit("n", i);
t.removeListener("n", fail);
t.emit("n", 5);
setTimeout(() => console.log("done", t.calls.length), 5);
