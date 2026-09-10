import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const cpp = fs.readFileSync(path.join(root, "native-network/MultiplayerProtocol.hpp"), "utf8");
const worker = fs.readFileSync(path.join(root, "multiplayer-server/src/protocol.ts"), "utf8");

function read(name, text, pattern) {
  const match = text.match(pattern);
  if (!match) throw new Error(`Missing ${name}`);
  return Number(match[1]);
}

const cppProtocol = read("C++ protocol", cpp, /PROTOCOL_VERSION\s*=\s*(\d+)/);
const cppGameplay = read("C++ gameplay", cpp, /GAMEPLAY_VERSION\s*=\s*(\d+)/);
const workerProtocol = read("Worker protocol", worker, /PROTOCOL_VERSION\s*=\s*(\d+)/);

const failures = [];
if (cppProtocol !== workerProtocol) failures.push(`protocol C++=${cppProtocol} Worker=${workerProtocol}`);

if (failures.length) {
  console.error(`Protocol consistency failed: ${failures.join("; ")}`);
  process.exit(1);
}

console.log(`Protocol consistency OK: protocol=${cppProtocol} nativeGameplay=${cppGameplay}`);
