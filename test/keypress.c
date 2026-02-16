#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>

// Callback triggered whenever a key is pressed or released anywhere
CGEventRef event_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon) {
  /* Engine* engine = (Engine*)refcon; */
  /* f64 now = engine_get_time(engine); */

  // Get the virtual key code (e.g., 0 for 'A', 1 for 'S')
  CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

  if (type == kCGEventKeyDown) {
    // Prevent "key repeat" from triggering multiple Note Ons
    if (!CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat)) {
      printf("KEYDOWN %d\n", keycode);
      // Map keycode to frequency and call note_on(freq, now)
      /* if (keycode == 0) note_on(FREQ(0), now); // 'A' key */
    }
  } else if (type == kCGEventKeyUp) {
    printf("KEYUP %d\n", keycode);
    /* note_off(now); */
  }

  return event;  // Pass the event through to other apps
}

void start_key_listener() {
  // Create a tap that listens for KeyDown and KeyUp
  CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
  CFMachPortRef tap = CGEventTapCreate(
    kCGSessionEventTap,
    kCGHeadInsertEventTap,
    kCGEventTapOptionListenOnly,
    mask,
    event_callback,
    NULL
  );

  if (!tap) {
    fprintf(stderr, "Error: Setup Accessibility permissions for this app.\n");
    exit(1);
  }

  CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
  CGEventTapEnable(tap, true);

  // This starts the loop that listens for keys.
  // You can run this in a separate thread if your main loop needs to stay active.
  CFRunLoopRun();
}

void* listener(void* arg) {
  start_key_listener();
  return NULL;
}

// Returns true if the key corresponding to 'keycode' is currently pressed
bool is_key_down(CGKeyCode keycode) {
  return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, keycode);
}

int main() {
  /* pthread_t thread; */
  /* pthread_create(&thread, NULL, listener, NULL); */
  /**/
  for (;;) {
    if (is_key_down(0)) {
      printf("down\n");
    }
  }

  return 0;
}
