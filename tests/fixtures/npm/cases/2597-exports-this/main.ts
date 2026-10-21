// Exported functions that call sibling exports through `this`: a call
// through the exports object (`lib.total()`, `exports.scale()` inside the
// module) passes that object as `this`.
import lib from "gtexportsthis";

console.log(lib.scale(2), lib.total([1, 2, 3]));
