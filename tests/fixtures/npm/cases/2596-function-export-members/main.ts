// cross-spawn 7's export shape: `module.exports = spawn` behind require
// bindings, then typed function members (`module.exports.sync = spawnSync`).
import xs from "gtspawnshape";

console.log(typeof xs);
