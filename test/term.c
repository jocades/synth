#include <ncurses.h>
#include <unistd.h>

#include "../kbd.h"

int main() {
  setlocale(LC_ALL, "en_US.UTF-8");
  initscr();
  cbreak();
  noecho();
  curs_set(1);

  const char* piano =
    "\n"
    "┌───┬───┬─┬───┬───┬───┬───┬─┬───┬─┬───┬───┐\n"
    "│   │   │ │   │   │   │   │ │   │ │   │   │\n"
    "│   │ W │ │ E │   │   │ T │ │ Y │ │ U │   │\n"
    "│   └─┬─┘ └─┬─┘   │   └─┬─┘ └─┬─┘ └─┬─┘   │\n"
    "│     │     │     │     │     │     │     │\n"
    "│  A  │  S  │  D  │  F  │  G  │  H  │  J  │\n"
    "└─────┴─────┴─────┴─────┴─────┴─────┴─────┘\n\n";

  float freq = 440.0f;
  char note = 'A';

  int y, x;
  mvprintw(0, 0, piano);
  getyx(stdscr, y, x);

  for (;;) {
    mvprintw(y, x, "Note: %c │ Frequency: %f Hz", note, freq);
    refresh();
    sleep(1);
    freq += 1;
  }

  endwin();
  return 0;
}
