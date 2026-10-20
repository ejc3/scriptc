"use strict";
function counted() {
  return counted.total;
}
module.exports = counted;
module.exports.total = 1;
module.exports.label = "one";
module.exports.unit = "item";
