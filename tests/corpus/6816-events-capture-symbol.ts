// A [captureRejectionSymbol] method takes the rejection instead of 'error'.
import { EventEmitter, captureRejectionSymbol } from "node:events";

class Jobs extends EventEmitter {
  label = "jobs";
  constructor() {
    super({ captureRejections: true });
  }
  [captureRejectionSymbol](err: Error, event: string, ...args: unknown[]): void {
    console.log("rejection:", this.label, this === jobs, err.message, event, JSON.stringify(args));
  }
}

const jobs = new Jobs();
console.log(captureRejectionSymbol === Symbol.for("nodejs.rejection"));
console.log(EventEmitter.captureRejectionSymbol === captureRejectionSymbol);
console.log(typeof captureRejectionSymbol, captureRejectionSymbol.toString(), captureRejectionSymbol.description);
jobs.on("error", (err: Error) => console.log("unexpected error listener", err.message));
jobs.on("run", async (id: number, name: string): Promise<void> => {
  throw new Error(`run ${id} ${name}`);
});
jobs.on("run", async (id: number): Promise<void> => {
  if (id === 2) throw new TypeError(`second ${id}`);
});
jobs.emit("run", 1, "alpha");
jobs.emit("run", 2, "beta");
console.log("sync end");
