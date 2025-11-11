#include "../lib/webpage.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC.h"
#include "../lib/main.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ----------------- Shared state (already present in your project) -----------------
extern volatile uint16_t total_blocks;   // highest index+1 seen
extern volatile uint16_t next_to_send;   // used by your TX state machine
extern volatile uint8_t  start_send;     // used by your TX state machine

extern uint8_t plaintext_blocks[MAX_BLOCKS][16];
extern uint8_t have_block[MAX_BLOCKS];

// ----------------- Small ACK blink (optional) -----------------
static void led_blink_short(void) {
  digitalWrite(LED_PIN, 1);
  for (volatile int i = 0; i < 60000; ++i) __NOP();
  digitalWrite(LED_PIN, 0);
}

// ----------------- HTTP helpers -----------------
static const char http_header_ok[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_json[] =
"HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_text[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_no_content[] =
"HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";

// ----------------- Receiver HTML/JS page -----------------
// ----------------- Receiver HTML/JS page -----------------
static const char receiver_page[] =
"<!DOCTYPE html><html lang=\"en\"><head>"
"<meta charset=\"utf-8\"/>"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
"<title>MechaCrypt Receiver – Blocks & Reassembled Message</title>"
"<style>"
":root{--edge:#dcdcdc;--muted:#666}"
"body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,Arial,sans-serif;max-width:960px;margin:24px auto;padding:0 16px;line-height:1.35}"
"h1{font-size:1.25rem;margin:0 0 10px}"
".overline{font-size:.9rem;font-weight:700;letter-spacing:.04em;color:var(--muted);margin:0 0 4px}"
".card{border:1px solid var(--edge);border-radius:12px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,.04)}"
".row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:10px}"
".muted{color:var(--muted)}"
"button{padding:10px 14px;border-radius:10px;border:1px solid var(--edge);cursor:pointer;background:#111;color:#fff;font-weight:600}"
"button.ghost{background:#fff;color:#111}"
"table{width:100%;border-collapse:collapse;margin-top:16px}"
"th,td{border:1px solid var(--edge);padding:8px 10px;text-align:left}"
"th{background:#fafafa}"
"code,.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
".right{text-align:right}"
".wrap{word-break:break-word}"
".pill{display:inline-flex;gap:8px;align-items:center;padding:6px 10px;border:1px solid var(--edge);border-radius:999px}"
"textarea{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:10px;border:1px solid var(--edge);font:inherit;resize:vertical;min-height:90px}"
".ok{color:#177245;font-weight:700} .miss{color:#a00;font-weight:700}"
"</style>"
"</head><body>"
"<div class=\"overline\">MechaCrypt Receiver</div>"
"<h1>Incoming 16-byte Blocks & Reassembly</h1>"
"<div class=\"card\">"
"  <div class=\"row\">"
"    <span class=\"muted\">This page polls the MCU for blocks every second.</span>"
"    <button id=\"refresh\" class=\"ghost\">Refresh now</button>"
"    <button id=\"copyHex\" class=\"ghost\">Copy all hex</button>"
"    <button id=\"copyMsg\" class=\"ghost\">Copy message</button>"
"  </div>"
"  <div id=\"stats\" class=\"pill\" style=\"margin-top:10px\">—</div>"
"</div>"
"<div class=\"card\" style=\"margin-top:16px\">"
"  <table><thead><tr><th class=\"right\">#</th><th>ASCII (printable)</th><th>Hex</th><th>Status</th></tr></thead><tbody id=\"tbody\"></tbody></table>"
"</div>"
"<div class=\"card\" style=\"margin-top:16px\">"
"  <label for=\"msg\"><strong>Reassembled message (PKCS#7 unpadded)</strong></label>"
"  <textarea id=\"msg\" readonly></textarea>"
"</div>"
"<script>(function(){"
"const $=s=>document.querySelector(s),tbody=$('#tbody'),stats=$('#stats'),ta=$('#msg');"
"const btnR=$('#refresh'),btnHX=$('#copyHex'),btnMSG=$('#copyMsg');"
"let timer=null;"
"function toAsciiPrintable(arr){return arr.map(x=> (x>=32&&x<=126)?String.fromCharCode(x):'.').join('')}"
"function render(data){"
"  tbody.innerHTML='';"
"  const n=data.total||0;"
"  let hexAll=[]; let have=0;"
"  for(let i=0;i<n;i++){"
"    const item=data.blocks[String(i)]||null;"
"    const tr=document.createElement('tr');"
"    if(item){have++; const ascii=toAsciiPrintable(item.bytes);"
"      tr.innerHTML=`<td class='right mono'>${i}</td><td class='mono wrap'>${ascii}</td><td class='mono wrap'><code>${item.hex}</code></td><td class='ok'>OK</td>`;"
"      hexAll.push(item.hex);"
"    } else { tr.innerHTML=`<td class='right mono'>${i}</td><td colspan='2' class='muted'>—</td><td class='miss'>missing</td>`;}"
"    tbody.appendChild(tr);"
"  }"
"  stats.textContent=`Blocks: ${have}/${n}`;"
"  btnHX.onclick=()=>navigator.clipboard.writeText(hexAll.join(' ')).catch(()=>{});"
"}"
"async function refresh(){"
"  try{const r=await fetch('/status'); if(!r.ok) return; const data=await r.json(); render(data);"
"      const r2=await fetch('/combine'); if(r2.ok){ const s=await r2.text(); ta.value=s; }}catch(e){}"
"}"
"btnR.onclick=refresh;"
"timer=setInterval(refresh,1000); refresh();"
"})();</script>"
"</body></html>";


// ----------------- tiny helpers -----------------
static inline int line_has_lf(const char *buf) { return strchr(buf, '\n') != NULL; }

static void send_hex_byte(USART_TypeDef *USART, uint8_t b) {
  const char *hex = "0123456789ABCDEF";
  char out[2] = { hex[b>>4], hex[b&0xF] };
  sendChar(USART, out[0]);
  sendChar(USART, out[1]);
}

// PKCS#7 unpad: in-place on buf, returns new length (0 on error)
static int pkcs7_unpad(uint8_t *buf, int len){
  if(len<=0 || (len%16)!=0) return 0;
  int pad = buf[len-1];
  if(pad<=0 || pad>16) return 0;
  for(int i=0;i<pad;i++){ if(buf[len-1-i] != pad) return 0; }
  return len - pad;
}

// ----------------- Request handler additions -----------------
void processWebRequest(USART_TypeDef *USART)
{
  char request[BUFF_LEN] = {0};
  int idx = 0;

  // read request line (up to LF)
  while (!line_has_lf(request)) {
    while (!(USART->ISR & USART_ISR_RXNE)) { /*spin*/ }
    if (idx < (int)sizeof(request)-1) {
      request[idx++] = readChar(USART);
      request[idx]   = '\0';
    } else {
      (void)readChar(USART); // drop extra
    }
  }

  // --- 1) Serve the receiver UI at GET /rx ---
  if (strstr(request, " GET /rx ")) {
    sendString(USART, http_header_ok);
    sendString(USART, receiver_page);
    return;
  }

  // --- 2) JSON status of received blocks at GET /status ---
  if (strstr(request, " GET /status ")) {
    sendString(USART, http_header_json);
    sendString(USART, "{\"total\":");
    char tmp[16];
    itoa(total_blocks, tmp, 10); sendString(USART, tmp);
    sendString(USART, ",\"blocks\":{");

    int first = 1;
    for (uint16_t i=0; i<total_blocks; i++) {
      if (have_block[i]) {
        if (!first) sendChar(USART, ',');
        first = 0;
        // key
        sendChar(USART, '\"'); itoa(i, tmp, 10); sendString(USART, tmp); sendChar(USART, '\"');
        sendChar(USART, ':');
        // value object {hex:"..", bytes:[..]}
        sendString(USART, "{\"hex\":\"");
        for (int k=0;k<16;k++) {
          if (k) sendChar(USART, ' ');
          send_hex_byte(USART, plaintext_blocks[i][k]);
        }
        sendString(USART, "\",\"bytes\":[");
        for (int k=0;k<16;k++) {
          if (k) sendChar(USART, ',');
          itoa(plaintext_blocks[i][k], tmp, 10); sendString(USART, tmp);
        }
        sendString(USART, "]}");
      }
    }

    sendString(USART, "}}");
    return;
  }

  // --- 3) Combined message (PKCS#7 unpadded) at GET /combine ---
  if (strstr(request, " GET /combine ")) {
    // compute total bytes = 16 * total_blocks into a temp buffer
    uint32_t cap = (uint32_t)total_blocks * 16u;
    if (cap == 0u) { sendString(USART, http_header_text); sendString(USART, ""); return; }

    // ensure contiguous from block 0..total_blocks-1 are present
    for (uint16_t i=0;i<total_blocks;i++) {
      if (!have_block[i]) { sendString(USART, http_header_text); sendString(USART, ""); return; }
    }

    static uint8_t tmpbuf[MAX_BLOCKS*16];
    uint32_t pos = 0;
    for (uint16_t i=0;i<total_blocks;i++) {
      memcpy(&tmpbuf[pos], plaintext_blocks[i], 16);
      pos += 16;
    }

    int unpadded = pkcs7_unpad(tmpbuf, (int)pos);
    if (unpadded <= 0) unpadded = (int)pos; // if no valid padding, fall back to raw

    sendString(USART, http_header_text);
    // Stream as UTF-8 best-effort (non-printables will come through as raw bytes)
    for (int i=0;i<unpadded;i++) {
      sendChar(USART, (char)tmpbuf[i]);
    }
    return;
  }

  // --- 4) Existing sender path: accept /send?i=..&hex=.. and existing sender page ---
  if (strstr(request, " /send?")) {
    // parse index
    int index = -1; const char *pi = strstr(request, "i="); if (pi) index = atoi(pi+2);
    uint8_t blk[16] = {0}; int got = 0;

    const char *ph = strstr(request, "hex=");
    if (ph) {
      char hexline[3*16+32] = {0};
      // simple URL-decode for this param
      const char *src = ph+4; int n=0; while (*src && n < (int)sizeof(hexline)-1) {
        if (src[0]=='%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
          int a = isdigit((unsigned char)src[1]) ? src[1]-'0' : (toupper((unsigned char)src[1])-'A'+10);
          int b = isdigit((unsigned char)src[2]) ? src[2]-'0' : (toupper((unsigned char)src[2])-'A'+10);
          hexline[n++] = (char)((a<<4)|b); src += 3;
        } else { hexline[n++] = *src++; }
      }
      hexline[n] = 0;

      // decode "AA BB .."
      const char *p = hexline; int count=0; auto parse_hex_byte_local = [](const char *q, uint8_t *out){
        int v=0; for(int k=0;k<2;k++){ char c=q[k]; int d=(c>='0'&&c<='9')?c-'0':(c>='a'&&c<='f')?c-'a'+10:(c>='A'&&c<='F')?c-'A'+10:-1; if(d<0) return 0; v=(v<<4)|d;} *out=(uint8_t)v; return 1; };
      while (*p && count<16) { while (*p==' ') p++; if (!isxdigit((unsigned char)p[0])||!isxdigit((unsigned char)p[1])) break; if(!parse_hex_byte_local(p,&blk[count])) break; count++; p+=2; while(*p==' ') p++; }
      got = count;
    }

    if (index >= 0 && index < (int)MAX_BLOCKS && got == 16) {
      memcpy(plaintext_blocks[index], blk, 16);
      have_block[index] = 1;
      if ((uint16_t)(index + 1) > total_blocks) total_blocks = (uint16_t)(index + 1);
      if (!start_send) { start_send = 1; next_to_send = 0; }
      led_blink_short();
    }

    sendString(USART, http_header_no_content);
    return;
  }

  // Default: serve original sender page if you still keep it as `webpage`
  extern const char webpage[]; // from your existing sender UI
  sendString(USART, http_header_ok);
  sendString(USART, webpage);
}
