// events.on registers and removes its listeners through on() and
// removeListener(), so 'newListener' and 'removeListener' see them, in
// Node's order; a whole-arguments binding sees every argument.
import { EventEmitter, on } from "node:events";

const ee = new EventEmitter();
ee.on("newListener", (name: string) => console.log("new", name));
ee.on("removeListener", (name: string) => console.log("removed", name));

async function consume(): Promise<void> {
  for await (const args of on(ee, "pair", { close: ["finish", "abort-all"] })) {
    console.log("pair", args.length, args[0], args[1]);
    if (args[0] === "stop") break;
  }
  console.log("loop over");
}
const done = consume();
setTimeout(() => {
  ee.emit("pair", "a", 1);
  ee.emit("pair", "stop", 2);
}, 1);
done.then(() => console.log("listeners", ee.listenerCount("pair"), ee.eventNames().join(",")));
