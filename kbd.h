#include <ApplicationServices/ApplicationServices.h>

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

bool is_key_down(KeyboardKey keycode) {
  return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, keycode);
}
