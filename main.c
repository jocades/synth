#include <AudioToolbox/AudioToolbox.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdatomic.h>

#include "engine.h"
#include "kbd.h"

#define PI 3.14159265358979323846264338327950288
#define PITCH_STD 440.0
#define TWELFTH_ROOT_OF_TWO 1.05946309

// Convert frequency (Hz) to angular velocity
#define W(hz) ((hz) * 2.0 * PI)

// Get the frequency of a note relative to the pitch standard measured in semitones
#define FREQ(semitones) (PITCH_STD * pow(TWELFTH_ROOT_OF_TWO, (semitones)))

typedef enum {
  OSC_SINE,
  OSC_SQUARE,
  OSC_TRIANGLE,
  OSC_SAW,
  OSC_NOISE,
} OscKind;

f64 osc(f64 hz, f64 time, OscKind kind) {
  switch (kind) {
    case OSC_SINE: return sin(W(hz) * time);

    case OSC_SQUARE: return sin(W(hz) * time) > 0 ? 1.0 : -1.0;

    case OSC_TRIANGLE: return asin(sin(W(hz) * time)) * (2.0 / PI);

    case OSC_SAW: {
      f64 out = 0.0;
      for (f64 n = 1.0; n < 40.0; n++) {
        out += (sin(n * W(hz) * time)) / n;
      }
      return out * (2.0 / PI);
    }

    case OSC_NOISE: return 2.0 * ((f64)rand() / (f64)RAND_MAX) - 1.0;

    default: return 0.0;
  }
}

inline f64 modulate(f64 hz, f64 time, f64 lfo_hz, f64 lfo_amp) {
  return W(hz) * time + lfo_amp * hz * sin(W(lfo_hz) * time);
}

f64 osclfo(f64 hz, f64 time, OscKind kind, f64 lfo_hz, f64 lfo_amp) {
  f64 freq = modulate(hz, time, lfo_hz, lfo_amp);

  switch (kind) {
    case OSC_SINE: return sin(freq);

    case OSC_SQUARE: return sin(freq) > 0 ? 1.0 : -1.0;

    case OSC_TRIANGLE: return asin(sin(freq)) * (2.0 / PI);

    case OSC_SAW: {
      f64 out = 0.0;
      for (f64 n = 1.0; n < 40.0; n++) {
        out += (sin(n * freq)) / n;
      }
      return out * (2.0 / PI);
    }

    default: return 0.0;
  }
}

typedef struct {
  bool active;
  f64 attack_time;
  f64 decay_time;
  f64 start_amp;
  f64 sustain_amp;
  f64 release_time;
  f64 on_time;
  f64 off_time;
} Envelope;

f64 get_amp(Envelope* env, f64 time) {
  f64 amp = 0.0;
  f64 lifetime = time - env->on_time;

  if (env->active) {
    if (lifetime <= env->attack_time) {
      // Attack phase - approach max amplitude
      amp = (lifetime / env->attack_time) * env->start_amp;
    }

    if (lifetime > env->attack_time && lifetime <= env->attack_time + env->decay_time) {
      // Decay phase - reduce to sustained amplitude
      amp =
        ((lifetime - env->attack_time) / env->decay_time) * (env->sustain_amp - env->start_amp) +
        env->start_amp;
    }

    if (lifetime > env->attack_time + env->decay_time) {
      // Sustain phase - dont change until note release
      amp = env->sustain_amp;
    }
  } else {
    // Relase phase - key has been released
    amp =
      ((time - env->off_time) / env->release_time) * (0.0 - env->sustain_amp) + env->sustain_amp;
  }

  if (amp <= 0.0001) amp = 0.0;

  return amp;
}

typedef struct Instrument Instrument;
typedef f64 (*InstrumentSound)(Instrument* self, f64 freq, f64 time);

struct Instrument {
  f64 volume;
  Envelope env;
  InstrumentSound sound;
};

Instrument* inst_new(f64 volume) {
  Instrument* inst = malloc(sizeof(Instrument));
  memset(inst, 0, sizeof(Instrument));
  inst->volume = volume;
  return inst;
}

f64 inst_harmonica_sound(Instrument* self, f64 freq, f64 time) {
  // clang-format off
  return get_amp(&self->env, time) * (
    + 1.0 * osclfo(freq, time, OSC_SQUARE, 5.0, 0.001)
    + 0.5 * osc(freq * 1.5, time, OSC_SQUARE)
    + 0.25 * osc(freq * 2, time, OSC_SQUARE)
    + 0.05 * osc(0, time, OSC_NOISE)
  );  // clang-format on
}

Instrument* inst_harmonica_new() {
  Instrument* inst = inst_new(1.0);
  inst->env.attack_time = 0.10;
  inst->env.decay_time = 0.01;
  inst->env.start_amp = 1.0;
  inst->env.sustain_amp = 0.8;
  inst->env.release_time = 0.2;
  inst->sound = inst_harmonica_sound;
  return inst;
}

f64 inst_bell_sound(Instrument* self, f64 freq, f64 time) {
  // clang-format off
  return get_amp(&self->env, time) * (
    + 1.0 * osclfo(freq * 2.0, time, OSC_SINE, 5.0, 0.001)
    + 0.5 * osc(freq * 3.0, time, OSC_SINE)
    + 0.25 * osc(freq * 4.0, time, OSC_SINE)
  );  // clang-format on
}

Instrument* inst_bell_new() {
  Instrument* inst = inst_new(1.0);
  inst->env.attack_time = 0.01;
  inst->env.decay_time = 1.0;
  inst->env.start_amp = 1.0;
  inst->env.sustain_amp = 0.0;
  inst->env.release_time = 1.0;
  inst->sound = inst_bell_sound;
  return inst;
}

Instrument* voice = NULL;

Envelope env = {
  .attack_time = 0.10,
  .decay_time = 0.01,
  .start_amp = 1.0,
  .sustain_amp = 0.8,
  .release_time = 0.2,
};

f64 get_note_amp(Envelope* env, f64 time, f64 on, f64 off, bool* finished) {
  f64 amp = 0.0;

  if (on > off) {  // Note is on
    f64 lifetime = time - on;

    if (lifetime <= env->attack_time) {
      amp = (lifetime / env->attack_time) * env->start_amp;
    }

    if (lifetime > env->attack_time && lifetime <= env->attack_time + env->decay_time) {
      amp =
        ((lifetime - env->attack_time) / env->decay_time) * (env->sustain_amp - env->start_amp) +
        env->start_amp;
    }

    if (lifetime > env->attack_time + env->decay_time) {
      amp = env->sustain_amp;
    }

  } else {
    f64 lifetime = off - on;
    f64 release_amp = 0.0;

    if (lifetime <= env->attack_time) {
      release_amp = (lifetime / env->attack_time) * env->start_amp;
    }

    if (lifetime > env->attack_time && lifetime <= env->attack_time + env->decay_time) {
      release_amp =
        ((lifetime - env->attack_time) / env->decay_time) * (env->sustain_amp - env->start_amp) +
        env->start_amp;
    }

    if (lifetime > env->attack_time + env->decay_time) {
      release_amp = env->sustain_amp;
    }

    amp = ((time - off) / env->release_time) * (0.0 - release_amp) + release_amp;
  }

  if (amp <= 0.00) {
    amp = 0.0;
    *finished = true;
  };

  return amp;
}

typedef struct {
  KeyboardKey keycode;
  f64 freq;
  f64 on;
  f64 off;
  bool pressed;
  bool alive;
} Note;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Note notes[] = {0};

f32 make_sound(f64 time) {
  f64 master = 0.5;
  f64 freq = 0.0;
  pthread_mutex_lock(&lock);
  for (int i = 0; i < 18; i++) {
    bool finished = false;
    Note* note = &notes[i];
    if (!note->alive) continue;
    freq +=
      get_note_amp(&env, time, note->on, note->off, &finished) * osc(note->freq, time, OSC_SINE);
    if (finished && note->off > note->on) note->alive = false;
  }
  pthread_mutex_unlock(&lock);

  return master * freq;
  /* return master * get_amp(&env, time) * osc(freq, time, OSC_SINE); */
  /* return master * voice->sound(voice, freq, time); */
}

typedef struct {
  KeyboardKey code;
  f64 freq;
  const char* repr;
} PianoKey;

void draw_piano() {
  const char* piano =
    "\n"
    "┌───┬───┬─┬───┬───┬───┬───┬─┬───┬─┬───┬───┬───┬───┬─┬───┬───┬───┐\n"
    "│   │   │ │   │   │   │   │ │   │ │   │   │   │   │ │   │   │   │\n"
    "│   │ W │ │ E │   │   │ T │ │ Y │ │ U │   │   │ O │ │ P │   │   │\n"
    "│   └─┬─┘ └─┬─┘   │   └─┬─┘ └─┬─┘ └─┬─┘   │   └─┬─┘ └─┬─┘   │   └─┐\n"
    "│     │     │     │     │     │     │     │     │     │     │     │\n"
    "│  A  │  S  │  D  │  F  │  G  │  H  │  J  │  K  │  L  │  ;  │  '  │\n"
    "└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘\n"
    "┊  C  ┊  D  ┊  E  ┊  F  ┊  G  ┊  A  ┊  B  ┊  C' ┊  D' ┊  E' ┊  F'  \n\n";

  mvprintw(0, 0, piano);
}

void draw_stats(int y, int x, PianoKey* key) {
  move(y, 0);
  clrtoeol();
  const char* repr = key ? key->repr : "NONE";
  f32 freq = key ? key->freq : 0.0f;
  mvprintw(y, x, "Note: %-4s │ Frequency: %07.3f Hz", repr, freq);
}

int main() {
  setlocale(LC_ALL, "en_US.UTF-8");
  initscr();
  cbreak();
  noecho();
  curs_set(1);
  nodelay(stdscr, true);

  /* voice = inst_bell_new(); */

  Engine engine;
  engine_init(&engine, make_sound);

  PianoKey piano[] = {
    {KEY_A, FREQ(-9), "C4"},
    {KEY_W, FREQ(-8), "C#4"},
    {KEY_S, FREQ(-7), "D4"},
    {KEY_E, FREQ(-6), "D#4"},
    {KEY_D, FREQ(-5), "E4"},
    {KEY_F, FREQ(-4), "F4"},
    {KEY_T, FREQ(-3), "F#4"},
    {KEY_G, FREQ(-2), "G4"},
    {KEY_Y, FREQ(-1), "G#4"},
    {KEY_H, FREQ(0), "A4"},
    {KEY_U, FREQ(1), "A#4"},
    {KEY_J, FREQ(2), "B4"},
    {KEY_K, FREQ(3), "C5"},
    {KEY_O, FREQ(4), "C#5"},
    {KEY_L, FREQ(5), "D5"},
    {KEY_P, FREQ(6), "D#5"},
    {KEY_SEMI, FREQ(7), "E5"},
    {KEY_QUOTE, FREQ(8), "F5"},
  };

  int key_count = sizeof(piano) / sizeof(piano[0]);

  for (int i = 0; i < key_count; i++) {
    notes[i].keycode = piano[i].code;
    notes[i].freq = piano[i].freq;
  }

  draw_piano();
  int y, x;
  getyx(stdscr, y, x);

  PianoKey* key = NULL;

  for (;;) {
    f64 now = engine_get_time(&engine);

    for (int i = 0; i < key_count; i++) {
      bool pressed = is_key_down(notes[i].keycode);

      pthread_mutex_lock(&lock);
      Note* note = &notes[i];

      if (pressed && !note->pressed) {
        note->pressed = true;
        note->alive = true;
        note->on = now;
      }

      if (!pressed && note->pressed) {
        note->pressed = false;
        note->off = now;
      }

      pthread_mutex_unlock(&lock);
    }

    if (is_key_down(KEY_Q)) break;

    /* draw_stats(y, x, key); */
    /* refresh(); */

    usleep(1000);  // 1ms polling rate
  }

  engine_stop(&engine);
  endwin();

  return 0;
}
