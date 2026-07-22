#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), "..");
const modelPath = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.join(root, "native-models", "human.dbhuman");
const bytes = fs.readFileSync(modelPath);
let at = 0;
const magic = bytes.toString("ascii", at, at + 4);
at += 4;
if (magic !== "DBH1") throw new Error(`${modelPath} is not a DBH1 model`);
const u32 = () => {
  const value = bytes.readUInt32LE(at);
  at += 4;
  return value;
};
const i32 = () => {
  const value = bytes.readInt32LE(at);
  at += 4;
  return value;
};
const f32 = () => {
  const value = bytes.readFloatLE(at);
  at += 4;
  return value;
};
const vertexCount = u32();
const boneCount = u32();
const frameCount = u32();
u32();
const minY = f32();
const unitScale = f32();
const color = [f32(), f32(), f32(), f32()];
const bindMatrix = Array.from({ length: 16 }, () => f32());
at += 16 * 4 * 2;
const point = (m, p) => [
  m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
  m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
  m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14],
];

const influence = Array.from({ length: boneCount }, () => ({
  total: 0,
  dominant: 0,
  min: [Infinity, Infinity, Infinity],
  max: [-Infinity, -Infinity, -Infinity],
}));
for (let i = 0; i < vertexCount; ++i) {
  const rawPosition = [f32(), f32(), f32()];
  const bound = point(bindMatrix, rawPosition);
  const position = [bound[0] * unitScale, (bound[1] - minY) * unitScale, bound[2] * unitScale];
  const indices = [bytes.readUInt16LE(at), bytes.readUInt16LE(at + 2), bytes.readUInt16LE(at + 4), bytes.readUInt16LE(at + 6)];
  at += 8;
  const weights = [f32(), f32(), f32(), f32()];
  let dominant = 0;
  for (let k = 1; k < 4; ++k) if (weights[k] > weights[dominant]) dominant = k;
  for (let k = 0; k < 4; ++k) {
    const bone = indices[k];
    if (bone >= boneCount || weights[k] <= 0) continue;
    influence[bone].total += weights[k];
    if (k === dominant) {
      influence[bone].dominant += 1;
      for (let axis = 0; axis < 3; ++axis) {
        influence[bone].min[axis] = Math.min(influence[bone].min[axis], position[axis]);
        influence[bone].max[axis] = Math.max(influence[bone].max[axis], position[axis]);
      }
    }
  }
}

const bones = [];
for (let i = 0; i < boneCount; ++i) {
  const parent = i32();
  const flags = bytes.readUInt8(at++);
  const kind = bytes.readUInt8(at++);
  const side = bytes.readInt8(at++);
  at += 1;
  at += 16 * 4;
  bones.push({ parent, flags, kind, side });
}

const sideName = (side, flags) => side < 0 || flags & 4 ? "left" : side > 0 || flags & 8 ? "right" : "center";
const regionName = (bone) => {
  if (bone.kind === 1) return "shoulder";
  if (bone.kind === 2) return "upper-arm";
  if (bone.kind === 3) return "forearm";
  if (bone.kind === 4) return "hand";
  if (bone.flags & 2) return "head-neck";
  if (bone.flags & 1) return "spine-torso";
  return "other";
};
console.log(`model=${modelPath}`);
console.log(`vertices=${vertexCount} bones=${boneCount} frames=${frameCount} unitScale=${unitScale.toFixed(6)} color=${color.map((v) => v.toFixed(3)).join(",")}`);
console.log("idx parent side   region       flags kind dominant  y-range       x-range       z-range");
for (let i = 0; i < boneCount; ++i) {
  const inf = influence[i];
  const fmtRange = (axis) =>
    inf.dominant > 0
      ? `${inf.min[axis].toFixed(3)}..${inf.max[axis].toFixed(3)}`.padEnd(13)
      : "none".padEnd(13);
  console.log(
    `${String(i).padStart(3)} ${String(bones[i].parent).padStart(6)} ${sideName(bones[i].side, bones[i].flags).padEnd(6)} ${regionName(bones[i]).padEnd(12)} ` +
      `${String(bones[i].flags).padStart(5)} ${String(bones[i].kind).padStart(4)} ${String(inf.dominant).padStart(8)}  ${fmtRange(1)} ${fmtRange(0)} ${fmtRange(2)}`
  );
}
