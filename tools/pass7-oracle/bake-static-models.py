import base64, io, json, math, re, struct
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/'reference/browser-pass7/assets/embedded-assets.js').read_text(encoding='utf-8')
OUT=ROOT/'native-models'

def embedded(name):
    match=re.search(rf'const\s+{name}\s*=\s*`([\s\S]*?)`;',SOURCE)
    if not match: raise RuntimeError(f'missing {name}')
    return base64.b64decode(re.sub(r'\s','',match.group(1)))

def glb(name):
    raw=embedded(name); at=12; doc=blob=None
    while at<len(raw):
        size,kind=struct.unpack_from('<I4s',raw,at); data=raw[at+8:at+8+size];at+=8+size
        if kind==b'JSON':doc=json.loads(data.rstrip(b'\0').decode())
        elif kind==b'BIN\0':blob=data
    return doc,blob

COMP={5120:('b',1),5121:('B',1),5122:('h',2),5123:('H',2),5125:('I',4),5126:('f',4)}
WIDTH={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}
def accessor(doc,blob,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];fmt,size=COMP[a['componentType']];count=WIDTH[a['type']];stride=v.get('byteStride',size*count);start=v.get('byteOffset',0)+a.get('byteOffset',0)
    return [struct.unpack_from('<'+fmt*count,blob,start+i*stride) for i in range(a['count'])]

def matmul(a,b):
    return [sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
def node_matrix(n):
    if 'matrix'in n:return n['matrix']
    x,y,z,w=n.get('rotation',[0,0,0,1]);sx,sy,sz=n.get('scale',[1,1,1]);tx,ty,tz=n.get('translation',[0,0,0])
    return [(1-2*y*y-2*z*z)*sx,(2*x*y+2*z*w)*sx,(2*x*z-2*y*w)*sx,0,(2*x*y-2*z*w)*sy,(1-2*x*x-2*z*z)*sy,(2*y*z+2*x*w)*sy,0,(2*x*z+2*y*w)*sz,(2*y*z-2*x*w)*sz,(1-2*x*x-2*y*y)*sz,0,tx,ty,tz,1]
IDENT=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def point(m,p):return (m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14])

def images(doc,blob):
    result=[]
    for image in doc.get('images',[]):
        view=doc['bufferViews'][image['bufferView']];raw=blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
        result.append(Image.open(io.BytesIO(raw)).convert('RGBA'))
    return result
def wrapped(v,mode):
    if mode==33071:return max(0,min(1,v))
    if mode==33648:
        t=v%2;return 2-t if t>1 else t
    return v%1

def bake(name,height=None,max_dimension=None):
    doc,blob=glb(name);decoded=images(doc,blob);groups={}
    def visit(index,parent):
        n=doc['nodes'][index];world=matmul(parent,node_matrix(n))
        if 'mesh'in n:
            for primitive in doc['meshes'][n['mesh']]['primitives']:
                if primitive.get('mode',4)!=4 or 'POSITION' not in primitive['attributes']:continue
                positions=[point(world,p) for p in accessor(doc,blob,primitive['attributes']['POSITION'])]
                uvs=accessor(doc,blob,primitive['attributes']['TEXCOORD_0']) if 'TEXCOORD_0'in primitive['attributes'] else None
                indices=[v[0] for v in accessor(doc,blob,primitive['indices'])] if 'indices'in primitive else list(range(len(positions)))
                mat=doc.get('materials',[{}])[primitive.get('material',0)];pbr=mat.get('pbrMetallicRoughness',{});factor=pbr.get('baseColorFactor',[1,1,1,1]);tex_info=pbr.get('baseColorTexture');texture=sampler=image=None
                if tex_info and uvs:
                    texture=doc['textures'][tex_info['index']];image=decoded[texture['source']];sampler=doc.get('samplers',[{}])[texture.get('sampler',0)] if doc.get('samplers') else {}
                for tri in range(0,len(indices)-2,3):
                    ids=indices[tri:tri+3];color=list(factor)
                    if image:
                        u=sum(uvs[i][0] for i in ids)/3;v=sum(uvs[i][1] for i in ids)/3;u=wrapped(u,sampler.get('wrapS',10497));v=wrapped(v,sampler.get('wrapT',10497));x=min(image.width-1,max(0,int(u*(image.width-1))));y=min(image.height-1,max(0,int(v*(image.height-1))));pixel=image.getpixel((x,y));color=[pixel[k]/255*factor[k] for k in range(4)]
                    key=(round(color[0]*7),round(color[1]*7),round(color[2]*3),round(color[3]*3))
                    groups.setdefault(key,[]).extend(positions[i] for i in ids)
        for child in n.get('children',[]):visit(child,world)
    for node in doc['scenes'][doc.get('scene',0)]['nodes']:visit(node,IDENT)
    all_points=[p for points in groups.values() for p in points];mins=[min(p[k] for p in all_points) for k in range(3)];maxs=[max(p[k] for p in all_points) for k in range(3)];center=[(mins[k]+maxs[k])/2 for k in range(3)];size=[maxs[k]-mins[k] for k in range(3)];scale=max_dimension/max(size) if max_dimension else height/size[1]
    vertices=[];batches=[]
    for key,points in sorted(groups.items(),key=lambda item:item[0][3],reverse=True):
        start=len(vertices)//3
        for p in points:vertices.extend((p[k]-center[k])*scale for k in range(3))
        batches.append((start,len(points),key[0]/7,key[1]/7,key[2]/3,key[3]/3))
    return vertices,batches

def write(filename,model):
    vertices,batches=model;data=bytearray(b'DBM1'+struct.pack('<II',len(vertices)//3,len(batches)))
    data.extend(struct.pack('<'+'f'*len(vertices),*vertices))
    for start,count,r,g,b,a in batches:data.extend(struct.pack('<IIffff',start,count,r,g,b,a))
    (OUT/filename).write_bytes(data);return {'vertices':len(vertices)//3,'batches':len(batches),'bytes':len(data)}

OUT.mkdir(exist_ok=True)
report={'phone':write('phone.dbmesh',bake('IPHONE_GLB_BASE64',height=.16)),'flower':write('flower.dbmesh',bake('PENTAGONAL_FLOWER_GLB_BASE64',max_dimension=.72))}
manifest_path=OUT/'manifest.json'
manifest=json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
manifest.update({'format':'DBM1 texture-baked/DBH1','source':'reference/browser-pass7/assets/embedded-assets.js',**report})
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(report,indent=2))
