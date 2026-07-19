import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import * as THREE from "three";
import { FBXLoader } from "three/addons/loaders/FBXLoader.js";

const here=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(here,"../..");
const source=fs.readFileSync(path.join(root,"reference/browser-pass7/assets/embedded-assets.js"),"utf8");
const outDir=path.join(root,"native-models");

function embedded(name){const match=source.match(new RegExp("const\\s+"+name+"\\s*=\\s*`([\\s\\S]*?)`;"));if(!match)throw new Error(`Missing ${name}`);return Buffer.from(match[1].replace(/\s/g,""),"base64");}
function glb(bytes){let at=12,json,bin;while(at<bytes.length){const len=bytes.readUInt32LE(at),type=bytes.toString("ascii",at+4,at+8),data=bytes.subarray(at+8,at+8+len);if(type==="JSON")json=JSON.parse(data.toString("utf8").replace(/\0+$/,"").trim());if(type==="BIN\0")bin=data;at+=8+len;}return{json,bin};}
const identity=()=>[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
function mul(a,b){const o=Array(16).fill(0);for(let c=0;c<4;c++)for(let r=0;r<4;r++)for(let k=0;k<4;k++)o[c*4+r]+=a[k*4+r]*b[c*4+k];return o;}
function nodeMatrix(n){if(n.matrix)return n.matrix;const[x,y,z,w]=n.rotation??[0,0,0,1],[sx,sy,sz]=n.scale??[1,1,1],[tx,ty,tz]=n.translation??[0,0,0];return[(1-2*y*y-2*z*z)*sx,(2*x*y+2*z*w)*sx,(2*x*z-2*y*w)*sx,0,(2*x*y-2*z*w)*sy,(1-2*x*x-2*z*z)*sy,(2*y*z+2*x*w)*sy,0,(2*x*z+2*y*w)*sz,(2*y*z-2*x*w)*sz,(1-2*x*x-2*y*y)*sz,0,tx,ty,tz,1];}
function point(m,p){return[m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]];}
const components={5120:["getInt8",1],5121:["getUint8",1],5122:["getInt16",2],5123:["getUint16",2],5125:["getUint32",4],5126:["getFloat32",4]};
const widths={SCALAR:1,VEC2:2,VEC3:3,VEC4:4};
function accessor(doc,index){const a=doc.json.accessors[index],v=doc.json.bufferViews[a.bufferView],[getter,size]=components[a.componentType],count=widths[a.type],stride=v.byteStride??size*count,start=(v.byteOffset??0)+(a.byteOffset??0),view=new DataView(doc.bin.buffer,doc.bin.byteOffset,doc.bin.byteLength);return Array.from({length:a.count},(_,i)=>Array.from({length:count},(_,k)=>view[getter](start+i*stride+k*size,true)));}
function flatten(name,height,maxDimension=null){const doc=glb(embedded(name)),raw=[],primitives=[];function walk(i,parent){const n=doc.json.nodes[i],world=mul(parent,nodeMatrix(n));if(n.mesh!==undefined)for(const p of doc.json.meshes[n.mesh].primitives){if(p.attributes.POSITION===undefined)continue;const pos=accessor(doc,p.attributes.POSITION).map(v=>point(world,v)),indices=p.indices===undefined?pos.map((_,i)=>i):accessor(doc,p.indices).map(v=>v[0]),mode=p.mode??4;if(mode!==4)continue;const color=doc.json.materials?.[p.material]?.pbrMetallicRoughness?.baseColorFactor??[0.72,0.76,0.78,1],start=raw.length/3;for(const idx of indices){raw.push(...pos[idx]);}primitives.push({start,count:indices.length,color});}for(const child of n.children??[])walk(child,world);}for(const n of doc.json.scenes[doc.json.scene??0].nodes??[])walk(n,identity());const mins=[Infinity,Infinity,Infinity],maxs=[-Infinity,-Infinity,-Infinity];for(let i=0;i<raw.length;i+=3)for(let k=0;k<3;k++){mins[k]=Math.min(mins[k],raw[i+k]);maxs[k]=Math.max(maxs[k],raw[i+k]);}const center=maxs.map((v,i)=>(v+mins[i])*0.5),size=maxs.map((v,i)=>v-mins[i]),scale=maxDimension?maxDimension/Math.max(...size):height/size[1];for(let i=0;i<raw.length;i+=3)for(let k=0;k<3;k++)raw[i+k]=(raw[i+k]-center[k])*scale;return{vertices:raw,batches:primitives};}
function writeModel(filename,model){
  const bytes=Buffer.alloc(12+model.vertices.length*4+model.batches.length*24); let at=0;
  bytes.write("DBM1",at);at+=4;bytes.writeUInt32LE(model.vertices.length/3,at);at+=4;bytes.writeUInt32LE(model.batches.length,at);at+=4;
  for(const value of model.vertices){bytes.writeFloatLE(value,at);at+=4;}
  for(const batch of model.batches){bytes.writeUInt32LE(batch.start,at);at+=4;bytes.writeUInt32LE(batch.count,at);at+=4;for(const value of batch.color){bytes.writeFloatLE(value,at);at+=4;}}
  fs.writeFileSync(path.join(outDir,filename),bytes);
  return bytes.length;
}

function humanBoneMetadata(name) {
  const lower=name.toLowerCase();
  let flags=0;
  if(lower.includes("spine")||lower.includes("chest")||lower.includes("torso"))flags|=1;
  if(lower.includes("head")||lower.includes("neck"))flags|=2;
  const arm=lower.includes("arm")||lower.includes("hand")||lower.includes("forearm")||lower.includes("shoulder");
  let side=0;
  if(arm&&(lower.includes("left")||lower.includes("_l")||lower.endsWith("l")||lower.endsWith(".l"))){flags|=4;side=-1;}
  if(arm&&(lower.includes("right")||lower.includes("_r")||lower.endsWith("r")||lower.endsWith(".r"))){flags|=8;side=1;}
  let kind=0;
  if(lower.includes("shoulder"))kind=1;else if(lower.includes("upper"))kind=2;else if(lower.includes("lower")||lower.includes("forearm"))kind=3;else if(lower.includes("hand"))kind=4;
  return {flags,kind,side};
}

function writeHumanModel(filename) {
  const raw=embedded("HUMAN_FBX_BASE64");
  const buffer=raw.buffer.slice(raw.byteOffset,raw.byteOffset+raw.byteLength);
  const root=new FBXLoader().parse(buffer,"");
  root.updateMatrixWorld(true);
  const box=new THREE.Box3().setFromObject(root),size=new THREE.Vector3();box.getSize(size);
  const minY=box.min.y,unitScale=1.16/size.y;
  let mesh=null;root.traverse(node=>{if(node.isSkinnedMesh&&!mesh)mesh=node;});
  if(!mesh)throw new Error("Authoritative human FBX has no skinned mesh");
  const positions=mesh.geometry.attributes.position.array;
  const skinIndices=mesh.geometry.attributes.skinIndex.array;
  const skinWeights=mesh.geometry.attributes.skinWeight.array;
  const bones=mesh.skeleton.bones;
  const parentIndices=bones.map(bone=>bones.indexOf(bone.parent));
  const rootParentMatrix=(bones[0].parent?.matrixWorld??new THREE.Matrix4()).elements;
  const boneInverses=mesh.skeleton.boneInverses.map(matrix=>matrix.elements);
  const frameCount=60;
  const samples=Array.from({length:frameCount},()=>[]);
  const mixer=new THREE.AnimationMixer(root);
  const action=mixer.clipAction(root.animations[0]);action.play();
  mixer.setTime(0);
  for(let frame=0;frame<frameCount;frame++) {
    mixer.setTime(frame/frameCount);
    for(const bone of bones)samples[frame].push(...bone.position.toArray(),...bone.quaternion.toArray(),...bone.scale.toArray());
  }
  const material=Array.isArray(mesh.material)?mesh.material[mesh.geometry.groups[0]?.materialIndex??0]:mesh.material;
  const color=[material?.color?.r??0.60382736,material?.color?.g??0.60382736,material?.color?.b??0.60382736,material?.opacity??1];
  const vertexCount=positions.length/3,boneCount=bones.length;
  const headerBytes=4+4*4+4*6+16*4*3;
  const vertexBytes=vertexCount*(3*4+4*2+4*4);
  const boneBytes=boneCount*(4+4+16*4);
  const sampleBytes=frameCount*boneCount*10*4;
  const bytes=Buffer.alloc(headerBytes+vertexBytes+boneBytes+sampleBytes);let at=0;
  bytes.write("DBH1",at);at+=4;
  for(const value of [vertexCount,boneCount,frameCount,0]){bytes.writeUInt32LE(value,at);at+=4;}
  for(const value of [minY,unitScale,...color]){bytes.writeFloatLE(value,at);at+=4;}
  for(const matrix of [mesh.bindMatrix.elements,mesh.bindMatrixInverse.elements,rootParentMatrix])for(const value of matrix){bytes.writeFloatLE(value,at);at+=4;}
  for(let i=0;i<vertexCount;i++){
    for(let k=0;k<3;k++){bytes.writeFloatLE(positions[i*3+k],at);at+=4;}
    for(let k=0;k<4;k++){bytes.writeUInt16LE(skinIndices[i*4+k],at);at+=2;}
    for(let k=0;k<4;k++){bytes.writeFloatLE(skinWeights[i*4+k],at);at+=4;}
  }
  for(let i=0;i<boneCount;i++){
    bytes.writeInt32LE(parentIndices[i],at);at+=4;
    const meta=humanBoneMetadata(bones[i].name);bytes.writeUInt8(meta.flags,at++);bytes.writeUInt8(meta.kind,at++);bytes.writeInt8(meta.side,at++);bytes.writeUInt8(0,at++);
    for(const value of boneInverses[i]){bytes.writeFloatLE(value,at);at+=4;}
  }
  for(const frame of samples)for(const value of frame){bytes.writeFloatLE(value,at);at+=4;}
  if(at!==bytes.length)throw new Error(`Human writer size mismatch ${at} != ${bytes.length}`);
  fs.writeFileSync(path.join(outDir,filename),bytes);
  return {vertices:vertexCount,bones:boneCount,frames:frameCount,bytes:bytes.length,minY,unitScale,color};
}
const phone=flatten("IPHONE_GLB_BASE64",0.16),flower=flatten("PENTAGONAL_FLOWER_GLB_BASE64",null,0.72);
fs.mkdirSync(outDir,{recursive:true});
const phoneBytes=writeModel("phone.dbmesh",phone),flowerBytes=writeModel("flower.dbmesh",flower);
const human=writeHumanModel("human.dbhuman");
const manifest={format:"DBM1/DBH1",source:"reference/browser-pass7/assets/embedded-assets.js",phone:{vertices:phone.vertices.length/3,batches:phone.batches.length,bytes:phoneBytes},flower:{vertices:flower.vertices.length/3,batches:flower.batches.length,bytes:flowerBytes},human};
fs.writeFileSync(path.join(outDir,"manifest.json"),JSON.stringify(manifest,null,2)+"\n");
console.log(JSON.stringify({output:outDir,...manifest},null,2));
