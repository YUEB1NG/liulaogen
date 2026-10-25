 'use strict';
// Local Motion spring animations; all controls remain usable without this file.
const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)');
function animateUI(el,frames,options={}){if(reducedMotion.matches||!window.Motion)return null;return Motion.animate(el,frames,{duration:.24,...options});}
const nativeShow=HTMLDialogElement.prototype.showModal,nativeClose=HTMLDialogElement.prototype.close;
const closing=new WeakSet();
function openSharedDialog(dialog,source){
 if(!source||reducedMotion.matches||!document.startViewTransition){dialog.showModal();return;}
 const title=dialog.querySelector('h2');if(!title){dialog.showModal();return;}
 source.style.viewTransitionName='selected-actor';
 const transition=document.startViewTransition(()=>{source.style.viewTransitionName='';title.style.viewTransitionName='selected-actor';dialog.showModal();});
 transition.finished.catch(()=>{}).finally(()=>{source.style.viewTransitionName='';title.style.viewTransitionName='';});
}
HTMLDialogElement.prototype.showModal=function(){if(this.open)return;closing.delete(this);nativeShow.call(this);animateUI(this,{opacity:[0,1],scale:[.96,1],y:[12,0]},{type:'spring',visualDuration:.24,bounce:.12});};
HTMLDialogElement.prototype.close=function(value){if(!this.open||closing.has(this))return;if(reducedMotion.matches||!window.Motion){nativeClose.call(this,value);return;}closing.add(this);const a=animateUI(this,{opacity:[1,0],scale:[1,.98],y:[0,8]},{duration:.12});Promise.resolve(a).then(()=>{closing.delete(this);nativeClose.call(this,value);this.style.opacity='';this.style.transform='';});};
window.addEventListener('DOMContentLoaded',()=>{
 document.querySelectorAll('dialog').forEach(d=>d.addEventListener('cancel',e=>{if(e.defaultPrevented)return;e.preventDefault();d.close();}));
 const targets=document.querySelectorAll('.site-header,.page-heading,.control-panel,.management-panel,.lineup-panel,.page-footer');
 targets.forEach((el,i)=>animateUI(el,{opacity:[0,1],y:[10,0]},{type:'spring',visualDuration:.35,bounce:.13,delay:i*.035}));
 // Keyed rows preserve their visual position as the layout changes (FLIP).
 const area=document.getElementById('groups');let previous=new Map();let oldRows=new Map();
 const update=()=>{const next=new Map(),nextRows=new Map();for(const [i,rect] of previous){if(i<area.children.length||reducedMotion.matches)continue;const ghost=oldRows.get(i)?.cloneNode(true);if(!ghost)continue;ghost.inert=true;ghost.setAttribute('aria-hidden','true');Object.assign(ghost.style,{position:'fixed',left:rect.x+'px',top:rect.y+'px',width:rect.width+'px',margin:'0',pointerEvents:'none',zIndex:'3'});document.body.append(ghost);Promise.resolve(animateUI(ghost,{opacity:[1,0],scale:[1,.96]},{duration:.12})).then(()=>ghost.remove());}[...area.children].forEach((el,i)=>{const rect=el.getBoundingClientRect(),old=previous.get(i);next.set(i,rect);nextRows.set(i,el);if(old)animateUI(el,{x:[old.x-rect.x,0],y:[old.y-rect.y,0],opacity:[.65,1]},{type:'spring',visualDuration:.25,bounce:.1});else animateUI(el,{opacity:[0,1],y:[8,0]},{type:'spring',visualDuration:.25,delay:Math.min(i,5)*.025});});previous=next;oldRows=nextRows;};
 new MutationObserver(update).observe(area,{childList:true});
 for(const id of ['actor-list','library-list','dates-list','overview-counts']){const list=document.getElementById(id);new MutationObserver(()=>{[...list.children].slice(0,10).forEach((el,i)=>animateUI(el,{opacity:[0,1],y:[5,0]},{duration:.16,delay:i*.012}));}).observe(list,{childList:true});}
});
