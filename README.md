# 💡 LightLink — Text Messages Sent Over a Beam of Light

**Two computers talking with light instead of wires or radio.** A
Raspberry Pi blinks an LED in Morse code; an ESP32-C3 watches the
blinks with a light sensor and reconstructs the message, character
by character.

```
Pi:  ./send 1 "hello ESP32"
        - .... . .-.. .-.. ---   . ... .--. ...-- ..---
ESP32: Received message: "HELLO ESP32"
```

## How it works

**Sender (Raspberry Pi 4, `sender/`)**
- `send.c` — converts text to standard Morse timing (1/3/7-unit
  gaps) and flashes an LED on GPIO 17
- `send_fast.c` — a speed variant with a configurable unit length
  (microsecond sleeps) that reports the achieved characters/sec

**Receiver (ESP32-C3, `receiver/` and `receiver-fast/`)**
- Samples a light sensor through the ADC and runs a state machine
  that classifies pulses and gaps against Morse timing windows:
  dot vs dash, letter gaps, word gaps
- **Calibrates itself at startup:** it averages 50 ambient light
  readings and sets its detection threshold from the room it's
  actually in, instead of trusting a fixed value
- Full A–Z / 0–9 lookup table; partial symbols, words, and the
  final message are flushed by silence timeouts
- Pulses shorter than a quarter of one Morse unit are rejected as
  noise — the filter scales with the link speed, so fast mode no
  longer eats its own dots
- `receiver/` is tuned for the standard 80 ms unit;
  `receiver-fast/` for an 11 ms unit

## Build

Sender (needs wiringPi):

```bash
cd sender && make
./send 4 "hello ESP32"
./send_fast 4 "hello ESP32"      # defaults to the fast receiver's 11 ms unit
```

Receiver (ESP-IDF 6.1):

```bash
cd receiver        # or receiver-fast
idf.py set-target esp32c3
idf.py build flash monitor
```

## Known limits (measured in the code, not hidden)

- The receiver polls the ADC every 5 ms, so at the 11 ms fast
  unit a dot spans only ~2 samples — the fast link works but is
  the fragile configuration; standard speed is the reliable one.
- The receiver disables the task watchdog to keep its polling
  loop simple; a production version should feed it instead.
