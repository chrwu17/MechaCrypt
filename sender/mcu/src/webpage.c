#include "../lib/main.h"
#include "../lib/bridge.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ----------------- Shared state -----------------
volatile uint8_t plaintext_blocks[MAX_BLOCKS][16];
volatile uint8_t keys[MAX_BLOCKS][16];
volatile uint8_t have_block[MAX_BLOCKS];
volatile uint16_t total_blocks = 0;
volatile uint16_t debug_request_count = 0;
volatile int debug_last_block_idx = -1;
volatile int debug_last_got_bytes = 0;
volatile int debug_last_trng_result = -99;

// Bridge state tracking
volatile uint8_t message_length = 0;
volatile int bridge_keys_sent = 0;

// ------------ Transmit state machine (SPI + LOAD/DONE) ------------
typedef enum {
  TX_IDLE = 0,
  TX_ENCRYPTING,      // Waiting for AES encryption to complete
  TX_SENDING          // Waiting for message transmission to complete
} tx_state_t;

static volatile tx_state_t tx_state = TX_IDLE;
static volatile int current_idx = -1;
static volatile int next_idx = 0;
static volatile uint8_t prev_done_state = 0;  // Track previous DONE pin state

// LED debug
static void led_blink_short(void) {
  digitalWrite(LED_PIN, 1);
  delay_millis(TIM15, 150);
  digitalWrite(LED_PIN, 0);
}

static void led_error_blink(void) {
  for (int j = 0; j < 3; j++) {
    digitalWrite(LED_PIN, 1);
    delay_millis(TIM15, 50);
    digitalWrite(LED_PIN, 0);
    delay_millis(TIM15, 50);
  }
}

static void led_blink_bridge(void) {
  digitalWrite(LED_PIN, 1);
  delay_millis(TIM15, 300);
  digitalWrite(LED_PIN, 0);
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
".card{border:1px solid var(--edge);border-radius:12px;padding:16px;box-shadow:0 1px 6px rgba(0,0,0,.04)}"
"label{display:block;font-weight:600;margin-bottom:6px}"
"textarea{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:10px;border:1px solid var(--edge);font:inherit;resize:vertical;min-height:90px}"
".row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:10px}"
".muted{color:var(--muted)}"
"button{padding:10px 14px;border-radius:10px;border:1px solid var(--edge);cursor:pointer;background:#111;color:#fff;font-weight:600}"
"button.ghost{background:#fff;color:#111}"
"button:disabled{opacity:0.6;cursor:not-allowed}"
"table{width:100%;border-collapse:collapse;margin-top:16px}"
"th,td{border:1px solid var(--edge);padding:8px 10px;text-align:left}"
"th{background:#fafafa}"
"code,.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
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
"    <button id=\"sendAll\" class=\"ghost\" title=\"Load blocks to MCU and start transmission\">Send</button>"
"    <button id=\"copyAll\" class=\"ghost\" title=\"Copy all hex bytes (space-separated)\">Copy All Hex</button>"
"  </div>"
"</div>"
"<div id=\"out\" class=\"card\" style=\"margin-top:16px;display:none;\">"
"  <div id=\"summary\" class=\"muted\"></div>"
"  <table id=\"tbl\"><thead>"
"    <tr><th class=\"right\">Block #</th><th>ASCII (printable)</th><th>Hex bytes</th></tr>"
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
"function sendBlock(i,hex){return fetch(`/send?i=${i}&hex=${encodeURIComponent(hex)}`,{method:'GET'})}"
"function startTransmission(len){return fetch(`/start?len=${len}`,{method:'GET'});}"
"function convert(){const text=msgEl.value||\"\";let bytes=asciiToBytes(text);bytes=padPkcs7(bytes);const blocks=toBlocks(bytes);summary.innerHTML=`<strong>Input length:</strong> ${text.length} chars &nbsp;&nbsp; <strong>Blocks:</strong> ${blocks.length} × 16 bytes`;tbody.innerHTML=\"\";let allHex=[];blocks.forEach((blk,i)=>{const ascii=toAsciiPrintable(blk);const hex=toHexLine(blk);allHex.push(hex);const tr=document.createElement('tr');const tdIdx=document.createElement('td');tdIdx.className='right mono';tdIdx.textContent=i;const tdAscii=document.createElement('td');tdAscii.className='mono wrap';tdAscii.textContent=ascii;const tdHex=document.createElement('td');tdHex.className='mono wrap';tdHex.innerHTML=`<code>${hex}</code>`;tr.appendChild(tdIdx);tr.appendChild(tdAscii);tr.appendChild(tdHex);tbody.appendChild(tr)});btnCopyAll.onclick=()=>copy(allHex.join(' '));btnSendAll.onclick=async()=>{btnSendAll.disabled=true;btnSendAll.textContent='Loading...';for(let i=0;i<allHex.length;i++){await sendBlock(i,allHex[i]);}btnSendAll.textContent='Starting...';const origLen=text.length;await startTransmission(origLen);btnSendAll.textContent='Sent';};out.style.display=blocks.length?\"block\":\"none\"}"
"msgEl.addEventListener('input',()=>{const n=msgEl.value.length;statsEl.textContent=`${n} char${n===1?'':'s'}`});"
"btnConvert.addEventListener('click',()=>{convert();btnSendAll.disabled=false;btnSendAll.textContent='Send';});"
"msgEl.value=\"\";statsEl.textContent=\"0 chars\";"
"})();</script>"
"</body></html>";

// ----------------- Helpers -----------------
static inline int line_has_lf(const char *buf) { 
  return strchr(buf, '\n') != NULL; 
}

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

// ----------------- BIT-BANGING SPI CORE -----------------

/* Send a single bit via bit-banged SPI */
static inline void spi_send_bit(uint8_t bit) {
    digitalWrite(SPI_COPI, bit & 1);
    for (volatile int i = 0; i < 4; i++) { __NOP(); }
    
    digitalWrite(SPI_SCK, 1);
    for (volatile int i = 0; i < 4; i++) { __NOP(); }
    
    digitalWrite(SPI_SCK, 0);
    for (volatile int i = 0; i < 4; i++) { __NOP(); }
}

/* Send 128-bit value MSB first, one bit at a time */
static void spi_send_128bits(const uint8_t *data) {
    for (int byte_idx = 0; byte_idx < 16; byte_idx++) {
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            uint8_t bit = (data[byte_idx] >> bit_idx) & 1;
            spi_send_bit(bit);
        }
    }
}

/* Send plaintext + key pair to FPGA 1 using bit-banging */
static void spi_send_pair_blocking(const uint8_t *pt16, const uint8_t *key16) {
    digitalWrite(LOAD_PIN, 1);
    digitalWrite(SPI_CE, 0);
    
    // Send 128-bit plaintext MSB first
    spi_send_128bits(pt16);
    
    // Send 128-bit key MSB first
    spi_send_128bits(key16);
    
    digitalWrite(SPI_CE, 1);
    digitalWrite(LOAD_PIN, 0);
}

// ----------------- Edge detection for DONE signal -----------------
static inline int detect_done_rising_edge(void) {
  uint8_t current_done = digitalRead(DONE_PIN);
  int rising_edge = (current_done && !prev_done_state);
  prev_done_state = current_done;
  return rising_edge;
}

// ----------------- Block transmission control -----------------
static void start_send_if_valid(int i) {
  if (i < 0 || i >= (int)total_blocks) return;
  if (!have_block[i]) return;

  spi_send_pair_blocking((const uint8_t*)plaintext_blocks[i], (const uint8_t*)keys[i]);
  current_idx = i;
  next_idx = i + 1;
  tx_state = TX_ENCRYPTING;  // Now waiting for encryption to finish
  led_blink_short();
}

static void try_send_next_ready(void) {
  for (int i = next_idx; i < (int)total_blocks; i++) {
    if (have_block[i]) {
      start_send_if_valid(i);
      return;
    }
  }
  // No more blocks to send
  tx_state = TX_IDLE;
  current_idx = -1;
  next_idx = 0;
  
  // Optional: signal completion
  for (int j = 0; j < 2; j++) {
    digitalWrite(LED_PIN, 1);
    delay_millis(TIM15, 100);
    digitalWrite(LED_PIN, 0);
    delay_millis(TIM15, 100);
  }
}

// ----------------- Public: init IO + SPI -----------------
void mechacrypt_init_io_and_spi(void) {
  pinMode(LOAD_PIN, GPIO_OUTPUT);  digitalWrite(LOAD_PIN, 0);
  pinMode(DONE_PIN, GPIO_INPUT);  

  // Configure SPI pins as GPIO for bit-banging
  pinMode(SPI_SCK, GPIO_OUTPUT);   digitalWrite(SPI_SCK, 0);
  pinMode(SPI_COPI, GPIO_OUTPUT);  digitalWrite(SPI_COPI, 0);
  pinMode(SPI_CE, GPIO_OUTPUT);    digitalWrite(SPI_CE, 1);
  
  // Set SCK to high speed
  GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED3;
  
  // Initialize edge detection
  prev_done_state = digitalRead(DONE_PIN);
  
  // Clear state
  tx_state = TX_IDLE;
  current_idx = -1;
  next_idx = 0;
  bridge_keys_sent = 0;
}

// ----------------- Public: main-loop poll -----------------
void mechacrypt_poll_and_advance(void) {
  if (tx_state == TX_ENCRYPTING) {
    // Wait for send_done rising edge (encryption + transmission complete)
    if (detect_done_rising_edge()) {
      delay_millis(TIM15, 5);  // Small delay for stability
      
      // Move to next block
      tx_state = TX_IDLE;
      try_send_next_ready();
    }
  }
}

void mechacrypt_maybe_start_after_block(int block_idx) {
  // Not used anymore - we start explicitly via /start command
}

// ----------------- Bridge: Send all keys to FPGA 2 -----------------
static int bridge_send_all_keys(uint8_t num_blocks, uint8_t orig_length) {
  // Send header: original message length + number of blocks
  bridgeSelect();
  delay_millis(TIM15, 1);
  
  // Send message length (8 bits)
  for (int b = 7; b >= 0; b--) {
    uint8_t bit = (orig_length >> b) & 1;
    spi_send_bit(bit);
  }
  
  // Send number of blocks (8 bits)
  for (int b = 7; b >= 0; b--) {
    uint8_t bit = (num_blocks >> b) & 1;
    spi_send_bit(bit);
  }
  
  bridgeDeselect();
  delay_millis(TIM15, 5);
  
  // Send all keys, one at a time
  for (int block = 0; block < num_blocks; block++) {
    if (!have_block[block]) {
      led_error_blink();
      return -1;
    }
    
    bridgeSelect();
    delay_millis(TIM15, 1);
    
    // Send 128-bit key for this block, MSB first, bit-by-bit
    spi_send_128bits((const uint8_t*)keys[block]);
    
    bridgeDeselect();
    delay_millis(TIM15, 5);
    
    // Brief LED flash every 4 keys
    if ((block % 4) == 0) {
      digitalWrite(LED_PIN, 1);
      delay_millis(TIM15, 50);
      digitalWrite(LED_PIN, 0);
    }
  }
  
  return 0;
}

// ----------------- Request handler -----------------
void processWebRequest(USART_TypeDef *USART)
{
  if (USART == NULL) return;
  if (!(USART->ISR & USART_ISR_RXNE)) return;

  char request[BUFF_LEN] = {0};
  int idx = 0;

  while (!line_has_lf(request)) {
    while (!(USART->ISR & USART_ISR_RXNE)) {/*spin*/}
    if (idx < (int)sizeof(request)-1) {
      request[idx++] = readChar(USART);
      request[idx]   = '\0';
    } else {
      (void)readChar(USART);
    }
  }

  debug_request_count++;

  // Handle: GET /send?i=##&hex=AA%20BB%20...
  if (strstr(request, "/send?") || strstr(request, "send?")) {
    uint8_t blk[16] = {0};
    int got = 0;
    int block_idx = -1;

    const char *pi = strstr(request, "i=");
    if (pi) {
      block_idx = atoi(pi+2);
      debug_last_block_idx = block_idx;
    }

    const char *ph = strstr(request, "hex=");
    if (ph) {
      char hexline[3*16+32] = {0};
      const char *src = ph+4; 
      char *dst = hexline;
      while (*src && *src!=' ' && *src!='\r' && *src!='\n' && (dst-hexline) < (int)sizeof(hexline)-1) {
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
      debug_last_got_bytes = got;
    }

    if (got == 16 && block_idx >= 0 && block_idx < MAX_BLOCKS) {
      memcpy((void*)plaintext_blocks[block_idx], blk, 16);
      
      int trng_result = read_trng((uint8_t*)keys[block_idx]);
      debug_last_trng_result = trng_result;
      
      if (trng_result == 0) {
        have_block[block_idx] = 1;
        if (block_idx >= total_blocks) total_blocks = block_idx + 1;
        led_blink_short();
      } else {
        have_block[block_idx] = 0;
        led_error_blink();
      }
    } else {
      debug_last_got_bytes = got;
      debug_last_block_idx = block_idx;
    }

    sendString(USART, (char*)http_header_no_content);
    return;
  }

  // Handle: GET /start?len=##
  if (strstr(request, "/start") || strstr(request, "start")) {
    uint8_t orig_len = 0;
    const char *plen = strstr(request, "len=");
    if (plen) {
      orig_len = (uint8_t)atoi(plen+4);
      message_length = orig_len;
    }

    if (total_blocks > 0 && have_block[0]) {
      // STEP 1: Send ALL keys to FPGA 2 via bridge
      led_blink_bridge();
      
      int bridge_result = bridge_send_all_keys((uint8_t)total_blocks, orig_len);
      
      if (bridge_result == 0) {
        bridge_keys_sent = 1;
        led_blink_bridge();
        
        delay_millis(TIM15, 10);
        
        // STEP 2: Start SPI transmission to FPGA 1 (encryption)
        if (tx_state == TX_IDLE) {
          start_send_if_valid(0);
        }
      } else {
        led_error_blink();
      }
    } else {
      led_error_blink();
    }
    
    sendString(USART, (char*)http_header_no_content);
    return;
  }

  // Otherwise: serve full page
  sendString(USART, (char*)http_header_ok);
  sendString(USART, (char*)webpage);
}