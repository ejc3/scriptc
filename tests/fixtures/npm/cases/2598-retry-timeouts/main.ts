// retry 0.13.1 (vendored unmodified, MIT): `exports.timeouts` reads
// `this.createTimeout`, and `exports.operation` calls it through the exports
// object. async-retry wraps every request in such an operation.
import retry from "retry";

console.log(retry.timeouts({ retries: 4, minTimeout: 10, maxTimeout: 100, factor: 2 }).join(","));
const op = retry.operation({ retries: 2, minTimeout: 1, maxTimeout: 2 });
op.attempt((attempt: number) => {
  console.log("attempt", attempt);
  if (op.retry(attempt < 3 ? new Error(`fail ${attempt}`) : null)) return;
  console.log("done", op.attempts(), op.errors().length);
});
