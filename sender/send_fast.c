#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <wiringPi.h>

#define LED_PIN 17

static long UNIT_US;
#define DOT_US (1 * UNIT_US)
#define DASH_US (3 * UNIT_US)
#define INTRA_US (1 * UNIT_US)
#define INTER_US (3 * UNIT_US)
#define WORD_US (7 * UNIT_US)

static const char *MORSE[36] = {
".-", "-...", "-.-.", "-..", ".", "..-.", "--.",
"....", "..", ".---", "-.-", ".-..", "--", "-.",
"---", ".--.", "--.-", ".-.", "...", "-", "..-",
"...-", ".--", "-..-", "-.--", "--..",
"-----", ".----", "..---", "...--", "....-",
".....", "-....", "--...", "---..", "----."
};

static const char *char_to_morse(char c) {
c = toupper((unsigned char)c);
if (c >= 'A' && c <= 'Z') return MORSE[c - 'A'];
if (c >= '0' && c <= '9') return MORSE[26 + (c - '0')];
return NULL;
}

static void sleep_us(long us) {
struct timespec ts;
ts.tv_sec = us / 1000000L;
ts.tv_nsec = (us % 1000000L) * 1000L;
nanosleep(&ts, NULL);
}

static void led_on(void) { digitalWrite(LED_PIN, HIGH); }
static void led_off(void) { digitalWrite(LED_PIN, LOW); }

static int send_message(const char *msg) {
int chars_sent = 0;
int first_char = 1;
for (int i = 0; msg[i]; i++) {
char c = msg[i];
if (c == ' ') { sleep_us(WORD_US - INTER_US); first_char = 1; continue; }
const char *sym = char_to_morse(c);
if (!sym) continue;
if (!first_char) sleep_us(INTER_US);
first_char = 0;
for (int j = 0; sym[j]; j++) {
led_on();
sleep_us(sym[j] == '.' ? DOT_US : DASH_US);
led_off();
if (sym[j + 1]) sleep_us(INTRA_US);
}
chars_sent++;
}
return chars_sent;
}

int main(int argc, char *argv[]) {
if (argc < 3 || argc > 4) {
fprintf(stderr, "Usage: %s <count> \"<message>\" [unit_ms]\n", argv[0]);
return 1;
}
int count = atoi(argv[1]);
const char *msg = argv[2];
long unit_ms = (argc == 4) ? atol(argv[3]) : 11;
UNIT_US = unit_ms * 1000L;

if (wiringPiSetupGpio() == -1) {
fprintf(stderr, "Error: run as root\n");
return 1;
}
pinMode(LED_PIN, OUTPUT);
led_off();

printf("Sending \"%s\" %d times at %ld ms/unit\n", msg, count, unit_ms);

struct timespec t_start, t_end;
clock_gettime(CLOCK_MONOTONIC, &t_start);

int total_chars = 0;
for (int rep = 0; rep < count; rep++) {
total_chars += send_message(msg);
if (rep < count - 1) sleep_us(WORD_US);
}

clock_gettime(CLOCK_MONOTONIC, &t_end);
led_off();

double elapsed = (t_end.tv_sec - t_start.tv_sec) +
(t_end.tv_nsec - t_start.tv_nsec) * 1e-9;
double cps = total_chars / elapsed;

printf("Total chars: %d Time: %.3fs Rate: %.2f chars/sec\n",
total_chars, elapsed, cps);
return 0;
}
