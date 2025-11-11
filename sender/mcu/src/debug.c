#include <stm32l432xx.h>
#include "../lib/debug.h"

// Use CMSIS ITM_SendChar if available (included via core headers)
static inline void swo_putc(char c) {
    // Check ITM enabled and Stimulus 0 enabled
    if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & 1U)) {
        // Busy wait until ready (recommended simple pattern)
        while (ITM->PORT[0].u32 == 0U) { __NOP(); }
        ITM->PORT[0].u8 = (uint8_t)c;
    }
}

void debug_init(void) {
    // Enable tracing
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // (Some devices lock ITM/DWT with LAR; if your CMSIS has LAR fields un-comment)
    // ITM->LAR = 0xC5ACCE55; // if defined
    // DWT->LAR = 0xC5ACCE55; // if defined

    // Enable ITM + SWO, sync + timestamp optional
    ITM->TCR = (1U << 0)   /* ITMENA */
             | (1U << 3)   /* TSENA (optional) */
             | (1U << 16); /* SWOENA/TraceBusID default OK */

    ITM->TER = 1U; // enable stimulus port 0
}

void debug_putc(char c) {
    if (c == '\n') swo_putc('\r');
    swo_putc(c);
}

void debug_print(const char *s) {
    while (*s) debug_putc(*s++);
}

static char hexNibble(uint8_t v){ return (v < 10) ? ('0'+v) : ('A'+(v-10)); }

void debug_print_hex(const uint8_t *buf, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t b = buf[i];
        char h[3] = { hexNibble((uint8_t)(b>>4)), hexNibble((uint8_t)(b&0xF)), 0 };
        debug_putc(h[0]); debug_putc(h[1]);
        if (i+1 != len) debug_putc(' ');
    }
}

void debug_print_block(int index, const uint8_t *buf, int len) {
    // Header
    debug_print("BLK "); 
    // print index
    char tmp[16]; int n = 0;
    if (index == 0) tmp[n++]='0';
    else {
        int x = index, d[10], k=0;
        while (x>0 && k<10){ d[k++]=x%10; x/=10; }
        for (int i=k-1;i>=0;--i) tmp[n++]= '0'+d[i];
    }
    tmp[n]=0; debug_print(tmp); debug_print(": ");

    // Hex
    debug_print_hex(buf, len);
    debug_print("  | ");
    // ASCII
    for (int i=0;i<len;i++){
        uint8_t c = buf[i];
        debug_putc((c>=32 && c<=126) ? (char)c : '.');
    }
    debug_print("\n");
}
