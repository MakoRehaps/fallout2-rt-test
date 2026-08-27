#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, json, math, random, re, struct, zipfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence

HEADER_SIZE=236
SQ=100
SQ_COUNT=10000
CELL=4096.0

def log(s): print(s, flush=True)
def fnum(v,d=0.0):
    try:return float(v)
    except:return d

def jlines(z:zipfile.ZipFile, suffix:str):
    names=[n for n in z.namelist() if n.endswith('/'+suffix) or n==suffix]
    if not names: return
    with z.open(names[0]) as f:
        for raw in f:
            try: yield json.loads(raw)
            except: pass

@dataclass
class Header:
    version:int; name:str; enter_tile:int; enter_elev:int; enter_rot:int
    local_count:int; script_index:int; flags:int; darkness:int
    global_count:int; map_id:int; last_visit:int
    @property
    def elevations(self):
        out=[]
        if not self.flags&2: out.append(0)
        if not self.flags&4: out.append(1)
        if not self.flags&8: out.append(2)
        return out
    @property
    def tile_offset(self): return HEADER_SIZE+4*(max(0,self.local_count)+max(0,self.global_count))

@dataclass
class CMap:
    game:str; member:str; data:bytes; h:Header; category:str
    layers:Dict[int,List[int]]; tail:int
    @property
    def base(self): return Path(self.member).name

def parse_header(b:bytes)->Header:
    if len(b)<HEADER_SIZE: raise ValueError('short MAP')
    ver=struct.unpack_from('>i',b,0)[0]
    if ver not in (19,20): raise ValueError('bad MAP version')
    name=b[4:20].split(b'\0',1)[0].decode('ascii','replace')
    v=struct.unpack_from('>10i',b,20)
    return Header(ver,name,*v)

def classic_cat(name:str)->str:
    n=name.lower()
    if any(x in n for x in ('vault','vlt','necvl','v13','v15')): return 'vault'
    if any(x in n for x in ('cave','cav','mine','ratcv','toxic')): return 'cave'
    if any(x in n for x in ('mil','base','encl','oil','sierra','marip','plant')): return 'industrial'
    if any(x in n for x in ('city','hub','ncr','reno','newr','den','mod','junk','broken','klam','gecko','sanfran','vaultct')): return 'urban'
    if any(x in n for x in ('desert','rnd','enc','caravan','bridge','arroyo')): return 'wasteland'
    return 'generic'

def parse_map(game,member,b):
    h=parse_header(b); off=h.tile_offset; layers={}
    for e in h.elevations:
        end=off+SQ_COUNT*4
        if end>len(b): raise ValueError('truncated tiles')
        layers[e]=list(struct.unpack_from('>'+('I'*SQ_COUNT),b,off)); off=end
    return CMap(game,member,b,h,classic_cat(Path(member).stem),layers,len(b)-off)

class Library:
    def __init__(self): self.maps=[]; self.by=defaultdict(list)
    def add(self,game,path:Path):
        ok=bad=0
        with zipfile.ZipFile(path) as z:
            for n in z.namelist():
                if not n.lower().endswith('.map'): continue
                try:m=parse_map(game,n,z.read(n))
                except: bad+=1; continue
                self.maps.append(m); self.by[m.category].append(m); ok+=1
        log(f'[classic] {game}: {ok} maps indexed ({bad} skipped)')
    def cand(self,cat,prefer=20):
        a=list(self.by.get(cat,[]))
        if len(a)<3:a+=self.by.get('generic',[])
        if len(a)<3:a+=self.by.get('wasteland',[])
        if not a:a=list(self.maps)
        return sorted(a,key=lambda m:(m.h.version!=prefer,len(m.h.elevations)!=1,m.tail,len(m.data)))
    def template(self,cat,prefer=20): return self.cand(cat,prefer)[0]
    def source(self,cat,rng,prefer=20):
        a=self.cand(cat,prefer); return rng.choice(a[:min(30,len(a))])

@dataclass
class Marker:
    fid:str; name:str; typ:str; editor:str; x:float; y:float; z:float; cx:int; cy:int

class Scan:
    def __init__(self,path:Path):
        self.path=path; self.markers=[]; self.rough={}
        with zipfile.ZipFile(path) as z:
            self._markers(z); self._terrain(z)
        log(f'[fo3] markers={len(self.markers)} terrain cells={len(self.rough)}')
    def _field(self,r,suffix):
        for f in (r.get('raw') or {}).get('fields',[]):
            if str(f.get('path','')).endswith(suffix): return str(f.get('value',''))
        return ''
    def _markers(self,z):
        seen=set()
        for r in jlines(z,'markers.jsonl') or []:
            fid=str(r.get('form_id',''))
            if not fid or fid in seen: continue
            seen.add(fid); x=fnum(r.get('pos_x')); y=fnum(r.get('pos_y')); zz=fnum(r.get('pos_z'))
            name=self._field(r,'FULL - Name') or r.get('editor_id') or fid
            typ=self._field(r,'TNAM\\Type') or 'Unknown'
            self.markers.append(Marker(fid,re.sub(r'[^\w \-]','',str(name)).strip() or fid,str(typ),str(r.get('editor_id','')),x,y,zz,math.floor(x/CELL),math.floor(y/CELL)))
    @staticmethod
    def rough_blob(s):
        try:b=bytes.fromhex(s)
        except:return 0.0
        vals=[x-256 if x>127 else x for x in b[4:]]
        if not vals:return 0.0
        mean=sum(vals)/len(vals); am=sum(abs(x) for x in vals)/len(vals)
        sd=math.sqrt(sum((x-mean)**2 for x in vals)/len(vals))
        return am+sd*.35
    def _terrain(self,z):
        accum=defaultdict(list); seen=set()
        for r in jlines(z,'terrain.jsonl') or []:
            k=(str(r.get('land_form_id','')),int(r.get('grid_x',0)),int(r.get('grid_y',0)))
            if k in seen: continue
            seen.add(k); blob=''
            for f in r.get('vhgt_fields') or []:
                if f.get('path')=='VHGT': blob=f.get('value',''); break
            accum[(k[1],k[2])].append(self.rough_blob(blob))
        self.rough={k:sum(v)/len(v) for k,v in accum.items() if v}
    def rough_near(self,cx,cy,rad=1):
        a=[self.rough[(x,y)] for y in range(cy-rad,cy+rad+1) for x in range(cx-rad,cx+rad+1) if (x,y) in self.rough]
        return sum(a)/len(a) if a else 0.0

def marker_cat(m:Marker,rough:float)->str:
    s=f'{m.name} {m.typ} {m.editor}'.lower()
    if 'vault' in s:return 'vault'
    if any(x in s for x in ('metro','station','city','monument','school','hospital','hotel','mall')):return 'urban'
    if any(x in s for x in ('factory','plant','fort','base','robot','power','satellite')):return 'industrial'
    if any(x in s for x in ('cave','cavern','tunnel')):return 'cave'
    if rough>7.5:return 'cave'
    return 'wasteland'

def chunk(layer,x,y,n):
    out=[]
    for yy in range(y,y+n):out+=layer[yy*SQ+x:yy*SQ+x+n]
    return out

def put(dst,c,x,y,n):
    k=0
    for yy in range(y,y+n): dst[yy*SQ+x:yy*SQ+x+n]=c[k:k+n]; k+=n

def synth(lib,cat,rough,rng,prefer=20,n=10):
    dst=[0x00010001]*SQ_COUNT; used=[]
    mix=[cat]
    if cat=='urban':mix+=['industrial']
    if cat=='wasteland' and rough>5:mix+=['cave']
    if cat!='wasteland':mix+=['wasteland']
    for oy in range(0,SQ,n):
        for ox in range(0,SQ,n):
            src=lib.source(rng.choice(mix),rng,prefer); e=0 if 0 in src.layers else src.h.elevations[0]
            margin=0 if rough>7 else 10; hi=SQ-n-margin; lo=min(margin,hi)
            sx=rng.randint(lo,hi); sy=rng.randint(lo,hi)
            put(dst,chunk(src.layers[e],sx,sy,n),ox,oy,n)
            used.append(f'{src.game}:{src.base}:{sx},{sy}')
    return dst,used

def patch(template:CMap,name:str,tiles:Sequence[int])->bytes:
    b=bytearray(template.data); nm=(name.upper()+'.MAP').encode('ascii')[:15]
    b[4:20]=nm+b'\0'*(16-len(nm)); struct.pack_into('>i',b,0x14,20100); struct.pack_into('>i',b,0x18,0); struct.pack_into('>i',b,0x1c,0); struct.pack_into('>i',b,0x24,-1)
    off=template.h.tile_offset; p=struct.pack('>'+('I'*SQ_COUNT),*tiles); b[off:off+len(p)]=p
    return bytes(b)

def maps_entry(mid,lookup,name):
    return f'[Map {mid:03d}]\nlookup_name={lookup}\nmap_name={name}\nmusic=07desert\nsaved=Yes\ndead_bodies_age=Yes\ncan_rest_here=Yes,Yes,Yes\npipboy_active=Yes\n\n'

def generate(scan,fo1,fo2,out,limit,seed,start_id,prefer):
    out.mkdir(parents=True,exist_ok=True); mout=out/'MAPS'; mout.mkdir(exist_ok=True)
    lib=Library()
    if fo1:lib.add('FO1',fo1)
    if fo2:lib.add('FO2',fo2)
    if not lib.maps:raise RuntimeError('No classic MAP files found')
    s=Scan(scan); ms=sorted(s.markers,key=lambda m:(m.cy,m.cx,m.name.lower()))
    if limit>0:ms=ms[:limit]
    rng=random.Random(seed); manifest=[]; txt=[]; rows=[]
    for i,m in enumerate(ms):
        rough=s.rough_near(m.cx,m.cy); cat=marker_cat(m,rough); t=lib.template(cat,prefer); name=f'F3M{i:04d}'
        tiles,used=synth(lib,cat,rough,rng,prefer); (mout/(name+'.MAP')).write_bytes(patch(t,name,tiles))
        e={'index':i,'map_id':start_id+i,'map_name':name,'lookup_name':'FO3: '+m.name,'marker_form_id':m.fid,'marker_name':m.name,'marker_type':m.typ,'world_position':[m.x,m.y,m.z],'cell':[m.cx,m.cy],'roughness':round(rough,4),'category':cat,'template':{'game':t.game,'map':t.base,'version':t.h.version,'tail_size':t.tail},'classic_chunks':used,'output':'MAPS/'+name+'.MAP'}
        manifest.append(e); txt.append(maps_entry(start_id+i,e['lookup_name'],name)); rows.append([i,start_id+i,name,m.name,m.typ,m.cx,m.cy,f'{rough:.3f}',cat,t.game,t.base])
        if (i+1)%25==0 or i+1==len(ms):log(f'[generate] {i+1}/{len(ms)}')
    (out/'fo3_world_manifest.json').write_text(json.dumps({'format':'PhoBoi.FO3ClassicForge/1','seed':seed,'generated_maps':manifest},indent=2),encoding='utf-8')
    (out/'maps_txt_fragment.txt').write_text(''.join(txt),encoding='utf-8')
    with (out/'map_index.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.writer(f);w.writerow(['index','map_id','map_name','fo3_location','marker_type','cell_x','cell_y','roughness','category','template_game','template_map']);w.writerows(rows)
    nodes=[{'map_id':e['map_id'],'map_name':e['map_name'],'x':e['cell'][0],'y':e['cell'][1],'location':e['marker_name']} for e in manifest]; links=set()
    for i,a in enumerate(nodes):
        ds=sorted((abs(a['x']-b['x'])+abs(a['y']-b['y']),j) for j,b in enumerate(nodes) if j!=i)[:4]
        for _,j in ds: links.add(tuple(sorted((a['map_name'],nodes[j]['map_name']))))
    (out/'fallout_rt_world_graph.json').write_text(json.dumps({'nodes':nodes,'links':[list(x) for x in sorted(links)]},indent=2),encoding='utf-8')
    log('[done] '+str(out))

def gui():
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox
    app=tk.Tk();app.title('FO3 Classic Forge — Fallout RT');app.geometry('780x410')
    scan=tk.StringVar();fo1=tk.StringVar();fo2=tk.StringVar();out=tk.StringVar(value=str(Path.cwd()/'FO3_GENERATED'));limit=tk.StringVar(value='0');seed=tk.StringVar(value='3003');sid=tk.StringVar(value='200')
    def rr(label,var,row,dir=False):
        ttk.Label(app,text=label).grid(row=row,column=0,sticky='w',padx=8,pady=6);ttk.Entry(app,textvariable=var,width=70).grid(row=row,column=1,sticky='ew',padx=8)
        def b():
            p=filedialog.askdirectory() if dir else filedialog.askopenfilename(filetypes=[('ZIP','*.zip'),('All','*.*')]);
            if p:var.set(p)
        ttk.Button(app,text='Browse',command=b).grid(row=row,column=2,padx=8)
    app.columnconfigure(1,weight=1);rr('FO3 scan ZIP',scan,0);rr('Fallout 1 MAPS ZIP',fo1,1);rr('Fallout 2 maps ZIP',fo2,2);rr('Output',out,3,True)
    for row,(lab,var) in enumerate((('Limit (0=all)',limit),('Seed',seed),('First map id',sid)),start=4):ttk.Label(app,text=lab).grid(row=row,column=0,sticky='w',padx=8);ttk.Entry(app,textvariable=var).grid(row=row,column=1,sticky='w',padx=8,pady=5)
    def run():
        try:generate(Path(scan.get()),Path(fo1.get()) if fo1.get() else None,Path(fo2.get()) if fo2.get() else None,Path(out.get()),int(limit.get()),int(seed.get()),int(sid.get()),20);messagebox.showinfo('FO3 Classic Forge','Done')
        except Exception as e:messagebox.showerror('FO3 Classic Forge',str(e))
    ttk.Button(app,text='GENERATE FALLOUT 3 CLASSIC MAPS',command=run).grid(row=7,column=0,columnspan=3,pady=20);app.mainloop()

def main():
    p=argparse.ArgumentParser();p.add_argument('--scan',type=Path);p.add_argument('--fo1',type=Path);p.add_argument('--fo2',type=Path);p.add_argument('--output',type=Path,default=Path('FO3_GENERATED'));p.add_argument('--limit',type=int,default=0);p.add_argument('--seed',type=int,default=3003);p.add_argument('--start-map-id',type=int,default=200);p.add_argument('--prefer-version',type=int,choices=(19,20),default=20);p.add_argument('--gui',action='store_true');a=p.parse_args()
    if a.gui or not a.scan:return gui()
    generate(a.scan,a.fo1,a.fo2,a.output,a.limit,a.seed,a.start_map_id,a.prefer_version)
if __name__=='__main__':main()
