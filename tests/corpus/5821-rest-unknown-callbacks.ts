// A `(...args: unknown[])` function passed as an array callback receives
// every argument the method passes (the element, its index and the array),
// and as a promise handler it receives the settled value, as in Node.
const seen: unknown[][] = [];
const log = (...args: unknown[]): void => {
  seen.push(args);
};
const first = (...args: unknown[]): unknown => args[0];

[10, 20].forEach(log);
console.log(["a", "b"].map(first));
console.log(seen);

Promise.resolve(7)
  .then(log)
  .then(() => {
    console.log(seen.at(-1));
    return Promise.resolve("next");
  })
  .then(first)
  .then((v) => console.log("then", v));
