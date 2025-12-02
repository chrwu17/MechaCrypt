/**
 * @file webpage.c
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Webpage and HTTP request handling for MechaCrypt Receiver MCU
 */

#include "../lib/main.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ======================================================
// Shared State (Receiver)
// ======================================================

volatile uint8_t received_blocks[MAX_BLOCKS][16];      // Raw ciphertext (not used in display)
volatile uint8_t plaintext_blocks[MAX_BLOCKS][16];     // Decrypted plaintext for display
volatile uint8_t have_received[MAX_BLOCKS];
volatile uint16_t total_received      = 0;
volatile uint16_t debug_request_count = 0;

// ======================================================
// Debug LED Helper
// ======================================================

static void led_blink_short(void) {
    digitalWrite(LED_PIN, 1);
    delay_millis(TIM15, 100);
    digitalWrite(LED_PIN, 0);
}

// ======================================================
// HTTP Headers
// ======================================================

static const char http_header_ok[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"Connection: close\r\n\r\n";

static const char http_header_json[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: application/json; charset=utf-8\r\n"
"Connection: close\r\n\r\n";

static const char http_header_no_content[] =
"HTTP/1.1 204 No Content\r\n"
"Connection: close\r\n\r\n";

// ======================================================
// Static HTML + JS (Receiver UI)
// ======================================================

const char webpage[] =
"<!DOCTYPE html><html lang=\"en\"><head>"
"<meta charset=\"utf-8\"/>"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
"<title>MechaCrypt Receiver Output</title>"
"<style>"
":root{--edge:#dcdcdc;--muted:#666;--success:#0a7;--warn:#f80}"
"body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,Arial,sans-serif;max-width:960px;margin:24px auto;padding:0 16px;line-height:1.35}"
"h1{font-size:1.25rem;margin:0 0 10px}"
".overline{font-size:.9rem;font-weight:700;letter-spacing:.04em;color:var(--muted);margin:0 0 4px}"
".card{border:1px solid var(--edge);border-radius:12px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,.04);margin-bottom:16px}"
"label{display:block;font-weight:600;margin-bottom:6px}"
".row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:10px}"
".muted{color:var(--muted)}"
"button{padding:10px 14px;border-radius:10px;border:1px solid var(--edge);cursor:pointer;background:#111;color:#fff;font-weight:600}"
"button.ghost{background:#fff;color:#111}"
"button:disabled{opacity:0.6;cursor:not-allowed}"
"table{width:100%;border-collapse:collapse;margin-top:16px}"
"th,td{border:1px solid var(--edge);padding:8px 10px;text-align:left}"
"th{background:#fafafa}"
"code,.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:0.9em}"
".right{text-align:right}"
".wrap{word-break:break-word}"
".pill{display:inline-flex;gap:8px;align-items:center;padding:6px 10px;border:1px solid var(--edge);border-radius:999px;font-size:0.9em}"
".success{color:var(--success)}"
".warn{color:var(--warn)}"
".decoded-text{background:#f9f9f9;padding:12px;border-radius:8px;white-space:pre-wrap;word-wrap:break-word;font-family:ui-monospace,monospace;font-size:0.95em;line-height:1.5;max-height:400px;overflow-y:auto}"
".status-box{display:flex;gap:16px;align-items:center;padding:12px;background:#f9f9f9;border-radius:8px;margin-bottom:12px}"
".status-indicator{width:12px;height:12px;border-radius:50%;background:var(--muted)}"
".status-indicator.active{background:var(--success);animation:pulse 2s infinite}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}"
"</style>"
"</head><body>"
"<div class=\"overline\">MechaCrypt Receiver Output</div>"
"<h1>Decrypted Message from FPGA</h1>"

"<div class=\"card\">"
"  <div class=\"status-box\">"
"    <div class=\"status-indicator\" id=\"indicator\"></div>"
"    <div>"
"      <div><strong>Status:</strong> <span id=\"status\">Waiting for data...</span></div>"
"      <div class=\"muted\" style=\"font-size:0.9em;margin-top:4px\"><span id=\"blockCount\">0</span> blocks received</div>"
"    </div>"
"  </div>"
"  <div class=\"row\">"
"    <button id=\"refresh\" class=\"ghost\">Refresh</button>"
"    <button id=\"clear\" class=\"ghost\">Clear All</button>"
"    <button id=\"copyText\" class=\"ghost\">Copy Decoded Text</button>"
"  </div>"
"</div>"

"<div class=\"card\" id=\"decodedCard\" style=\"display:none;\">"
"  <h2 style=\"font-size:1.1rem;margin:0 0 12px;\">Decoded Text (ASCII)</h2>"
"  <div class=\"decoded-text\" id=\"decodedText\"></div>"
"</div>"

"<div class=\"card\" id=\"blocksCard\" style=\"display:none;\">"
"  <h2 style=\"font-size:1.1rem;margin:0 0 12px;\">Received Blocks</h2>"
"  <table id=\"tbl\"><thead>"
"    <tr><th class=\"right\">Block #</th><th>Hex Bytes</th><th>ASCII (printable)</th></tr>"
"  </thead><tbody></tbody></table>"
"</div>"

"<script>(function(){"
"const $=s=>document.querySelector(s);"
"const status=$(\"#status\"),blockCount=$(\"#blockCount\"),indicator=$(\"#indicator\");"
"const decodedCard=$(\"#decodedCard\"),decodedText=$(\"#decodedText\");"
"const blocksCard=$(\"#blocksCard\"),tbody=$(\"#tbl tbody\");"
"const btnRefresh=$(\"#refresh\"),btnClear=$(\"#clear\"),btnCopyText=$(\"#copyText\");"

"function toHexLine(bytes){return bytes.map(x=>x.toString(16).padStart(2,'0').toUpperCase()).join(' ')}"
"function toAsciiPrintable(bytes){return bytes.map(x=>x>=32&&x<=126?String.fromCharCode(x):'.').join('')}"
"function toAsciiText(bytes){return bytes.map(x=>String.fromCharCode(x)).join('')}"

"function removePKCS7Padding(bytes){"
"  if(bytes.length===0)return bytes;"
"  const padVal=bytes[bytes.length-1];"
"  if(padVal>16||padVal<1)return bytes;"
"  for(let i=bytes.length-padVal;i<bytes.length;i++){"
"    if(bytes[i]!==padVal)return bytes;"
"  }"
"  return bytes.slice(0,bytes.length-padVal);"
"}"

"function copy(t){navigator.clipboard.writeText(t).catch(()=>{})}"

"async function fetchData(){"
"  try{"
"    const resp=await fetch('/data');"
"    const raw=await resp.text();"
"    const data=JSON.parse(raw);"
"    updateUI(data);"
"  }catch(e){"
"    console.error('Fetch/JSON error:',e);"
"    status.textContent='Error fetching /data';"
"  }"
"}"

"function updateUI(data){"
"  const count=data.total_received||0;"
"  blockCount.textContent=count;"
"  "
"  if(count===0){"
"    status.textContent='Waiting for data...';"
"    indicator.classList.remove('active');"
"    decodedCard.style.display='none';"
"    blocksCard.style.display='none';"
"    return;"
"  }"
"  "
"  status.textContent='Data received';"
"  indicator.classList.add('active');"
"  decodedCard.style.display='block';"
"  blocksCard.style.display='block';"
"  "
"  const blocks=data.blocks||[];"
"  let allBytes=[];"
"  tbody.innerHTML='';"
"  "
"  blocks.forEach((blk,i)=>{"
"    if(!blk||blk.length!==16)return;"
"    allBytes=allBytes.concat(blk);"
"    const tr=document.createElement('tr');"
"    const tdIdx=document.createElement('td');"
"    tdIdx.className='right mono';"
"    tdIdx.textContent=i;"
"    const tdHex=document.createElement('td');"
"    tdHex.className='mono wrap';"
"    tdHex.innerHTML=`<code>${toHexLine(blk)}</code>`;"
"    const tdAscii=document.createElement('td');"
"    tdAscii.className='mono wrap';"
"    tdAscii.textContent=toAsciiPrintable(blk);"
"    tr.appendChild(tdIdx);"
"    tr.appendChild(tdHex);"
"    tr.appendChild(tdAscii);"
"    tbody.appendChild(tr);"
"  });"
"  "
"  const unpaddedBytes=removePKCS7Padding(allBytes);"
"  const decodedStr=toAsciiText(unpaddedBytes);"
"  decodedText.textContent=decodedStr;"
"  "
"  btnCopyText.onclick=()=>copy(decodedStr);"
"}"

"btnRefresh.onclick=fetchData;"
"btnClear.onclick=async()=>{"
"  await fetch('/clear');"
"  fetchData();"
"};"

"fetchData();"
"setInterval(fetchData,2000);"
"})();</script>"
"</body></html>";

// ======================================================
// Helper Functions
// ======================================================

static inline int line_has_lf(const char *buf) {
    return strchr(buf, '\n') != NULL;
}

// Stream JSON out over USART to avoid large buffers
static void send_json_status(USART_TypeDef *USART) {
    char numbuf[16];

    // JSON body
    sendString(USART, "{ \"total_received\": ");

    snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)total_received);
    sendString(USART, numbuf);
    sendString(USART, ", \"blocks\":[");

    for (uint16_t i = 0; i < total_received; i++) {
        sendChar(USART, '[');
        for (int j = 0; j < 16; j++) {
            snprintf(numbuf, sizeof(numbuf), "%u",
                     (unsigned)plaintext_blocks[i][j]);
            sendString(USART, numbuf);
            if (j < 15) {
                sendChar(USART, ',');
            }
        }
        sendChar(USART, ']');
        if (i + 1 < total_received) {
            sendChar(USART, ',');
        }
    }

    sendString(USART, "] }\r\n");
}

// ======================================================
// Public API: Store a received plaintext block
// ======================================================

void receiver_store_block(uint16_t idx, const uint8_t plaintext[16]) {
    if (idx >= MAX_BLOCKS) return;

    // Store plaintext for webpage display
    memcpy((void*)plaintext_blocks[idx], plaintext, 16);
    have_received[idx] = 1;

    if (idx + 1 > total_received) {
        total_received = idx + 1;
    }

    // Optional visual feedback
    led_blink_short();
}

// ======================================================
// Request Handler
// ======================================================

void processWebRequest(USART_TypeDef *USART)
{
    if (USART == NULL) {
        return;
    }

    // Only act if something is waiting
    if (!(USART->ISR & USART_ISR_RXNE)) {
        return;
    }

    char request[BUFF_LEN] = {0};
    int idx = 0;

    // Read request line up to LF
    while (!line_has_lf(request)) {
        while (!(USART->ISR & USART_ISR_RXNE)) { /* spin */ }
        if (idx < (int)sizeof(request) - 1) {
            request[idx++] = readChar(USART);
            request[idx]   = '\0';
        } else {
            (void)readChar(USART);  // discard extras
        }
    }

    debug_request_count++;

    // Handle: GET /data
    if (strstr(request, "/data") || strstr(request, "data")) {
        sendString(USART, (char*)http_header_json);
        send_json_status(USART);
        return;
    }

    // Handle: GET /clear
    if (strstr(request, "/clear") || strstr(request, "clear")) {
        for (int i = 0; i < MAX_BLOCKS; i++) {
            memset((void*)received_blocks[i],  0, 16);
            memset((void*)plaintext_blocks[i], 0, 16);
            have_received[i] = 0;
        }
        total_received = 0;
        sendString(USART, (char*)http_header_no_content);
        return;
    }

    // Default: serve the full receiver UI page
    sendString(USART, (char*)http_header_ok);
    sendString(USART, (char*)webpage);
}