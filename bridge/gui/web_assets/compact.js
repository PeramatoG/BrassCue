/* BrassCue - GPL-3.0. Input and DOM only: C++ owns every value and transition. */
'use strict';
(() => {
  let epoch = '', sequence = 0;
  const byId = id => document.getElementById(id);
  const send = (...fields) => {
    if (!epoch || !window.brasscueHost) return;
    window.brasscueHost.postMessage(['BCW1', epoch, ++sequence, ...fields].join(' '));
  };
  const encoders = [], buttons = [], bars = [];
  const edit = (node, index) => node.addEventListener('contextmenu', event => {
    event.preventDefault(); send('release'); send('edit', index);
  });
  for (let index = 0; index < 4; ++index) {
    const row = document.createElement('div'); row.className = 'encoder';
    const label = document.createElement('label'); label.id = `label-${index}`;
    const knob = document.createElement('div'); knob.className = 'knob'; knob.tabIndex = 0;
    knob.setAttribute('role', 'slider'); knob.setAttribute('aria-labelledby', label.id);
    knob.setAttribute('aria-valuemin', '0'); knob.setAttribute('aria-valuemax', '255');
    const indicator = document.createElement('span'); indicator.className = 'indicator'; knob.append(indicator);
    row.append(label, knob); byId('encoders').append(row); encoders.push({label, knob, indicator}); edit(row, index);
    const point = event => { const r = knob.getBoundingClientRect(); return [(event.clientX-r.left-r.width/2)/(r.width/2), (event.clientY-r.top-r.height/2)/(r.height/2)]; };
    knob.addEventListener('pointerdown', event => { if(event.button!==0)return; event.preventDefault(); knob.focus(); knob.setPointerCapture(event.pointerId); send('drag-start', index, ...point(event)); });
    knob.addEventListener('pointermove', event => { if(knob.hasPointerCapture(event.pointerId))send('drag-move', index, ...point(event)); });
    const end = () => send('drag-end', index);
    knob.addEventListener('pointerup', end); knob.addEventListener('pointercancel', end); knob.addEventListener('lostpointercapture', end); knob.addEventListener('blur', end);
    knob.addEventListener('wheel', event => {event.preventDefault(); if(event.deltaY)send('wheel', index, event.deltaY < 0 ? 1 : -1);}, {passive:false});
    knob.addEventListener('keydown', event => {
      const keys = {ArrowUp:0,ArrowRight:0,ArrowDown:1,ArrowLeft:1,PageUp:2,PageDown:3};
      if(Object.hasOwn(keys,event.key)){event.preventDefault();send('key',index,keys[event.key]);}
    });
    const level = document.createElement('div'); level.className='level';
    const name = document.createElement('span'), bar = document.createElement('progress'); bar.max=100;
    level.append(name,bar);byId('levels').append(level);bars.push({name,bar});
  }
  for(let index=0;index<3;++index){
    const button=document.createElement('button');button.className='cue';button.id=`cue-${index}`;button.setAttribute('aria-pressed','false');
    byId('buttons').append(button);buttons.push(button);edit(button,index+5);
    const source = event => event.pointerType==='mouse'?1:0;
    button.addEventListener('pointerdown',event=>{if(event.button!==0)return;event.preventDefault();button.focus();button.setPointerCapture(event.pointerId);send('button',index,1,source(event));});
    const end=event=>send('button',index,0,source(event));
    button.addEventListener('pointerup',end);button.addEventListener('pointercancel',end);button.addEventListener('lostpointercapture',end);
    button.addEventListener('keydown',event=>{if(event.key===' '||event.key==='Enter'){event.preventDefault();if(!event.repeat)send('button',index,1,0);}});
    button.addEventListener('keyup',event=>{if(event.key===' '||event.key==='Enter'){event.preventDefault();send('button',index,0,0);}});
    button.addEventListener('blur',()=>send('release'));
    button.addEventListener('click',event=>{if(event.detail===0){send('button',index,1,2);send('button',index,0,2);}});
  }
  byId('fader').addEventListener('input',event=>send('fader',event.target.value));
  edit(document.querySelector('.fader-column'),4);edit(byId('screen'),8);
  window.addEventListener('blur',()=>send('release'));window.addEventListener('pagehide',()=>send('release'));
  document.addEventListener('visibilitychange',()=>{if(document.hidden)send('release');});
  window.brasscuePanel = Object.freeze({
    begin: value => {epoch=value;sequence=0;send('ready');},
    apply: data => {
      if(data.version!==1)return;
      if(data.fader){const f=data.fader;byId('fader-label').textContent=f.label;byId('fader').setAttribute('aria-label',f.label);byId('fader').value=f.value;byId('fader-percent').textContent=`${f.percent}%`;byId('effective-master').textContent=`Effective GM ${f.master}%`;}
      if(data.display!==undefined)byId('display').textContent=data.display;
      if(data.connection!==undefined)byId('connection').textContent=data.connection;
      for(const e of data.encoders){const item=encoders[e.index];item.label.textContent=`${e.label} ${e.percent}%`;item.knob.setAttribute('aria-valuenow',e.value);item.knob.setAttribute('aria-valuetext',`${e.percent}%`);item.indicator.style.transform=`rotate(${e.angle}deg)`;}
      for(const b of data.buttons){const item=buttons[b.index];item.textContent=b.label;item.setAttribute('aria-pressed',String(b.selected));item.classList.toggle('transitioning',b.phase.startsWith('FADE'));item.title=b.phase.startsWith('FADE')?'Transitioning':'';}
      if(data.screen){const s=data.screen;byId('screen-title').textContent=s.title;byId('color').hidden=s.mode!==0;byId('levels').hidden=s.mode===0;byId('color').style.backgroundColor=`rgb(${s.color.join(',')})`;for(let i=0;i<4;++i){bars[i].name.textContent=s.labels[i];bars[i].bar.value=s.levels[i];bars[i].bar.setAttribute('aria-label',s.labels[i]);}}
      return true;
    }
  });
})();
