import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../..");
const sourcePath = path.join(root, "reference/browser-pass7/assets/embedded-assets.js");
const outDir = path.join(root, "build/pass7-oracle/assets");
const source = fs.readFileSync(sourcePath, "utf8");

function embedded(name) {
  const match = source.match(new RegExp("const\\s+" + name + "\\s*=\\s*`([\\s\\S]*?)`;"));
  if (!match) throw new Error(`Missing embedded asset ${name}`);
  return Buffer.from(match[1].replace(/\s/g, ""), "base64");
}

function embeddedAudio() {
  const block = source.match(/const\s+SFX_AUDIO_BASE64\s*=\s*Object\.freeze\(\{([\s\S]*?)\}\);/);
  if (!block) throw new Error("Missing embedded SFX_AUDIO_BASE64");
  const entries = {};
  for (const match of block[1].matchAll(/([A-Za-z0-9_]+)\s*:\s*"([A-Za-z0-9+/=\s]+)"\s*,?/g)) {
    entries[match[1]] = Buffer.from(match[2].replace(/\s/g, ""), "base64");
  }
  if (!Object.keys(entries).length) throw new Error("Embedded SFX object was empty");
  return entries;
}

function parseGlb(bytes) {
  if (bytes.toString("ascii", 0, 4) !== "glTF") throw new Error("Not a GLB");
  const version = bytes.readUInt32LE(4);
  const declaredLength = bytes.readUInt32LE(8);
  let offset = 12;
  let json;
  let binary;
  while (offset < declaredLength) {
    const length = bytes.readUInt32LE(offset);
    const type = bytes.toString("ascii", offset + 4, offset + 8);
    const chunk = bytes.subarray(offset + 8, offset + 8 + length);
    if (type === "JSON") json = JSON.parse(chunk.toString("utf8").replace(/\0+$/, "").trim());
    if (type === "BIN\0") binary = chunk;
    offset += 8 + length;
  }
  if (!json || !binary) throw new Error("GLB is missing JSON or BIN chunk");
  return { version, declaredLength, json, binary };
}

const identity = () => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
function multiply(a, b) {
  const out = Array(16).fill(0);
  for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++)
    for (let k = 0; k < 4; k++) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return out;
}
function nodeMatrix(node) {
  if (node.matrix) return node.matrix;
  const [x, y, z, w] = node.rotation ?? [0, 0, 0, 1];
  const [sx, sy, sz] = node.scale ?? [1, 1, 1];
  const [tx, ty, tz] = node.translation ?? [0, 0, 0];
  return [
    (1 - 2*y*y - 2*z*z)*sx, (2*x*y + 2*z*w)*sx, (2*x*z - 2*y*w)*sx, 0,
    (2*x*y - 2*z*w)*sy, (1 - 2*x*x - 2*z*z)*sy, (2*y*z + 2*x*w)*sy, 0,
    (2*x*z + 2*y*w)*sz, (2*y*z - 2*x*w)*sz, (1 - 2*x*x - 2*y*y)*sz, 0,
    tx, ty, tz, 1
  ];
}
function transform(m, p) {
  return [m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12], m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13], m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]];
}
function emptyBounds() { return { min: [Infinity, Infinity, Infinity], max: [-Infinity, -Infinity, -Infinity] }; }
function include(bounds, p) { for (let i=0;i<3;i++) { bounds.min[i]=Math.min(bounds.min[i],p[i]); bounds.max[i]=Math.max(bounds.max[i],p[i]); } }

function auditGlb(name, bytes, normalizedHeight = null) {
  const { version, declaredLength, json, binary } = parseGlb(bytes);
  const bounds = emptyBounds();
  const meshBounds = [];
  const component = { 5120:["getInt8",1], 5121:["getUint8",1], 5122:["getInt16",2], 5123:["getUint16",2], 5125:["getUint32",4], 5126:["getFloat32",4] };
  function positions(accessorIndex) {
    const a = json.accessors[accessorIndex], v = json.bufferViews[a.bufferView];
    const [getter, width] = component[a.componentType];
    const stride = v.byteStride ?? width * 3;
    const start = (v.byteOffset ?? 0) + (a.byteOffset ?? 0);
    const view = new DataView(binary.buffer, binary.byteOffset, binary.byteLength);
    return Array.from({length:a.count}, (_, i) => [0,1,2].map(k => view[getter](start+i*stride+k*width, true)));
  }
  function walk(index, parent, lineage) {
    const node = json.nodes[index], world = multiply(parent, nodeMatrix(node));
    const trail = [...lineage, node.name ?? `node-${index}`];
    if (node.mesh !== undefined) {
      const local = emptyBounds();
      for (const primitive of json.meshes[node.mesh].primitives) {
        if (primitive.attributes.POSITION === undefined) continue;
        for (const p of positions(primitive.attributes.POSITION)) { const q=transform(world,p); include(local,q); include(bounds,q); }
      }
      meshBounds.push({ node:index, nodeName:node.name ?? null, mesh:node.mesh, meshName:json.meshes[node.mesh].name ?? null, lineage:trail, bounds:local,
        materials:json.meshes[node.mesh].primitives.map(p => p.material === undefined ? null : json.materials?.[p.material]?.name ?? `material-${p.material}`) });
    }
    for (const child of node.children ?? []) walk(child, world, trail);
  }
  const scene = json.scenes[json.scene ?? 0];
  for (const index of scene.nodes ?? []) walk(index, identity(), []);
  const size = bounds.max.map((v,i)=>v-bounds.min[i]);
  const center = bounds.max.map((v,i)=>(v+bounds.min[i])/2);
  const scale = normalizedHeight ? normalizedHeight / Math.max(size[1], 0.0001) : 1;
  const normalizedBounds = { min: bounds.min.map((v,i)=>(v-center[i])*scale), max: bounds.max.map((v,i)=>(v-center[i])*scale) };
  return {
    name, sha256:crypto.createHash("sha256").update(bytes).digest("hex"), bytes:bytes.length, glb:{version,declaredLength},
    counts:{scenes:json.scenes?.length??0,nodes:json.nodes?.length??0,meshes:json.meshes?.length??0,materials:json.materials?.length??0,textures:json.textures?.length??0,images:json.images?.length??0,animations:json.animations?.length??0},
    asset:json.asset, rawBounds:bounds, rawSize:size, rawCenter:center, runtimeNormalization:normalizedHeight ? {height:normalizedHeight,scale,normalizedBounds,normalizedSize:normalizedBounds.max.map((v,i)=>v-normalizedBounds.min[i])}:null,
    materials:(json.materials??[]).map((m,i)=>({index:i,name:m.name??null,alphaMode:m.alphaMode??"OPAQUE",doubleSided:m.doubleSided??false,pbr:m.pbrMetallicRoughness??null,emissiveFactor:m.emissiveFactor??null})),
    nodes:(json.nodes??[]).map((n,i)=>({index:i,name:n.name??null,children:n.children??[],mesh:n.mesh??null,translation:n.translation??null,rotation:n.rotation??null,scale:n.scale??null,matrix:n.matrix??null})),
    meshBounds
  };
}

fs.mkdirSync(outDir, { recursive:true });
const phoneBytes = embedded("IPHONE_GLB_BASE64");
const flowerBytes = embedded("PENTAGONAL_FLOWER_GLB_BASE64");
const humanBytes = embedded("HUMAN_FBX_BASE64");
const audioBytes = embeddedAudio();
const flowerAudit = auditGlb("PENTAGONAL_FLOWER_GLB_BASE64",flowerBytes);
const flowerScale = 0.72 / Math.max(...flowerAudit.rawSize, 0.0001);
flowerAudit.runtimeNormalization = {
  maxDimension:0.72,
  scale:flowerScale,
  normalizedBounds:{
    min:flowerAudit.rawBounds.min.map((v,i)=>(v-flowerAudit.rawCenter[i])*flowerScale),
    max:flowerAudit.rawBounds.max.map((v,i)=>(v-flowerAudit.rawCenter[i])*flowerScale)
  },
  normalizedSize:flowerAudit.rawSize.map(v=>v*flowerScale)
};
const report = { source:path.relative(root,sourcePath).replaceAll("\\","/"), phone:auditGlb("IPHONE_GLB_BASE64",phoneBytes,0.16), flower:flowerAudit, human:{
  name:"HUMAN_FBX_BASE64", sha256:crypto.createHash("sha256").update(humanBytes).digest("hex"), bytes:humanBytes.length,
  header:humanBytes.subarray(0,23).toString("ascii").replace(/\0/g, "\\0")
}, audio:Object.fromEntries(Object.entries(audioBytes).map(([name,bytes])=>[name,{bytes:bytes.length,sha256:crypto.createHash("sha256").update(bytes).digest("hex")}])) };
fs.writeFileSync(path.join(outDir,"iphone.glb"),phoneBytes);
fs.writeFileSync(path.join(outDir,"pentagonal-flower.glb"),flowerBytes);
fs.writeFileSync(path.join(outDir,"walk-cycle.fbx"),humanBytes);
const audioDir=path.join(outDir,"audio");
fs.mkdirSync(audioDir,{recursive:true});
for(const [name,bytes] of Object.entries(audioBytes)) fs.writeFileSync(path.join(audioDir,`${name}.mp3`),bytes);
fs.writeFileSync(path.join(outDir,"asset-audit.json"),JSON.stringify(report,null,2)+"\n");
const screen = report.phone.meshBounds.filter(m => m.materials.some(n => /Screen_(BG|Glass|Rim)/.test(n??"")));
const md = `# Pass 7 embedded asset audit\n\n- Phone SHA-256: \`${report.phone.sha256}\`\n- Phone GLB bytes: ${report.phone.bytes}\n- Raw size: ${report.phone.rawSize.join(", ")}\n- Runtime scale: ${report.phone.runtimeNormalization.scale}\n- Runtime normalized size: ${report.phone.runtimeNormalization.normalizedSize.join(", ")}\n- Materials: ${report.phone.counts.materials}; nodes: ${report.phone.counts.nodes}; meshes: ${report.phone.counts.meshes}\n\n## Flower\n\n- SHA-256: \`${report.flower.sha256}\`\n- Runtime scale: ${report.flower.runtimeNormalization.scale}\n- Runtime normalized size: ${report.flower.runtimeNormalization.normalizedSize.join(", ")}\n\n## Screen meshes\n\n${screen.map(m=>`- ${m.nodeName ?? m.meshName}: ${m.materials.join(", ")} — min ${m.bounds.min.join(", ")}, max ${m.bounds.max.join(", ")}`).join("\n") || "No screen-named material meshes found."}\n`;
fs.writeFileSync(path.join(outDir,"asset-audit.md"),md);
console.log(JSON.stringify({output:path.join(outDir,"asset-audit.json"),phone:{sha256:report.phone.sha256,bytes:report.phone.bytes,counts:report.phone.counts,rawSize:report.phone.rawSize,normalization:report.phone.runtimeNormalization},human:report.human,screenMeshes:screen},null,2));
