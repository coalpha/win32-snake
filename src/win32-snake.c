#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>

#define W 40
#define H 15

#define i8 char
#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long

static HANDLE hStdout;
static HANDLE hStdin;
static HWND consoleWindow;
static COORD const widthHeight = {.X = W, .Y = H};
static COORD const zeroZero = {.X = 0, .Y = 0};

static void configWindow(void) {
   SMALL_RECT const consoleWindowRect =
      { .Left   = 0
      , .Top    = 0
      , .Right  = W
      , .Bottom = H
      };
   SetConsoleWindowInfo(hStdout, TRUE, &consoleWindowRect);
   ShowScrollBar(consoleWindow, SB_BOTH, FALSE);
}

static SMALL_RECT consoleWriteArea =
   { .Left   = 0
   , .Top    = 0
   , .Right  = W
   , .Bottom = H
   };
static CHAR_INFO frameBuffer[H * W];
static inline void writeFrame(void) {
   configWindow();
   WriteConsoleOutputW(
      hStdout,
      frameBuffer,
      widthHeight,
      zeroZero,
      &consoleWriteArea
   );
}

enum Direction {
   Up,
   Down,
   Left,
   Right
};

static enum Direction snakeDirection = Right;

struct GameCoord {
   i8 x;
   i8 y;
};

static inline u8 gameCoordEq(struct GameCoord const gcA, struct GameCoord const gcB) {
   return (gcA.x == gcB.x) && (gcA.y == gcB.y);
}

// ring buffer of coordinates
#define snakeBodyRingBufferLength (W * H)
#define ringNext(idx) ((idx + 1) % snakeBodyRingBufferLength)
static struct GameCoord snakeBody[snakeBodyRingBufferLength];
static size_t snakeTail = 0;
static size_t snakeHead = 1;

static inline CHAR_INFO *gameCoordToBufferPtr(struct GameCoord const gc) {
   return frameBuffer + (size_t) gc.y * W + (size_t) gc.x;
}

static inline u8 isInSnake(struct GameCoord const gc) {
   for (
      size_t snakePos = snakeTail;
      snakePos != snakeHead;
      snakePos = ringNext(snakePos)
   ) {
      struct GameCoord const snakeBlock = snakeBody[snakePos];
      if (gameCoordEq(gc, snakeBlock)) {
         return 1;
      }
   }
   return 0;
}

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

static inline u64 rand() {
   SYSTEMTIME st;
   GetSystemTime(&st);
   return *((u64 *) &(st.wSecond));
}

static struct GameCoord snakeFood = {.x = 0, .y = 0};
static void randomizeFood(void) {
   CHAR_INFO *const oldSnakeFoodPtr = gameCoordToBufferPtr(snakeFood);
   CHAR_INFO *newSnakeFoodPtr;
   oldSnakeFoodPtr->Char.UnicodeChar = L'\0';
   oldSnakeFoodPtr->Attributes = 0;
   do {
      snakeFood.x = rand() % W;
      snakeFood.y = rand() % H;
      newSnakeFoodPtr = gameCoordToBufferPtr(snakeFood);
   } while (isInSnake(snakeFood) || oldSnakeFoodPtr == newSnakeFoodPtr);
   newSnakeFoodPtr->Char.UnicodeChar = L'%';
   newSnakeFoodPtr->Attributes = fLight | fRed;
}

static inline void color(struct GameCoord const gc) {
   gameCoordToBufferPtr(gc)->Attributes = bLight | bGreen;
}

static inline void uncolor(struct GameCoord const gc) {
   gameCoordToBufferPtr(gc)->Attributes = 0;
}

int __stdcall start(void) {
   hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   hStdin = GetStdHandle(STD_INPUT_HANDLE);
   consoleWindow = GetConsoleWindow();

   DWORD oldMode;
   GetConsoleMode(hStdin, &oldMode);
   SetConsoleMode(hStdin, 0);

   SetConsoleScreenBufferSize(hStdout, widthHeight);
   SetConsoleTitleW(L"win32-snake");

   snakeBody[snakeTail] = (struct GameCoord) {.x = W/2, .y = H/2};
   snakeBody[snakeHead] = (struct GameCoord) {.x = W/2 + 1, .y = H/2};

   color(snakeBody[snakeTail]);
   color(snakeBody[snakeHead]);
   randomizeFood();
   writeFrame();

   while (TRUE) {
      Sleep(200);
      enum Direction prevDirection = snakeDirection;
      while (TRUE) {
         DWORD numEvents;
         GetNumberOfConsoleInputEvents(hStdin, &numEvents);
         if (numEvents == 0) {
            break;
         }

         INPUT_RECORD ir;
         DWORD numEventsRead;
         while (numEvents --> 0) {
            ReadConsoleInputW(hStdin, &ir, 1, &numEventsRead);
            if (ir.EventType != KEY_EVENT) {
               continue;
            }

            // we want key down
            if (ir.Event.KeyEvent.bKeyDown == FALSE) {
               continue;
            }

            switch (ir.Event.KeyEvent.uChar.UnicodeChar) {
            case L'w': if (prevDirection != Down)  snakeDirection = Up;    break;
            case L'a': if (prevDirection != Right) snakeDirection = Left;  break;
            case L's': if (prevDirection != Up)    snakeDirection = Down;  break;
            case L'd': if (prevDirection != Left)  snakeDirection = Right; break;
            case L'q': goto END;
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

      if (newHead.x < 0 || newHead.y < 0 || newHead.x == W || newHead.y == H) {
         goto END;
      }

      if (isInSnake(newHead)) {
         goto END;
      }

      if (gameCoordEq(newHead, snakeFood)) {
         randomizeFood();
      } else {
         uncolor(snakeBody[snakeTail]);
         snakeTail = ringNext(snakeTail);
      }

      snakeHead = ringNext(snakeHead);
      snakeBody[snakeHead] = newHead;

      color(newHead);
      writeFrame();
   }
   END:
   SetConsoleMode(hStdin, oldMode);
   return 0;
}
