#!/usr/bin/env node

// Disposable deterministic probe for emergent enemy coordination.
// This is deliberately not runtime authority. It answers one question:
// can a tiny shared recurrent policy, selected only on group outcome,
// discover better collective pressure than its starting population?

const POPULATION = 24;
const ELITES = 6;
const GENERATIONS = 48;
const SEEDS = 24;
const ENEMIES = 3;
const STEPS = 360;
const DT = 1 / 30;
const INPUTS = 10;
const HIDDEN = 8;
const OUTPUTS = 5;

class Rng {
  constructor(seed) { this.s = (seed >>> 0) || 0x9e3779b9; }
  u32() { let x=this.s; x^=x<<13; x^=x>>>17; x^=x<<5; return this.s=x>>>0; }
  f() { return this.u32() / 0xffffffff; }
  signed() { return this.f() * 2 - 1; }
}

function clamp(v,a=-1,b=1){ return Math.max(a,Math.min(b,v)); }
function tanh(v){ return Math.tanh(v); }
function len(x,z){ return Math.hypot(x,z); }
function normalize(x,z){ const l=len(x,z)||1; return [x/l,z/l]; }

const GENE_COUNT = HIDDEN * (INPUTS + HIDDEN + 1) + OUTPUTS * (HIDDEN + 1);

function randomGenome(rng, scale=0.55) {
  return Array.from({length: GENE_COUNT}, () => rng.signed() * scale);
}

function mutate(genome, rng, sigma=0.11) {
  return genome.map(g => {
    // Deterministic pseudo-Gaussian-ish mutation from four uniforms.
    const n = (rng.signed()+rng.signed()+rng.signed()+rng.signed()) * 0.5;
    return clamp(g + n * sigma, -3, 3);
  });
}

function policy(genome, input, memory) {
  let k = 0;
  const next = new Array(HIDDEN);
  for (let h=0; h<HIDDEN; h++) {
    let v = genome[k++];
    for (let i=0; i<INPUTS; i++) v += genome[k++] * input[i];
    for (let j=0; j<HIDDEN; j++) v += genome[k++] * memory[j];
    next[h] = tanh(v);
  }
  const out = new Array(OUTPUTS);
  for (let o=0; o<OUTPUTS; o++) {
    let v = genome[k++];
    for (let h=0; h<HIDDEN; h++) v += genome[k++] * next[h];
    out[o] = tanh(v);
  }
  return {memory: next, out};
}

function runEncounter(genome, seed) {
  const rng = new Rng(seed);
  const player = {x:0,z:0,vx:0,vz:0,hp:100,vacuum:0,captureLock:0};
  const enemies = Array.from({length: ENEMIES}, (_,i) => ({
    x: Math.cos(i * Math.PI*2/ENEMIES) * (7.5 + rng.f()*2),
    z: Math.sin(i * Math.PI*2/ENEMIES) * (7.5 + rng.f()*2),
    vx:0,vz:0,hp:1,alive:true,memory:Array(HIDDEN).fill(0),
    attackCd: rng.f()*0.4, exposure:0, contributed:0
  }));

  let damage = 0, attacks = 0, usefulExposure = 0, spacingAccum = 0, aliveAccum = 0;
  let directionChanges = 0, lastPlayerSign = 0;

  for (let step=0; step<STEPS && player.hp>0; step++) {
    player.captureLock=Math.max(0,player.captureLock-DT);
    // Simple deterministic player: orbits away from local enemy pressure and periodically vacuums.
    let cx=0,cz=0,count=0;
    for (const e of enemies) if(e.alive){cx+=e.x;cz+=e.z;count++;}
    if(count){cx/=count;cz/=count;}
    player.vacuum = player.captureLock<=0 && ((step + seed) % 150) < 44 ? 1 : 0;
    const awayX=player.x-cx, awayZ=player.z-cz;
    const [ax,az]=normalize(awayX + Math.sin(step*0.031)*0.5, awayZ + Math.cos(step*0.027)*0.5);
    const playerAccel = player.captureLock>0 ? 0.65 : (player.vacuum ? 2.2 : 5.5);
    player.vx += ax*playerAccel*DT; player.vz += az*playerAccel*DT;
    const ps=len(player.vx,player.vz); if(ps>3.6){player.vx*=3.6/ps;player.vz*=3.6/ps;}
    player.x += player.vx*DT; player.z += player.vz*DT;
    if(Math.abs(player.x)>10){player.x=clamp(player.x,-10,10);player.vx*=-0.6;}
    if(Math.abs(player.z)>10){player.z=clamp(player.z,-10,10);player.vz*=-0.6;}
    const sign=Math.sign(player.vx); if(sign && lastPlayerSign && sign!==lastPlayerSign) directionChanges++; if(sign)lastPlayerSign=sign;

    for (let i=0;i<ENEMIES;i++) {
      const e=enemies[i]; if(!e.alive) continue;
      e.attackCd=Math.max(0,e.attackCd-DT);
      let nearest=99, allyDx=0, allyDz=0;
      for(let j=0;j<ENEMIES;j++) if(i!==j && enemies[j].alive){
        const a=enemies[j], d=len(a.x-e.x,a.z-e.z);
        if(d<nearest){nearest=d;allyDx=a.x-e.x;allyDz=a.z-e.z;}
      }
      const dx=player.x-e.x,dz=player.z-e.z,dist=len(dx,dz);
      const [pdx,pdz]=normalize(dx,dz);
      const [adx,adz]=normalize(allyDx,allyDz);
      const localPressure=enemies.filter(q=>q.alive && len(q.x-player.x,q.z-player.z)<3.5).length/ENEMIES;
      const input=[pdx,pdz,clamp(dist/12,0,1), player.vx/3.6, player.vz/3.6,
        nearest===99?0:clamp(nearest/8,0,1), adx, adz, player.vacuum || player.captureLock>0 ? 1 : 0, localPressure];
      const r=policy(genome,input,e.memory); e.memory=r.memory;
      const [mx,mz]=normalize(r.out[0],r.out[1]);
      const aggression=(r.out[2]+1)*0.5;
      const attack=(r.out[3]+1)*0.5;
      const brace=(r.out[4]+1)*0.5;
      const speed=1.6+2.5*aggression;
      e.vx += mx*speed*5*DT; e.vz += mz*speed*5*DT;
      const es=len(e.vx,e.vz); if(es>speed){e.vx*=speed/es;e.vz*=speed/es;}
      e.x+=e.vx*DT;e.z+=e.vz*DT;

      // Vacuum punishes proximity unless the motor chooses to brace. There is no sacrifice action.
      // A policy can only discover sacrifice indirectly by choosing movement/brace patterns that
      // let one body absorb the vacuum while another converts the opening into damage.
      if(player.vacuum && dist<3.15){
        e.exposure += DT*(0.75 + 0.9*(1.0-brace));
        if(e.exposure>1.0){ e.alive=false; e.exposure=1.0; e.deathStep=step; player.captureLock=Math.max(player.captureLock,1.15); }
      } else e.exposure=Math.max(0,e.exposure-DT*0.30);

      if(dist<1.45 && e.attackCd<=0 && attack>0.58){
        const hit=4.0 + 6.0*aggression;
        player.hp-=hit; damage+=hit; attacks++; e.contributed+=hit; e.attackCd=0.72;
        // Group-credit proxy: reward damage shortly after an ally was consumed by vacuum.
        // The motor has no named sacrifice/protect/flank action; this is only group outcome credit.
        for(let j=0;j<ENEMIES;j++) if(j!==i){
          const q=enemies[j];
          if(q.deathStep!==undefined && step-q.deathStep>=0 && step-q.deathStep<45) usefulExposure += hit;
        }
      }
    }

    const alive=enemies.filter(e=>e.alive);
    aliveAccum += alive.length;
    if(alive.length>1){
      let s=0,n=0; for(let i=0;i<alive.length;i++)for(let j=i+1;j<alive.length;j++){s+=len(alive[i].x-alive[j].x,alive[i].z-alive[j].z);n++;}
      spacingAccum += s/n;
    }
  }

  const deaths=enemies.filter(e=>!e.alive).length;
  const avgSpacing=spacingAccum/STEPS;
  const avgAlive=aliveAccum/STEPS;
  // Group outcome dominates. Death has a cost, but can be outweighed by damage during exposure.
  const fitness = damage*1.0 + usefulExposure*0.30 + directionChanges*0.12 + avgSpacing*0.15 - deaths*3.0 - Math.max(0,avgAlive-2.8)*0.4;
  return {fitness,damage,attacks,deaths,usefulExposure,directionChanges,avgSpacing};
}

function evaluate(genome) {
  const totals={fitness:0,damage:0,attacks:0,deaths:0,usefulExposure:0,directionChanges:0,avgSpacing:0};
  for(let s=0;s<SEEDS;s++){
    const r=runEncounter(genome, 0x51f15e + s*7919);
    for(const k of Object.keys(totals)) totals[k]+=r[k];
  }
  for(const k of Object.keys(totals)) totals[k]/=SEEDS;
  return totals;
}

function main() {
  const rng=new Rng(0xdecafbad);
  let population=Array.from({length:POPULATION},()=>randomGenome(rng));
  const history=[];
  let baseline=null;
  for(let g=0;g<GENERATIONS;g++){
    const scored=population.map((genome,index)=>({genome,index,...evaluate(genome)})).sort((a,b)=>b.fitness-a.fitness || a.index-b.index);
    if(g===0) baseline={...scored[0]};
    const best=scored[0];
    history.push({generation:g,fitness:best.fitness,damage:best.damage,deaths:best.deaths,usefulExposure:best.usefulExposure,directionChanges:best.directionChanges,avgSpacing:best.avgSpacing});
    const elites=scored.slice(0,ELITES).map(x=>x.genome);
    const next=[...elites];
    while(next.length<POPULATION){
      const parent=elites[next.length%ELITES];
      next.push(mutate(parent,rng,0.10 + 0.035*(1-g/GENERATIONS)));
    }
    population=next;
  }
  const finalScored=population.map((genome,index)=>({genome,index,...evaluate(genome)})).sort((a,b)=>b.fitness-a.fitness || a.index-b.index);
  const winner=finalScored[0];
  const result={
    config:{population:POPULATION,elites:ELITES,generations:GENERATIONS,seeds:SEEDS,enemies:ENEMIES,steps:STEPS,geneCount:GENE_COUNT},
    baseline:{fitness:baseline.fitness,damage:baseline.damage,deaths:baseline.deaths,usefulExposure:baseline.usefulExposure,directionChanges:baseline.directionChanges,avgSpacing:baseline.avgSpacing},
    winner:{fitness:winner.fitness,damage:winner.damage,deaths:winner.deaths,usefulExposure:winner.usefulExposure,directionChanges:winner.directionChanges,avgSpacing:winner.avgSpacing},
    improvement:winner.fitness-baseline.fitness,
    history
  };
  console.log(JSON.stringify(result,null,2));
}

main();
