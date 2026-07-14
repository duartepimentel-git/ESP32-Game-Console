/*
  ESP32 Game Console
  -------------------
  Hardware: ESP32 devkit + ILI9341 TFT (320x240 landscape) + 6 push buttons

  Wiring:
    TFT_CS   -> GPIO 5
    TFT_RST  -> GPIO 4
    TFT_DC   -> GPIO 2
    TFT_MOSI -> GPIO 23 (hardware VSPI)
    TFT_SCK  -> GPIO 18 (hardware VSPI)
    TFT_LED  -> 3.3V

    BTN_UP    -> GPIO 32
    BTN_DOWN  -> GPIO 33
    BTN_LEFT  -> GPIO 25
    BTN_RIGHT -> GPIO 26
    BTN_A     -> GPIO 27
    BTN_B     -> GPIO 14
    (all buttons: pin -> button -> GND, using internal pull-ups)

  Libraries required: Adafruit_GFX, Adafruit_ILI9341
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <string.h>

// ---------------- Pin configuration ----------------
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_LEFT  25
#define BTN_RIGHT 26
#define BTN_A     27
#define BTN_B     14

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define BLACK  ILI9341_BLACK
#define WHITE  ILI9341_WHITE
#define RED    ILI9341_RED
#define GREEN  ILI9341_GREEN
#define BLUE   ILI9341_BLUE
#define YELLOW ILI9341_YELLOW
#define CYAN   ILI9341_CYAN
#define PURPLE ILI9341_PURPLE
#define ORANGE ILI9341_ORANGE
#define GREY   ILI9341_DARKGREY

const int SCREEN_W = 320;
const int SCREEN_H = 240;

// ---------------- Button handling ----------------
struct Button {
  uint8_t pin;
  bool lastState;
  bool pressed; // true for exactly one loop() when newly pressed
};

Button btnUp    = {BTN_UP, HIGH, false};
Button btnDown  = {BTN_DOWN, HIGH, false};
Button btnLeft  = {BTN_LEFT, HIGH, false};
Button btnRight = {BTN_RIGHT, HIGH, false};
Button btnA     = {BTN_A, HIGH, false};
Button btnB     = {BTN_B, HIGH, false};

void updateButton(Button &b) {
  bool state = digitalRead(b.pin);
  b.pressed = (state == LOW && b.lastState == HIGH);
  b.lastState = state;
}

void updateAllButtons() {
  updateButton(btnUp);
  updateButton(btnDown);
  updateButton(btnLeft);
  updateButton(btnRight);
  updateButton(btnA);
  updateButton(btnB);
}

bool held(uint8_t pin) { return digitalRead(pin) == LOW; }

// ---------------- State machine ----------------
enum GameState { MENU, SNAKE_GAME, BREAKOUT_GAME, TETRIS_GAME, PONG_GAME, INVADERS_GAME, FLAPPY_GAME };
GameState currentState = MENU;

const char* menuItems[] = {"Snake", "Breakout", "Tetris", "Pong", "Space Invaders", "Flappy Bird"};
const int NUM_GAMES = 6;
int menuIndex = 0;

// ================= MENU =================
void drawMenu() {
  tft.fillScreen(BLACK);
  tft.setTextSize(1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(130, 6);
  tft.print("GAME MENU");

  tft.setTextSize(2);
  for (int i = 0; i < NUM_GAMES; i++) {
    int y = 26 + i * 28;
    if (i == menuIndex) {
      tft.fillRect(20, y - 4, 280, 22, BLUE);
      tft.setTextColor(WHITE, BLUE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(30, y);
    tft.print(menuItems[i]);
  }

  tft.setTextSize(1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(20, 224);
  tft.print("UP/DOWN: select    A: start");
}

void loopMenu() {
  bool changed = false;
  if (btnUp.pressed)   { menuIndex = (menuIndex - 1 + NUM_GAMES) % NUM_GAMES; changed = true; }
  if (btnDown.pressed) { menuIndex = (menuIndex + 1) % NUM_GAMES; changed = true; }
  if (changed) drawMenu();

  if (btnA.pressed) {
    switch (menuIndex) {
      case 0: currentState = SNAKE_GAME;    snakeReset();    break;
      case 1: currentState = BREAKOUT_GAME; breakoutReset(); break;
      case 2: currentState = TETRIS_GAME;   tetrisReset();   break;
      case 3: currentState = PONG_GAME;     pongReset();     break;
      case 4: currentState = INVADERS_GAME; invReset();      break;
      case 5: currentState = FLAPPY_GAME;   flappyReset();   break;
    }
  }
}

// ================= SNAKE =================
const int SNK_CELL = 10;
const int SNK_COLS = SCREEN_W / SNK_CELL; // 32
const int SNK_ROWS = SCREEN_H / SNK_CELL; // 24

struct Pt { int8_t x, y; };
Pt snakeBody[SNK_COLS * SNK_ROWS];
int snakeLen;
int snakeDirX, snakeDirY;
Pt snakeFood;
unsigned long snakeLastMove;
const int SNAKE_SPEED_MS = 120;
int snakeScore;
bool snakeGameOver;

void snakePlaceFood() {
  bool valid;
  do {
    valid = true;
    snakeFood.x = random(0, SNK_COLS);
    snakeFood.y = random(0, SNK_ROWS);
    for (int i = 0; i < snakeLen; i++) {
      if (snakeBody[i].x == snakeFood.x && snakeBody[i].y == snakeFood.y) valid = false;
    }
  } while (!valid);
}

void snakeReset() {
  snakeLen = 3;
  snakeBody[0] = {10, 12};
  snakeBody[1] = {9, 12};
  snakeBody[2] = {8, 12};
  snakeDirX = 1; snakeDirY = 0;
  snakeScore = 0;
  snakeGameOver = false;
  snakePlaceFood();
  snakeLastMove = millis();
  drawSnakeFrame();
}

void drawSnakeFrame() {
  tft.fillScreen(BLACK);
  tft.fillRect(snakeFood.x * SNK_CELL, snakeFood.y * SNK_CELL, SNK_CELL, SNK_CELL, RED);
  for (int i = 0; i < snakeLen; i++) {
    uint16_t c = (i == 0) ? GREEN : CYAN;
    tft.fillRect(snakeBody[i].x * SNK_CELL, snakeBody[i].y * SNK_CELL, SNK_CELL - 1, SNK_CELL - 1, c);
  }
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Score: ");
  tft.print(snakeScore);
}

void snakeShowGameOver() {
  tft.fillScreen(BLACK);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(60, 90);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(90, 120);
  tft.print("Score: "); tft.print(snakeScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void loopSnake() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (snakeGameOver) {
    if (btnA.pressed) snakeReset();
    return;
  }

  if (btnUp.pressed    && snakeDirY != 1)  { snakeDirX = 0;  snakeDirY = -1; }
  if (btnDown.pressed  && snakeDirY != -1) { snakeDirX = 0;  snakeDirY = 1;  }
  if (btnLeft.pressed  && snakeDirX != 1)  { snakeDirX = -1; snakeDirY = 0;  }
  if (btnRight.pressed && snakeDirX != -1) { snakeDirX = 1;  snakeDirY = 0;  }

  if (millis() - snakeLastMove < SNAKE_SPEED_MS) return;
  snakeLastMove = millis();

  Pt newHead = { (int8_t)(snakeBody[0].x + snakeDirX), (int8_t)(snakeBody[0].y + snakeDirY) };

  if (newHead.x < 0 || newHead.x >= SNK_COLS || newHead.y < 0 || newHead.y >= SNK_ROWS) {
    snakeGameOver = true; snakeShowGameOver(); return;
  }
  for (int i = 0; i < snakeLen; i++) {
    if (snakeBody[i].x == newHead.x && snakeBody[i].y == newHead.y) {
      snakeGameOver = true; snakeShowGameOver(); return;
    }
  }

  bool ate = (newHead.x == snakeFood.x && newHead.y == snakeFood.y);
  int newLen = ate ? snakeLen + 1 : snakeLen;
  for (int i = newLen - 1; i > 0; i--) snakeBody[i] = snakeBody[i - 1];
  snakeBody[0] = newHead;
  snakeLen = newLen;
  if (ate) { snakeScore += 10; snakePlaceFood(); }

  drawSnakeFrame();
}

// ================= BREAKOUT =================
const int BRK_ROWS = 5;
const int BRK_COLS = 10;
const int BRK_BRICK_W = SCREEN_W / BRK_COLS; // 32
const int BRK_BRICK_H = 12;
const int BRK_TOP_OFFSET = 20;
bool bricks[BRK_ROWS][BRK_COLS];

float paddleX;
const int PADDLE_W = 50, PADDLE_H = 8;
const int PADDLE_Y = 225;

float ballX, ballY, ballVX, ballVY;
const int BALL_R = 4;

int breakoutScore;
bool breakoutGameOver;
unsigned long breakoutLastFrame;
const int BREAKOUT_FRAME_MS = 20; // ~50 fps

void breakoutDrawBricks() {
  for (int r = 0; r < BRK_ROWS; r++) {
    for (int c = 0; c < BRK_COLS; c++) {
      if (bricks[r][c]) {
        uint16_t color = (r == 0) ? RED : (r == 1) ? YELLOW : (r == 2) ? GREEN : (r == 3) ? CYAN : BLUE;
        tft.fillRect(c * BRK_BRICK_W + 1, BRK_TOP_OFFSET + r * BRK_BRICK_H + 1,
                     BRK_BRICK_W - 2, BRK_BRICK_H - 2, color);
      }
    }
  }
}

void breakoutReset() {
  for (int r = 0; r < BRK_ROWS; r++) for (int c = 0; c < BRK_COLS; c++) bricks[r][c] = true;
  paddleX = SCREEN_W / 2 - PADDLE_W / 2;
  ballX = SCREEN_W / 2; ballY = 200;
  ballVX = 2.2; ballVY = -2.2;
  breakoutScore = 0;
  breakoutGameOver = false;
  breakoutLastFrame = millis();
  tft.fillScreen(BLACK);
  breakoutDrawBricks();
}

void breakoutShowGameOver(bool win) {
  tft.fillScreen(BLACK);
  tft.setTextColor(win ? GREEN : RED);
  tft.setTextSize(2);
  tft.setCursor(60, 90);
  tft.print(win ? "YOU WIN!" : "GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(90, 120);
  tft.print("Score: "); tft.print(breakoutScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void loopBreakout() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (breakoutGameOver) {
    if (btnA.pressed) breakoutReset();
    return;
  }

  if (millis() - breakoutLastFrame < BREAKOUT_FRAME_MS) return;
  breakoutLastFrame = millis();

  // erase old paddle & ball
  tft.fillRect((int)paddleX, PADDLE_Y, PADDLE_W, PADDLE_H, BLACK);
  tft.fillCircle((int)ballX, (int)ballY, BALL_R + 1, BLACK);

  if (held(BTN_LEFT))  paddleX -= 4;
  if (held(BTN_RIGHT)) paddleX += 4;
  if (paddleX < 0) paddleX = 0;
  if (paddleX > SCREEN_W - PADDLE_W) paddleX = SCREEN_W - PADDLE_W;

  ballX += ballVX;
  ballY += ballVY;

  if (ballX <= BALL_R || ballX >= SCREEN_W - BALL_R) ballVX = -ballVX;
  if (ballY <= BALL_R) ballVY = -ballVY;

  // paddle collision
  if (ballVY > 0 && ballY + BALL_R >= PADDLE_Y && ballY + BALL_R <= PADDLE_Y + PADDLE_H &&
      ballX >= paddleX && ballX <= paddleX + PADDLE_W) {
    ballVY = -ballVY;
    float hit = (ballX - (paddleX + PADDLE_W / 2.0)) / (PADDLE_W / 2.0);
    ballVX = hit * 3.0;
  }

  // brick collision
  if (ballY - BALL_R <= BRK_TOP_OFFSET + BRK_ROWS * BRK_BRICK_H) {
    int col = ballX / BRK_BRICK_W;
    int row = (ballY - BRK_TOP_OFFSET) / BRK_BRICK_H;
    if (row >= 0 && row < BRK_ROWS && col >= 0 && col < BRK_COLS && bricks[row][col]) {
      bricks[row][col] = false;
      ballVY = -ballVY;
      breakoutScore += 10;
      tft.fillRect(col * BRK_BRICK_W + 1, BRK_TOP_OFFSET + row * BRK_BRICK_H + 1,
                   BRK_BRICK_W - 2, BRK_BRICK_H - 2, BLACK);
    }
  }

  if (ballY > SCREEN_H) {
    breakoutGameOver = true;
    breakoutShowGameOver(false);
    return;
  }

  bool anyLeft = false;
  for (int r = 0; r < BRK_ROWS && !anyLeft; r++)
    for (int c = 0; c < BRK_COLS; c++) if (bricks[r][c]) { anyLeft = true; break; }
  if (!anyLeft) {
    breakoutGameOver = true;
    breakoutShowGameOver(true);
    return;
  }

  tft.fillRect((int)paddleX, PADDLE_Y, PADDLE_W, PADDLE_H, WHITE);
  tft.fillCircle((int)ballX, (int)ballY, BALL_R, WHITE);
  tft.fillRect(0, 0, 90, 10, BLACK);
  tft.setTextColor(WHITE); tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Score: "); tft.print(breakoutScore);
}

// ================= TETRIS =================
const int BOARD_COLS = 10;
const int BOARD_ROWS = 20;
const int CELL = 11;
const int BOARD_X = 6;
const int BOARD_Y = 8;
const int PANEL_X = 132;

// 7 pieces x 4 rotations x 4 cells x (dx,dy), each within a 4x4 bounding box
const int8_t SHAPES[7][4][4][2] = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  // O
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}
};

const uint16_t TET_COLORS[7] = {CYAN, YELLOW, PURPLE, GREEN, RED, BLUE, ORANGE};

byte board[BOARD_ROWS][BOARD_COLS]; // 0 = empty, else piece type + 1

int curType, curRot, curX, curY;
int nextType;
bool tetrisGameOver;
int tetrisScore, tetrisLines, tetrisLevel;
unsigned long tetrisLastDrop;
int tetrisDropInterval;
const int TETRIS_SOFT_DROP_MS = 60;

bool pieceFits(int type, int rot, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int cx = px + SHAPES[type][rot][i][0];
    int cy = py + SHAPES[type][rot][i][1];
    if (cx < 0 || cx >= BOARD_COLS || cy >= BOARD_ROWS) return false;
    if (cy >= 0 && board[cy][cx] != 0) return false;
  }
  return true;
}

void tetrisDrawCell(int col, int row, uint16_t color) {
  tft.fillRect(BOARD_X + col * CELL, BOARD_Y + row * CELL, CELL - 1, CELL - 1, color);
}

void tetrisDrawFrame() {
  tft.drawRect(BOARD_X - 2, BOARD_Y - 2, BOARD_COLS * CELL + 4, BOARD_ROWS * CELL + 4, WHITE);
}

void tetrisDrawBoard() {
  for (int r = 0; r < BOARD_ROWS; r++) {
    for (int c = 0; c < BOARD_COLS; c++) {
      tetrisDrawCell(c, r, board[r][c] ? TET_COLORS[board[r][c] - 1] : BLACK);
    }
  }
  for (int i = 0; i < 4; i++) {
    int cx = curX + SHAPES[curType][curRot][i][0];
    int cy = curY + SHAPES[curType][curRot][i][1];
    if (cy >= 0 && cy < BOARD_ROWS && cx >= 0 && cx < BOARD_COLS) {
      tetrisDrawCell(cx, cy, TET_COLORS[curType]);
    }
  }
}

void tetrisDrawSidePanel() {
  tft.fillRect(PANEL_X, 0, SCREEN_W - PANEL_X, SCREEN_H, BLACK);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.setCursor(PANEL_X, 4);
  tft.print("NEXT");

  int pc = 10;
  int px = PANEL_X + 10, py = 16;
  for (int i = 0; i < 4; i++) {
    int dx = SHAPES[nextType][0][i][0];
    int dy = SHAPES[nextType][0][i][1];
    tft.fillRect(px + dx * pc, py + dy * pc, pc - 1, pc - 1, TET_COLORS[nextType]);
  }

  tft.setCursor(PANEL_X, 72);  tft.print("Score:");
  tft.setCursor(PANEL_X, 84);  tft.print(tetrisScore);
  tft.setCursor(PANEL_X, 102); tft.print("Lines:");
  tft.setCursor(PANEL_X, 114); tft.print(tetrisLines);
  tft.setCursor(PANEL_X, 132); tft.print("Level:");
  tft.setCursor(PANEL_X, 144); tft.print(tetrisLevel);

  tft.setCursor(PANEL_X, 190); tft.print("Up:Rotate");
  tft.setCursor(PANEL_X, 202); tft.print("Dn:Drop");
  tft.setCursor(PANEL_X, 214); tft.print("A:Slam B:Menu");
}

void tetrisShowGameOver() {
  tft.fillScreen(BLACK);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(60, 90);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(90, 120);
  tft.print("Score: "); tft.print(tetrisScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void tetrisSpawn() {
  curType = nextType;
  nextType = random(0, 7);
  curRot = 0;
  curX = 3;
  curY = 0;
  if (!pieceFits(curType, curRot, curX, curY)) {
    tetrisGameOver = true;
    tetrisShowGameOver();
  }
}

void tetrisLockPiece() {
  for (int i = 0; i < 4; i++) {
    int cx = curX + SHAPES[curType][curRot][i][0];
    int cy = curY + SHAPES[curType][curRot][i][1];
    if (cy >= 0 && cy < BOARD_ROWS && cx >= 0 && cx < BOARD_COLS) {
      board[cy][cx] = curType + 1;
    }
  }

  int cleared = 0;
  for (int r = BOARD_ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < BOARD_COLS; c++) if (board[r][c] == 0) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr = r; rr > 0; rr--)
        for (int c = 0; c < BOARD_COLS; c++) board[rr][c] = board[rr - 1][c];
      for (int c = 0; c < BOARD_COLS; c++) board[0][c] = 0;
      r++; // re-check this row index since rows shifted down into it
    }
  }

  if (cleared > 0) {
    const int lineScore[5] = {0, 100, 300, 500, 800};
    tetrisScore += lineScore[cleared] * tetrisLevel;
    tetrisLines += cleared;
    tetrisLevel = 1 + tetrisLines / 10;
    int newInterval = 550 - (tetrisLevel - 1) * 35;
    tetrisDropInterval = newInterval < 120 ? 120 : newInterval;
  }

  tetrisSpawn();
  if (!tetrisGameOver) tetrisDrawSidePanel();
}

void tetrisReset() {
  memset(board, 0, sizeof(board));
  tetrisScore = 0;
  tetrisLines = 0;
  tetrisLevel = 1;
  tetrisDropInterval = 550;
  tetrisGameOver = false;
  nextType = random(0, 7);
  tetrisSpawn();
  tetrisLastDrop = millis();
  tft.fillScreen(BLACK);
  tetrisDrawFrame();
  tetrisDrawBoard();
  tetrisDrawSidePanel();
}

void loopTetris() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (tetrisGameOver) {
    if (btnA.pressed) tetrisReset();
    return;
  }

  bool moved = false;

  if (btnLeft.pressed && pieceFits(curType, curRot, curX - 1, curY)) { curX--; moved = true; }
  if (btnRight.pressed && pieceFits(curType, curRot, curX + 1, curY)) { curX++; moved = true; }

  if (btnUp.pressed) {
    int newRot = (curRot + 1) % 4;
    if (pieceFits(curType, newRot, curX, curY)) { curRot = newRot; moved = true; }
    else if (pieceFits(curType, newRot, curX - 1, curY)) { curRot = newRot; curX--; moved = true; }
    else if (pieceFits(curType, newRot, curX + 1, curY)) { curRot = newRot; curX++; moved = true; }
  }

  if (btnA.pressed) {
    while (pieceFits(curType, curRot, curX, curY + 1)) { curY++; tetrisScore += 2; }
    tetrisLockPiece();
    if (tetrisGameOver) return;
    moved = true;
  }

  int interval = held(BTN_DOWN) ? TETRIS_SOFT_DROP_MS : tetrisDropInterval;
  if (millis() - tetrisLastDrop >= (unsigned long)interval) {
    tetrisLastDrop = millis();
    if (pieceFits(curType, curRot, curX, curY + 1)) {
      curY++;
    } else {
      tetrisLockPiece();
      if (tetrisGameOver) return;
    }
    moved = true;
  }

  if (moved) tetrisDrawBoard();
}

// ================= PONG =================
const int PONG_PADDLE_W = 6, PONG_PADDLE_H = 40;
const int PONG_PLAYER_X = 10;
const int PONG_AI_X = SCREEN_W - 10 - PONG_PADDLE_W;
float pongPlayerY, pongAiY;
float pongBallX, pongBallY, pongBallVX, pongBallVY;
const int PONG_BALL_R = 4;
int pongPlayerScore, pongAiScore;
bool pongGameOver;
unsigned long pongLastFrame;
const int PONG_FRAME_MS = 20; // ~50 fps
const int PONG_WIN_SCORE = 5;

void pongDrawMiddleLine() {
  for (int y = 0; y < SCREEN_H; y += 12) tft.fillRect(SCREEN_W / 2 - 1, y, 2, 6, WHITE);
}

void pongResetBall(int direction) {
  pongBallX = SCREEN_W / 2;
  pongBallY = SCREEN_H / 2;
  pongBallVX = 3.0 * direction;
  pongBallVY = (random(0, 2) == 0 ? 2.0 : -2.0);
}

void pongReset() {
  pongPlayerY = SCREEN_H / 2 - PONG_PADDLE_H / 2;
  pongAiY = SCREEN_H / 2 - PONG_PADDLE_H / 2;
  pongPlayerScore = 0;
  pongAiScore = 0;
  pongGameOver = false;
  pongResetBall(random(0, 2) == 0 ? 1 : -1);
  pongLastFrame = millis();
  tft.fillScreen(BLACK);
  pongDrawMiddleLine();
}

void pongShowGameOver() {
  tft.fillScreen(BLACK);
  bool won = pongPlayerScore > pongAiScore;
  tft.setTextColor(won ? GREEN : RED);
  tft.setTextSize(2);
  tft.setCursor(won ? 70 : 75, 90);
  tft.print(won ? "YOU WIN!" : "AI WINS");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(95, 120);
  tft.print(pongPlayerScore); tft.print(" - "); tft.print(pongAiScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void loopPong() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (pongGameOver) {
    if (btnA.pressed) pongReset();
    return;
  }

  if (millis() - pongLastFrame < PONG_FRAME_MS) return;
  pongLastFrame = millis();

  tft.fillRect(PONG_PLAYER_X, (int)pongPlayerY, PONG_PADDLE_W, PONG_PADDLE_H, BLACK);
  tft.fillRect(PONG_AI_X, (int)pongAiY, PONG_PADDLE_W, PONG_PADDLE_H, BLACK);
  tft.fillCircle((int)pongBallX, (int)pongBallY, PONG_BALL_R + 1, BLACK);

  if (held(BTN_UP))   pongPlayerY -= 4;
  if (held(BTN_DOWN)) pongPlayerY += 4;
  if (pongPlayerY < 0) pongPlayerY = 0;
  if (pongPlayerY > SCREEN_H - PONG_PADDLE_H) pongPlayerY = SCREEN_H - PONG_PADDLE_H;

  float aiCenter = pongAiY + PONG_PADDLE_H / 2.0;
  if (aiCenter < pongBallY - 4) pongAiY += 2.6;
  else if (aiCenter > pongBallY + 4) pongAiY -= 2.6;
  if (pongAiY < 0) pongAiY = 0;
  if (pongAiY > SCREEN_H - PONG_PADDLE_H) pongAiY = SCREEN_H - PONG_PADDLE_H;

  pongBallX += pongBallVX;
  pongBallY += pongBallVY;

  if (pongBallY <= PONG_BALL_R || pongBallY >= SCREEN_H - PONG_BALL_R) pongBallVY = -pongBallVY;

  if (pongBallVX < 0 && pongBallX - PONG_BALL_R <= PONG_PLAYER_X + PONG_PADDLE_W &&
      pongBallX - PONG_BALL_R >= PONG_PLAYER_X &&
      pongBallY >= pongPlayerY && pongBallY <= pongPlayerY + PONG_PADDLE_H) {
    pongBallVX = -pongBallVX;
    float hit = (pongBallY - (pongPlayerY + PONG_PADDLE_H / 2.0)) / (PONG_PADDLE_H / 2.0);
    pongBallVY = hit * 3.0;
  }
  if (pongBallVX > 0 && pongBallX + PONG_BALL_R >= PONG_AI_X &&
      pongBallX + PONG_BALL_R <= PONG_AI_X + PONG_PADDLE_W &&
      pongBallY >= pongAiY && pongBallY <= pongAiY + PONG_PADDLE_H) {
    pongBallVX = -pongBallVX;
    float hit = (pongBallY - (pongAiY + PONG_PADDLE_H / 2.0)) / (PONG_PADDLE_H / 2.0);
    pongBallVY = hit * 3.0;
  }

  bool scored = false;
  if (pongBallX < 0)          { pongAiScore++;     scored = true; pongResetBall(1); }
  if (pongBallX > SCREEN_W)   { pongPlayerScore++; scored = true; pongResetBall(-1); }

  if (scored) {
    tft.fillScreen(BLACK);
    pongDrawMiddleLine();
    if (pongPlayerScore >= PONG_WIN_SCORE || pongAiScore >= PONG_WIN_SCORE) {
      pongGameOver = true;
      pongShowGameOver();
      return;
    }
  }

  tft.fillRect(PONG_PLAYER_X, (int)pongPlayerY, PONG_PADDLE_W, PONG_PADDLE_H, WHITE);
  tft.fillRect(PONG_AI_X, (int)pongAiY, PONG_PADDLE_W, PONG_PADDLE_H, WHITE);
  tft.fillCircle((int)pongBallX, (int)pongBallY, PONG_BALL_R, WHITE);

  tft.fillRect(SCREEN_W / 2 - 30, 2, 60, 10, BLACK);
  tft.setTextColor(WHITE); tft.setTextSize(1);
  tft.setCursor(SCREEN_W / 2 - 20, 2);
  tft.print(pongPlayerScore); tft.print(" - "); tft.print(pongAiScore);
}

// ================= SPACE INVADERS =================
const int INV_ROWS = 3;
const int INV_COLS = 6;
const int INV_SPACING_X = 34;
const int INV_SPACING_Y = 22;
const int INV_START_X = 30;
const int INV_START_Y = 20;
const int INV_W = 16, INV_H = 10;

bool invaders[INV_ROWS][INV_COLS];
float invBlockX;
int invBlockDirection;
int invBlockY;
int invAliveCount;

float invPlayerX;
const int INV_PLAYER_Y = 220;
const int INV_PLAYER_W = 18, INV_PLAYER_H = 8;

bool invBulletActive;
float invBulletX, invBulletY;

bool invEnemyBulletActive;
float invEnemyBulletX, invEnemyBulletY;

int invScore;
bool invGameOver;
unsigned long invLastFrame;
const int INV_FRAME_MS = 30;
unsigned long invLastEnemyShot;

void invDrawEnemy(int r, int c, uint16_t color) {
  int x = INV_START_X + c * INV_SPACING_X + (int)invBlockX;
  int y = INV_START_Y + r * INV_SPACING_Y + invBlockY;
  tft.fillRect(x, y, INV_W, INV_H, color);
}

void invReset() {
  for (int r = 0; r < INV_ROWS; r++) for (int c = 0; c < INV_COLS; c++) invaders[r][c] = true;
  invBlockX = 0;
  invBlockDirection = 1;
  invBlockY = 0;
  invAliveCount = INV_ROWS * INV_COLS;
  invPlayerX = SCREEN_W / 2 - INV_PLAYER_W / 2;
  invBulletActive = false;
  invEnemyBulletActive = false;
  invScore = 0;
  invGameOver = false;
  invLastFrame = millis();
  invLastEnemyShot = millis();
  tft.fillScreen(BLACK);
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) invDrawEnemy(r, c, GREEN);
  tft.fillRect((int)invPlayerX, INV_PLAYER_Y, INV_PLAYER_W, INV_PLAYER_H, CYAN);
  tft.setTextColor(WHITE, BLACK); tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Score: "); tft.print(invScore);
}

void invShowGameOver(bool win) {
  tft.fillScreen(BLACK);
  tft.setTextColor(win ? GREEN : RED);
  tft.setTextSize(2);
  tft.setCursor(win ? 70 : 60, 90);
  tft.print(win ? "YOU WIN!" : "GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(90, 120);
  tft.print("Score: "); tft.print(invScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void loopInvaders() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (invGameOver) {
    if (btnA.pressed) invReset();
    return;
  }

  // fire is edge-triggered so it's safe to check on every raw loop() call
  if (btnA.pressed && !invBulletActive) {
    invBulletActive = true;
    invBulletX = invPlayerX + INV_PLAYER_W / 2;
    invBulletY = INV_PLAYER_Y;
  }

  if (millis() - invLastFrame < INV_FRAME_MS) return;
  invLastFrame = millis();

  // player movement
  tft.fillRect((int)invPlayerX, INV_PLAYER_Y, INV_PLAYER_W, INV_PLAYER_H, BLACK);
  if (held(BTN_LEFT))  invPlayerX -= 3;
  if (held(BTN_RIGHT)) invPlayerX += 3;
  if (invPlayerX < 0) invPlayerX = 0;
  if (invPlayerX > SCREEN_W - INV_PLAYER_W) invPlayerX = SCREEN_W - INV_PLAYER_W;
  tft.fillRect((int)invPlayerX, INV_PLAYER_Y, INV_PLAYER_W, INV_PLAYER_H, CYAN);

  // player bullet
  if (invBulletActive) {
    tft.fillRect((int)invBulletX, (int)invBulletY, 2, 6, BLACK);
    invBulletY -= 6;
    if (invBulletY < 0) {
      invBulletActive = false;
    } else {
      bool hit = false;
      for (int r = 0; r < INV_ROWS && !hit; r++) {
        for (int c = 0; c < INV_COLS && !hit; c++) {
          if (!invaders[r][c]) continue;
          int ex = INV_START_X + c * INV_SPACING_X + (int)invBlockX;
          int ey = INV_START_Y + r * INV_SPACING_Y + invBlockY;
          if (invBulletX >= ex && invBulletX <= ex + INV_W && invBulletY >= ey && invBulletY <= ey + INV_H) {
            invaders[r][c] = false;
            invDrawEnemy(r, c, BLACK);
            invAliveCount--;
            invScore += 10;
            invBulletActive = false;
            hit = true;
          }
        }
      }
      if (invBulletActive) tft.fillRect((int)invBulletX, (int)invBulletY, 2, 6, WHITE);
    }
  }

  // enemy block movement
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) invDrawEnemy(r, c, BLACK);

  invBlockX += invBlockDirection * 1.5;
  int minC = -1, maxC = -1;
  for (int c = 0; c < INV_COLS; c++) {
    bool colAlive = false;
    for (int r = 0; r < INV_ROWS; r++) if (invaders[r][c]) colAlive = true;
    if (colAlive) { if (minC == -1) minC = c; maxC = c; }
  }
  if (minC != -1) {
    int leftX = INV_START_X + minC * INV_SPACING_X + (int)invBlockX;
    int rightX = INV_START_X + maxC * INV_SPACING_X + (int)invBlockX + INV_W;
    if (leftX <= 4 || rightX >= SCREEN_W - 4) {
      invBlockDirection = -invBlockDirection;
      invBlockY += 10;
    }
  }

  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) invDrawEnemy(r, c, GREEN);

  // enemy return fire
  if (!invEnemyBulletActive && millis() - invLastEnemyShot > 1200) {
    for (int tries = 0; tries < 10; tries++) {
      int c = random(0, INV_COLS);
      int bottomR = -1;
      for (int r = 0; r < INV_ROWS; r++) if (invaders[r][c]) bottomR = r;
      if (bottomR != -1) {
        invEnemyBulletActive = true;
        invEnemyBulletX = INV_START_X + c * INV_SPACING_X + (int)invBlockX + INV_W / 2;
        invEnemyBulletY = INV_START_Y + bottomR * INV_SPACING_Y + invBlockY + INV_H;
        invLastEnemyShot = millis();
        break;
      }
    }
  }

  if (invEnemyBulletActive) {
    tft.fillRect((int)invEnemyBulletX, (int)invEnemyBulletY, 2, 6, BLACK);
    invEnemyBulletY += 4;
    if (invEnemyBulletY > SCREEN_H) {
      invEnemyBulletActive = false;
    } else {
      if (invEnemyBulletX >= invPlayerX && invEnemyBulletX <= invPlayerX + INV_PLAYER_W &&
          invEnemyBulletY >= INV_PLAYER_Y && invEnemyBulletY <= INV_PLAYER_Y + INV_PLAYER_H) {
        invGameOver = true;
        invShowGameOver(false);
        return;
      }
      tft.fillRect((int)invEnemyBulletX, (int)invEnemyBulletY, 2, 6, RED);
    }
  }

  int lowestY = 0;
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) {
        int ey = INV_START_Y + r * INV_SPACING_Y + invBlockY + INV_H;
        if (ey > lowestY) lowestY = ey;
      }
  if (lowestY >= INV_PLAYER_Y) {
    invGameOver = true;
    invShowGameOver(false);
    return;
  }

  if (invAliveCount <= 0) {
    invGameOver = true;
    invShowGameOver(true);
    return;
  }

  tft.fillRect(0, 0, 90, 10, BLACK);
  tft.setTextColor(WHITE); tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Score: "); tft.print(invScore);
}

// ================= FLAPPY BIRD =================
float birdY, birdVY;
const int BIRD_X = 60;
const int BIRD_R = 6;
const float GRAVITY = 0.28;
const float JUMP_V = -5.0;

struct Pipe { float x; int gapY; bool passed; };
const int NUM_PIPES = 3;
Pipe pipes[NUM_PIPES];
const int PIPE_W = 24;
const int GAP_H = 85;
const float PIPE_SPEED = 1.8;
int flappyScore;
bool flappyGameOver;
bool flappyStarted;
unsigned long flappyLastFrame;
const int FLAPPY_FRAME_MS = 25; // ~40 fps

void flappyReset() {
  birdY = 120; birdVY = 0;
  for (int i = 0; i < NUM_PIPES; i++) {
    pipes[i].x = SCREEN_W + i * 130;
    pipes[i].gapY = random(30, SCREEN_H - 30 - GAP_H);
    pipes[i].passed = false;
  }
  flappyScore = 0;
  flappyGameOver = false;
  flappyStarted = false;
  flappyLastFrame = millis();
  tft.fillScreen(BLACK);
  tft.fillCircle(BIRD_X, (int)birdY, BIRD_R, YELLOW);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.setCursor(90, 110);
  tft.print("Press A to start");
}

void flappyShowGameOver() {
  tft.fillScreen(BLACK);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(60, 90);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(90, 120);
  tft.print("Score: "); tft.print(flappyScore);
  tft.setCursor(40, 150);
  tft.print("A: Retry    B: Menu");
}

void loopFlappy() {
  if (btnB.pressed) { currentState = MENU; drawMenu(); return; }
  if (flappyGameOver) {
    if (btnA.pressed) flappyReset();
    return;
  }
  if (!flappyStarted) {
    if (btnA.pressed) {
      flappyStarted = true;
      birdVY = JUMP_V;
      tft.fillScreen(BLACK);
    }
    return;
  }

  if (btnA.pressed) birdVY = JUMP_V;

  if (millis() - flappyLastFrame < FLAPPY_FRAME_MS) return;
  flappyLastFrame = millis();

  tft.fillCircle(BIRD_X, (int)birdY, BIRD_R + 1, BLACK);

  birdVY += GRAVITY;
  birdY += birdVY;
  if (birdY < 0) birdY = 0;
  if (birdY > SCREEN_H) {
    flappyGameOver = true;
    flappyShowGameOver();
    return;
  }

  for (int i = 0; i < NUM_PIPES; i++) {
    tft.fillRect((int)pipes[i].x, 0, PIPE_W, pipes[i].gapY, BLACK);
    tft.fillRect((int)pipes[i].x, pipes[i].gapY + GAP_H, PIPE_W, SCREEN_H - (pipes[i].gapY + GAP_H), BLACK);

    pipes[i].x -= PIPE_SPEED;
    if (pipes[i].x + PIPE_W < 0) {
      pipes[i].x = SCREEN_W;
      pipes[i].gapY = random(30, SCREEN_H - 30 - GAP_H);
      pipes[i].passed = false;
    }
    if (!pipes[i].passed && pipes[i].x + PIPE_W < BIRD_X) {
      pipes[i].passed = true;
      flappyScore++;
    }
    if (BIRD_X + BIRD_R > pipes[i].x && BIRD_X - BIRD_R < pipes[i].x + PIPE_W) {
      if (birdY - BIRD_R < pipes[i].gapY || birdY + BIRD_R > pipes[i].gapY + GAP_H) {
        flappyGameOver = true;
        flappyShowGameOver();
        return;
      }
    }
    tft.fillRect((int)pipes[i].x, 0, PIPE_W, pipes[i].gapY, GREEN);
    tft.fillRect((int)pipes[i].x, pipes[i].gapY + GAP_H, PIPE_W, SCREEN_H - (pipes[i].gapY + GAP_H), GREEN);
  }

  tft.fillCircle(BIRD_X, (int)birdY, BIRD_R, YELLOW);
  tft.fillRect(0, 0, 90, 10, BLACK);
  tft.setTextColor(WHITE); tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.print("Score: "); tft.print(flappyScore);
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  randomSeed(analogRead(34)); // use a floating/unused ADC pin for entropy

  tft.begin();
  tft.setRotation(1); // landscape, 320x240
  tft.fillScreen(BLACK);

  drawMenu();
}

void loop() {
  updateAllButtons();

  switch (currentState) {
    case MENU:          loopMenu();      break;
    case SNAKE_GAME:    loopSnake();     break;
    case BREAKOUT_GAME: loopBreakout();  break;
    case TETRIS_GAME:   loopTetris();    break;
    case PONG_GAME:     loopPong();      break;
    case INVADERS_GAME: loopInvaders();  break;
    case FLAPPY_GAME:   loopFlappy();    break;
  }
}
