#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

static const char *TAG = "morse_rx";
#define ADC_CHANNEL ADC_CHANNEL_2
#define LIGHT_THRESHOLD 50
#define UNIT_US 80000ULL
#define DOT_MAX_US (2 * UNIT_US)
#define INTRA_MAX_US (2 * UNIT_US)
#define INTER_MIN_US (2 * UNIT_US)
#define INTER_MAX_US (5 * UNIT_US)
#define WORD_MIN_US (5 * UNIT_US)
#define NOISE_FILTER_US (UNIT_US / 4)
#define FLUSH_TIMEOUT_US (12 * UNIT_US)

typedef struct { const char *code; char ch; } MorseEntry;
static const MorseEntry MORSE_TABLE[] = {
{".-",'A'},{"-...",'B'},{"-.-.",'C'},{"-..",'D'},{".",'E'},
{"..-.",'F'},{"--.",'G'},{"....", 'H'},{"..",'I'},{".---",'J'},
{"-.-",'K'},{".-..",'L'},{"--",'M'},{"-.",'N'},{"---",'O'},
{".--.",'P'},{"--.-",'Q'},{".-.",'R'},{"...",'S'},{"-",'T'},
{"..-",'U'},{"...-",'V'},{".--",'W'},{"-..-",'X'},{"-.--",'Y'},
{"--..",'Z'},
{"-----",'0'},{".----",'1'},{"..---",'2'},{"...--",'3'},{"....-",'4'},
{".....", '5'},{"-....", '6'},{"--...",'7'},{"---..",'8'},{"----.",'9'},
{NULL,0}
};

static char morse_lookup(const char *code) {
for (int i = 0; MORSE_TABLE[i].code; i++)
if (strcmp(MORSE_TABLE[i].code, code) == 0) return MORSE_TABLE[i].ch;
return '?';
}

static char sym_buf[16]; static int sym_len = 0;
static char word_buf[128]; static int word_len = 0;
static char msg_buf[512]; static int msg_len = 0;

static void flush_symbol(void) {
if (!sym_len) return;
sym_buf[sym_len] = '\0';
char ch = morse_lookup(sym_buf);
ESP_LOGI(TAG, " symbol: %s -> '%c'", sym_buf, ch);
if (word_len < (int)sizeof(word_buf)-1) word_buf[word_len++] = ch;
sym_len = 0;
}
static void flush_word(void) {
flush_symbol();
if (!word_len) return;
word_buf[word_len] = '\0';
if (msg_len > 0 && msg_len < (int)sizeof(msg_buf)-1) msg_buf[msg_len++] = ' ';
int sp = (int)sizeof(msg_buf) - msg_len - 1;
if (sp > 0) { strncat(msg_buf, word_buf, sp); msg_len += (word_len < sp) ? word_len : sp; }
word_len = 0;
}
static void flush_message(void) {
flush_word();
if (!msg_len) return;
msg_buf[msg_len] = '\0';
printf("\n========================================\n");
printf("Received message: \"%s\"\n", msg_buf);
printf("========================================\n\n");
msg_len = 0; memset(msg_buf, 0, sizeof(msg_buf));
}

typedef enum { STATE_IDLE, STATE_LIGHT_ON, STATE_LIGHT_OFF } rx_state_t;
static adc_oneshot_unit_handle_t adc_handle;
static int g_light_threshold = LIGHT_THRESHOLD;

static void morse_receiver_task(void *pvParameters) {
rx_state_t state = STATE_IDLE;
int64_t transition_time = 0, last_activity = 0;
bool had_activity = false;
ESP_LOGI(TAG, "Morse receiver started, threshold=%d", g_light_threshold);
while (1) {
int adc_val = 0;
adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val);
bool light = (adc_val >= g_light_threshold);
int64_t now = esp_timer_get_time();
switch (state) {
case STATE_IDLE:
if (light) { state = STATE_LIGHT_ON; transition_time = last_activity = now; had_activity = true; }
else if (had_activity && (now - last_activity) > (int64_t)FLUSH_TIMEOUT_US) { flush_message(); had_activity = false; }
break;
case STATE_LIGHT_ON:
if (!light) {
int64_t d = now - transition_time;
if (d >= (int64_t)NOISE_FILTER_US && sym_len < (int)sizeof(sym_buf)-1)
sym_buf[sym_len++] = (d < (int64_t)DOT_MAX_US) ? '.' : '-';
last_activity = now; state = STATE_LIGHT_OFF; transition_time = now;
}
break;
case STATE_LIGHT_OFF:
if (light) {
int64_t g = now - transition_time;
if (g >= (int64_t)NOISE_FILTER_US) {
if (g >= (int64_t)INTER_MAX_US) flush_word();
else if (g >= (int64_t)INTRA_MAX_US) flush_symbol();
}
state = STATE_LIGHT_ON; transition_time = last_activity = now;
} else {
int64_t g = now - transition_time;
if (g >= (int64_t)INTER_MIN_US && g < (int64_t)INTER_MAX_US && sym_len > 0) { flush_symbol(); transition_time = now; }
else if (g >= (int64_t)WORD_MIN_US && (word_len > 0 || sym_len > 0)) { flush_word(); transition_time = now; }
if (had_activity && (now - last_activity) > (int64_t)FLUSH_TIMEOUT_US) { flush_message(); had_activity = false; state = STATE_IDLE; }
}
break;
}
vTaskDelay(pdMS_TO_TICKS(5));
}
}

static void calibrate_threshold(void) {
int64_t sum = 0; int v = 0;
for (int i = 0; i < 50; i++) {
adc_oneshot_read(adc_handle, ADC_CHANNEL, &v);
sum += v;
vTaskDelay(pdMS_TO_TICKS(10));
}
int ambient = (int)(sum / 50);
g_light_threshold = ambient + 250;
if (g_light_threshold > 4000) g_light_threshold = 4000;
ESP_LOGI(TAG, "Calibrated light threshold: ambient=%d threshold=%d", ambient, g_light_threshold);
}

void app_main(void) {
esp_task_wdt_deinit();
adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
adc_oneshot_new_unit(&init_cfg, &adc_handle);
adc_oneshot_chan_cfg_t chan_cfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);
calibrate_threshold();
ESP_LOGI(TAG, "ADC configured threshold=%d", LIGHT_THRESHOLD);
xTaskCreate(morse_receiver_task, "morse_rx", 4096, NULL, 5, NULL);
}
