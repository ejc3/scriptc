// events.on through the EventEmitter static and a namespace import; the
// loop body's continue, throw and return each leave the iterator as Node's
// for-await does (return() stops the listening).
import * as events from "node:events";
import { EventEmitter } from "node:events";

async function viaStatic(ee: EventEmitter): Promise<number> {
  let total = 0;
  for await (const [n] of EventEmitter.on(ee, "n")) {
    if ((n as number) % 2 === 0) continue;
    total += n as number;
    if ((n as number) >= 5) return total;
  }
  return -1;
}

async function viaNamespace(ee: EventEmitter): Promise<string> {
  try {
    for await (const args of events.on(ee, "msg")) {
      if (args[0] === "bad") throw new Error("bad message " + args.length);
      console.log("msg", args[0]);
    }
  } catch (e) {
    return "threw " + (e as Error).message + " " + ee.listenerCount("msg");
  }
  return "unreachable";
}

const a = new EventEmitter();
const b = new EventEmitter();
viaStatic(a).then((total) => console.log("static total", total, a.listenerCount("n")));
viaNamespace(b).then((r) => console.log(r));
setTimeout(() => {
  for (let i = 1; i <= 6; i++) a.emit("n", i);
  b.emit("msg", "hello", 1);
  b.emit("msg", "bad", 2);
  b.emit("msg", "after", 3);
}, 1);
