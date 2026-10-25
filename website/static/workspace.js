 'use strict';
function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.classList.add('icon');svg.setAttribute('aria-hidden','true');const use=document.createElementNS(svg.namespaceURI,'use');use.setAttribute('href','/vendor/icons.svg#'+name);svg.append(use);return svg;}
let profileRequest=0;
async function showPublicProfile(group,source){
 const serial=++profileRequest;
 const date=activeDate,revision=draft.revision,dialog=$('public-profile');$('profile-title').textContent=group.members.map(m=>m.name).join(' · ');$('profile-content').textContent='正在读取本版资料…';openSharedDialog(dialog,source);
 try{const q=new URLSearchParams({date,revision,columns:13,m0:group.members[0].id,m1:group.members[1].id}),data=await api('/api/device/profile?'+q);if(serial!==profileRequest)return;$('profile-content').replaceChildren();for(const p of data.pages){$('profile-content').append(node('h3',p.title),node('p',p.text,'profile-page'));}}
 catch(e){if(serial!==profileRequest)return;$('profile-content').textContent='资料读取失败：'+e.message;}
}
function chooseDate(date){$('date').value=date;$('date').dispatchEvent(new Event('change'));}
window.addEventListener('DOMContentLoaded',()=>{
 for(const [id,name] of [['account','log-in'],['guide-open','circle-help'],['overview-open','database'],['library-open','users'],['dates-open','calendar-days'],['devices-open','upload']])$(id).prepend(icon(name));
 $('guide-open').onclick=()=>$('guide').showModal();
 for(const [id,offset] of [['date-prev',-1],['date-next',1]])$(id).onclick=()=>{if(busy||!activeDate)return;const d=new Date(activeDate+'T12:00:00Z');d.setUTCDate(d.getUTCDate()+offset);chooseDate(d.toISOString().slice(0,10));};
 $('date-today').onclick=()=>{if(!busy)chooseDate(new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Shanghai',year:'numeric',month:'2-digit',day:'2-digit'}).format(new Date()));};
 $('dates-open').onclick=()=>work(async()=>{const {dates}=await api('/api/published-dates');$('dates-list').replaceChildren();for(const d of dates){const b=node('button','','record record-button');b.append(node('span',d.date),node('small',`${d.cities} 座城市 · v${d.revision}`));b.onclick=()=>{$('published-dates').close();chooseDate(d.date);};$('dates-list').append(b);}if(!dates.length)$('dates-list').append(node('p','尚无已发布名单','note'));$('published-dates').showModal();});
 $('overview-open').onclick=()=>work(async()=>{if(!auth())return;const data=await api('/api/overview');$('overview-counts').replaceChildren();for(const [key,label] of [['actors','演员资料'],['cities','城市'],['published_dates','已发布天数'],['online','在线设备'],['pending','等待保存'],['failed','上传失败']]){const card=node('div','','metric');card.append(node('strong',String(data.counts[key])),node('span',label));$('overview-counts').append(card);}$('overview-recent').replaceChildren();for(const r of data.recent){const row=node('div','','record');row.append(node('span',`${r.date} · v${r.revision}`),node('small',new Date(r.at).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false})));$('overview-recent').append(row);}if(!data.recent.length)$('overview-recent').append(node('p','发布后将在这里显示记录','note'));$('overview-storage').textContent=`内容及备份占用 ${bytes(data.storage.used_bytes)} · ${data.counts.draft_dates} 天草稿 · ${data.counts.devices} 台已配对设备`;$('overview').showModal();});
});
