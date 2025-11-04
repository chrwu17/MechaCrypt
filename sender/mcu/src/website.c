// website.c
// MCU-side storage of user text as 128-bit (16-byte) ASCII blocks.
// Parses /submit?msg=... and stores blocks for later FPGA transfer.
// Serves a minimal HTML page over the ESP8266 UART link.

#include "../lib/website.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ---------------- Configuration ----------------
#ifndef WEBSITE_MAX_MSG
#define WEBSITE_MAX_MSG 512  // cap input chars (URL-decoded)
#endif

#ifndef WEBSITE_BLOCK_SIZE
#define WEBSITE_BLOCK_SIZE 16
#endif

#ifndef WEBSITE_MAX_BLOCKS
#define WEBSITE_MAX_BLOCKS ((WEBSITE_MAX_MSG + WEBSITE_BLOCK_SIZE - 1) / WEBSITE_BLOCK_SIZE)
#endif

// ---------------- Internal storage ----------------
static uint8_t g_blocks[WEBSITE_MAX_BLOCKS][WEBSITE_BLOCK_SIZE];
static size_t  g_count = 0;  // number of valid blocks in g_blocks (0..WEBSITE_MAX_BLOCKS)
static size_t  g_head  = 0;  // index of next block to pop (queue behavior)
static int     g_new   = 0;  // set to 1 when new data stored

// ---------------- UART helpers ----------------
static inline void uart_wait_rx(USART_TypeDef *U){ while(!(U->ISR & USART_ISR_RXNE)){} }
static inline char uart_read(USART_TypeDef *U){ return readChar(U); }
static inline void uart_write_str(USART_TypeDef *U, const char *s){ sendString(U, (char*)s); }

// ---------------- Small utils ----------------
static int hexVal(char c){
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return 10+(c-'a');
  if(c>='A'&&c<='F')return 10+(c-'A');
  return -1;
}

static size_t urlDecode(char *out, const char *in, size_t cap){
  size_t oi=0;
  for(size_t i=0; in[i] && oi+1<cap; ){
    char c=in[i];
    if(c=='+'){ out[oi++]=' '; i++; }
    else if(c=='%' && in[i+1] && in[i+2]){
      int hi=hexVal(in[i+1]), lo=hexVal(in[i+2]);
      if(hi>=0 && lo>=0){ out[oi++]=(char)((hi<<4)|lo); i+=3; }
      else { out[oi++]=c; i++; }
    } else { out[oi++]=c; i++; }
  }
  out[oi]='\0';
  return oi;
}

static int getQueryParam(const char *reqPath, const char *key, char *out, size_t cap){
  const char *qm = strchr(reqPath, '?'); if(!qm) return 0;
  size_t klen = strlen(key);
  const char *p = qm+1;
  while(p && *p){
    const char *eq = strchr(p, '=');
    if(!eq) break;
    if((size_t)(eq - p) == klen && strncmp(p, key, klen)==0){
      const char *val = eq+1;
      const char *amp = strchr(val, '&');
      size_t vlen = amp ? (size_t)(amp - val) : strlen(val);
      if(vlen >= cap) vlen = cap-1;
      memcpy(out, val, vlen); out[vlen]='\0';
      urlDecode(out, out, cap);
      return 1;
    }
    const char *amp = strchr(eq+1, '&');
    p = amp ? (amp+1) : NULL;
  }
  return 0;
}

// ---------------- HTML (server-side form) ----------------
static const char *PAGE_HEAD =
"<!DOCTYPE html><html><head><meta charset='utf-8'/>"
"<meta name='viewport' content='width=device-width,initial-scale=1'/>"
"<title>ASCII → 128-bit Blocks</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;max-width:900px;margin:24px auto;padding:0 12px}"
"input,textarea{width:100%;box-sizing:border-box;padding:10px 12px;border:1px solid #dcdcdc;border-radius:10px;font:inherit}"
"button{padding:10px 14px;border-radius:10px;border:1px solid #dcdcdc;background:#111;color:#fff;font-weight:600;cursor:pointer}"
"table{border-collapse:collapse;width:100%;margin-top:12px}"
"th,td{border:1px solid #dcdcdc;padding:6px 8px;text-align:left}"
".mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}"
".muted{color:#666}"
"</style>"
"</head><body><h1>Text → 128-bit (16-byte) ASCII Blocks</h1>";

static const char *FORM_HTML =
"<form method='GET' action='/submit'>"
"<label for='msg'><strong>Enter text</strong> (stored on MCU, zero-padded to 16B):</label><br>"
"<textarea id='msg' name='msg' rows='5' maxlength='512' placeholder='Hello, AES!'></textarea><br>"
"<button type='submit'>Convert & Store</button>"
"</form>";

static const char *PAGE_FOOT = "</body></html>";

// ---------------- Conversion & storage ----------------
static size_t toBlocks_zeroPad(const char *msg){
  size_t len = strnlen(msg, WEBSITE_MAX_MSG);
  size_t nBlocks = (len + WEBSITE_BLOCK_SIZE - 1) / WEBSITE_BLOCK_SIZE;
  if (nBlocks > WEBSITE_MAX_BLOCKS) nBlocks = WEBSITE_MAX_BLOCKS;

  // reset queue
  g_head = 0;
  g_count = 0;

  for(size_t b=0; b<nBlocks; ++b){
    size_t base = b*WEBSITE_BLOCK_SIZE;
    for(size_t i=0;i<WEBSITE_BLOCK_SIZE;i++){
      size_t idx = base + i;
      g_blocks[b][i] = (idx < len) ? (uint8_t)msg[idx] : 0x00;
    }
  }
  g_count = nBlocks;
  g_new = (g_count > 0);
  return nBlocks;
}

// ---------------- Public storage API ----------------
size_t website_total_blocks(void){
  return (g_count >= g_head) ? (g_count - g_head) : 0;
}

int website_get_block(size_t i, uint8_t out[WEBSITE_BLOCK_SIZE]){
  size_t avail = website_total_blocks();
  if(i >= avail) return 0;
  size_t idx = (g_head + i);
  if(idx >= WEBSITE_MAX_BLOCKS) return 0;
  memcpy(out, g_blocks[idx], WEBSITE_BLOCK_SIZE);
  return 1;
}

int website_pop_block(uint8_t out[WEBSITE_BLOCK_SIZE]){
  if(website_total_blocks()==0) return 0;
  memcpy(out, g_blocks[g_head], WEBSITE_BLOCK_SIZE);
  if (g_head + 1 <= g_count) g_head++;
  if (g_head == g_count) g_new = 0; // emptied
  return 1;
}

void website_clear_blocks(void){
  g_head = 0; g_count = 0; g_new = 0;
}

int website_has_new_data(void){ return g_new; }

// ---------------- Main request handler ----------------
void processWebRequest(USART_TypeDef *USART, uint8_t *precision, int *led_status)
{
  (void)precision; (void)led_status;

  // Read request line "GET /path HTTP/1.1"
  char line[256]; int li=0;
  do {
    uart_wait_rx(USART);
    char c = uart_read(USART);
    if(li < (int)sizeof(line)-1) line[li++] = c;
    if(c=='\n') break;
  } while (1);
  line[li] = '\0';

  // Parse path (between "GET " and next space)
  const char *p = strstr(line, "GET ");
  const char *sp = p ? strchr(p+4, ' ') : NULL;
  char path[200] = "/";
  if(p && sp){
    size_t len = (size_t)(sp - (p+4));
    if(len > sizeof(path)-1) len = sizeof(path)-1;
    memcpy(path, p+4, len); path[len]='\0';
  }

  // Drain headers until CRLF CRLF
  int state = 0;
  while(state < 4){
    uart_wait_rx(USART);
    char c = uart_read(USART);
    if(state==0 && c=='\r') state=1;
    else if(state==1 && c=='\n') state=2;
    else if(state==2 && c=='\r') state=3;
    else if(state==3 && c=='\n') state=4;
    else state = (c=='\r')?1:0;
  }

  // If /submit?msg=..., decode and store
  char msg[WEBSITE_MAX_MSG+1] = {0};
  int haveMsg = 0;
  if(strncmp(path, "/submit", 7)==0){
    haveMsg = getQueryParam(path, "msg", msg, sizeof(msg));
    if(haveMsg){
      toBlocks_zeroPad(msg);
    }
  }

  // Respond with page + (optional) summary table
  uart_write_str(USART, "HTTP/1.1 200 OK\r\n");
  uart_write_str(USART, "Content-Type: text/html; charset=utf-8\r\n");
  uart_write_str(USART, "Connection: close\r\n\r\n");

  uart_write_str(USART, PAGE_HEAD);
  uart_write_str(USART, FORM_HTML);

  if(haveMsg){
    char buf[200];
    size_t blocks = website_total_blocks();
    snprintf(buf, sizeof(buf),
      "<p><strong>Stored on MCU:</strong> %u × 16-byte blocks "
      "(input %u chars, zero-padded)</p>",
      (unsigned)blocks, (unsigned)strnlen(msg, WEBSITE_MAX_MSG));
    uart_write_str(USART, buf);

    // small preview (first up to 8 blocks)
    uart_write_str(USART,
      "<table><thead><tr><th>#</th><th>ASCII</th><th>Hex</th></tr></thead><tbody>");
    uint8_t tmp[WEBSITE_BLOCK_SIZE];
    size_t preview = (blocks < 8) ? blocks : 8;

    for (size_t i = 0; i < preview; i++) {
      website_get_block(i, tmp);

      // ASCII printable
      char ascii[WEBSITE_BLOCK_SIZE + 1];
      for (int k = 0; k < WEBSITE_BLOCK_SIZE; k++) {
        unsigned char c = tmp[k];
        ascii[k] = (c >= 32 && c <= 126) ? (char)c : '.';
      }
      ascii[WEBSITE_BLOCK_SIZE] = '\0';

      // Hex line
      char hex[WEBSITE_BLOCK_SIZE * 3 + 1];
      int hi = 0;
      for (int k = 0; k < WEBSITE_BLOCK_SIZE; k++) {
        if (hi < (int)sizeof(hex)) {
          hi += snprintf(hex + hi, sizeof(hex) - (size_t)hi,
                         "%02X%s", tmp[k], (k == 15) ? "" : " ");
        }
      }

      // Row
      char row[256];
      int rn = snprintf(row, sizeof(row),
        "<tr>"
          "<td class=\"mono\">%u</td>"
          "<td class=\"mono\">%s</td>"
          "<td class=\"mono\"><code>%s</code></td>"
        "</tr>",
        (unsigned)i, ascii, hex);
      if (rn > 0) {
        uart_write_str(USART, row);
      }
    }
    uart_write_str(USART, "</tbody></table>");

    if(blocks > preview){
      uart_write_str(USART,
        "<p class='muted'>Preview limited to first 8 blocks. All blocks are stored in RAM.</p>");
    }
  } else {
    uart_write_str(USART, "<p class='muted'>Submit text to store 16-byte blocks on the MCU for FPGA transfer.</p>");
  }

  uart_write_str(USART, PAGE_FOOT);
}
