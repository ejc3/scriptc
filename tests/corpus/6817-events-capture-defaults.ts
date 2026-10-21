// EventEmitter.captureRejections sets the default for later emitters.
import { EventEmitter } from "node:events";

console.log("default", EventEmitter.captureRejections);
const before = new EventEmitter();
EventEmitter.captureRejections = true;
console.log("now", EventEmitter.captureRejections);
const after = new EventEmitter();
const off = new EventEmitter({ captureRejections: false });
const fromEmpty = new EventEmitter({});
class Sub extends EventEmitter {}
const sub = new Sub();
EventEmitter.captureRejections = false;

const fail = async (tag: string): Promise<void> => {
  throw new Error(`fail ${tag}`);
};
function wire(tag: string, ee: EventEmitter): void {
  ee.on("error", (err: Error) => console.log(tag, "error:", err.message));
  ee.on("go", fail);
  ee.emit("go", tag);
}
wire("after", after);
wire("fromEmpty", fromEmpty);
wire("sub", sub);
process.on("unhandledRejection", (reason: unknown) => {
  console.log("unhandled", (reason as Error).message);
});
before.on("go", fail);
before.emit("go", "before");
// A false option is falsy, so it takes the default, which was true.
off.on("error", (err: Error) => console.log("off error:", err.message));
off.on("go", fail);
off.emit("go", "off");

try {
  const one = JSON.parse("1");
  EventEmitter.captureRejections = one;
} catch (e) {
  const err = e as NodeJS.ErrnoException;
  console.log(err.code, err.message);
}
try {
  const options = JSON.parse('{"captureRejections":"yes"}');
  new EventEmitter(options);
} catch (e) {
  const err = e as NodeJS.ErrnoException;
  console.log(err.code, err.message);
}
console.log("still", EventEmitter.captureRejections);
