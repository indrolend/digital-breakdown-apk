import { createReadStream, statSync } from "node:fs";
import { mkdir, writeFile } from "node:fs/promises";
import { createServer } from "node:http";
import { extname, resolve, sep } from "node:path";

const root = resolve(new URL("../..", import.meta.url).pathname.replace(/^\/(.:)/, "$1"));
const port = Number(process.argv[2] || 8765);
const host = process.argv[3] || "127.0.0.1";
const types = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".glb": "model/gltf-binary",
  ".fbx": "application/octet-stream",
  ".mp3": "audio/mpeg"
};

createServer(async (request, response) => {
  if (request.method === "POST" && request.url === "/__pass7_oracle_trace") {
    let body = "";
    for await (const chunk of request) {
      body += chunk;
      if (body.length > 8 * 1024 * 1024) {
        response.writeHead(413).end("Trace too large");
        return;
      }
    }
    try {
      JSON.parse(body);
      const traceDir = resolve(root, "build", "pass7-oracle", "traces");
      await mkdir(traceDir, { recursive: true });
      await writeFile(resolve(traceDir, "pass7-vacuum-crosshair-suite.json"), body + "\n");
      response.writeHead(204).end();
    } catch {
      response.writeHead(400).end("Invalid trace JSON");
    }
    return;
  }
  const requestPath = decodeURIComponent(new URL(request.url, `http://${request.headers.host}`).pathname);
  const relative = requestPath === "/" ? "build/pass7-oracle/index.html" : requestPath.slice(1);
  const path = resolve(root, relative);
  if (path !== root && !path.startsWith(root + sep)) {
    response.writeHead(403).end("Forbidden");
    return;
  }
  try {
    if (!statSync(path).isFile()) throw new Error("not a file");
    response.writeHead(200, {
      "Content-Type": types[extname(path).toLowerCase()] || "application/octet-stream",
      "Cache-Control": "no-store"
    });
    createReadStream(path).pipe(response);
  } catch {
    response.writeHead(404).end("Not found");
  }
}).listen(port, host, () => {
  console.log(`Pass 7 oracle listening on ${host}:${port}`);
});
