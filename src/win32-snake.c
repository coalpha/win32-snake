#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>

#define BUF_W 40
#define BUF_H 15
#define BUF_AREA (BUF_W * BUF_H)

#define GAME_W (BUF_W - 2)
#define GAME_H (BUF_H - 2)

#define MS_PER_FRAME_HORIZONTAL 150
#define MS_PER_FRAME_VERTICAL 190

#define false 0
#define true 1
#define i8 char
#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long

#define fRed   FOREGROUND_RED
#define fGreen FOREGROUND_GREEN
#define fBlue  FOREGROUND_BLUE
#define fLight FOREGROUND_INTENSITY

#define bRed   BACKGROUND_RED
#define bGreen BACKGROUND_GREEN
#define bBlue  BACKGROUND_BLUE
#define bLight BACKGROUND_INTENSITY

#define fWhite fRed | fGreen | fBlue
#define bWhite bRed | bGreen | bBlue

static HANDLE hStdout;
static HANDLE hStdin;
static HWND consoleWindow;
static COORD const widthHeight = {.X = BUF_W, .Y = BUF_H};
static COORD const zeroZero = {.X = 0, .Y = 0};
static SMALL_RECT const consoleWindowRect =
   { .Left   = 0
   , .Top    = 0
   , .Right  = BUF_W
   , .Bottom = BUF_H - 1
   };
static void configWindow(void) {
   SetConsoleWindowInfo(hStdout, true, &consoleWindowRect);
   ShowScrollBar(consoleWindow, SB_BOTH, false);
}

static SMALL_RECT consoleWriteArea =
   { .Left   = 0
   , .Top    = 0
   , .Right  = BUF_W
   , .Bottom = BUF_H
   };
static CHAR_INFO frameBuffer[BUF_AREA];
static void writeFrame(void) {
   WriteConsoleOutputW(
      hStdout,
      frameBuffer,
      widthHeight,
      zeroZero,
      &consoleWriteArea
   );
}

static u64 rand() {
   // lol
   SYSTEMTIME st;
   GetSystemTime(&st);
   return *((u64 *) &(st.wSecond));
}

static inline void clearChar(CHAR_INFO *const restrict charPtr) {
   charPtr->Char.UnicodeChar = L' ';
   charPtr->Attributes = 0;
}

static inline void drawBorders(void) {
   // rows
   for (size_t x = 0; x < BUF_W; ++x) {
      frameBuffer[x].Attributes = bLight;
      frameBuffer[(BUF_H - 1) * BUF_W + x].Attributes = bLight;
   }
   // cols
   for (size_t y = 0; y < BUF_H; ++y) {
      frameBuffer[BUF_W * y].Attributes = bLight;
      frameBuffer[BUF_W * y + BUF_W - 1].Attributes = bLight;
   }
}

enum Direction {
   Up,
   Down,
   Left,
   Right
};

static enum Direction snakeDirection = Right;

// a game coordinate does not include the borders
// 0, 0 is a valid place for the snake to be
struct GameCoord {i8 x; i8 y;};

static inline u8 gameCoordEq(struct GameCoord const gcA, struct GameCoord const gcB) {
   return gcA.x == gcB.x && gcA.y == gcB.y;
}

// ring buffer of coordinates
static struct GameCoord snakeBody[BUF_AREA];
static size_t snakeTail = 0;
static size_t snakeHead = 1;
static inline size_t ringNext(size_t idx) {
   ++idx;
   return idx == BUF_AREA ? 0 : idx;
}
static inline size_t ringPrev(size_t const idx) {
   return (idx ? idx : BUF_AREA) - 1;
}

static inline CHAR_INFO *gameCoordToBufferPtr(struct GameCoord const gc) {
   return frameBuffer + (size_t) (gc.y + 1llu) * BUF_W + (size_t) gc.x + 1llu;
}

static u8 isInSnake(struct GameCoord const gc) {
   for (
      size_t snakePos = snakeTail;
      snakePos != snakeHead;
      snakePos = ringNext(snakePos)
   ) {
      if (gameCoordEq(gc, snakeBody[snakePos])) {
         return true;
      }
   }
   return gameCoordEq(gc, snakeBody[snakeHead]);
}

static struct GameCoord snakeFood = {.x = 0, .y = 0};
// do not call this function if there are no remaining spaces on the game board!
// it will loop forever.
static void randomizeFood(void) {
   clearChar(gameCoordToBufferPtr(snakeFood));

   snakeFood.x = rand() % GAME_W;
   snakeFood.y = rand() % GAME_H;

   while (isInSnake(snakeFood)) {
      snakeFood.x++;
      if (snakeFood.x == GAME_W) {
         snakeFood.x = 0;
         snakeFood.y++;
         if (snakeFood.y == GAME_H) {
            snakeFood.y = 0;
         }
      }
   }

   CHAR_INFO *const restrict newSnakeFoodPtr = gameCoordToBufferPtr(snakeFood);
   newSnakeFoodPtr->Char.UnicodeChar = L'%';
   newSnakeFoodPtr->Attributes = fLight | fRed;
}

static inline void green(struct GameCoord const gc) {
   gameCoordToBufferPtr(gc)->Attributes = bGreen;
}

static inline void cyan(struct GameCoord const gc) {
   gameCoordToBufferPtr(gc)->Attributes = bGreen | bBlue;
}

static inline void uncolor(struct GameCoord const gc) {
   gameCoordToBufferPtr(gc)->Attributes = 0;
}

static inline void deadSnake(void) {
   for (
      size_t snakePos = snakeHead;
      snakePos != snakeTail;
      snakePos = ringPrev(snakePos)
   ) {
      cyan(snakeBody[snakePos]);
      writeFrame();
      Sleep(MS_PER_FRAME_HORIZONTAL);
   }
   cyan(snakeBody[snakeTail]);
   writeFrame();
}

static inline void clear(void) {
   for (
      size_t charPos = 0;
      charPos != BUF_AREA;
      ++charPos
   ) clearChar(frameBuffer + charPos);
   writeFrame();
}

int start(void) {
   hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   hStdin = GetStdHandle(STD_INPUT_HANDLE);
   consoleWindow = GetConsoleWindow();

   DWORD oldConsoleMode;
   GetConsoleMode(hStdin, &oldConsoleMode);
   SetConsoleMode(hStdin, 0);

   CONSOLE_CURSOR_INFO oldCursorInfo;
   GetConsoleCursorInfo(hStdout, &oldCursorInfo);
   CONSOLE_CURSOR_INFO const newCursorInfo = {.bVisible = false, .dwSize = 1};
   SetConsoleCursorInfo(hStdout, &newCursorInfo);

   SetConsoleScreenBufferSize(hStdout, widthHeight);
   SetConsoleTitleW(L"win32-snake");

   drawBorders();
   snakeBody[snakeTail] = (struct GameCoord) {.x = GAME_W/2, .y = GAME_H/2};
   snakeBody[snakeHead] = snakeBody[snakeTail];
   snakeBody[snakeHead].x += 1;

   green(snakeBody[snakeTail]);
   green(snakeBody[snakeHead]);
   randomizeFood();
   configWindow();
   writeFrame();

   while (true) {
      configWindow();
      if (snakeDirection == Up || snakeDirection == Down) {
         Sleep(MS_PER_FRAME_VERTICAL);
      } else {
         Sleep(MS_PER_FRAME_HORIZONTAL);
      }

      // if there are keyboard inputs for this frame, we wanna change the
      // direction of the snake.
      {
         DWORD numEvents;
         // oops! no error handling! :P
         GetNumberOfConsoleInputEvents(hStdin, &numEvents);
         // we need to make sure that the snake cannot turn back on itself.
         // there might be multiple keyboard events within a single frame as
         // well so we need to store the direction the snake was going in the
         // last frame.
         enum Direction const last = snakeDirection;
         INPUT_RECORD ir;
         DWORD numEventsRead;
         while (numEvents --> 0) {
            ReadConsoleInputW(hStdin, &ir, 1, &numEventsRead);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
               switch (ir.Event.KeyEvent.uChar.UnicodeChar) {
               case L'w': if (last != Down)  snakeDirection = Up;    break;
               case L'a': if (last != Right) snakeDirection = Left;  break;
               case L's': if (last != Up)    snakeDirection = Down;  break;
               case L'd': if (last != Left)  snakeDirection = Right; break;
               case L'q': goto END_SCREEN;
               }
            }
         }
      }

      struct GameCoord newHead =
         { .x = snakeBody[snakeHead].x
         , .y = snakeBody[snakeHead].y
         };
      switch (snakeDirection) {
      case Up:    newHead.y--; break;
      case Down:  newHead.y++; break;
      case Right: newHead.x++; break;
      case Left:  newHead.x--; break;
      }

      if (false
         || newHead.x < 0
         || newHead.y < 0
         || newHead.x == GAME_W
         || newHead.y == GAME_H
         || isInSnake(newHead)
      ) goto END_SCREEN;

      // the head needs to be changed before the food is potentially consumed.
      // randomizeFood will not place food inside the snake.
      snakeHead = ringNext(snakeHead);
      snakeBody[snakeHead] = newHead;

      if (snakeHead == snakeTail) {
         // you won the game
         goto END_SCREEN;
      }

      if (gameCoordEq(newHead, snakeFood)) {
         randomizeFood();
      } else {
         uncolor(snakeBody[snakeTail]);
         snakeTail = ringNext(snakeTail);
      }

      green(newHead);
      writeFrame();
   }
END_SCREEN:
   Sleep(400);
   // clear the input buffer
   {
      DWORD numEvents;
      GetNumberOfConsoleInputEvents(hStdin, &numEvents);
      INPUT_RECORD ir;
      DWORD numEventsRead;
      while (numEvents --> 0) {
         ReadConsoleInputW(hStdin, &ir, 1, &numEventsRead);
      }
      deadSnake();
      wchar_t chr;
      DWORD charsRead;
      ReadConsoleW(hStdin, &chr, 1, &charsRead, NULL);
   }
   clear();
   SetConsoleMode(hStdin, oldConsoleMode);
   SetConsoleCursorInfo(hStdout, &oldCursorInfo);
   return 0;
}
