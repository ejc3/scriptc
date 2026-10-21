// An AbortSignal ends events.on iteration with an AbortError: one already
// aborted throws at the call, a later abort rejects the waiting next().
import { EventEmitter, on } from "node:events";

async function aborted(): Promise<void> {
  const ee = new EventEmitter();
  const controller = new AbortController();
  controller.abort("gone");
  try {
    for await (const [x] of on(ee, "x", { signal: controller.signal })) console.log("x", x);
  } catch (e) {
    const err = e as NodeJS.ErrnoException;
    console.log("aborted up front", err.name, err.code, err.cause, ee.listenerCount("x"));
  }
}

async function abortLater(): Promise<void> {
  const ee = new EventEmitter();
  const controller = new AbortController();
  setTimeout(() => {
    ee.emit("x", 1);
    setTimeout(() => controller.abort(), 1);
  }, 1);
  try {
    for await (const [x] of on(ee, "x", { signal: controller.signal })) console.log("x", x);
  } catch (e) {
    const err = e as NodeJS.ErrnoException;
    console.log("aborted later", err.name, err.code, err.message, ee.listenerCount("x"));
  }
}

async function notASignal(): Promise<void> {
  const ee = new EventEmitter();
  const fake: unknown = {};
  try {
    for await (const [x] of on(ee, "x", { signal: fake as AbortSignal })) console.log("x", x);
  } catch (e) {
    const err = e as NodeJS.ErrnoException;
    console.log("bad signal", err.name, err.code);
  }
}

aborted()
  .then(abortLater)
  .then(notASignal)
  .then(() => console.log("done"));
