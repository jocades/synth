#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>

typedef struct {
  double phase;
} SineWaveState;

void audio_callback(void* ud, AudioQueueRef queue, AudioQueueBufferRef buf) {
  SineWaveState* state = (SineWaveState*)ud;
  float* out = (float*)buf->mAudioData;

  for (size_t i = 0; i < 4096 / sizeof(float); i++) {
    out[i] = sin(state->phase);
    state->phase += 2.0 * M_PI * 440.0 / 44100.0;
    if (state->phase > 2.0 * M_PI) state->phase -= 2.0 * M_PI;
  }

  buf->mAudioDataByteSize = 4096;
  AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
}

int main() {
  AudioStreamBasicDescription fmt = {0};
  fmt.mSampleRate = 44100.0;
  fmt.mFormatID = kAudioFormatLinearPCM;
  fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  fmt.mBytesPerPacket = 4;
  fmt.mFramesPerPacket = 1;
  fmt.mBytesPerFrame = 4;
  fmt.mChannelsPerFrame = 1;
  fmt.mBitsPerChannel = 32;

  AudioQueueRef queue;
  SineWaveState state = {0};

  AudioQueueNewOutput(&fmt, audio_callback, &state, NULL, NULL, 0, &queue);
  for (int i = 0; i < 3; i++) {
    AudioQueueBufferRef buf;
    AudioQueueAllocateBuffer(queue, 4096, &buf);
    audio_callback(&state, queue, buf);
  }

  printf("Playing 440Hz for 3sec...\n");
  AudioQueueStart(queue, NULL);
  sleep(3);

  AudioQueueStop(queue, true);
  AudioQueueDispose(queue, true);

  return 0;
}
