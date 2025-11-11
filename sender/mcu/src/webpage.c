#include "../lib/webpage.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC.h"
#include "../lib/main.h"
#include "../lib/trng.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ----------------- Shared state -----------------
uint8_t plaintext_blocks[MAX_BLOCKS][16];
uint8_t keys[MAX_BLOCKS][16];
uint8_t have_block[MAX_BLOCKS];
volatile uint16_t total_blocks = 0;

// ----------------- LED blink with timer delays -----------------
static void led_blink_short(void) {
  digitalWrite(LED_PIN, 1);
  delay_millis(TIM15, 150);
  digitalWrite(LED_PIN, 0);
}

// Error blink (3 fast blinks)
static void led_error_blink(void) {
  for (int j = 0; j < 3; j++) {
    digitalWrite(LED_PIN, 1);
    delay_millis(TIM15, 50);
    digitalWrite(LED_PIN, 0);
    delay_millis(TIM15, 50);
  }
}

// ----------------- HTTP helpers -----------------
static const char http_header_ok[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_no_content[] =
"HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";

// ----------------- Static HTML/JS page -----------------
const char webpage[] =
"<!DOCTYPE html><html lang=\"en\"><head>"
"<meta charset=\"utf-8\"/>"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
"<title>MechaCrypt Sender Input</title>"
"<style>"
":root{--edge:#dcdcdc;--muted:#666}"
"body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,Arial,sans-serif;max-width:960px;margin:24px auto;padding:0 16px;line-height:1.35}"
"h1{font-size:1.25rem;margin:0 0 10px}"
".overline{font-size:.9rem;font-weight:700;letter-spacing:.04em;color:var(--muted);margin:0 0 4px}"
".card{border:1px solid var(--edge);border-radius:12px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,04)}"
"label{display:block;font-weight:600;margin-bottom:6px}"
"textarea{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:10px;border:1px solid var(--edge);font:inherit;resize:vertical;min-height:90px}"
".row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:10px}"
".muted{color:var(--muted)}"
"button{padding:10px 14px;border-radius:10px;border:1px solid var(--edge);cursor:pointer;background:#111;color:#fff;font-weight:600}"
"button.ghost{background:#fff;color:#111}"
"table{width:100%;border-collapse:collapse;margin-top:16px}"
"th,td{border:1px solid var(--edge);padding:8px 10px;text-align:left}"
"th{background:#fafafa}"
"code,mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
".right{text-align:right}"
".wrap{word-break:break-word}"
".pill{display:inline-flex;gap:8px;align-items:center;padding:6px 10px;border:1px solid var(--edge);border-radius:999px}"
"</style>"
"</head><body>"
"<div class=\"overline\">MechaCrypt Sender Input</div>"
"<h1>Text → 128-bit (16-byte) ASCII Blocks</h1>"
"<div class=\"card\">"
"  <label for=\"msg\">Enter text</label>"
"  <textarea id=\"msg\" placeholder=\"\"></textarea>"
"  <div class=\"row\">"
"    <span class=\"pill\"><strong>Padding:</strong> PKCS#7</span>"
"    <span class=\"pill\"><strong>Encoding:</strong> ASCII (bytes = char codes 0–255)</span>"
"    <span class=\"muted\" id=\"stats\">0 chars</span>"
"    <button id=\"convert\">Convert</button>"
"    <button id=\"sendAll\" class=\"ghost\" title=\"Send all blocks to MCU\">Send All</button>"
"    <button id=\"copyAll\" class=\"ghost\" title=\"Copy all hex bytes (space-separated)\">Copy All Hex</button>"
"  </div>"
"</div>"
"<div id=\"out\" class=\"card\" style=\"margin-top:16px;display:none;"
"grid-template-columns:1fr;gap:10px\"></div>"
"<script>(()=>{"
"const $=s=>document.querySelector(s);"
"const msg=$('#msg');const out=$('#out');const stats=$('#stats');"
"const btnC=$('#convert'),btnS=$('#sendAll'),btnCopy=$('#copyAll');"
"const blocks=[];"
"function pkcs7(arr){let r=arr.slice();let pad=16-(r.length%16||16);for(let i=0;i<pad;i++)r.push(pad);return r}"
"function toBytes(s){return [...s].map(ch=>ch.charCodeAt(0)&255)}"
"function hex(b){return b.map(x=>x.toString(16).padStart(2,'0')).join(' ')}"
"function render(){out.style.display=blocks.length?'grid':'none';"
"out.innerHTML=blocks.map((b,i)=>`"
"<div class=card><div class=row>"
"<strong>Block #${i}</strong>"
"<button data-i=${i} class=send>Send</button>"
"</div><div><code>${hex(b)}</code></div></div>`).join('');"
"out.querySelectorAll('.send').forEach(btn=>btn.onclick=()=>sendOne(+btn.dataset.i));}"
"function sendOne(i){"
"const xhr=new XMLHttpRequest();"
"xhr.open('GET',`/send?i=${i}&hex=${hex(blocks[i]).replace(/\\s+/g,'%20')}`);"
"xhr.onload=()=>{};xhr.onerror=()=>{};xhr.send();}"
"btnC.onclick=()=>{blocks.length=0;"
"let raw=toBytes(msg.value);raw=pkcs7(raw);"
"for(let i=0;i<raw.length;i+=16)blocks.push(raw.slice(i,i+16));render();};"
"btnS.onclick=()=>{for(let i=0;i<blocks.length;i++)sendOne(i)};"
"btnCopy.onclick=()=>{navigator.clipboard.writeText(hex(blocks.flat()));};"
"msg.addEventListener('input',()=>{let n=msg.value.length;"
"stats.textContent=`${n} char${n===1?'':'s'}`});"
"})();</script>"
"</body></html>";

// ----------------- Helpers -----------------
static inline int line_has_lf(const char *buf) { return strchr(buf, '\n') != NULL; }

static int parse_hex_byte(const char *p, uint8_t *out) {
  int v = 0;
  for (int k=0;k<2;k++){
    char c = p[k];
    int d = (c>='0'&&c<='9')? c-'0' :
            (c>='a'&&c<='f')? c-'a'+10 :
            (c>='A'&&c<='F')? c-'A'+10 : -1;
    if (d<0) return 0;
    v = (v<<4) | d;
  }
  *out = (uint8_t)v;
  return 1;
}

static int decode_hex_list(const char *hex, uint8_t *buf, int maxlen) {
  int count = 0;
  const char *p = hex;
  while (*p && count < maxlen) {
    while (*p==' ') p++;
    if (!isxdigit((unsigned char)p[0]) || !isxdigit((unsigned char)p[1])) break;
    if (!parse_hex_byte(p, &buf[count])) break;
    count++;
    p += 2;
    while (*p==' ') p++;
  }
  return count;
}

// ----------------- Request handler -----------------
void processWebRequest(USART_TypeDef *USART)
{
  char request[BUFF_LEN] = {0};
  int idx = 0;

  // read request line (up to LF)
  while (!line_has_lf(request)) {
    while (!(USART->ISR & USART_ISR_RXNE)) {/*spin*/}
    if (idx < (int)sizeof(request)-1) {
      request[idx++] = readChar(USART);
      request[idx]   = '\0';
    } else {
      (void)readChar(USART); // drop extra
    }
  }

  // Handle: GET /send?i=##&hex=AA%20BB%20... HTTP/1.1
  if (strstr(request, " /send?")) {
    uint8_t blk[16] = {0};
    int got = 0;
    int block_idx = -1;

    // Parse block index
    const char *pi = strstr(request, "i=");
    if (pi) block_idx = atoi(pi+2);

    // Parse hex payload
    const char *ph = strstr(request, "hex=");
    if (ph) {
      const char *start = ph + 4;
      const char *end = strstr(start, " ");
      int span = end ? (int)(end - start) : (int)strlen(start);
      char tmp[16*3+1]; // enough for "AA " * 16
      int copy = (span < (int)sizeof(tmp)-1 ? span : (int)sizeof(tmp)-1);
      for (int i=0;i<copy;i++) {
        char c = start[i];
        tmp[i] = (c=='%' && i+2<copy && start[i+1]=='2' && start[i+2]=='0') ? ' ' : c;
      }
      tmp[copy] = '\0';
      got = decode_hex_list(tmp, blk, 16);
    }

    if (got == 16 && block_idx >= 0 && block_idx < MAX_BLOCKS) {
      // Store plaintext block
      memcpy(plaintext_blocks[block_idx], blk, 16);

      // Generate a TRNG key strictly (no dummy fallback)
      int tr = read_trng(keys[block_idx]);
      if (tr == 0) {
        have_block[block_idx] = 1;           // valid only if TRNG succeeded
        if (block_idx >= total_blocks) total_blocks = block_idx + 1;
        led_blink_short();                   // success cue
      } else {
        have_block[block_idx] = 0;           // do NOT mark valid if TRNG failed
        led_error_blink();                   // failure cue
      }
    }

    // Minimal response; keeps UI on current page
    sendString(USART, (char*)http_header_no_content);
    return;
  }

  // Otherwise: serve full page
  sendString(USART, (char*)http_header_ok);
  sendString(USART, (char*)webpage);
}
