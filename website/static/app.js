'use strict';
const $=id=>document.getElementById(id);
let config,actors=[],draft,csrf='',dirty=false,selection,previewed,busy=false;
let activeDate='',activeCity='',activeSession='';
const clone=x=>JSON.parse(JSON.stringify(x));
const empty=()=>({members:[{id:'',name:''},{id:'',name:''}]});
function node(tag,text,cls){const n=document.createElement(tag);n.textContent=text;if(cls)n.className=cls;return n;}
function message(text){$('notice').textContent=text;}
async function api(path,body){
 const r=await fetch(path,{credentials:'same-origin',headers:body?{'Content-Type':'application/json','X-CSRF-Token':csrf}:{},method:body?'POST':'GET',body:body?JSON.stringify(body):undefined});
 const d=await r.json();if(!r.ok){if(r.status===401){csrf='';$('account').textContent='排班管理 · 登录';}throw Error(d.error?.message||'请求失败');}return d;
}
function auth(){if(csrf)return true;$('login').showModal();return false;}
function venue(){return draft.venues.find(v=>v.id===activeCity);}
function current(create=false){let v=venue();if(!v&&create){v={id:activeCity,city:config.cities.find(c=>c.id===activeCity).city,sessions:[]};draft.venues.push(v);}let s=v?.sessions.find(s=>s.id===activeSession);if(!s&&create){s={id:activeSession,label:config.sessions.find(x=>x.id===activeSession).label,groups:Array.from({length:5},empty)};v.sessions.push(s);}return s;}
function mark(){dirty=true;render();}
function render(){
 const s=current(),groups=s?.groups||Array.from({length:5},empty),words=['零','一','二','三','四','五','六','七','八','九','十'];
 $('count').textContent=`${words[groups.length]||groups.length}组 · ${groups.length*2===10?'十':groups.length*2}位演员`;
 $('groups').replaceChildren();groups.forEach((g,i)=>{const row=node('div','','row');row.append(node('span',String(i+1).padStart(2,'0'),'number'));g.members.forEach((m,j)=>{const b=node('button','','member'+(!m.name?' empty':''));b.append(node('span',m.name||'选择演员'),node('i','›'));b.setAttribute('aria-label',`第${i+1}组第${j+1}位：${m.name||'选择演员'}`);b.onclick=()=>{if(!auth())return;selection=[i,j];$('search').value='';showActors();$('picker').showModal();$('search').focus();};row.append(b);});$('groups').append(row);});
 const city=config.cities.find(c=>c.id===activeCity).city,label=config.sessions.find(s=>s.id===activeSession).label;
 $('status').textContent=`${dirty?'未保存草稿':'草稿'} · ${city}${label} · ${draft.revision?'线上已有 v'+draft.revision+'，草稿修改不影响线上':'该日期尚未发布'}`;
}
function showActors(){const query=$('search').value.trim();$('actor-list').replaceChildren();const used=new Set((current()?.groups||[]).flatMap(g=>g.members.map(m=>m.id)));const results=actors.filter(a=>a.name.includes(query));for(const a of results){const b=node('button','','actor');b.append(node('strong',a.name+(used.has(a.id)?' · 已在本场':'')));b.append(node('small',a.credits.slice(0,2).join(' / ')||'暂无明确作品资料'));if(a.user_highlights?.[0])b.append(node('small','用户填写：'+a.user_highlights[0].text));else if(a.highlights[0])b.append(node('small','历史资料：'+a.highlights[0].text));b.onclick=()=>{const s=current(true);if(s.groups.some((g,i)=>g.members.some((m,j)=>m.id===a.id&&!(i===selection[0]&&j===selection[1])))){message('同一场次演员不可重复，请选择其他演员。');return;}
 s.groups[selection[0]].members[selection[1]]={id:a.id,name:a.name};$('picker').close();message('演员已更新，尚未保存。');mark();};$('actor-list').append(b);}if(!results.length)$('actor-list').append(node('p','未找到演员，请检查姓名。','note'));}
async function load(){draft=csrf?await api('/api/draft?date='+activeDate):{date:activeDate,version:0,revision:0,venues:[]};dirty=false;render();}
async function work(fn){if(busy)return;busy=true;document.querySelector('main').inert=true;try{await fn();}catch(e){message(e.message);}finally{busy=false;document.querySelector('main').inert=false;}}
async function save(){current(true);draft=await api('/api/draft',{date:activeDate,version:draft.version,venues:draft.venues});dirty=false;render();message('草稿已保存，仅管理员可见。');}
async function init(){try{[config,{actors}]=await Promise.all([api('/api/config'),api('/api/actors')]);for(const c of config.cities){const o=node('option',c.city);o.value=c.id;$('city').append(o);}for(const s of config.sessions){const o=node('option',s.label);o.value=s.id;$('session').append(o);}
 activeDate=new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Shanghai',year:'numeric',month:'2-digit',day:'2-digit'}).format(new Date());activeCity='zhongjie';activeSession='evening';$('date').value=activeDate;$('session').value=activeSession;
 try{csrf=(await api('/api/session')).csrf;$('account').textContent='管理员 · 退出';}catch{}await load();if(!csrf)message('登录后编辑；历史演员库不会自动填充或发布今日阵容。');
 }catch(e){message('加载失败：'+e.message);}}
$('account').onclick=()=>work(async()=>{if(!csrf){$('login').showModal();return;}if(dirty&&!confirm('有未保存修改，退出将放弃。'))return;await api('/api/logout',{});csrf='';$('account').textContent='排班管理 · 登录';await load();message('已安全退出。');});
$('login-form').onsubmit=e=>{e.preventDefault();work(async()=>{try{const form=new FormData(e.target);csrf=(await api('/api/login',Object.fromEntries(form))).csrf;e.target.password.value='';$('login-error').textContent='';$('login').close();$('account').textContent='管理员 · 退出';await load();message('已登录。请选择演员或主动复制上次阵容。');}catch(e){$('login-error').textContent=e.message;}});};
document.querySelectorAll('[data-close]').forEach(b=>b.onclick=()=>$(b.dataset.close).close());
$('search').oninput=showActors;
$('clear').onclick=()=>{current(true).groups[selection[0]].members[selection[1]]={id:'',name:''};$('picker').close();mark();};
$('city').onchange=()=>{activeCity=$('city').value;render();};$('session').onchange=()=>{activeSession=$('session').value;render();};
$('date').onchange=()=>work(async()=>{if(!$('date').value||(dirty&&!confirm('切换日期将放弃未保存修改，继续吗？'))){$('date').value=activeDate;return;}const previous=activeDate;activeDate=$('date').value;try{await load();message('已切换日期。');}catch(e){activeDate=previous;$('date').value=previous;throw e;}});
$('add').onclick=()=>{if(!auth())return;const s=current(true);if(s.groups.length===12){message('每场最多12组。');return;}s.groups.push(empty());mark();};
$('remove').onclick=()=>{if(!auth())return;const s=current(true);if(s.groups.length===1){message('至少保留1组。');return;}if(s.groups.at(-1).members.some(m=>m.name)&&!confirm('移除最后一组的演员？'))return;s.groups.pop();mark();};
$('copy').onclick=()=>work(async()=>{if(!auth())return;const old=await api(`/api/previous?date=${activeDate}&city=${activeCity}&session=${activeSession}`);if(!confirm(`复制 ${old.date} 的${old.source==='historical_library'?'历史资料（非当日已发布阵容）':'已发布阵容'}，替换当前场次？`))return;current(true).groups=clone(old.groups);mark();message(`已复制 ${old.date}；请核对后保存并发布。`);});
$('reload-draft').onclick=()=>work(async()=>{if(!auth())return;if(dirty&&!confirm('重新加载将放弃未保存排班，请先记录需要保留的修改。继续吗？'))return;actors=(await api('/api/actors')).actors;await load();message('已加载最新草稿和演员资料，请核对后预览。');});
$('save').onclick=()=>work(async()=>{if(auth())await save();});
$('preview').onclick=()=>work(async()=>{if(!auth())return;await save();previewed=await api('/api/preview',{date:activeDate,version:draft.version,revision:draft.revision});$('preview-content').replaceChildren(node('h3',`${previewed.date} · 版本 ${previewed.revision}`));for(const v of previewed.venues){for(const s of v.sessions){$('preview-content').append(node('h3',`${v.city} · ${s.label} · ${s.groups.length}组`));const list=node('ol','','preview-list');for(const g of s.groups)list.append(node('li',g.members.map(m=>m.name).join(' · ')));$('preview-content').append(list);}}$('publication').showModal();});
$('publish').onclick=()=>work(async()=>{if(!previewed)return;$('publish').disabled=true;try{const result=await api('/api/publish',{date:previewed.date,version:draft.version,revision:previewed.revision-1});draft.revision=result.revision;$('publication').close();render();message(`已发布 ${result.date} · v${result.revision}。请点击“上传到设备”选择 Passport。`);}finally{$('publish').disabled=false;}});
let editingActor=null,actorSaving=false;
function showLibrary(){const q=$('library-search').value.trim();$('library-list').replaceChildren();for(const a of actors.filter(a=>a.name.includes(q))){const b=node('button','','actor');b.append(node('strong',a.name),node('small',`${a.user_edited?'用户已编辑':'历史资料'} · 资料 v${a.version}`),node('small',a.credits.slice(0,2).join(' / ')||'暂无作品角色'));b.onclick=()=>editActor(a);$('library-list').append(b);}if(!$('library-list').children.length)$('library-list').append(node('p','未找到演员，可点“新增演员”手动录入。','note'));}
function editActor(a){editingActor=a?clone(a):null;$('actor-title').textContent=a?'编辑演员资料':'新增演员';$('actor-name').value=a?.name||'';$('actor-credits').value=(a?.credits||[]).join('\n');$('actor-highlights').value=(a?.user_highlights||[]).map(h=>h.text).join('\n');$('actor-bio').value=a?.bio||'';$('actor-error').textContent='';$('actor-history').hidden=!a?.historical_profile;$('actor-source').textContent=a?.historical_profile?JSON.stringify(a.historical_profile,null,2):'';$('actor-editor').showModal();$('actor-name').focus();}
$('library-open').onclick=()=>work(async()=>{if(!auth())return;actors=(await api('/api/actors')).actors;showLibrary();$('library').showModal();});
$('library-search').oninput=showLibrary;$('actor-new').onclick=()=>editActor(null);
$('actor-cancel').onclick=()=>{if(!actorSaving)$('actor-editor').close();};
$('actor-editor').addEventListener('cancel',e=>{if(actorSaving)e.preventDefault();});
$('actor-form').onsubmit=async e=>{e.preventDefault();if(actorSaving)return;actorSaving=true;$('actor-submit').disabled=true;$('actor-cancel').disabled=true;$('actor-error').textContent='';
 try{const lines=id=>$(id).value.split('\n').map(v=>v.trim()).filter(Boolean);const hs=lines('actor-highlights');const data={name:$('actor-name').value.trim(),credits:lines('actor-credits'),bio:$('actor-bio').value,user_highlights:hs.map((text,i)=>({label:editingActor?.user_highlights?.[i]?.label||'舞台特色',text})),version:editingActor?.version||0};
 const a=await api('/api/actors'+(editingActor?'/'+encodeURIComponent(editingActor.id):''),data);const index=actors.findIndex(x=>x.id===a.id);if(index<0)actors.push(a);else actors[index]=a;
 previewed=null;showLibrary();showActors();$('library-message').textContent=`${a.name} · 资料已保存，可立即用于排班。`;$('actor-editor').close();
 if(!dirty)await load();else{for(const v of draft.venues)for(const s of v.sessions)for(const g of s.groups)for(const m of g.members)if(m.id===a.id)m.name=a.name;render();}
 message('演员资料已保存。已发布阵容不变；含该演员的旧草稿窗口可能需重新加载后核对。');
 }catch(err){$('actor-error').textContent=err.message+'；未保存内容仍保留。若版本冲突，请取消并重新打开演员库后核对。';}finally{actorSaving=false;$('actor-submit').disabled=false;$('actor-cancel').disabled=false;}};
window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
init();

let deviceRefreshing=false,deviceAction=false,selectedUpload=null,deviceTimer;
const deliveryLabels={queued:'等待设备',transferring:'传输中，等待保存回执',saved:'设备已保存',failed:'上传失败，旧名单保留'};
async function refreshDevices(){
 if(deviceRefreshing||deviceAction||!$('devices').open||!csrf)return;
 deviceRefreshing=true;
 try{const {devices}=await api('/api/devices');if(!$('devices').open)return;
  $('device-list').replaceChildren();
  for(const d of devices){const card=node('section');card.append(node('h3',d.name+' · '+(d.online?'在线':'离线')));
   if(d.delivery)card.append(node('p',`${d.delivery.date} · v${d.delivery.revision} · ${deliveryLabels[d.delivery.status]||'状态未知'}`,'status'));
   const pending=d.delivery&&['queued','transferring'].includes(d.delivery.status);
   const upload=node('button','上传到此设备','primary wide');upload.disabled=!d.online||pending||!selectedUpload;
   upload.onclick=async()=>{if(deviceAction||!selectedUpload)return;deviceAction=true;upload.disabled=true;
    try{await api('/api/devices/upload',{device:d.id,...selectedUpload});$('device-message').textContent='已提交上传任务，进度见下方。';}
    catch(e){$('device-message').textContent=e.message;}finally{deviceAction=false;await refreshDevices();}};
   const forget=node('button','移除此设备','text');forget.onclick=async()=>{if(deviceAction||!confirm('移除后需在 Passport 重新配对，未完成的上传将取消。'))return;deviceAction=true;
    try{await api('/api/devices/forget',{device:d.id});}catch(e){$('device-message').textContent=e.message;}finally{deviceAction=false;await refreshDevices();}};
   card.append(upload,forget);$('device-list').append(card);
  }
  if(!devices.length)$('device-list').append(node('p','尚未添加设备，请输入 Passport 上的配对码。','note'));
 }catch(e){$('device-message').textContent='设备状态读取失败：'+e.message;$('device-list').replaceChildren();}
 finally{deviceRefreshing=false;}
}
$('devices-open').onclick=()=>work(async()=>{if(!auth())return;
 selectedUpload=null;$('device-message').textContent='';
 try{const published=await api('/api/lineup?date='+activeDate);selectedUpload={date:published.date,revision:published.revision};
 $('device-selection').textContent=`所选名单：${published.date} · 已发布 v${published.revision}${dirty?'（未保存草稿不上传）':''}`;
 }catch(e){$('device-selection').textContent='当前日期尚无可上传的发布名单，请先预览并发布。';}
 $('devices').showModal();await refreshDevices();clearInterval(deviceTimer);deviceTimer=setInterval(refreshDevices,4000);
});
$('devices').addEventListener('close',()=>clearInterval(deviceTimer));
$('pair-form').onsubmit=async e=>{e.preventDefault();if(deviceAction)return;deviceAction=true;
 try{await api('/api/devices/claim',{code:$('pair-code').value});$('pair-code').value='';$('device-message').textContent='设备已添加，可上传所选名单。';}
 catch(e){$('device-message').textContent=e.message;}finally{deviceAction=false;await refreshDevices();}
};
