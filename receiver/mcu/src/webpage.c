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

volatile uint8_t received_blocks[MAX_BLOCKS][16];
volatile uint8_t plaintext_blocks[MAX_BLOCKS][16]; 
volatile uint8_t have_received[MAX_BLOCKS];
volatile uint16_t total_received      = 0;
volatile uint16_t debug_request_count = 0;


// ======================================================
// Test helper: inject one known ciphertext + key
// ======================================================
/**
 * Replace inject_test_ciphertext() in webpage.c with this:
 */

/**
 * @brief Diagnostic test - read more bytes to see the full pattern
 */

void inject_test_ciphertext(void) {
    const uint8_t key[16] = {
        0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
        0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C
    };
    
    const uint8_t plaintext[16] = {
        0x32,0x43,0xF6,0xA8,0x88,0x5A,0x30,0x8D,
        0x31,0x31,0x98,0xA2,0xE0,0x37,0x07,0x34
    };
    
    uint8_t buffer[48];  // Read 3 blocks worth to see pattern
    
    // Send data
    digitalWrite(PA5, 1);
    delay_millis(TIM15, 5);
    
    digitalWrite(SPI_CE, 0);
    delay_millis(TIM15, 2);
    
    for (int i = 15; i >= 0; i--) spiSendReceive(key[i]);
    for (int i = 15; i >= 0; i--) spiSendReceive(plaintext[i]);
    
    delay_millis(TIM15, 2);
    digitalWrite(SPI_CE, 1);
    digitalWrite(PA5, 0);
    
    // Wait for DONE
    uint32_t timeout = 10000000;
    while (!digitalRead(PA6) && timeout > 0) timeout--;
    
    if (timeout > 0) {
        delay_millis(TIM15, 20);
        
        // Read 48 bytes to see full pattern
        digitalWrite(SPI_CE, 0);
        delay_millis(TIM15, 2);
        
        for (int i = 0; i < 48; i++) {
            buffer[i] = (uint8_t)spiSendReceive(0x00);
        }
        
        delay_millis(TIM15, 2);
        digitalWrite(SPI_CE, 1);
        
        // Store all 3 blocks
        receiver_store_block(0, &buffer[0]);
        receiver_store_block(1, &buffer[16]);
        receiver_store_block(2, &buffer[32]);
    } else {
        // Timeout
        for (int i = 0; i < 16; i++) buffer[i] = 0xEE;
        receiver_store_block(0, buffer);
    }
}


// Debug LED
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
"    console.log('RAW /data response:',raw);"
"    const data=JSON.parse(raw);"
"    updateUI(data);"
"  }catch(e){"
"    console.error('Fetch/JSON error:',e);"
"    status.textContent='Error fetching /data (see console)';"
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
// Small Helpers
// ======================================================

static inline int line_has_lf(const char *buf) {
    return strchr(buf, '\n') != NULL;
}

// Stream JSON out over USART so we don't need a giant buffer.
static void send_json_status(USART_TypeDef *USART) {
    char numbuf[16];

    // JSON body only:
    sendString(USART, "{ \"total_received\": ");

    snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)total_received);
    sendString(USART, numbuf);
    sendString(USART, ", \"blocks\":[");

    for (uint16_t i = 0; i < total_received; i++) {
        sendChar(USART, '[');
        for (int j = 0; j < 16; j++) {
            snprintf(numbuf, sizeof(numbuf), "%u",
                     (unsigned)plaintext_blocks[i][j]); // or received_blocks[i][j]
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
// Demo helper: seed plaintext_blocks with sample ASCII
// ======================================================

void receiver_demo_init_plaintext(void)
{
    const char *msg =
        "MECHACRYPT DEMO\n"
        "Hello from the MCU receiver.\n"
        "These bytes are shown as HEX and ASCII.\n";

    size_t len = strlen(msg);

    // Simple PKCS#7-like padding so the JS unpadding logic is happy
    uint8_t pad = (uint8_t)(16 - (len % 16));
    if (pad == 0) pad = 16;

    size_t total_bytes = len + pad;
    size_t blocks = total_bytes / 16;
    if (blocks > MAX_BLOCKS) {
        blocks = MAX_BLOCKS;
        total_bytes = blocks * 16;
        if (len > total_bytes) {
            len = total_bytes;  // hard cut if somehow longer
        }
    }

    // Clear existing state
    memset((void*)received_blocks,  0, sizeof(received_blocks));
    memset((void*)plaintext_blocks, 0, sizeof(plaintext_blocks));
    memset((void*)have_received,    0, sizeof(have_received));

    // Fill blocks with the ASCII message + padding
    for (size_t i = 0; i < blocks; i++) {
        for (size_t j = 0; j < 16; j++) {
            size_t idx = i * 16 + j;
            uint8_t b;
            if (idx < len) {
                b = (uint8_t)msg[idx];
            } else {
                b = pad;
            }
            received_blocks[i][j]  = b;  // raw data
            plaintext_blocks[i][j] = b;  // what the UI will show
        }
        have_received[i] = 1;
    }

    total_received = (uint16_t)blocks;
}



// ======================================================
// Public helper: store a received block
// ======================================================

void receiver_store_block(uint16_t idx, const uint8_t blk[16]) {
    if (idx >= MAX_BLOCKS) return;

    // Raw data from FPGA 
    memcpy((void*)received_blocks[idx], blk, 16);

    // For now, treat it as “plaintext” too (later you can copy decrypted data here)
    memcpy((void*)plaintext_blocks[idx], blk, 16);

    have_received[idx] = 1;

    if (idx + 1 > total_received) {
        total_received = idx + 1;
    }

    // Optional visual feedback
    led_blink_short();
}


// ======================================================
// Request handler
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
        send_json_status(USART);
        return;
    }

    // Handle: GET /clear
    if (strstr(request, "/clear") || strstr(request, "clear")) {
        for (int i = 0; i < MAX_BLOCKS; i++) {
            memset((void*)received_blocks[i],  0, 16);
            memset((void*)plaintext_blocks[i], 0, 16);  // NEW
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

// ----------------- SPI demo: fetch one block from FPGA -----------------

static void spi_demo_fetch_block(void)
{
    if (total_received >= MAX_BLOCKS) {
        return;
    }

    uint8_t buf[16];

    // Select FPGA (CS low)
    digitalWrite(SPI_CE, 0);

    // 16 bytes = 128 bits
    for (int i = 0; i < 16; i++) {
        buf[i] = (uint8_t) spiSendReceive(0x00);  // send dummy, read data
    }

    // De-select FPGA (CS high)
    digitalWrite(SPI_CE, 1);

    // Store into shared state so /data + UI see it
    receiver_store_block(total_received, buf);
}

// Public function called from main loop
void receiver_spi_demo_poll(void)
{
    // For a simple demo: just grab 4 blocks, then stop
    if (total_received < 4) {
        spi_demo_fetch_block();
    }
}
