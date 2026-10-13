/*
 * LightLink - Morse Code LED Sender (Raspberry Pi 4)
 * Usage: ./send <count> "<message>"
 * Example: ./send 4 "hello ESP32"
 *
 * Requires: wiringPi library
 * Install: sudo apt-get install wiringpi
 * Compile: gcc -o send send.c -lwiringPi
 *
 * Wiring: LED + 330Ω resistor connected to GPIO 17 (BCM pin 17 = wiring pin 0)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wiringPi.h>

#define UNIT_MS       80
#define DOT_MS        (1  * UNIT_MS)
#define DASH_MS       (3  * UNIT_MS)
#define INTRA_MS      (1  * UNIT_MS)
#define INTER_MS      (3  * UNIT_MS)
#define WORD_MS       (7  * UNIT_MS)

#define LED_PIN       17

static const char *MORSE[36] = {
    ".-",    "-...",  "-.-.",  "-..",   ".",     "..-.",  "--.",
    "....",  "..",    ".---",  "-.-",   ".-..",  "--",    "-.",
    "---",   ".--.",  "--.-",  ".-.",   "...",   "-",     "..-",
    "...-",  ".--",   "-..-",  "-.--",  "--..",
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."
};

static const char *char_to_morse(char c)
{
    c = toupper((unsigned char)c);
    if (c >= 'A' && c <= 'Z') return MORSE[c - 'A'];
    if (c >= '0' && c <= '9') return MORSE[26 + (c - '0')];
    return NULL;
}

static void led_on(void)  { digitalWrite(LED_PIN, HIGH); }
static void led_off(void) { digitalWrite(LED_PIN, LOW);  }
static void wait(int ms)  { delay(ms); }

static void send_morse_symbol(const char *sym)
{
    for (int i = 0; sym[i]; i++) {
        led_on();
        if (sym[i] == '.') wait(DOT_MS);
        else                wait(DASH_MS);
        led_off();
        if (sym[i + 1]) wait(INTRA_MS);
    }
}

static void send_message(const char *msg)
{
    int first_char = 1;
    for (int i = 0; msg[i]; i++) {
        char c = msg[i];
        if (c == ' ') {
            wait(WORD_MS - INTER_MS);
            first_char = 1;
            continue;
        }
        const char *sym = char_to_morse(c);
        if (sym == NULL) continue;
        if (!first_char) wait(INTER_MS);
        first_char = 0;
        printf("%s ", sym);
        fflush(stdout);
        send_morse_symbol(sym);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <count> \"<message>\"\n", argv[0]);
        return 1;
    }
    int count = atoi(argv[1]);
    if (count <= 0) {
        fprintf(stderr, "Error: count must be a positive integer.\n");
        return 1;
    }
    const char *message = argv[2];
    if (wiringPiSetupGpio() == -1) {
        fprintf(stderr, "Error: wiringPiSetupGpio() failed. Run as root?\n");
        return 1;
    }
    pinMode(LED_PIN, OUTPUT);
    led_off();
    printf("Sending \"%s\" %d time(s)\n", message, count);
    for (int rep = 0; rep < count; rep++) {
        printf("Pass %d: ", rep + 1);
        fflush(stdout);
        send_message(message);
        if (rep < count - 1) wait(WORD_MS);
    }
    led_off();
    printf("\nDone.\n");
    return 0;
}
