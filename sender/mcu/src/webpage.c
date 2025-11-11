#include "../lib/webpage.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC.h"
#include "../lib/main.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ---------- Optional hook you can implement elsewhere ----------
// Provide your own non-weak implementation later to push to FPGA via SPI.
__attribute__((weak)) void spi_send_block(const uint8_t blk[16]) {
  (void)blk; // no-op for now
}

// Simple quick LED blink as a visible ACK (adjust as you like)
static void led_blink_short(void) {
  digitalWrite(LED_PIN, 1);
  for (volatile int i = 0; i < 60000; ++i) __NOP();
  digitalWrite(LED_PIN, 0);
}

// ---------- HTTP helpers ----------
static const char http_header_ok[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_no_content[] =
"HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";

// ---------- Static HTML (client-side PKCS#7 tool) ----------
const char webpage[] =
"<!DOCTYPE html><html lang=\"en\"><head>"
"<meta charset=\"utf-8\"/>"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
"<title>MechaCrypt user Sender Input</title>"
"<style>"
"  :root{--edge:#dcdcdc;--muted:#666}"
"  body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,Arial,sans-serif;max-width:960px;margin:24px auto;padding:0 16px;line-height:1.35}"
"  h1{font-size:1.25rem;margin:0 0 10px}"
"  .overline{font-size:.9rem;font-weight:700;letter-spacing:.04em;color:var(--muted);margin:0 0 4px}"
"  .card{border:1px solid var(--edge);border-radius:12px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,.04)}"
"  label{display:block;font-weight:600;margin-bottom:6px}"
"  textarea{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:10px;border:1px solid var(--edge);font:inherit;resize:vertical;min-height:90px}"
"  .row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:10px}"
"  .muted{color:var(--muted)}"
"  button{padding:10px 14px;border-radius:10px;border:1px solid var(--edge);cursor:pointer;background:#111;color:#fff;font-weight:600}"
"  button.ghost{background:#fff;color:#111}"
"  table{width:100%;border-collapse:collapse;margin-top:16px}"
"  th,td{border:1px solid var(--edge);padding:8px 10px;text-align:left}"
"  th{background:#fafafa}"
"  code,.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
"  .right{text-align:right}"
"  .wrap{word-break:break-word}"
"  .pill{display:inline-flex;gap:8px;align-items:center;padding:6px 10px;border:1px solid var(--edge);border-radius:999px}"
"</style>"
"</head><body>"
"<div class=\"overline\">MechaCrypt user Sender Input</div>"
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
"<div id=\"out\" class=\"card\" style=\"margin-top:16px;display:none;\">"
"  <div id=\"summary\" class=\"muted\"></div>"
"  <table id=\"tbl\"><thead>"
"    <tr><th class=\"right\">Block #</th><th>ASCII (printable)</th><th>Hex bytes</th><th class=\"right\">Actions</th></tr>"
"  </thead><tbody></tbody></table>"
"</div>"
"<script>(function(){"
"const $=s=>document.querySelector(s),msgEl=$(\"#msg\"),statsEl=$(\"#stats\"),out=$(\"#out\"),tbody=$(\"#tbl tbody\"),summary=$(\"#summary\"),btnConvert=$(\"#convert\"),btnCopyAll=$(\"#copyAll\"),btnSendAll=$(\"#sendAll\");"
"function asciiToBytes(str){const a=new Uint8Array(str.length);for(let i=0;i<str.length;i++)a[i]=str.charCodeAt(i)&255;return a}"
"function padPkcs7(bytes){const rem=bytes.length%16,padLen=rem===0?16:(16-rem);const out=new Uint8Array(bytes.length+padLen);out.set(bytes,0);out.fill(padLen,bytes.length);return out}"
"function toBlocks(b){const z=[];for(let i=0;i<b.length;i+=16)z.push(b.slice(i,i+16));return z}"
"function toHexLine(b){return Array.from(b).map(x=>x.toString(16).padStart(2,'0').toUpperCase()).join(' ')}"
"function toAsciiPrintable(b){return Array.from(b).map(x=>x>=32&&x<=126?String.fromCharCode(x):'.').join('')}"
"function copy(t){navigator.clipboard.writeText(t).catch(()=>{})}"
"function sendBlock(i,hex){fetch(`/send?i=${i}&hex=${encodeURIComponent(hex)}`,{method:'GET'})}"
"function convert(){const text=msgEl.value||\"\";let bytes=asciiToBytes(text);bytes=padPkcs7(bytes);const blocks=toBlocks(bytes);summary.innerHTML=`<strong>Input length:</strong> ${text.length} chars &nbsp;&nbsp; <strong>Blocks:</strong> ${blocks.length} × 16 bytes`;tbody.innerHTML=\"\";let allHex=[];blocks.forEach((blk,i)=>{const ascii=toAsciiPrintable(blk);const hex=toHexLine(blk);allHex.push(hex);const tr=document.createElement('tr');const tdIdx=document.createElement('td');tdIdx.className='right mono';tdIdx.textContent=i;const tdAscii=document.createElement('td');tdAscii.className='mono wrap';tdAscii.textContent=ascii;const tdHex=document.createElement('td');tdHex.className='mono wrap';tdHex.innerHTML=`<code>${hex}</code>`;const tdAct=document.createElement('td');tdAct.className='right';const btn=document.createElement('button');btn.className='ghost';btn.textContent='Send';btn.title=`Send block ${i}`;btn.addEventListener('click',()=>sendBlock(i,hex));tdAct.appendChild(btn);tr.appendChild(tdIdx);tr.appendChild(tdAscii);tr.appendChild(tdHex);tr.appendChild(tdAct);tbody.appendChild(tr)});btnCopyAll.onclick=()=>copy(allHex.join(' '));btnSendAll.onclick=()=>{allHex.forEach((h,i)=>sendBlock(i,h));};out.style.display=blocks.length?\"block\":\"none\"}"
"msgEl.addEventListener('input',()=>{const n=msgEl.value.length;statsEl.textContent=`${n} char${n===1?'':'s'}`});"
"btnConvert.addEventListener('click',convert);"
"msgEl.value=\"\";statsEl.textContent=\"0 chars\";"
"})();</script>"
"</body></html>";

// ---------- tiny parser / utils ----------
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

// ---------- Request handler ----------
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

    const char *ph = strstr(request, "hex=");
    if (ph) {
      // Decode %20 -> space so we can parse "AA BB ..."
      char hexline[3*16+32] = {0};
      const char *src = ph+4; char *dst = hexline;
      while (*src && *src!=' ' && (dst-hexline) < (int)sizeof(hexline)-1) {
        if (src[0]=='%' && src[1] && src[2]) {
          char a = src[1], b = src[2];
          int hv = (isdigit((unsigned char)a)?a-'0':(toupper((unsigned char)a)-'A'+10));
          hv <<= 4;
          hv |= (isdigit((unsigned char)b)?b-'0':(toupper((unsigned char)b)-'A'+10));
          *dst++ = (char)hv;
          src += 3;
        } else {
          *dst++ = *src++;
        }
      }
      *dst = 0;
      got = decode_hex_list(hexline, blk, 16);
    }

    if (got == 16) {
      // ACK visibly
      led_blink_short();
      // Hand off to (optional) SPI path
      spi_send_block(blk);
    }

    // Minimal response; keeps UI on current page
    sendString(USART, http_header_no_content);
    return;
  }

  // Otherwise: serve full page
  sendString(USART, http_header_ok);
  sendString(USART, webpage);
}
