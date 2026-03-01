#pragma once
/**
 * index_html.h — Web Monitor Dashboard HTML
 * Auto-extracted from device_main.cpp for cleaner separation.
 */

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Keyboard Monitor</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI','Roboto',sans-serif;background:#0a0e17;color:#e0e6ed;min-height:100vh;display:flex;flex-direction:column}
header{padding:16px 24px;background:linear-gradient(135deg,#1a1f35,#0d1221);border-bottom:1px solid rgba(99,179,237,.15);text-align:center}
header h1{font-size:1.4em;font-weight:700;background:linear-gradient(90deg,#63b3ed,#b794f4);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
header p{font-size:.8em;color:#718096;margin:2px 0 6px}
.status-bar{display:flex;align-items:center;gap:8px;justify-content:center}
.dot{width:10px;height:10px;border-radius:50%;background:#f56565;transition:.3s}
.dot.on{background:#48bb78;box-shadow:0 0 8px rgba(72,187,120,.5)}
.stxt{font-size:.78em;color:#a0aec0}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;padding:12px 24px;max-width:860px;margin:0 auto;width:100%}
.stat-card{background:#111827;border:1px solid rgba(99,179,237,.08);border-radius:10px;padding:12px 14px;text-align:center}
.stat-card .val{font-size:1.5em;font-weight:700;color:#63b3ed}
.stat-card .lbl{font-size:.7em;color:#718096;margin-top:2px;text-transform:uppercase;letter-spacing:.5px}
.container{max-width:860px;margin:0 auto;width:100%;padding:0 24px 24px;flex:1;display:flex;flex-direction:column}
.toolbar{display:flex;justify-content:space-between;align-items:center;margin:12px 0 8px;flex-wrap:wrap;gap:6px}
.toolbar .left,.toolbar .right{display:flex;align-items:center;gap:6px}
.btn{padding:5px 14px;border:1px solid rgba(99,179,237,.25);background:transparent;color:#a0aec0;border-radius:6px;cursor:pointer;font-size:.75em;transition:.2s}
.btn:hover{background:rgba(99,179,237,.1);color:#e0e6ed}
.btn.active{background:rgba(99,179,237,.2);color:#63b3ed;border-color:#63b3ed}
.btn.danger{border-color:rgba(245,101,101,.4);color:#f56565}
.btn.danger:hover{background:rgba(245,101,101,.15)}
.view-toggle .btn.active{pointer-events:none}
.monitor-box{flex:1;background:#111827;border:1px solid rgba(99,179,237,.08);border-radius:12px;padding:16px;overflow-y:auto;min-height:260px;max-height:62vh}
/* Stream View */
#stream{font-family:'Cascadia Code','Fira Code',Consolas,monospace;font-size:1em;line-height:1.7;word-wrap:break-word;white-space:pre-wrap}
#stream .ts{color:#4a5568;font-size:.7em;margin-right:4px}
#stream .key-char{color:#63b3ed}
#stream .key-special{color:#b794f4;font-size:.85em}
#stream .key-mod{color:#f6ad55;font-size:.85em}
#stream .key-release{color:#4a5568;font-size:.8em;text-decoration:line-through}
#stream .key-web{color:#ecc94b;background:rgba(236,201,75,.1);padding:0 2px;border-radius:3px}
/* Log Table View */
#logTable{display:none;width:100%;border-collapse:collapse;font-size:.8em}
#logTable th{position:sticky;top:0;background:#1a1f35;color:#a0aec0;padding:8px 10px;text-align:left;font-weight:600;font-size:.75em;text-transform:uppercase;letter-spacing:.5px;border-bottom:1px solid rgba(99,179,237,.15)}
#logTable td{padding:6px 10px;border-bottom:1px solid rgba(255,255,255,.03);font-family:'Cascadia Code',Consolas,monospace;vertical-align:middle}
#logTable tr:hover{background:rgba(99,179,237,.04)}
.badge{padding:2px 8px;border-radius:4px;font-size:.72em;font-weight:600;text-transform:uppercase;letter-spacing:.3px}
.badge.press{background:rgba(72,187,120,.15);color:#48bb78}
.badge.release{background:rgba(160,174,192,.1);color:#718096}
.badge.special{background:rgba(183,148,244,.15);color:#b794f4}
.badge.mod{background:rgba(246,173,85,.15);color:#f6ad55}
.badge.mod_up{background:rgba(246,173,85,.08);color:#a0aec0}
.badge.web{background:rgba(236,201,75,.15);color:#ecc94b}
.hid-raw{color:#4a5568;font-size:.85em}
/* Web Keyboard Input */
.kb-panel{max-width:860px;margin:0 auto;width:100%;padding:0 24px}
.kb-toggle{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.kb-toggle label{font-size:.78em;color:#718096;cursor:pointer;display:flex;align-items:center;gap:4px}
.kb-area{width:100%;min-height:80px;background:#111827;border:1px solid rgba(99,179,237,.15);border-radius:10px;padding:14px;color:#63b3ed;font-family:'Cascadia Code',Consolas,monospace;font-size:1em;outline:none;caret-color:#63b3ed;resize:none}
.kb-area:focus{border-color:rgba(99,179,237,.4);box-shadow:0 0 12px rgba(99,179,237,.1)}
.kb-area::placeholder{color:#4a5568}
.kb-info{font-size:.7em;color:#4a5568;margin-top:4px;text-align:center}
.kb-hidden{display:none}
footer{text-align:center;padding:8px;font-size:.7em;color:#2d3748}
@media(max-width:600px){.stats{grid-template-columns:repeat(2,1fr)}.stat-card .val{font-size:1.2em}}
</style>
</head>
<body>
<header>
<h1>&#9000; Keyboard Monitor</h1>
<p>ESP32-S3 Wireless USB HID Bridge &mdash; Live Dashboard</p>
<div class="status-bar">
<div class="dot" id="dot"></div>
<span class="stxt" id="stxt">Disconnected</span>
<span class="stxt" id="uptime"></span>
</div>
</header>

<div class="stats">
<div class="stat-card"><div class="val" id="sTotal">0</div><div class="lbl">Total Keys</div></div>
<div class="stat-card"><div class="val" id="sKPM">0</div><div class="lbl">Keys/Min</div></div>
<div class="stat-card"><div class="val" id="sSession">00:00</div><div class="lbl">Session</div></div>
<div class="stat-card"><div class="val" id="sTop">-</div><div class="lbl">Top Key</div></div>
</div>

<div class="container">
<div class="toolbar">
<div class="left">
<div class="view-toggle">
<button class="btn active" id="btnStream" onclick="setView('stream')">Stream</button>
<button class="btn" id="btnLog" onclick="setView('log')">Log</button>
</div>
<label style="display:flex;align-items:center;gap:4px;font-size:.75em;color:#718096;cursor:pointer">
<input type="checkbox" id="chkScroll" checked style="accent-color:#63b3ed"> Auto-scroll
</label>
<label style="display:flex;align-items:center;gap:4px;font-size:.75em;color:#718096;cursor:pointer">
<input type="checkbox" id="chkSound" style="accent-color:#63b3ed"> Sound
</label>
</div>
<div class="right">
<button class="btn" onclick="exportCSV()">Export CSV</button>
<button class="btn danger" onclick="clearAll()">Clear</button>
</div>
</div>
<div class="monitor-box" id="monBox">
<div id="stream"></div>
<table id="logTable">
<thead><tr><th>#</th><th>Time</th><th>Event</th><th>Key</th><th>Modifier</th><th>HID Code</th></tr></thead>
<tbody id="logBody"></tbody>
</table>
</div>
</div>
<div class="kb-panel" id="kbPanel">
<div class="kb-toggle">
<label><input type="checkbox" id="chkKb" style="accent-color:#b794f4" onchange="toggleKb()"> &#9000; Web Keyboard</label>
<span style="font-size:.7em;color:#4a5568">(type here to send keys to PC)</span>
</div>
<textarea class="kb-area kb-hidden" id="kbInput" placeholder="Click here and type to send keystrokes to PC..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false"></textarea>
<div class="kb-info kb-hidden" id="kbInfo">Keys typed here are sent as USB HID to the PC via ESP32</div>
</div>
<footer id="footer">Waiting for connection...</footer>

<script>
const stream=document.getElementById('stream'),logBody=document.getElementById('logBody');
const dot=document.getElementById('dot'),stxt=document.getElementById('stxt');
const monBox=document.getElementById('monBox'),footer=document.getElementById('footer');
const sTotal=document.getElementById('sTotal'),sKPM=document.getElementById('sKPM');
const sSession=document.getElementById('sSession'),sTop=document.getElementById('sTop');
const uptimeEl=document.getElementById('uptime');
let ws,keyCount=0,pressCount=0,sessionStart=null,connStart=null;
let logData=[],keyStat={},viewMode='stream';
let lastStreamTime=0,streamLineCount=0;
const audioCtx=new(window.AudioContext||window.webkitAudioContext)();

function beep(){
  if(!document.getElementById('chkSound').checked)return;
  const o=audioCtx.createOscillator(),g=audioCtx.createGain();
  o.connect(g);g.connect(audioCtx.destination);
  o.frequency.value=800;g.gain.value=0.05;
  o.start();o.stop(audioCtx.currentTime+0.04);
}

function fmtTime(ms){
  let s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  s%=60;m%=60;
  return h>0?(h+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0'))
            :(String(m).padStart(2,'0')+':'+String(s).padStart(2,'0'));
}

function now(){return new Date().toLocaleTimeString('en-GB',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit',fractionalSecondDigits:1})}

function setView(v){
  viewMode=v;
  document.getElementById('stream').style.display=v==='stream'?'':'none';
  document.getElementById('logTable').style.display=v==='log'?'table':'none';
  document.getElementById('btnStream').classList.toggle('active',v==='stream');
  document.getElementById('btnLog').classList.toggle('active',v==='log');
}

function updateStats(){
  sTotal.textContent=pressCount;
  if(sessionStart){
    const elapsed=(Date.now()-sessionStart)/1000;
    sKPM.textContent=elapsed>5?Math.round(pressCount/(elapsed/60)):0;
    sSession.textContent=fmtTime(Date.now()-sessionStart);
  }
  let topK='-',topV=0;
  for(const[k,v]of Object.entries(keyStat)){if(v>topV){topV=v;topK=k;}}
  sTop.textContent=topK;
}

function addStream(data){
  const nowMs=Date.now();
  const gap=nowMs-lastStreamTime;
  lastStreamTime=nowMs;

  if(gap>5000 && streamLineCount>0){
    const divider=document.createElement('div');
    divider.style.cssText='margin:6px 0 2px;border-top:1px solid rgba(99,179,237,.08);padding-top:4px;';
    const tsBig=document.createElement('span');
    tsBig.className='ts';
    tsBig.textContent='\u23F1 '+now()+' (#'+(streamLineCount+1)+')';
    divider.appendChild(tsBig);
    stream.appendChild(divider);
  } else if(streamLineCount===0){
    const tsBig=document.createElement('span');
    tsBig.className='ts';
    tsBig.textContent='\u23F1 '+now()+' (#1) ';
    stream.appendChild(tsBig);
  }
  streamLineCount++;

  const span=document.createElement('span');
  if(data.src==='web'){
    if(data.e==='release') return;
    span.className='key-web';span.textContent=data.k;
  } else if(data.e==='release'||data.e==='mod_up'){
    span.className='key-release';span.textContent=data.k+'\u2191 ';
  } else if(data.e==='special'){
    span.className='key-special';span.textContent=data.k+' ';
  } else if(data.e==='mod'){
    span.className='key-mod';span.textContent='['+data.k+'] ';
  } else {
    span.className='key-char';span.textContent=data.k;
  }
  stream.appendChild(span);
  if(document.getElementById('chkScroll').checked) monBox.scrollTop=monBox.scrollHeight;
}

function addLog(data){
  keyCount++;
  const tr=document.createElement('tr');
  if(data.src==='web'){
    if(data.e==='release'){keyCount--;return;}
    tr.innerHTML='<td style="color:#4a5568">'+keyCount+'</td>'
      +'<td>'+now()+'</td>'
      +'<td><span class="badge web">\u2328 web</span></td>'
      +'<td style="color:#ecc94b;font-weight:600">'+data.k+'</td>'
      +'<td class="hid-raw">-</td>'
      +'<td class="hid-raw">-</td>';
    logBody.appendChild(tr);
    logData.push({n:keyCount,time:now(),event:'web',key:data.k,mod:'-',hid:'-'});
  } else {
  const badgeCls=data.e==='press'?'press':data.e==='release'?'release':data.e==='special'?'special':data.e.startsWith('mod')?data.e:'mod';
  tr.innerHTML='<td style="color:#4a5568">'+keyCount+'</td>'
    +'<td>'+now()+'</td>'
    +'<td><span class="badge '+badgeCls+'">'+data.e+'</span></td>'
    +'<td style="color:#e0e6ed;font-weight:600">'+data.k+'</td>'
    +'<td class="hid-raw">'+data.m+'</td>'
    +'<td class="hid-raw">'+data.h+'</td>';
  logBody.appendChild(tr);
  logData.push({n:keyCount,time:now(),event:data.e,key:data.k,mod:data.m,hid:data.h});
  }
  if(document.getElementById('chkScroll').checked) monBox.scrollTop=monBox.scrollHeight;
}

function handleMsg(raw){
  let data;
  try{data=JSON.parse(raw);}catch(e){
    data={e:'press',k:raw,h:'',m:'',t:0};
  }
  if(!sessionStart) sessionStart=Date.now();

  if(data.e==='press'||data.e==='special'){
    pressCount++;
    keyStat[data.k]=(keyStat[data.k]||0)+1;
    beep();
  }
  addStream(data);
  addLog(data);
  updateStats();
}

function exportCSV(){
  if(!logData.length) return;
  let csv='#,Time,Event,Key,Modifier,HID Code\n';
  logData.forEach(r=>{csv+=r.n+',"'+r.time+'",'+r.event+',"'+r.key+'",'+r.mod+','+r.hid+'\n';});
  const blob=new Blob([csv],{type:'text/csv'});
  const a=document.createElement('a');a.href=URL.createObjectURL(blob);
  a.download='keyboard_log_'+new Date().toISOString().slice(0,19).replace(/:/g,'-')+'.csv';
  a.click();
}

function clearAll(){
  stream.innerHTML='';logBody.innerHTML='';
  keyCount=0;pressCount=0;keyStat={};logData=[];sessionStart=null;
  lastStreamTime=0;streamLineCount=0;
  updateStats();sSession.textContent='00:00';sKPM.textContent='0';
}

function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=()=>{
    dot.classList.add('on');stxt.textContent='Connected';
    footer.textContent='Live \u2014 receiving keystrokes';
    connStart=Date.now();
  };
  ws.onclose=()=>{
    dot.classList.remove('on');stxt.textContent='Reconnecting...';
    footer.textContent='Connection lost. Reconnecting...';connStart=null;
    setTimeout(connect,2000);
  };
  ws.onerror=()=>{ws.close();};
  ws.onmessage=(ev)=>{
    if(ev.data.startsWith('Connected')) return;
    handleMsg(ev.data);
  };
}

setInterval(()=>{
  if(connStart) uptimeEl.textContent='\u23F1 '+fmtTime(Date.now()-connStart);
  if(sessionStart) sSession.textContent=fmtTime(Date.now()-sessionStart);
},1000);

// === Web Keyboard Input ===
const kbInput=document.getElementById('kbInput');
const kbInfo=document.getElementById('kbInfo');
const JS2HID={KeyA:0x04,KeyB:0x05,KeyC:0x06,KeyD:0x07,KeyE:0x08,KeyF:0x09,KeyG:0x0A,KeyH:0x0B,KeyI:0x0C,KeyJ:0x0D,KeyK:0x0E,KeyL:0x0F,KeyM:0x10,KeyN:0x11,KeyO:0x12,KeyP:0x13,KeyQ:0x14,KeyR:0x15,KeyS:0x16,KeyT:0x17,KeyU:0x18,KeyV:0x19,KeyW:0x1A,KeyX:0x1B,KeyY:0x1C,KeyZ:0x1D,Digit1:0x1E,Digit2:0x1F,Digit3:0x20,Digit4:0x21,Digit5:0x22,Digit6:0x23,Digit7:0x24,Digit8:0x25,Digit9:0x26,Digit0:0x27,Enter:0x28,Escape:0x29,Backspace:0x2A,Tab:0x2B,Space:0x2C,Minus:0x2D,Equal:0x2E,BracketLeft:0x2F,BracketRight:0x30,Backslash:0x31,Semicolon:0x33,Quote:0x34,Backquote:0x35,Comma:0x36,Period:0x37,Slash:0x38,CapsLock:0x39,F1:0x3A,F2:0x3B,F3:0x3C,F4:0x3D,F5:0x3E,F6:0x3F,F7:0x40,F8:0x41,F9:0x42,F10:0x43,F11:0x44,F12:0x45,ArrowRight:0x4F,ArrowLeft:0x50,ArrowDown:0x51,ArrowUp:0x52,Delete:0x4C,Home:0x4A,End:0x4D,PageUp:0x4B,PageDown:0x4E,Insert:0x49};
const KEY2HID={a:0x04,b:0x05,c:0x06,d:0x07,e:0x08,f:0x09,g:0x0A,h:0x0B,i:0x0C,j:0x0D,k:0x0E,l:0x0F,m:0x10,n:0x11,o:0x12,p:0x13,q:0x14,r:0x15,s:0x16,t:0x17,u:0x18,v:0x19,w:0x1A,x:0x1B,y:0x1C,z:0x1D,A:0x04,B:0x05,C:0x06,D:0x07,E:0x08,F:0x09,G:0x0A,H:0x0B,I:0x0C,J:0x0D,K:0x0E,L:0x0F,M:0x10,N:0x11,O:0x12,P:0x13,Q:0x14,R:0x15,S:0x16,T:0x17,U:0x18,V:0x19,W:0x1A,X:0x1B,Y:0x1C,Z:0x1D,'1':0x1E,'2':0x1F,'3':0x20,'4':0x21,'5':0x22,'6':0x23,'7':0x24,'8':0x25,'9':0x26,'0':0x27,Enter:0x28,Escape:0x29,Backspace:0x2A,Tab:0x2B,' ':0x2C,'-':0x2D,'=':0x2E,'[':0x2F,']':0x30,'\\':0x31,';':0x33,"'":0x34,'`':0x35,',':0x36,'.':0x37,'/':0x38,ArrowRight:0x4F,ArrowLeft:0x50,ArrowDown:0x51,ArrowUp:0x52,Delete:0x4C,Home:0x4A,End:0x4D,PageUp:0x4B,PageDown:0x4E,Insert:0x49};

function resolveHID(e){
  return JS2HID[e.code]||KEY2HID[e.key]||0;
}
let webKeysDown=new Set();

function toggleKb(){
  const on=document.getElementById('chkKb').checked;
  kbInput.classList.toggle('kb-hidden',!on);
  kbInfo.classList.toggle('kb-hidden',!on);
  if(on) kbInput.focus();
}

function sendWebKey(code,mod,isDown){
  if(!ws||ws.readyState!==1){console.log('[WebKB] WS not connected!');return;}
  const msg='KB:'+(isDown?'D':'U')+':'+code+':'+mod;
  console.log('[WebKB] Sending:',msg);
  ws.send(msg);
  footer.textContent='[WebKB] '+(isDown?'DOWN':'UP')+' hid='+code+' mod='+mod;
}

kbInput.addEventListener('keydown',function(e){
  const hid=resolveHID(e);
  console.log('[WebKB] keydown code=',e.code,'key=',e.key,'hid=',hid);
  if(!hid) return;
  e.preventDefault();
  let mod=0;
  if(e.shiftKey||(/^[A-Z]$/.test(e.key))) mod|=0x02;
  if(e.ctrlKey) mod|=0x01;
  if(e.altKey) mod|=0x04;
  if(e.metaKey) mod|=0x08;
  if(!webKeysDown.has(hid)){
    webKeysDown.add(hid);
    sendWebKey(hid,mod,true);
  }
});

kbInput.addEventListener('keyup',function(e){
  e.preventDefault();
  const hid=resolveHID(e);
  let mod=0;
  if(e.ctrlKey) mod|=0x01;
  if(e.shiftKey) mod|=0x02;
  if(e.altKey) mod|=0x04;
  if(e.metaKey) mod|=0x08;
  webKeysDown.delete(hid);
  if(hid) sendWebKey(hid,mod,false);
});

kbInput.addEventListener('blur',function(){
  if(ws&&ws.readyState===1) ws.send('KB:R:0:0');
  webKeysDown.clear();
});

// === Mobile IME fallback ===
let isComposing=false;
function sendChars(str){
  for(const ch of str){
    const hid=KEY2HID[ch]||KEY2HID[ch.toLowerCase()]||0;
    if(hid){
      let mod=0;
      if(/^[A-Z]$/.test(ch)) mod|=0x02;
      sendWebKey(hid,mod,true);
      setTimeout(()=>sendWebKey(hid,mod,false),50);
      console.log('[WebKB] char=',ch,'hid=',hid);
    }
  }
}
kbInput.addEventListener('compositionstart',function(){isComposing=true;});
kbInput.addEventListener('compositionend',function(e){
  isComposing=false;
  if(e.data) sendChars(e.data);
});
kbInput.addEventListener('input',function(e){
  if(isComposing) return;
  if(e.inputType==='insertText' && e.data) sendChars(e.data);
});

connect();
</script>
</body>
</html>
)rawliteral";
