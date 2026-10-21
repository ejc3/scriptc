// A resizable ArrayBuffer resized only through computed member keys on an
// untyped value: typed arrays over it check their bounds, and their lengths
// follow the storage.
const report = (label, f) => {
  try {
    console.log(label, f());
  } catch (e) {
    console.log(label, e.name, e.message);
  }
};
function same(x) {
  return x;
}
const storage = same(new ArrayBuffer(8, { maxByteLength: 16 }));
const fixedView = new Uint8Array(storage, 0, 8);
const trackingView = new Uint8Array(storage);
fixedView.fill(3);
const key = ["re", "size"].join("");
const sum = (view) => {
  let total = 0;
  for (let i = 0; i < view.length; i++) total += view[i];
  return total;
};
console.log(sum(fixedView), sum(trackingView));
storage[key](4);
console.log(storage.byteLength, fixedView.length, fixedView.byteLength, trackingView.length);
console.log(sum(fixedView), sum(trackingView));
report("fill", () => fixedView.fill(1));
report("join", () => fixedView.join());
report("tracking join", () => trackingView.join());
const methodName = "resize";
const { [methodName]: resize } = storage;
resize.call(storage, 12);
console.log(storage.byteLength, fixedView.length, trackingView.length, sum(fixedView), sum(trackingView));
report("fill again", () => fixedView.fill(2).join());
