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

int main() {
  Engine engine;
  engine_init(&engine, make_sound);

  int keys[7] = {0, 1, 2, 3, 4, 5, 6};
  f64 freqs[7] = {FREQ(-9), FREQ(-7), FREQ(-5), FREQ(-4), FREQ(-2), FREQ(0), FREQ(2)};

  bool active = false;
  bool pressed = false;

  for (;;) {
    pressed = is_key_down(0);
    if (pressed && !active) {
      atomic_store(&freq, FREQ(0));
      active = true;
    } else if (!pressed && active) {
      atomic_store(&freq, 0.0);
      active = false;
    }

    if (is_key_down(12)) break;

    usleep(1000);  // 1ms polling rate
  }

  engine_stop(&engine);

  return 0;
}
