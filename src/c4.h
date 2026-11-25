/* Connect Four for the Shanling Q2 under Q2 Pod.
 * Shared declarations for every file in src/. */
#ifndef C4_H
#define C4_H
#include <stdint.h>

#define VIEW_W 375 /* the UI's landscape view: the portrait panel turned */
#define VIEW_H 320
#define CORNER 80 /* the glass's rounded corners, px */

/* Q2 Pod's AWTK key codes (without audio=1 only Return and Centre come to the game) */
#define K_RETURN 170
#define K_PLAY 171
#define K_CENTRE 218
#define K_PREV 222
#define K_NEXT 223

/* ---------------------------------------------------------------------------------------------
 * q2.c: the platform */
uint64_t now_us(void);
void sleep_us(unsigned us);
void present(const uint32_t *frame); /* a VIEW_W x VIEW_H XRGB frame onto the panel */
int quit_asked(void);
int next_event(int *kind, int *a, int *b, int *c); /* 'k' (a 1 down/0 up, b code), 't' (a 'd'/'m'/'u', b x, c y), 'w' (a travel) */
void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *app_dir(void);

/* ---------------------------------------------------------------------------------------------
 * logic.c: the board and the computer player (no platform calls: the host tests link it alone) */
#define COLS 7
#define ROWS 6
enum { EMPTY = 0, P1 = 1, P2 = 2 };
enum { EASY, MEDIUM, HARD, LEVELS };

typedef struct {
    uint8_t cell[ROWS][COLS]; /* row 0 is the bottom */
    uint8_t height[COLS];
    int moves;
} Board;

void board_clear(Board *b);
int board_can(const Board *b, int col);
int board_drop(Board *b, int col, int who); /* the row it lands in, or -1 */
void board_undo(Board *b, int col);
int board_full(const Board *b);
int board_wins(const Board *b, int row, int col, int line[4][2]); /* the piece at row, col makes four: its four (row, col) */

void rnd_seed(unsigned s);
unsigned rnd(void);
int ai_move(Board *b, int who, int level); /* the column; the board is left as it was */
long ai_nodes(void); /* positions the last ai_move looked at */

/* ---------------------------------------------------------------------------------------------
 * gfx.c: drawing into the frame */
extern uint32_t fb[VIEW_W * VIEW_H];

typedef struct {
    int w, h;
    uint32_t *px; /* ARGB, straight alpha */
} Img;

typedef struct {
    int h, base;                   /* line height, rows from the top to the baseline */
    const uint8_t *px;             /* grey levels, atlas_w wide */
    int atlas_w;
    short x[96], w[96], left[96], adv[96]; /* per glyph, from ' ' */
} Font;
extern const Font font_s, font_m, font_menu, font_l; /* baked in ITC Avant Garde Bold by tools/font.py */
extern const Img logo; /* the title, baked by tools/logo.py */

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void fill(uint32_t c);
void blend(int x, int y, uint32_t c, int alpha); /* alpha 0-256 */
void rect(int x, int y, int w, int h, uint32_t c, int alpha);
void rrect(int x, int y, int w, int h, int r, uint32_t c, int alpha); /* rounded, smooth corners */
void disc(float cx, float cy, float r, uint32_t c, int alpha);
void ring(float cx, float cy, float r, float width, uint32_t c, int alpha);
void img(const Img *i, int x, int y, int alpha);
void darken(int alpha);
int text_w(const Font *f, const char *s);
void text(const Font *f, const char *s, int x, int y, uint32_t c, int alpha); /* y: the line's top */
void text_c(const Font *f, const char *s, int cx, int y, uint32_t c, int alpha);
Img img_new(int w, int h);
uint32_t mix(uint32_t a, uint32_t b, int t); /* t 0-256 from a to b */

/* ---------------------------------------------------------------------------------------------
 * game.c */
int game_main(void);
#endif
