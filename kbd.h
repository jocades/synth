#include <ApplicationServices/ApplicationServices.h>

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
  KEY_SPACE = 49,
} KeyboardKey;

bool is_key_down(KeyboardKey keycode) {
  return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, keycode);
}
