// execa 5's export shape: `const execa = (file, args, options) => …` exported
// whole behind identifier and destructuring requires, then members attached
// to it (`module.exports.command = …`). A default import reads them.
import execa from "gtexecashape";

console.log(execa("/usr/bin/node", ["-v"]));
console.log(execa.command("git status --short", { maxBuffer: 10 }));
console.log(execa.sync("ls", []));
