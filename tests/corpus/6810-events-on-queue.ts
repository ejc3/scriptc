// events.on in a for-await loop: events emitted while the body runs queue
// up and arrive in order with their arguments; break removes the listeners.
import { EventEmitter, on } from "node:events";

const ee = new EventEmitter();

async function consume(): Promise<void> {
  let seen = 0;
  for await (const [n, label] of on(ee, "tick")) {
    console.log("tick", n, label, ee.listenerCount("tick"), ee.listenerCount("error"));
    if (n === 1) {
      // Emitted while the loop body runs: queued for the next iterations.
      ee.emit("tick", 2, "b");
      ee.emit("tick", 3, "c");
    }
    seen++;
    if (seen === 4) break;
  }
  console.log("after break", ee.listenerCount("tick"), ee.listenerCount("error"));
}

const done = consume();
setTimeout(() => {
  ee.emit("tick", 1, "a");
  setTimeout(() => ee.emit("tick", 4, "d"), 1);
}, 1);
done.then(() => console.log("consumed"));
