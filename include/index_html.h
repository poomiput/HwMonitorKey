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
/* CapsLock indicator */
.caps-ind{display:inline-flex;align-items:center;gap:4px;font-size:.72em;padding:2px 10px;border-radius:6px;border:1px solid rgba(99,179,237,.15);color:#718096;transition:.3s}
.caps-ind.on{background:rgba(72,187,120,.15);color:#48bb78;border-color:rgba(72,187,120,.4)}
.caps-ind.off{background:rgba(160,174,192,.05);color:#4a5568}
/* Keybind buttons */
.kb-shortcuts{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.kb-shortcuts .sbtn{padding:5px 12px;border:1px solid rgba(183,148,244,.25);background:rgba(183,148,244,.08);color:#b794f4;border-radius:6px;cursor:pointer;font-size:.72em;font-weight:600;transition:.2s;white-space:nowrap}
.kb-shortcuts .sbtn:hover{background:rgba(183,148,244,.2);border-color:#b794f4}
.kb-shortcuts .sbtn:active{transform:scale(.95)}
/* Key buttons (common keys) */
.key-btns{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.key-btns .kbtn{padding:5px 12px;border:1px solid rgba(99,179,237,.25);background:rgba(99,179,237,.08);color:#63b3ed;border-radius:6px;cursor:pointer;font-size:.72em;font-weight:600;transition:.2s;white-space:nowrap}
.key-btns .kbtn:hover{background:rgba(99,179,237,.2);border-color:#63b3ed}
.key-btns .kbtn:active{transform:scale(.95)}
/* Two-column layout */
.kb-cols{display:flex;gap:16px;margin-top:8px}
.kb-col{flex:1;min-width:0}
.kb-col-title{font-size:.7em;color:#718096;text-transform:uppercase;letter-spacing:.5px;margin-bottom:6px;font-weight:600}
@media(max-width:600px){.kb-cols{flex-direction:column}}
/* Run script panel */
.run-panel{display:flex;gap:6px;margin-top:8px;align-items:center}
.run-input{flex:1;background:#111827;border:1px solid rgba(72,187,120,.2);border-radius:6px;padding:8px 12px;color:#48bb78;font-family:'Cascadia Code',Consolas,monospace;font-size:.82em;outline:none}
.run-input:focus{border-color:rgba(72,187,120,.5);box-shadow:0 0 8px rgba(72,187,120,.1)}
.run-input::placeholder{color:#2d3748}
.run-btn{padding:8px 16px;border:1px solid rgba(72,187,120,.4);background:rgba(72,187,120,.12);color:#48bb78;border-radius:6px;cursor:pointer;font-size:.75em;font-weight:700;transition:.2s;white-space:nowrap}
.run-btn:hover{background:rgba(72,187,120,.25);border-color:#48bb78}
/* Language indicator */
.lang-ind{display:inline-flex;align-items:center;gap:4px;font-size:.72em;padding:2px 10px;border-radius:6px;border:1px solid rgba(99,179,237,.15);color:#718096;transition:.3s}
.lang-ind.th{background:rgba(246,173,85,.15);color:#f6ad55;border-color:rgba(246,173,85,.4)}
.lang-ind.en{background:rgba(99,179,237,.15);color:#63b3ed;border-color:rgba(99,179,237,.4)}
.lang-ind.detecting{animation:langPulse 1s infinite}
@keyframes langPulse{0%,100%{opacity:1}50%{opacity:.4}}
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
<label style="margin-left:12px"><input type="checkbox" id="chkThai" style="accent-color:#f6ad55"> &#127481;&#127469; Thai KB</label>
<span style="font-size:.7em;color:#4a5568">(type here to send keys to PC)</span>
</div>
<div style="display:flex;align-items:center;gap:12px;margin-bottom:6px">
<span class="caps-ind off" id="capsInd">&#8682; CapsLock: OFF</span>
<span class="lang-ind" id="langInd">&#127760; Lang: ?</span>
<button class="sbtn" onclick="detectLang()" id="btnDetectLang" style="border-color:rgba(72,187,120,.4);background:rgba(72,187,120,.08);color:#48bb78;border-radius:6px;font-size:.72em;font-weight:600;padding:5px 12px;border-width:1px;border-style:solid;cursor:pointer">&#127760; Detect</button>
</div>
<textarea class="kb-area kb-hidden" id="kbInput" placeholder="Click here and type to send keystrokes to PC..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false"></textarea>
<div class="kb-cols kb-hidden" id="kbCols">
<div class="kb-col">
<div class="kb-col-title">&#9000; Keys</div>
<div class="key-btns">
<button class="kbtn" onclick="wsSendText('Tab')">Tab &#8677;</button>
<button class="kbtn" onclick="wsSendText('Escape')">Esc</button>
<button class="kbtn" onclick="wsSendText('Enter')">Enter &#9166;</button>
<button class="kbtn" onclick="wsSendText('Backspace')">&#9003; Bksp</button>
<button class="kbtn" onclick="wsSendCombo('Delete')">Del</button>
<button class="kbtn" onclick="wsSendCombo('Insert')">Ins</button>
<button class="kbtn" onclick="wsSendCombo('Home')">Home</button>
<button class="kbtn" onclick="wsSendCombo('End')">End</button>
<button class="kbtn" onclick="wsSendCombo('PageUp')">PgUp</button>
<button class="kbtn" onclick="wsSendCombo('PageDown')">PgDn</button>
<button class="kbtn" onclick="wsSendText('ArrowUp')">&#9650;</button>
<button class="kbtn" onclick="wsSendText('ArrowDown')">&#9660;</button>
<button class="kbtn" onclick="wsSendText('ArrowLeft')">&#9664;</button>
<button class="kbtn" onclick="wsSendText('ArrowRight')">&#9654;</button>
</div>
</div>
<div class="kb-col">
<div class="kb-col-title">&#128268; Hotkeys</div>
<div class="kb-shortcuts">
<button class="sbtn" onclick="wsSendCombo('CapsLock')">&#8682; CapsLock</button>
<button class="sbtn" onclick="wsSendCombo('Win+Space')">&#127760; Win+Space</button>
<button class="sbtn" onclick="wsSendCombo('Win+D')">&#128187; Win+D</button>
<button class="sbtn" onclick="wsSendCombo('Win+R')">&#9654; Win+R</button>
<button class="sbtn" onclick="wsSendCombo('Win+L')">&#128274; Win+L</button>
<button class="sbtn" onclick="wsSendCombo('Ctrl+Shift+Esc')">&#9881; TaskMgr</button>
<button class="sbtn" onclick="wsSendCombo('Ctrl+C')">Ctrl+C</button>
<button class="sbtn" onclick="wsSendCombo('Ctrl+V')">Ctrl+V</button>
<button class="sbtn" onclick="wsSendCombo('Ctrl+Z')">Ctrl+Z</button>
<button class="sbtn" onclick="wsSendCombo('Ctrl+A')">Ctrl+A</button>
<button class="sbtn" onclick="wsSendCombo('Alt+Tab')">Alt+Tab</button>
<button class="sbtn" onclick="wsSendCombo('Alt+F4')">Alt+F4</button>
<button class="sbtn" onclick="wsSendCombo('PrtSc')">&#128247; PrtSc</button>
</div>
</div>
</div>
<div class="run-panel kb-hidden" id="runPanel">
<input class="run-input" id="runInput" type="text" placeholder="&#9654; Paste command here, press Enter to run via Win+R..." autocomplete="off" spellcheck="false">
<button class="run-btn" onclick="runScript()">&#9654; Run</button>
</div>
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
    if(ev.data.startsWith('LED:CAPS:')){
      capsState=ev.data.charAt(9)==='1';
      updateCapsUI();
      return;
    }
    if(ev.data.startsWith('LANG:')){
      const lang=ev.data.substring(5);
      const li=document.getElementById('langInd');
      if(lang==='TH'){
        li.innerHTML='&#127481;&#127469; Thai';
        li.className='lang-ind th';
        document.getElementById('chkThai').checked=true;
      } else if(lang==='EN'){
        li.innerHTML='&#127482;&#127480; English';
        li.className='lang-ind en';
        document.getElementById('chkThai').checked=false;
      } else if(lang==='DETECTING'){
        li.innerHTML='&#127760; Detecting...';
        li.className='lang-ind detecting';
      }
      footer.textContent='[LangDetect] '+lang;
      return;
    }
    handleMsg(ev.data);
  };
}

setInterval(()=>{
  if(connStart) uptimeEl.textContent='\u23F1 '+fmtTime(Date.now()-connStart);
  if(sessionStart) sSession.textContent=fmtTime(Date.now()-sessionStart);
},1000);

// === Web Keyboard Input (Simple Text-based) ===
const kbInput=document.getElementById('kbInput');
const kbInfo=document.getElementById('kbInfo');

// Thai Kedmanee layout → English key mapping
// When phone KB is Thai, convert Thai chars to the English key at the same
// physical position so Keyboard.print() sends the correct HID keycode.
// PC with Thai layout will then produce the correct Thai character.
const THAI2EN={
  // ── Unshifted layer ──
  '_':'`',
  '\u0E45':'1','/'  :'2','-'  :'3','\u0E20':'4','\u0E16':'5',
  '\u0E38':'6','\u0E36':'7','\u0E04':'8','\u0E15':'9','\u0E08':'0',
  '\u0E02':'-','\u0E0A':'=',
  '\u0E46':'q','\u0E44':'w','\u0E33':'e','\u0E1E':'r','\u0E30':'t',
  '\u0E31':'y','\u0E35':'u','\u0E23':'i','\u0E19':'o','\u0E22':'p',
  '\u0E1A':'[','\u0E25':']','\u0E03':'\\',
  '\u0E1F':'a','\u0E2B':'s','\u0E01':'d','\u0E14':'f','\u0E40':'g',
  '\u0E49':'h','\u0E48':'j','\u0E32':'k','\u0E2A':'l','\u0E27':';','\u0E07':"'",
  '\u0E1C':'z','\u0E1B':'x','\u0E41':'c','\u0E2D':'v','\u0E34':'b',
  '\u0E37':'n','\u0E17':'m','\u0E21':',','\u0E43':'.','\u0E1D':'/',
  // ── Shifted layer ──
  '%':'~','+':'!',
  '\u0E51':'@','\u0E52':'#','\u0E53':'$','\u0E54':'%','\u0E39':'^',
  '\u0E3F':'&','\u0E55':'*','\u0E56':'(','\u0E57':')','\u0E58':'_','\u0E59':'+',
  '\u0E50':'Q','"':'W','\u0E0E':'E','\u0E11':'R','\u0E18':'T',
  '\u0E4D':'Y','\u0E4A':'U','\u0E13':'I','\u0E2F':'O','\u0E0D':'P',
  '\u0E10':'{',',':'}','\u0E05':'|',
  '\u0E24':'A','\u0E06':'S','\u0E0F':'D','\u0E42':'F','\u0E0C':'G',
  '\u0E47':'H','\u0E4B':'J','\u0E29':'K','\u0E28':'L','\u0E0B':':','.':'"',
  '(':'Z',')':'X','\u0E09':'C','\u0E2E':'V','\u0E3A':'B',
  '\u0E4C':'N','?':'M','\u0E12':'<','\u0E2C':'>','\u0E26':'?'
};

let capsState=false;
const capsInd=document.getElementById('capsInd');

function updateCapsUI(){
  capsInd.textContent='\u21E2 CapsLock: '+(capsState?'ON':'OFF');
  capsInd.className='caps-ind '+(capsState?'on':'off');
}

function toggleKb(){
  const on=document.getElementById('chkKb').checked;
  kbInput.classList.toggle('kb-hidden',!on);
  kbInfo.classList.toggle('kb-hidden',!on);
  document.getElementById('kbCols').classList.toggle('kb-hidden',!on);
  document.getElementById('runPanel').classList.toggle('kb-hidden',!on);
  if(on) kbInput.focus();
}

function wsSendCombo(combo){
  if(!ws||ws.readyState!==1){console.log('[WebKB] WS not connected!');return;}
  ws.send('CMD:'+combo);
  footer.textContent='[WebKB] combo: '+combo;
  console.log('[WebKB] combo:',combo);
  if(combo==='CapsLock'){capsState=!capsState;updateCapsUI();}
}

function detectLang(){
  if(!ws||ws.readyState!==1){console.log('[WebKB] WS not connected!');return;}
  ws.send('CMD:DetectLang');
  footer.textContent='[LangDetect] Starting detection...';
}

function runScript(){
  const ri=document.getElementById('runInput');
  const cmd=ri.value.trim();
  if(!cmd){return;}
  if(!ws||ws.readyState!==1){console.log('[WebKB] WS not connected!');return;}
  ws.send('RUNCMD:'+cmd);
  footer.textContent='[RunScript] '+cmd;
  ri.value='';
}
document.addEventListener('DOMContentLoaded',function(){
  const ri=document.getElementById('runInput');
  if(ri) ri.addEventListener('keydown',function(e){
    if(e.key==='Enter'){e.preventDefault();runScript();}
  });
});

function wsSendText(msg){
  if(!ws||ws.readyState!==1){console.log('[WebKB] WS not connected!');return;}
  ws.send(msg);
  footer.textContent='[WebKB] sent: '+msg;
  console.log('[WebKB] sent:',msg);
}

// Send with Thai display info: "TH:<en>:<thai>" so ESP can show Thai in monitor
function wsSendChar(original){
  const mapped=mapChar(original);
  if(document.getElementById('chkThai').checked && mapped!==original){
    wsSendText('TH:'+mapped+':'+original);
  } else {
    wsSendText(mapped);
  }
}

// Convert a character through Thai mapping if toggle is ON
function mapChar(ch){
  if(!document.getElementById('chkThai').checked) return ch;
  return THAI2EN[ch]||ch;
}

// PATH 1: keydown — special keys + Backspace fallback when empty
kbInput.addEventListener('keydown',function(e){
  if(e.key==='Enter'){e.preventDefault();wsSendText('Enter');return;}
  if(e.key==='Backspace' && kbInput.value.length===0){
    e.preventDefault();wsSendText('Backspace');return; // textarea empty fallback
  }
  if(e.key==='Tab'){e.preventDefault();wsSendText('Tab');return;}
  if(e.key==='Escape'){e.preventDefault();wsSendText('Escape');return;}
  if(e.key==='ArrowUp'){e.preventDefault();wsSendText('ArrowUp');return;}
  if(e.key==='ArrowDown'){e.preventDefault();wsSendText('ArrowDown');return;}
  if(e.key==='ArrowLeft'){e.preventDefault();wsSendText('ArrowLeft');return;}
  if(e.key==='ArrowRight'){e.preventDefault();wsSendText('ArrowRight');return;}
  // Let all other keys (including Backspace) fall through to input event
});

// PATH 2: input event — catches ALL typed characters (works on mobile IME too)
let isComposing=false;
kbInput.addEventListener('compositionstart',function(){isComposing=true;});
kbInput.addEventListener('compositionend',function(e){
  isComposing=false;
  if(e.data){
    for(const ch of e.data) wsSendChar(ch);
  }
});
kbInput.addEventListener('input',function(e){
  if(isComposing) return; // wait for compositionend
  if(e.inputType==='insertText' && e.data){
    for(const ch of e.data) wsSendChar(ch);
  } else if(e.inputType==='deleteContentBackward'){
    wsSendText('Backspace');
  }
});

connect();
</script>
</body>
</html>
)rawliteral";
