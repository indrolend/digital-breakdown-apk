#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), "..");
const modelPath = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.join(root, "native-models", "phone.dbmesh");
const bytes = fs.readFileSync(modelPath);
let at = 0;
if (bytes.toString("ascii", 0, 4) !== "DBM1") throw new Error(`${modelPath} is not a DBM1 model`);
at += 4;
const vertexCount = bytes.readUInt32LE(at);
at += 4;
const batchCount = bytes.readUInt32LE(at);
at += 4;
const min = [Infinity, Infinity, Infinity];
const max = [-Infinity, -Infinity, -Infinity];
let surfaceArea = 0;
const vertices = [];
for (let i = 0; i < vertexCount; ++i) {
  const p = [bytes.readFloatLE(at), bytes.readFloatLE(at + 4), bytes.readFloatLE(at + 8)];
  at += 12;
  vertices.push(p);
  for (let axis = 0; axis < 3; ++axis) {
    min[axis] = Math.min(min[axis], p[axis]);
    max[axis] = Math.max(max[axis], p[axis]);
  }
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [
  a[1] * b[2] - a[2] * b[1],
  a[2] * b[0] - a[0] * b[2],
  a[0] * b[1] - a[1] * b[0],
];
const len = (v) => Math.hypot(v[0], v[1], v[2]);
for (let i = 0; i + 2 < vertices.length; i += 3) {
  surfaceArea += len(cross(sub(vertices[i + 1], vertices[i]), sub(vertices[i + 2], vertices[i]))) * 0.5;
}
const batches = [];
for (let i = 0; i < batchCount; ++i) {
  const start = bytes.readUInt32LE(at);
  const count = bytes.readUInt32LE(at + 4);
  const color = [
    bytes.readFloatLE(at + 8),
    bytes.readFloatLE(at + 12),
    bytes.readFloatLE(at + 16),
    bytes.readFloatLE(at + 20),
  ];
  at += 24;
  batches.push({ start, count, color });
}
const size = max.map((v, i) => v - min[i]);
const center = max.map((v, i) => (v + min[i]) * 0.5);
const fmt = (v) => v.toFixed(5);
console.log(`model=${modelPath}`);
console.log(`vertices=${vertexCount} triangles=${Math.floor(vertexCount / 3)} batches=${batchCount}`);
console.log(`bounds x=${fmt(min[0])}..${fmt(max[0])} y=${fmt(min[1])}..${fmt(max[1])} z=${fmt(min[2])}..${fmt(max[2])}`);
console.log(`size width=${fmt(size[0])} height=${fmt(size[1])} depth=${fmt(size[2])}`);
console.log(`center x=${fmt(center[0])} y=${fmt(center[1])} z=${fmt(center[2])}`);
console.log(`surfaceArea=${fmt(surfaceArea)}`);
console.log("recommended shared constants:");
console.log(`  PHONE_BODY_WIDTH  ~= ${fmt(size[0])}`);
console.log(`  PHONE_BODY_HEIGHT ~= ${fmt(size[1])}`);
console.log(`  PHONE_BODY_DEPTH  ~= ${fmt(size[2])}`);
console.log(`  visual half extents = {${fmt(size[0] * 0.5)}, ${fmt(size[1] * 0.5)}, ${fmt(size[2] * 0.5)}}`);
for (const [i, batch] of batches.entries()) {
  console.log(`batch ${i}: start=${batch.start} count=${batch.count} color=${batch.color.map(fmt).join(",")}`);
}
