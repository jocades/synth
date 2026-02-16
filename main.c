#include <ApplicationServices/ApplicationServices.h>
#include <AudioToolbox/AudioToolbox.h>
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

_Atomic f64 freq = 0.0;

f32 make_sound(f64 time) {
  atomic_load(&freq);
  return 0.5 * sin(W(freq) * time);
}

typedef enum {
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
} KeyboardKey;

typedef struct {
  KeyboardKey code;
  f64 freq;
} PianoKey;

int main() {
  Engine engine;
  engine_init(&engine, make_sound);

  PianoKey piano[12] = {
    {KEY_A, FREQ(-9)},  // C4
    {KEY_W, FREQ(-8)},  // C#4
    {KEY_S, FREQ(-7)},  // D4
    {KEY_E, FREQ(-6)},  // D#4
    {KEY_D, FREQ(-5)},  // E4
    {KEY_F, FREQ(-4)},  // F4
    {KEY_T, FREQ(-3)},  // F#4
    {KEY_G, FREQ(-2)},  // G4
    {KEY_Y, FREQ(-1)},  // G#4
    {KEY_H, FREQ(0)},   // A4
    {KEY_U, FREQ(1)},   // A#4
    {KEY_J, FREQ(2)},   // B4
  };

  int active = -1;
  for (;;) {
    bool pressed = false;

    for (int i = 0; i < 12; i++) {
      if (is_key_down(piano[i].code)) {
        pressed = true;
        if (active != (int)piano[i].code) {
          atomic_store(&freq, piano[i].freq);
          active = piano[i].code;
        }
      }

      if (!pressed && active != -1) {
        atomic_store(&freq, 0.0);
        active = -1;
      }
    }

    if (is_key_down(KEY_Q)) break;

    usleep(1000);  // 1ms polling rate
  }

  engine_stop(&engine);

  return 0;
}
