// captureRejections routes an async listener's rejection to 'error'.
import { EventEmitter } from "node:events";

const ee = new EventEmitter({ captureRejections: true });
const boom = new Error("boom");
ee.on("error", (err: Error) => {
  console.log("error listener:", err === boom, err.message);
});
const job = async (n: number): Promise<void> => {
  console.log("job", n);
  if (n > 1) throw boom;
};
ee.on("job", job);
console.log("count", ee.listenerCount("job"));
ee.emit("job", 1);
ee.emit("job", 2);
process.nextTick(() => console.log("tick"));
Promise.resolve().then(() => console.log("microtask"));
console.log("sync end");

setTimeout(() => {
  ee.off("job", job);
  console.log("after off", ee.listenerCount("job"));
  ee.emit("job", 3);
  ee.once("job", async (n: number): Promise<void> => {
    await new Promise<void>((resolve) => setTimeout(resolve, 5));
    throw new Error(`once ${n}`);
  });
  ee.prependListener("job", async (n: number): Promise<void> => {
    console.log("prepended", n);
  });
  console.log("emitted", ee.emit("job", 4), ee.listenerCount("job"));
}, 10);
