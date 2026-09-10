import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(here,"../..");
const sourcePath=path.join(root,"native-models/source/DATA_phone_release.glb");
const desktopPath=path.join(root,"native-models/phone.dbmesh");
const manifestPath=path.join(root,"native-models/manifest.json");

function parseGlb(bytes){let at=12,json,bin;while(at<bytes.length){const len=bytes.readUInt32LE(at),type=bytes.toString("ascii",at+4,at+8),data=bytes.subarray(at+8,at+8+len);if(type==="JSON")json=JSON.parse(data.toString("utf8").replace(/\0+$/," ").trim());if(type==="BIN\0")bin=data;at+=8+len;}if(!json||!bin)throw new Error("Invalid DATA phone GLB");return{json,bin};}
const component={5120:["getInt8",1],5121:["getUint8",1],5122:["getInt16",2],5123:["getUint16",2],5125:["getUint32",4],5126:["getFloat32",4]};
const width={SCALAR:1,VEC2:2,VEC3:3,VEC4:4};
function accessor(doc,index){const a=doc.json.accessors[index],v=doc.json.bufferViews[a.bufferView],[getter,size]=component[a.componentType],count=width[a.type],stride=v.byteStride??size*count,start=(v.byteOffset??0)+(a.byteOffset??0),view=new DataView(doc.bin.buffer,doc.bin.byteOffset,doc.bin.byteLength);return Array.from({length:a.count},(_,i)=>Array.from({length:count},(_,k)=>view[getter](start+i*stride+k*size,true)));}
const identity=()=>[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
function multiply(a,b){const out=Array(16).fill(0);for(let c=0;c<4;c++)for(let r=0;r<4;r++)for(let k=0;k<4;k++)out[c*4+r]+=a[k*4+r]*b[c*4+k];return out;}
function nodeMatrix(n){if(n.matrix)return n.matrix;const[x,y,z,w]=n.rotation??[0,0,0,1],[sx,sy,sz]=n.scale??[1,1,1],[tx,ty,tz]=n.translation??[0,0,0];return[(1-2*y*y-2*z*z)*sx,(2*x*y+2*z*w)*sx,(2*x*z-2*y*w)*sx,0,(2*x*y-2*z*w)*sy,(1-2*x*x-2*z*z)*sy,(2*y*z+2*x*w)*sy,0,(2*x*z+2*y*w)*sz,(2*y*z-2*x*w)*sz,(1-2*x*x-2*y*y)*sz,0,tx,ty,tz,1];}
function point(m,p){return[m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]];}

function bake(bytes){const doc=parseGlb(bytes),raw=[],batches=[];function walk(index,parent){const n=doc.json.nodes[index],world=multiply(parent,nodeMatrix(n));if(n.mesh!==undefined)for(const primitive of doc.json.meshes[n.mesh].primitives){if((primitive.mode??4)!==4||primitive.attributes.POSITION===undefined)continue;const positions=accessor(doc,primitive.attributes.POSITION).map(v=>point(world,v)),indices=primitive.indices===undefined?positions.map((_,i)=>i):accessor(doc,primitive.indices).map(v=>v[0]),color=doc.json.materials?.[primitive.material]?.pbrMetallicRoughness?.baseColorFactor??[0.72,0.76,0.78,1],start=raw.length/3;for(const i of indices)raw.push(...positions[i]);batches.push({start,count:indices.length,color});}for(const child of n.children??[])walk(child,world);}for(const index of doc.json.scenes[doc.json.scene??0].nodes??[])walk(index,identity());const mins=[Infinity,Infinity,Infinity],maxs=[-Infinity,-Infinity,-Infinity];for(let i=0;i<raw.length;i+=3)for(let k=0;k<3;k++){mins[k]=Math.min(mins[k],raw[i+k]);maxs[k]=Math.max(maxs[k],raw[i+k]);}const center=maxs.map((v,i)=>(v+mins[i])/2),scale=0.16/(maxs[1]-mins[1]);for(let i=0;i<raw.length;i+=3)for(let k=0;k<3;k++)raw[i+k]=(raw[i+k]-center[k])*scale;return{vertices:raw,batches};}
function encode(model){const bytes=Buffer.alloc(12+model.vertices.length*4+model.batches.length*24);let at=0;bytes.write("DBM1",at);at+=4;bytes.writeUInt32LE(model.vertices.length/3,at);at+=4;bytes.writeUInt32LE(model.batches.length,at);at+=4;for(const value of model.vertices){bytes.writeFloatLE(value,at);at+=4;}for(const batch of model.batches){bytes.writeUInt32LE(batch.start,at);at+=4;bytes.writeUInt32LE(batch.count,at);at+=4;for(const value of batch.color){bytes.writeFloatLE(value,at);at+=4;}}return bytes;}

const model=bake(fs.readFileSync(sourcePath));
const encoded=encode(model);
fs.writeFileSync(desktopPath,encoded);
const manifest=JSON.parse(fs.readFileSync(manifestPath,"utf8"));
manifest.source="native-models/source";
manifest.sources={...(manifest.sources??{}),phone:"native-models/source/DATA_phone_release.glb"};
delete manifest.referenceSource;
manifest.phone={vertices:model.vertices.length/3,batches:model.batches.length,bytes:encoded.length};
fs.writeFileSync(manifestPath,JSON.stringify(manifest,null,2)+"\n");
console.log(JSON.stringify({source:path.relative(root,sourcePath).replaceAll("\\","/"),vertices:model.vertices.length/3,batches:model.batches.length,bytes:encoded.length,output:path.relative(root,desktopPath)},null,2));
