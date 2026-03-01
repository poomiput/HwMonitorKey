/**
 * =============================================================================
 * device_main.cpp — ESP32-S3 #2: USB Device HID + ESP-NOW Receiver + Web
 * Monitor
 * =============================================================================
 *
 * This board:
 *   1) Receives 8-byte HID keyboard reports via ESP-NOW
 *   2) Forwards them to the PC as a USB HID Keyboard device
 *   3) Runs a Wi-Fi AP with a sync WebServer + WebSocket (port 81)
 *      to broadcast live keystrokes to connected web clients
 *   4) Implements a watchdog to release all keys if no packet
 *      is received within KEY_RELEASE_TIMEOUT_MS
 *
 * Hardware:
 *   - ESP32-S3 DevKitC-1 with native USB port connected to the PC
 *   - Wi-Fi AP mode for the live web monitor
 *
 * =============================================================================
 */

#include "shared_protocol.h"
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// =====================================================================
// Dual-serial helper: print to both USB CDC and UART0
// =====================================================================
#define LOG(fmt, ...)                                                            \
  do {                                                                           \
    Serial.printf(fmt, ##__VA_ARGS__);                                           \
    Serial0.printf(fmt, ##__VA_ARGS__);                                          \
  } while (0)

#define LOGLN(msg)                                                               \
  do {                                                                           \
    Serial.println(msg);                                                         \
    Serial0.println(msg);                                                        \
  } while (0)

// =====================================================================
// Global Objects
// =====================================================================
static USBHIDKeyboard Keyboard;
static WebServer server(WS_PORT);
static WebSocketsServer webSocket(81);

// --- State ---
static volatile bool newReportAvailable = false;
static kbd_report_t latestReport;
static kbd_report_t prevReport;
static unsigned long lastPacketTime = 0;
static bool keysPressed = false;

// =====================================================================
// HID Keycode → ASCII Mapping
// =====================================================================
static String hidToAscii(uint8_t keycode, uint8_t modifier) {
  bool shift = (modifier & 0x22);
  bool ctrl = (modifier & 0x11);

  // Letters: 0x04 (A) to 0x1D (Z)
  if (keycode >= 0x04 && keycode <= 0x1D) {
    char c = 'a' + (keycode - 0x04);
    if (shift) c -= 32;
    if (ctrl) return String("Ctrl+") + String((char)(c - 32 + 64));
    return String(c);
  }

  // Numbers: 0x1E (1) to 0x27 (0)
  if (keycode >= 0x1E && keycode <= 0x27) {
    const char nums[] = "1234567890";
    const char shiftNums[] = "!@#$%^&*()";
    int idx = keycode - 0x1E;
    return String(shift ? shiftNums[idx] : nums[idx]);
  }

  switch (keycode) {
  case 0x28: return "[Enter]";
  case 0x29: return "[Esc]";
  case 0x2A: return "[Backspace]";
  case 0x2B: return "[Tab]";
  case 0x2C: return " ";
  case 0x2D: return shift ? String("_") : String("-");
  case 0x2E: return shift ? String("+") : String("=");
  case 0x2F: return shift ? String("{") : String("[");
  case 0x30: return shift ? String("}") : String("]");
  case 0x31: return shift ? String("|") : String("\\");
  case 0x33: return shift ? String(":") : String(";");
  case 0x34: return shift ? String("\"") : String("'");
  case 0x35: return shift ? String("~") : String("`");
  case 0x36: return shift ? String("<") : String(",");
  case 0x37: return shift ? String(">") : String(".");
  case 0x38: return shift ? String("?") : String("/");
  case 0x39: return "[CapsLock]";
  case 0x3A: return "[F1]";  case 0x3B: return "[F2]";
  case 0x3C: return "[F3]";  case 0x3D: return "[F4]";
  case 0x3E: return "[F5]";  case 0x3F: return "[F6]";
  case 0x40: return "[F7]";  case 0x41: return "[F8]";
  case 0x42: return "[F9]";  case 0x43: return "[F10]";
  case 0x44: return "[F11]"; case 0x45: return "[F12]";
  case 0x46: return "[PrintScreen]";
  case 0x47: return "[ScrollLock]";
  case 0x48: return "[Pause]";
  case 0x49: return "[Insert]";
  case 0x4A: return "[Home]";
  case 0x4B: return "[PageUp]";
  case 0x4C: return "[Delete]";
  case 0x4D: return "[End]";
  case 0x4E: return "[PageDown]";
  case 0x4F: return "[Right]";
  case 0x50: return "[Left]";
  case 0x51: return "[Down]";
  case 0x52: return "[Up]";
  default: return keycode > 0 ? String("[0x") + String(keycode, HEX) + "]" : "";
  }
}

static String modifierString(uint8_t mod) {
  String s = "";
  if (mod & 0x01) s += "LCtrl+";
  if (mod & 0x02) s += "LShift+";
  if (mod & 0x04) s += "LAlt+";
  if (mod & 0x08) s += "LGUI+";
  if (mod & 0x10) s += "RCtrl+";
  if (mod & 0x20) s += "RShift+";
  if (mod & 0x40) s += "RAlt+";
  if (mod & 0x80) s += "RGUI+";
  return s;
}

// =====================================================================
// Web Monitor HTML Page
// =====================================================================
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
  // Top key
  let topK='-',topV=0;
  for(const[k,v]of Object.entries(keyStat)){if(v>topV){topV=v;topK=k;}}
  sTop.textContent=topK;
}

function addStream(data){
  const nowMs=Date.now();
  const gap=nowMs-lastStreamTime;
  lastStreamTime=nowMs;

  // If gap > 5s, start a new line with timestamp + line count
  if(gap>5000 && streamLineCount>0){
    const divider=document.createElement('div');
    divider.style.cssText='margin:6px 0 2px;border-top:1px solid rgba(99,179,237,.08);padding-top:4px;';
    const tsBig=document.createElement('span');
    tsBig.className='ts';
    tsBig.textContent='\u23F1 '+now()+' (#'+(streamLineCount+1)+')';
    divider.appendChild(tsBig);
    stream.appendChild(divider);
  } else if(streamLineCount===0){
    // First event ever — show timestamp
    const tsBig=document.createElement('span');
    tsBig.className='ts';
    tsBig.textContent='\u23F1 '+now()+' (#1) ';
    stream.appendChild(tsBig);
  }
  streamLineCount++;

  const span=document.createElement('span');
  if(data.e==='release'||data.e==='mod_up'){
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
  const badgeCls=data.e==='press'?'press':data.e==='release'?'release':data.e==='special'?'special':data.e.startsWith('mod')?data.e:'mod';
  tr.innerHTML='<td style="color:#4a5568">'+keyCount+'</td>'
    +'<td>'+now()+'</td>'
    +'<td><span class="badge '+badgeCls+'">'+data.e+'</span></td>'
    +'<td style="color:#e0e6ed;font-weight:600">'+data.k+'</td>'
    +'<td class="hid-raw">'+data.m+'</td>'
    +'<td class="hid-raw">'+data.h+'</td>';
  logBody.appendChild(tr);
  logData.push({n:keyCount,time:now(),event:data.e,key:data.k,mod:data.m,hid:data.h});
  if(document.getElementById('chkScroll').checked) monBox.scrollTop=monBox.scrollHeight;
}

function handleMsg(raw){
  let data;
  try{data=JSON.parse(raw);}catch(e){
    // Legacy plain text fallback
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
    // Skip initial welcome message
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
let webKeysDown=new Set();

function toggleKb(){
  const on=document.getElementById('chkKb').checked;
  kbInput.classList.toggle('kb-hidden',!on);
  kbInfo.classList.toggle('kb-hidden',!on);
  if(on) kbInput.focus();
}

function sendWebKey(code,mod,isDown){
  if(!ws||ws.readyState!==1) return;
  // Format: "KB:<down|up>:<hidKeycode>:<modifier>"
  ws.send('KB:'+(isDown?'D':'U')+':'+code+':'+mod);
}

kbInput.addEventListener('keydown',function(e){
  e.preventDefault();
  const hid=JS2HID[e.code];
  if(!hid && !e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey) return;
  let mod=0;
  if(e.ctrlKey) mod|=0x01;
  if(e.shiftKey) mod|=0x02;
  if(e.altKey) mod|=0x04;
  if(e.metaKey) mod|=0x08;
  const k=hid||0;
  if(!webKeysDown.has(k)){
    webKeysDown.add(k);
    sendWebKey(k,mod,true);
  }
});

kbInput.addEventListener('keyup',function(e){
  e.preventDefault();
  const hid=JS2HID[e.code]||0;
  let mod=0;
  if(e.ctrlKey) mod|=0x01;
  if(e.shiftKey) mod|=0x02;
  if(e.altKey) mod|=0x04;
  if(e.metaKey) mod|=0x08;
  webKeysDown.delete(hid);
  sendWebKey(hid,mod,false);
});

// Release all on blur
kbInput.addEventListener('blur',function(){
  if(ws&&ws.readyState===1) ws.send('KB:R:0:0');
  webKeysDown.clear();
});

connect();
</script>
</body>
</html>
)rawliteral";

// =====================================================================
// Forward Declarations
// =====================================================================
static void forwardReportToPC(const kbd_report_t &report);
static void releaseAllKeys();

// =====================================================================
// ESP-NOW Receive Callback (runs in Wi-Fi task context)
// =====================================================================
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data,
                         int data_len) {
  if (data_len == sizeof(kbd_report_t)) {
    memcpy((void *)&latestReport, data, sizeof(kbd_report_t));
    newReportAvailable = true;
    lastPacketTime = millis();
  }
}

// =====================================================================
// WebSocket Event Handler (WebSocketsServer by Links2004)
// =====================================================================
static void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                           size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    LOG("[WS] Client #%u connected\n", num);
    webSocket.sendTXT(num, "Connected to KeyboardMonitor");
    break;
  case WStype_DISCONNECTED:
    LOG("[WS] Client #%u disconnected\n", num);
    break;
  case WStype_TEXT: {
    // Handle web keyboard input: "KB:D:<hid>:<mod>" or "KB:U:..." or "KB:R:..."
    if (length > 3 && payload[0] == 'K' && payload[1] == 'B' && payload[2] == ':') {
      char cmd = payload[3];
      if (cmd == 'R') {
        // Release all keys from web keyboard
        releaseAllKeys();
        LOG("[WebKB] Release all\n");
      } else {
        // Parse "KB:D:<hid>:<mod>" or "KB:U:<hid>:<mod>"
        int hid = 0, mod = 0;
        sscanf((const char *)payload + 5, "%d:%d", &hid, &mod);
        if (cmd == 'D') {
          // Key down — build report with this key
          kbd_report_t rpt = {};
          rpt.modifier = (uint8_t)mod;
          rpt.keycodes[0] = (uint8_t)hid;
          forwardReportToPC(rpt);
          // Also broadcast to monitor
          String ascii = hidToAscii(hid, mod);
          char json[128];
          snprintf(json, sizeof(json),
                   "{\"e\":\"press\",\"k\":\"%s\",\"h\":\"0x%02X\",\"m\":\"0x%02X\",\"t\":%lu,\"src\":\"web\"}",
                   ascii.c_str(), hid, mod, millis());
          webSocket.broadcastTXT(json);
          LOG("[WebKB] Down: %s (0x%02X) mod=0x%02X\n", ascii.c_str(), hid, mod);
        } else if (cmd == 'U') {
          // Key up — send empty report (release)
          kbd_report_t rpt = {};
          rpt.modifier = (uint8_t)mod;
          forwardReportToPC(rpt);
          String ascii = hidToAscii(hid, 0);
          char json[128];
          snprintf(json, sizeof(json),
                   "{\"e\":\"release\",\"k\":\"%s\",\"h\":\"0x%02X\",\"m\":\"0x%02X\",\"t\":%lu,\"src\":\"web\"}",
                   ascii.c_str(), hid, mod, millis());
          webSocket.broadcastTXT(json);
        }
      }
    }
    break;
  }
  default:
    break;
  }
}

// =====================================================================
// Forward HID Report → USB HID Keyboard to PC
// =====================================================================
static void forwardReportToPC(const kbd_report_t &report) {
  KeyReport kr;
  kr.modifiers = report.modifier;
  kr.reserved = 0;
  memcpy(kr.keys, report.keycodes, 6);
  Keyboard.sendReport(&kr);
}

static void releaseAllKeys() { Keyboard.releaseAll(); }

// =====================================================================
// Broadcast Keystroke to WebSocket Clients (JSON format)
// =====================================================================
static void broadcastKeystrokes(const kbd_report_t &curr,
                                const kbd_report_t &prev) {
  unsigned long ts = millis();

  // --- Detect newly pressed keys ---
  for (int i = 0; i < 6; i++) {
    if (curr.keycodes[i] == 0x00)
      continue;

    bool wasPressed = false;
    for (int j = 0; j < 6; j++) {
      if (curr.keycodes[i] == prev.keycodes[j]) {
        wasPressed = true;
        break;
      }
    }

    if (!wasPressed) {
      String ascii = hidToAscii(curr.keycodes[i], curr.modifier);
      String type = "press";
      if (ascii.startsWith("[") && ascii.endsWith("]")) type = "special";

      // JSON: {"e":"press","k":"a","h":"0x04","m":"0x00","t":12345}
      char json[128];
      snprintf(json, sizeof(json),
               "{\"e\":\"%s\",\"k\":\"%s\",\"h\":\"0x%02X\",\"m\":\"0x%02X\",\"t\":%lu}",
               type.c_str(), ascii.c_str(), curr.keycodes[i], curr.modifier, ts);
      webSocket.broadcastTXT(json);
      LOG("[Key] %s (0x%02X) mod=0x%02X\n", ascii.c_str(), curr.keycodes[i], curr.modifier);
    }
  }

  // --- Detect released keys ---
  for (int i = 0; i < 6; i++) {
    if (prev.keycodes[i] == 0x00)
      continue;

    bool stillPressed = false;
    for (int j = 0; j < 6; j++) {
      if (prev.keycodes[i] == curr.keycodes[j]) {
        stillPressed = true;
        break;
      }
    }

    if (!stillPressed) {
      String ascii = hidToAscii(prev.keycodes[i], prev.modifier);
      char json[128];
      snprintf(json, sizeof(json),
               "{\"e\":\"release\",\"k\":\"%s\",\"h\":\"0x%02X\",\"m\":\"0x%02X\",\"t\":%lu}",
               ascii.c_str(), prev.keycodes[i], prev.modifier, ts);
      webSocket.broadcastTXT(json);
    }
  }

  // --- Detect modifier changes ---
  uint8_t modPressed = curr.modifier & ~prev.modifier;
  uint8_t modReleased = prev.modifier & ~curr.modifier;

  if (modPressed) {
    String modStr = modifierString(curr.modifier);
    if (modStr.length() > 0) {
      modStr = modStr.substring(0, modStr.length() - 1); // remove trailing +
      char json[128];
      snprintf(json, sizeof(json),
               "{\"e\":\"mod\",\"k\":\"%s\",\"h\":\"\",\"m\":\"0x%02X\",\"t\":%lu}",
               modStr.c_str(), curr.modifier, ts);
      webSocket.broadcastTXT(json);
    }
  }
  if (modReleased) {
    String modStr = modifierString(prev.modifier);
    if (modStr.length() > 0) {
      modStr = modStr.substring(0, modStr.length() - 1);
      char json[128];
      snprintf(json, sizeof(json),
               "{\"e\":\"mod_up\",\"k\":\"%s\",\"h\":\"\",\"m\":\"0x%02X\",\"t\":%lu}",
               modStr.c_str(), prev.modifier, ts);
      webSocket.broadcastTXT(json);
    }
  }
}

// =====================================================================
// Wi-Fi AP + ESP-NOW Initialization
// =====================================================================
static void initWiFiAndEspNow() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, ESPNOW_WIFI_CHANNEL, 0,
              AP_MAX_CONNECTIONS);
  delay(500); // Wait for AP stack to be fully ready

  LOG("[WiFi] AP Started — SSID: %s, Channel: %d\n", AP_SSID,
      ESPNOW_WIFI_CHANNEL);
  LOG("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
  LOG("[WiFi] MAC: %s\n", WiFi.softAPmacAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    LOGLN("[ESP-NOW] Init FAILED! Restarting...");
    delay(1000);
    ESP.restart();
  }
  LOGLN("[ESP-NOW] Initialized OK");

  esp_now_register_recv_cb(onEspNowRecv);
}

// =====================================================================
// Web Server + WebSocket Initialization
// =====================================================================
static void initWebServer() {
  // HTTP: serve monitor page
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.begin();
  LOGLN("[WebServer] HTTP started on port 80");

  // WebSocket on port 81
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  LOG("[WebServer] WebSocket started on port 81\n");
  LOG("[WebServer] Monitor: http://%s/\n",
      WiFi.softAPIP().toString().c_str());
}

// =====================================================================
// Setup
// =====================================================================
void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(1000);

  LOGLN("========================================");
  LOGLN("  ESP32-S3 USB HID Bridge — DEVICE / RX");
  LOGLN("========================================");

  USB.begin();
  Keyboard.begin();
  LOGLN("[USB HID] Keyboard device started");

  initWiFiAndEspNow();
  initWebServer();

  memset((void *)&latestReport, 0, sizeof(kbd_report_t));
  memset(&prevReport, 0, sizeof(kbd_report_t));
  lastPacketTime = millis();

  LOGLN("[Ready] Waiting for ESP-NOW packets...");
  LOGLN("========================================");
}

// =====================================================================
// Main Loop
// =====================================================================
void loop() {
  // Handle HTTP + WebSocket clients (sync — must call every loop)
  server.handleClient();
  webSocket.loop();

  // --- Process new ESP-NOW report ---
  if (newReportAvailable) {
    newReportAvailable = false;

    kbd_report_t report;
    memcpy(&report, (void *)&latestReport, sizeof(kbd_report_t));

    // 1. Forward to PC via USB HID
    forwardReportToPC(report);

    // 2. Broadcast new keystrokes to WebSocket clients
    broadcastKeystrokes(report, prevReport);

    // 3. Track key state
    bool anyKey = (report.modifier != 0);
    for (int i = 0; i < 6 && !anyKey; i++) {
      if (report.keycodes[i] != 0)
        anyKey = true;
    }
    keysPressed = anyKey;

    // 4. Save current as previous
    memcpy(&prevReport, &report, sizeof(kbd_report_t));
  }

  // --- Watchdog: release all keys if no packet for timeout ---
  if (keysPressed && (millis() - lastPacketTime > KEY_RELEASE_TIMEOUT_MS)) {
    LOGLN("[Watchdog] No packet — releasing all keys");
    releaseAllKeys();
    keysPressed = false;
    memset(&prevReport, 0, sizeof(kbd_report_t));
  }
}
