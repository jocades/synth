#include <ApplicationServices/ApplicationServices.h>
#include <AudioToolbox/AudioToolbox.h>
#include <ncurses.h>
#include <stdatomic.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ssize_t isize;

typedef float f32;
typedef double f64;

void audio_callback(void* ud, AudioQueueRef queue, AudioQueueBufferRef buf);

typedef struct {
  AudioStreamBasicDescription fmt;
  AudioQueueRef queue;
  u64 total_samples;
  f32 (*callback)(f64 time);
} Engine;

void engine_init(Engine* engine, f32 (*callback)(f64 time)) {
  memset(engine, 0, sizeof(Engine));
  engine->fmt.mSampleRate = 44100.0;
  engine->fmt.mFormatID = kAudioFormatLinearPCM;
  engine->fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  engine->fmt.mBytesPerPacket = 4;
  engine->fmt.mFramesPerPacket = 1;
  engine->fmt.mBytesPerFrame = 4;
  engine->fmt.mChannelsPerFrame = 1;
  engine->fmt.mBitsPerChannel = 32;

  engine->callback = callback;

  AudioQueueNewOutput(&engine->fmt, audio_callback, engine, NULL, NULL, 0, &engine->queue);
  for (int i = 0; i < 3; i++) {
    AudioQueueBufferRef buf;
    AudioQueueAllocateBuffer(engine->queue, 4096, &buf);
    audio_callback(engine, engine->queue, buf);
  }

  AudioQueueStart(engine->queue, NULL);
}

void engine_stop(Engine* engine) {
  AudioQueueStop(engine->queue, true);
  AudioQueueDispose(engine->queue, true);
}

f64 engine_get_time(Engine* engine) {
  return (f64)engine->total_samples / engine->fmt.mSampleRate;
}

void audio_callback(void* ud, AudioQueueRef queue, AudioQueueBufferRef buf) {
  Engine* engine = (Engine*)ud;
  f32* out = (f32*)buf->mAudioData;
  int num_samples = buf->mAudioDataBytesCapacity / sizeof(f32);

  for (int i = 0; i < num_samples; i++) {
    f64 time = engine_get_time(engine);
    out[i] = engine->callback(time);
    engine->total_samples++;
  }

  buf->mAudioDataByteSize = buf->mAudioDataBytesCapacity;
  AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
}

bool is_key_down(CGKeyCode keycode) {
  return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, keycode);
}

#define PI 3.14159265358979323846264338327950288
#define PITCH_STD 440.0
#define TWELFTH_ROOT_OF_TWO 1.05946309

// Convert frequency (Hz) to angular velocity
#define W(hz) ((hz) * 2.0 * PI)

// Get the frequency of a note relative to the pitch standard measured in semitones
#define FREQ(semitones) (PITCH_STD * pow(TWELFTH_ROOT_OF_TWO, (semitones)))

_Atomic f64 current_freq = 0.0;
_Atomic f64 current_amp = 0.5;

typedef enum {
  OSC_SINE,
  OSC_SQUARE,
  OSC_TRIANGLE,
  OSC_SAW,
} OscKind;

f64 modulate(f64 hz, f64 time, f64 lfo_hz, f64 lfo_amp) {
  return W(hz) * time + lfo_amp * hz * sin(W(lfo_hz) * time);
}

f64 osclfo(f64 hz, f64 time, f64 lfo_hz, f64 lfo_amp) {
  f64 freq = modulate(hz, time, lfo_hz, lfo_amp);
  return sin(freq);
}

f64 osc(f64 hz, f64 time, OscKind kind) {
  switch (kind) {
    case OSC_SINE:
      return sin(W(hz) * time + 0.01 * hz * sin(W(5.0) * time));
      /* case OSC_SINE: return sin(W(hz) * time); */

    case OSC_SQUARE: return sin(W(hz) * time) > 0 ? 1.0 : -1.0;

    case OSC_TRIANGLE: return asin(sin(W(hz) * time)) * (2.0 / PI);

    case OSC_SAW: {
      f64 out = 0.0;
      for (f64 n = 1.0; n < 40.0; n++) {
        out += (sin(n * W(hz) * time)) / n;
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

void note_on(Envelope* env, f64 time) {
  env->on_time = time;
  env->active = true;
}

void note_off(Envelope* env, f64 time) {
  env->off_time = time;
  env->active = false;
}

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

Envelope env = {
  .attack_time = 0.10,
  .decay_time = 0.01,
  .start_amp = 1.0,
  .sustain_amp = 0.8,
  .release_time = 0.2,
};

f32 make_sound(f64 time) {
  f64 freq = atomic_load(&current_freq);
  f64 master = atomic_load(&current_amp);
  return master * get_amp(&env, time) * osc(freq, time, OSC_SINE);
}

typedef enum {
  KEY_1 = 18,
  KEY_2 = 19,
  KEY_3 = 20,
  KEY_4 = 21,
  KEY_5 = 23,
  KEY_6 = 22,
  KEY_7 = 26,
  KEY_8 = 28,
  KEY_9 = 25,
  KEY_0 = 29,
  KEY_MINUS = 27,
  KEY_EQUAL = 24,

  KEY_Q = 12,
  KEY_W = 13,
  KEY_E = 14,
  KEY_R = 15,
  KEY_T = 17,
  KEY_Y = 16,
  KEY_U = 32,
  KEY_I = 34,
  KEY_O = 31,
  KEY_P = 35,
  KEY_LBRACKET = 33,
  KEY_RBRACKET = 30,
  KEY_BSLASH = 42,

  KEY_A = 0,
  KEY_S = 1,
  KEY_D = 2,
  KEY_F = 3,
  KEY_G = 5,
  KEY_H = 4,
  KEY_J = 38,
  KEY_K = 40,
  KEY_L = 37,
  KEY_SEMI = 41,
  KEY_QUOTE = 39,
  KEY_ENTR = 36,
  KEY_HOM = 115,

  KEY_Z = 6,
  KEY_X = 7,
  KEY_C = 8,
  KEY_V = 9,
  KEY_B = 11,
  KEY_N = 45,
  KEY_M = 46,
  KEY_COMMA = 43,
  KEY_DOT = 47,
  KEY_FSLASH = 44,
} KeyboardKey;

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

typedef struct {
  KeyboardKey code;
  f64 freq;
  const char* repr;
} Note;

void draw_stats(int y, int x, Note* note) {
  move(y, 0);
  clrtoeol();
  const char* repr = note ? note->repr : "NONE";
  f32 freq = note ? note->freq : 0.0f;
  mvprintw(y, x, "Note: %-4s │ Frequency: %07.3f Hz", repr, freq);
}

int main() {
  setlocale(LC_ALL, "en_US.UTF-8");
  initscr();
  cbreak();
  noecho();
  curs_set(1);
  nodelay(stdscr, true);

  Engine engine;
  engine_init(&engine, make_sound);

  Note piano[] = {
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

  draw_piano();
  int y, x;
  getyx(stdscr, y, x);

  Note* note = NULL;

  for (;;) {
    f64 now = engine_get_time(&engine);

    bool pressed = false;
    for (int i = 0; i < key_count; i++) {
      if (is_key_down(piano[i].code)) {
        pressed = true;
        if (note != &piano[i]) {
          atomic_store(&current_freq, piano[i].freq);
          note = &piano[i];
          note_on(&env, now);
        }
        break;
      }
    }

    if (!pressed && note != NULL) {
      note = NULL;
      note_off(&env, now);
    }

    if (is_key_down(KEY_Q)) break;

    draw_stats(y, x, note);
    refresh();

    usleep(1000);  // 1ms polling rate
  }

  engine_stop(&engine);
  endwin();

  return 0;
}
