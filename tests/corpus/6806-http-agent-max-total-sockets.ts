// http.Agent's maxTotalSockets: option validation, the property and its
// default, the cap across origins (a request over the cap queues; a
// finished socket starts the first queued origin's request when that
// origin has no socket in use), the order requests reach the servers,
// and keep-alive sockets over the cap that close instead of pooling.
import * as http from "node:http";
import type { AddressInfo } from "node:net";

function attempt(label: string, run: () => unknown): void {
  try {
    console.log(label, "ok", run());
  } catch (e) {
    const err = e as Error & { code?: string };
    console.log(label, err.name, err.code, err.message);
  }
}

attempt("default", () => new http.Agent().maxTotalSockets);
attempt("one", () => new http.Agent({ maxTotalSockets: 1 }).maxTotalSockets);
attempt("fraction", () => new http.Agent({ maxTotalSockets: 1.5 }).maxTotalSockets);
attempt("infinity", () => new http.Agent({ maxTotalSockets: Infinity }).maxTotalSockets);
attempt("zero", () => new http.Agent({ maxTotalSockets: 0 }));
attempt("negative", () => new http.Agent({ maxTotalSockets: -1 }));
attempt("NaN", () => new http.Agent({ maxTotalSockets: NaN }));
attempt("scheduling first", () => new http.Agent({ scheduling: "x" as "fifo", maxTotalSockets: 0 }));
attempt("set", () => {
  const agent = new http.Agent({ maxTotalSockets: 2 });
  agent.maxTotalSockets = 5;
  return agent.maxTotalSockets;
});

const arrivals: string[] = [];
function server(label: string): Promise<http.Server> {
  return new Promise((resolve) => {
    const s = http.createServer((req, res) => {
      arrivals.push(`${label}${req.url}`);
      setTimeout(() => res.end(`${label}${req.url}`), 20);
    });
    // Idle keep-alive sockets close soon, which frees the agent's one socket.
    s.keepAliveTimeout = 200;
    s.listen(0, "127.0.0.1", () => resolve(s));
  });
}

function get(agent: http.Agent, s: http.Server, path: string, log: string[]): Promise<void> {
  const { port } = s.address() as AddressInfo;
  return new Promise((resolve, reject) => {
    const req = http.get({ host: "127.0.0.1", port, path, agent }, (res) => {
      let body = "";
      res.setEncoding("utf8");
      res.on("data", (chunk: string) => (body += chunk));
      res.on("end", () => {
        log.push(body);
        resolve();
      });
    });
    req.on("error", reject);
  });
}

async function main(): Promise<void> {
  const a = await server("A");
  const b = await server("B");

  // One socket across both origins: B waits for A's socket to close.
  const capped = new http.Agent({ maxTotalSockets: 1 });
  const order: string[] = [];
  const first = [get(capped, a, "/1", order), get(capped, b, "/1", order), get(capped, a, "/2", order)];
  console.log("in use after queueing", capped.totalSocketCount);
  await Promise.all(first);
  console.log("capped responses", order.join(" "));
  console.log("capped arrivals", arrivals.splice(0).join(" "));

  // Two sockets and maxSockets 1 per origin.
  const two = new http.Agent({ maxTotalSockets: 2, maxSockets: 1 });
  const order2: string[] = [];
  await Promise.all([
    get(two, a, "/1", order2),
    get(two, a, "/2", order2),
    get(two, b, "/1", order2),
    get(two, b, "/2", order2),
  ]);
  console.log("two responses", order2.join(" "));
  console.log("two arrivals", arrivals.splice(0).join(" "));

  // keepAlive with one socket: the free socket serves the next request to
  // its origin; a request to the other origin waits for it to close.
  const keep = new http.Agent({ keepAlive: true, maxTotalSockets: 1 });
  const order3: string[] = [];
  await get(keep, a, "/1", order3);
  await get(keep, a, "/2", order3);
  console.log("kept sockets", keep.totalSocketCount, Object.keys(keep.freeSockets).length);
  await get(keep, b, "/1", order3).catch((e: unknown) => order3.push(`error ${e instanceof Error ? e.message : e}`));
  console.log("keep responses", order3.join(" "));
  console.log("keep arrivals", arrivals.splice(0).join(" "));
  keep.destroy();
  capped.destroy();
  two.destroy();
  a.close();
  b.close();
}

main();
