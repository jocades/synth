#ifndef engine_h
#define engine_h

#include <AudioToolbox/AudioToolbox.h>

#include "ndefs.h"

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

#endif  // !engine_h
