// http.Agent's maxTotalSockets from untyped code: validation of values
// whose type is known only at run time, and the property read and set
// through an untyped agent.
"use strict";
const http = require("node:http");

function attempt(label, run) {
  try {
    console.log(label, "ok", run());
  } catch (e) {
    console.log(label, e.name, e.code, e.message);
  }
}

function make(value) {
  return new http.Agent({ maxTotalSockets: value });
}

for (const value of [undefined, 2, "2", null, true, {}, 0.5, 10n])
  attempt(`value ${typeof value} ${String(value)}`, () => make(value).maxTotalSockets);

function inspectAgent(agent) {
  agent.maxTotalSockets = 7;
  return [agent.maxTotalSockets, agent.totalSocketCount, typeof agent.maxTotalSockets].join(" ");
}
attempt("untyped agent", () => inspectAgent(make(3)));
