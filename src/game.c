/* Connect Four: the menus, the board on screen, turns and the controls.
 *
 *   Wheel     move the disc over the columns (and the highlight in menus)
 *   Centre    drop the disc (pick, in menus)
 *   Touch     put a finger on a column to aim, lift to drop; tap menu items
 *   Return    the pause menu (back, in menus); held, Q2 Pod ends the game */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c4.h"

/* ---------------------------------------------------------------------------------------------
 * Layout: the board sits low and in the middle, clear of the glass's rounded corners */

#define CELL 38
#define PAD 5
#define BW (COLS * CELL + 2 * PAD)
#define BH (ROWS * CELL + 2 * PAD)
#define BX ((VIEW_W - BW) / 2)
#define BY (VIEW_H - BH - 8)
#define HOLE_R 15.5f
#define PIECE_R 16.5f
#define HOVER_Y (BY - 23)
#define STEP 25 /* wheel travel a column: 8 a turn */

static int col_x(int c) { return BX + PAD + CELL / 2 + CELL * c; }
static int row_y(int r) { return BY + PAD + CELL / 2 + CELL * (ROWS - 1 - r); }

/* colours */
#define C_BG_TOP 0x1d2c52
#define C_BG_BOT 0x0b1126
#define C_BOARD 0x2160dd
#define C_BOARD_LO 0x1546b0
#define C_TEXT 0xf2f4fa
#define C_DIM 0x8f9bbd
#define C_ACCENT 0x5b8cff
static const uint32_t piece_base[3] = { 0, 0xe63b36, 0xf6c22c };
static const uint32_t piece_light[3] = { 0, 0xff8a78, 0xfff0a0 };
static const uint32_t piece_dark[3] = { 0, 0x9c1717, 0xb87b06 };

/* ---------------------------------------------------------------------------------------------
 * State */

enum { S_TITLE, S_PLAY, S_PAUSE, S_OVER };
enum { VS_AI, FRIEND };
static const char *level_name[LEVELS] = { "Easy", "Medium", "Hard" };

static struct {
    int state, mode, level;
    Board b;
    int turn, starter;       /* whose move; who began this game */
    int hist[ROWS * COLS], n_hist; /* the columns played, for Take Back */
    int target;              /* the column the disc hangs over */
    float hover_x;           /* where it is drawn */
    struct { int on, col, row, who; float y, vy; } fall;
    int ai_phase;            /* 0 not started, 1 to think, 2 moving over its column */
    int ai_col;
    uint64_t ai_t0;
    int winner;              /* P1, P2, or 3 a draw */
    int line[4][2];
    int score[4];            /* this sitting: P1 wins, P2 wins, -, draws */
    int sel;                 /* menu highlight */
    int wheel;               /* travel not yet turned into steps */
    int touching, touch_col;
    uint64_t over_t0;
    int dirty;
} g;

static struct { int level, mode, rec[LEVELS][3]; } save = { MEDIUM, VS_AI, { { 0 } } }; /* won, lost, drawn vs the computer */

static Img board_img, piece_img[3];
static uint32_t bg[VIEW_W * VIEW_H];

static int human(int who) { return g.mode == FRIEND || who == P1; }

/* ---------------------------------------------------------------------------------------------
 * The save file: connect4.sav */

static void save_load(void) {
    FILE *f = fopen("connect4.sav", "r");
    if (!f) return;
    char line[128];
    int a, b, c, d;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "level %d", &a) == 1 && a >= 0 && a < LEVELS) save.level = a;
        if (sscanf(line, "mode %d", &a) == 1 && (a == VS_AI || a == FRIEND)) save.mode = a;
        if (sscanf(line, "record %d %d %d %d", &a, &b, &c, &d) == 4 && a >= 0 && a < LEVELS)
            save.rec[a][0] = b, save.rec[a][1] = c, save.rec[a][2] = d;
    }
    fclose(f);
}

static void save_write(void) {
    FILE *f = fopen("connect4.tmp", "w");
    if (!f) return;
    fprintf(f, "level %d\nmode %d\n", save.level, save.mode);
    for (int l = 0; l < LEVELS; ++l) fprintf(f, "record %d %d %d %d\n", l, save.rec[l][0], save.rec[l][1], save.rec[l][2]);
    fclose(f);
    rename("connect4.tmp", "connect4.sav");
}

/* ---------------------------------------------------------------------------------------------
 * Pictures made once: the background, the board with its holes, the two discs */

static int cov(float d) { return d >= 0.5f ? 256 : d <= -0.5f ? 0 : (int)((d + 0.5f) * 256); }

static void make_pictures(void) {
    for (int y = 0; y < VIEW_H; ++y)
        for (int x = 0; x < VIEW_W; ++x) {
            float dx = (x - VIEW_W / 2) / (float)VIEW_W, dy = (y - 40) / (float)VIEW_H;
            int t = clampi((int)((dx * dx * 0.8f + dy * dy) * 300), 0, 256);
            bg[y * VIEW_W + x] = mix(C_BG_TOP, C_BG_BOT, t);
        }

    board_img = img_new(BW, BH);
    for (int y = 0; y < BH; ++y)
        for (int x = 0; x < BW; ++x) {
            float px = x + 0.5f, py = y + 0.5f;
            /* the rounded outline */
            const float R = 14;
            float cx = px < R ? R : px > BW - R ? BW - R : px, cy = py < R ? R : py > BH - R ? BH - R : py;
            float ex = px - cx, ey = py - cy, edge = R - __builtin_sqrtf(ex * ex + ey * ey);
            int a = cov(edge);
            uint32_t c = mix(0x2f74f0, C_BOARD, clampi(y * 256 / BH * 2, 0, 256));
            if (edge < 2.5f) c = mix(c, C_BOARD_LO, 140);
            /* the nearest hole */
            int col = clampi((int)((px - PAD) / CELL), 0, COLS - 1), row = clampi((int)((py - PAD) / CELL), 0, ROWS - 1);
            float hx = px - (PAD + CELL / 2.0f + CELL * col), hy = py - (PAD + CELL / 2.0f + CELL * row);
            float d = __builtin_sqrtf(hx * hx + hy * hy);
            int hole = cov(HOLE_R - d);
            if (d < HOLE_R + 3) { /* a bevel: shade above the hole, light below */
                float k = (HOLE_R + 3 - d) / 3;
                int t = (int)(k * 120);
                c = hy < 0 ? mix(c, C_BOARD_LO, t) : mix(c, 0x5d97ff, t * 2 / 3);
            }
            a = a * (256 - hole) >> 8;
            board_img.px[y * BW + x] = (uint32_t)(a > 255 ? 255 : a) << 24 | c;
        }

    for (int who = P1; who <= P2; ++who) {
        int S = (int)(PIECE_R * 2 + 2);
        Img im = img_new(S, S);
        float c0 = S / 2.0f;
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                float dx = x + 0.5f - c0, dy = y + 0.5f - c0, d = __builtin_sqrtf(dx * dx + dy * dy);
                int a = cov(PIECE_R - d);
                if (!a) continue;
                /* lit from the top left */
                float lx = dx + 5, ly = dy + 6, l = __builtin_sqrtf(lx * lx + ly * ly) / (PIECE_R * 1.5f);
                uint32_t c = mix(piece_light[who], piece_base[who], clampi((int)(l * 256), 0, 256));
                if (d > PIECE_R - 3) c = mix(c, piece_dark[who], (int)((d - (PIECE_R - 3)) / 3 * 200));
                /* the raised inner ring of a real disc */
                float rr = d - 11;
                if (rr > -1.2f && rr < 0) c = mix(c, piece_dark[who], 90);
                else if (rr >= 0 && rr < 1.2f) c = mix(c, piece_light[who], 70);
                im.px[y * S + x] = (uint32_t)(a > 255 ? 255 : a) << 24 | c;
            }
        piece_img[who] = im;
    }
}

static void piece(int who, float cx, float cy, int alpha) {
    const Img *im = &piece_img[who];
    img(im, (int)(cx - im->w / 2.0f + 0.5f), (int)(cy - im->h / 2.0f + 0.5f), alpha);
}

/* ---------------------------------------------------------------------------------------------
 * Games */

static void new_game(int starter) {
    board_clear(&g.b);
    g.starter = g.turn = starter;
    g.n_hist = 0;
    g.fall.on = 0;
    g.ai_phase = 0;
    g.winner = 0;
    g.target = COLS / 2;
    g.hover_x = (float)col_x(g.target);
    g.state = S_PLAY;
    g.touching = 0;
    g.dirty = 1;
}

static void start_sitting(int mode) {
    g.mode = save.mode = mode;
    g.level = save.level;
    memset(g.score, 0, sizeof g.score);
    save_write();
    new_game(P1);
}

static void drop(int col) {
    if (g.fall.on || g.winner || !board_can(&g.b, col)) return;
    int row = board_drop(&g.b, col, g.turn);
    g.hist[g.n_hist++] = col;
    g.fall.on = 1, g.fall.col = col, g.fall.row = row, g.fall.who = g.turn;
    g.fall.y = HOVER_Y, g.fall.vy = 120;
    g.hover_x = (float)col_x(col);
    g.target = col;
}

static void game_over(int winner) {
    g.winner = winner;
    g.score[winner]++;
    if (g.mode == VS_AI) {
        int r = winner == P1 ? 0 : winner == P2 ? 1 : 2;
        save.rec[g.level][r]++;
        save_write();
    }
    g.state = S_OVER;
    g.over_t0 = now_us();
    logf_("game over: %s after %d moves", winner == 3 ? "draw" : winner == P1 ? "P1 wins" : "P2 wins", g.b.moves);
}

/* the falling disc has landed */
static void landed(void) {
    int row = g.fall.row, col = g.fall.col;
    g.fall.on = 0;
    if (board_wins(&g.b, row, col, g.line)) game_over(g.turn);
    else if (board_full(&g.b)) game_over(3);
    else g.turn = 3 - g.turn, g.ai_phase = 0;
}

static int can_take_back(void) {
    if (g.mode == FRIEND) return g.n_hist > 0;
    for (int i = 0; i < g.n_hist; ++i) /* a move of the player's own to take back */
        if (((g.starter == P1) ^ (i % 2)) != 0) return 1;
    return 0;
}

static void take_back(void) {
    if (g.fall.on) landed();
    if (g.winner) return;
    do {
        board_undo(&g.b, g.hist[--g.n_hist]);
        g.turn = 3 - g.turn;
    } while (g.n_hist && !human(g.turn));
    g.ai_phase = 0;
    if (g.n_hist) g.target = g.hist[g.n_hist - 1];
}

/* ---------------------------------------------------------------------------------------------
 * Menus */

enum { T_AI, T_FRIEND, T_LEVEL, T_N };
enum { P_RESUME, P_UNDO, P_RESTART, P_MENU, P_N };
#define MENU_W 230
#define ITEM_H 36

static int title_item_y(int i) { return 150 + i * (ITEM_H + 8); }
static int pause_item_y(int i) { return 92 + i * (ITEM_H + 6); }

static void title_pick(int i) {
    if (i == T_AI) start_sitting(VS_AI);
    else if (i == T_FRIEND) start_sitting(FRIEND);
    else save.level = (save.level + 1) % LEVELS, save_write();
    g.dirty = 1;
}

static void pause_pick(int i) {
    if (i == P_RESUME) g.state = S_PLAY;
    else if (i == P_UNDO) {
        if (!can_take_back()) return;
        take_back();
        g.state = S_PLAY;
    } else if (i == P_RESTART) new_game(g.starter);
    else g.state = S_TITLE, g.sel = T_AI;
    g.dirty = 1;
}

/* ---------------------------------------------------------------------------------------------
 * Drawing */

static const char *name_of(int who) {
    if (g.mode == VS_AI) return who == P1 ? "YOU" : "AI";
    return who == P1 ? "P1" : "P2";
}

static void draw_menu_item(int y, const char *label, int selected, int enabled) {
    int x = (VIEW_W - MENU_W) / 2;
    if (selected) rrect(x, y, MENU_W, ITEM_H, ITEM_H / 2, C_ACCENT, 256);
    else rrect(x, y, MENU_W, ITEM_H, ITEM_H / 2, 0xffffff, 22);
    text_c(&font_m, label, VIEW_W / 2, y + (ITEM_H - font_m.h) / 2 + 1, enabled ? C_TEXT : C_DIM, enabled ? 256 : 160);
}

static void draw_title(void) {
    memcpy(fb, bg, sizeof fb);
    text_c(&font_l, "Connect Four", VIEW_W / 2, 30, C_TEXT, 256);
    for (int i = 0; i < 4; ++i) piece(i % 2 ? P2 : P1, VIEW_W / 2 - 57 + 38 * i, 112, 256);
    char lv[48];
    snprintf(lv, sizeof lv, "AI Level:  %s", level_name[save.level]);
    const char *items[T_N] = { "Play vs AI", "Play with a Friend", lv };
    for (int i = 0; i < T_N; ++i) draw_menu_item(title_item_y(i), items[i], g.sel == i, 1);
    int *r = save.rec[save.level];
    if (r[0] + r[1] + r[2]) {
        char rec[80];
        snprintf(rec, sizeof rec, "vs %s AI:  %d won   %d lost   %d drawn", level_name[save.level], r[0], r[1], r[2]);
        text_c(&font_s, rec, VIEW_W / 2, title_item_y(T_N) + 2, C_DIM, 256);
    }
}

static void draw_side(int who, int cx) {
    int active = g.state != S_OVER && g.turn == who;
    if (active) disc((float)cx, 128, 21, C_TEXT, 60), ring((float)cx, 128, 21, 2, C_TEXT, 230);
    piece(who, (float)cx, 128, 256);
    text_c(&font_s, name_of(who), cx, 154, active ? C_TEXT : C_DIM, 256);
    char n[12];
    snprintf(n, sizeof n, "%d", g.score[who]);
    text_c(&font_m, n, cx, 174, C_TEXT, 256);
}

static void draw_play(void) {
    memcpy(fb, bg, sizeof fb);
    uint64_t t = now_us();
    /* the discs in the board (the falling one where it is) */
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c) {
            int who = g.b.cell[r][c];
            if (!who || (g.fall.on && g.fall.row == r && g.fall.col == c)) continue;
            piece(who, (float)col_x(c), (float)row_y(r), 256);
        }
    int my_turn = g.state == S_PLAY && !g.fall.on && !g.winner && human(g.turn);
    if (my_turn && board_can(&g.b, g.target)) /* where it would land: an outline */
        ring((float)col_x(g.target), (float)row_y(g.b.height[g.target]), HOLE_R - 2, 2.5f, piece_base[g.turn], 200);
    if (g.fall.on) piece(g.fall.who, (float)col_x(g.fall.col), g.fall.y, 256);
    img(&board_img, BX, BY, 256);

    if (g.winner && g.winner != 3) { /* the four, ringed and pulsing */
        int phase = (int)((t - g.over_t0) / 1000 % 1000);
        int a = 140 + (phase < 500 ? phase : 1000 - phase) * 116 / 500;
        for (int k = 0; k < 4; ++k) ring((float)col_x(g.line[k][1]), (float)row_y(g.line[k][0]), PIECE_R - 1, 3.5f, 0xffffff, a);
    }

    draw_side(P1, 25);
    draw_side(P2, VIEW_W - 25);

    if (g.state == S_OVER) {
        char msg[40];
        if (g.winner == 3) snprintf(msg, sizeof msg, "It's a draw");
        else if (g.mode == VS_AI) snprintf(msg, sizeof msg, g.winner == P1 ? "You win!" : "The AI wins");
        else snprintf(msg, sizeof msg, "%s wins!", g.winner == P1 ? "Red" : "Yellow");
        text_c(&font_l, msg, VIEW_W / 2, 2, g.winner == 3 ? C_TEXT : piece_light[g.winner], 256);
        text_c(&font_s, "Centre: play again     Return: menu", VIEW_W / 2, 48, C_DIM, 256);
        return;
    }
    /* whose turn, and the disc waiting to drop */
    char msg[40];
    if (g.mode == VS_AI) snprintf(msg, sizeof msg, g.turn == P1 ? "Your turn" : "AI is thinking...");
    else snprintf(msg, sizeof msg, "%s's turn", g.turn == P1 ? "Red" : "Yellow");
    text_c(&font_s, msg, VIEW_W / 2, 6, g.mode == VS_AI && g.turn == P1 ? C_TEXT : piece_light[g.turn], 256);
    if (!g.fall.on) piece(g.turn, g.hover_x, HOVER_Y, 256);
}

static void draw_pause(void) {
    draw_play();
    darken(170);
    rrect((VIEW_W - MENU_W - 30) / 2, 40, MENU_W + 30, 240, 22, 0x17223f, 245);
    text_c(&font_m, "Paused", VIEW_W / 2, 56, C_TEXT, 256);
    const char *items[P_N] = { "Resume", "Take Back", "New Game", "Main Menu" };
    for (int i = 0; i < P_N; ++i) draw_menu_item(pause_item_y(i), items[i], g.sel == i, i != P_UNDO || can_take_back());
}

static void draw(void) {
    if (g.state == S_TITLE) draw_title();
    else if (g.state == S_PAUSE) draw_pause();
    else draw_play();
    present(fb);
}

/* ---------------------------------------------------------------------------------------------
 * Input */

static int col_at(int x) { return clampi((x - BX - PAD) / CELL, 0, COLS - 1); }

static int menu_hit(int y, int n, int (*item_y)(int)) {
    for (int i = 0; i < n; ++i)
        if (y >= item_y(i) - 3 && y < item_y(i) + ITEM_H + 3) return i;
    return -1;
}

static void on_wheel(int steps) {
    if (!steps) return;
    g.dirty = 1;
    if (g.state == S_TITLE) g.sel = clampi(g.sel - steps, 0, T_N - 1);
    else if (g.state == S_PAUSE) g.sel = clampi(g.sel - steps, 0, P_N - 1);
    else if (g.state == S_PLAY && human(g.turn) && !g.fall.on) g.target = clampi(g.target - steps, 0, COLS - 1);
}

static void on_key(int code) {
    g.dirty = 1;
    g.wheel = 0; /* half a step of travel left over shouldn't carry into the next screen */
    if (code == K_CENTRE) {
        if (g.state == S_TITLE) title_pick(g.sel);
        else if (g.state == S_PAUSE) pause_pick(g.sel);
        else if (g.state == S_OVER) new_game(3 - g.starter);
        else if (human(g.turn)) drop(g.target);
    } else if (code == K_RETURN || code == K_PLAY) {
        if (g.state == S_PLAY) g.state = S_PAUSE, g.sel = P_RESUME;
        else if (g.state == S_PAUSE) g.state = S_PLAY;
        else if (g.state == S_OVER) g.state = S_TITLE, g.sel = T_AI;
    } else if (code == K_NEXT || code == K_PREV) {
        on_wheel(code == K_NEXT ? -1 : 1);
    }
}

static void on_touch(int what, int x, int y) {
    g.dirty = 1;
    g.wheel = 0;
    int inside = x >= BX - 10 && x < BX + BW + 10;
    if (g.state == S_PLAY && human(g.turn) && !g.fall.on) {
        if (what == 'd') g.touching = inside;
        if (!g.touching) return;
        g.target = col_at(x);
        if (what == 'u') {
            g.touching = 0;
            if (inside) drop(g.target);
        }
        return;
    }
    if (what != 'u') return;
    if (g.state == S_TITLE) {
        int i = menu_hit(y, T_N, title_item_y);
        if (i >= 0) g.sel = i, title_pick(i);
    } else if (g.state == S_PAUSE) {
        int i = menu_hit(y, P_N, pause_item_y);
        if (i >= 0) g.sel = i, pause_pick(i);
    } else if (g.state == S_OVER) new_game(3 - g.starter);
}

/* ---------------------------------------------------------------------------------------------
 * Each frame */

static void update(float dt) {
    if (g.state != S_PLAY) return;
    if (g.fall.on) {
        float to = (float)row_y(g.fall.row);
        g.fall.vy += 2600 * dt;
        g.fall.y += g.fall.vy * dt;
        if (g.fall.y >= to) {
            g.fall.y = to;
            if (g.fall.vy > 260) g.fall.vy = -g.fall.vy * 0.28f; /* a little bounce */
            else landed();
        }
        g.dirty = 1;
        return;
    }
    if (!human(g.turn)) {
        uint64_t t = now_us();
        if (g.ai_phase == 0) { /* show "thinking" first */
            g.ai_phase = 1, g.ai_t0 = t, g.dirty = 1;
            return;
        }
        if (g.ai_phase == 1) {
            g.ai_col = ai_move(&g.b, g.turn, g.level);
            logf_("AI (%s) plays %d after %llu ms, %ld positions", level_name[g.level], g.ai_col + 1,
                  (unsigned long long)((now_us() - t) / 1000), ai_nodes());
            g.ai_phase = 2;
            return;
        }
        /* glide over to its column, then let go */
        float to = (float)col_x(g.ai_col), step = 420 * dt;
        if (g.hover_x < to) g.hover_x = g.hover_x + step > to ? to : g.hover_x + step;
        else if (g.hover_x > to) g.hover_x = g.hover_x - step < to ? to : g.hover_x - step;
        g.dirty = 1;
        if (g.hover_x == to && t - g.ai_t0 > 650000) drop(g.ai_col);
        return;
    }
    float to = (float)col_x(g.target), d = to - g.hover_x;
    if (d > 0.5f || d < -0.5f) {
        float k = dt * 22;
        g.hover_x += d * (k > 1 ? 1 : k);
        g.dirty = 1;
    } else if (g.hover_x != to) g.hover_x = to, g.dirty = 1;
}

static void write_state(void) {
    FILE *f = fopen("state.tmp", "w");
    if (!f) return;
    fprintf(f, "state %d\nmode %d\nlevel %d\nturn %d\nmoves %d\nwinner %d\ntarget %d\nfalling %d\nsel %d\n"
               "p1 %d\np2 %d\ndraws %d\nstarter %d\nsave_level %d\n",
            g.state, g.mode, g.level, g.turn, g.b.moves, g.winner, g.target, g.fall.on, g.sel, g.score[P1],
            g.score[P2], g.score[3], g.starter, save.level);
    fclose(f);
    rename("state.tmp", "state.txt");
}

int game_main(void) {
    const char *seed = getenv("C4_SEED");
    rnd_seed(seed ? (unsigned)atoi(seed) : (unsigned)now_us());
    save_load();
    make_pictures();
    g.state = S_TITLE;
    g.sel = save.mode == FRIEND ? T_FRIEND : T_AI;
    g.dirty = 1;
    int tests = getenv("C4_STATE") != 0;
    uint64_t last = now_us();
    static uint8_t held[256]; /* Q2 Pod repeats a held key as more downs: act on the first */
    while (!quit_asked()) {
        int kind, a, b, c;
        while (next_event(&kind, &a, &b, &c)) {
            if (kind == 'k') {
                int was = b >= 0 && b < 256 ? held[b] : 0;
                if (b >= 0 && b < 256) held[b] = (uint8_t)a;
                if (a && !was) on_key(b);
            }
            else if (kind == 't') on_touch(a, b, c);
            else if (kind == 'w') {
                g.wheel += a;
                int steps = g.wheel / STEP;
                g.wheel -= steps * STEP;
                on_wheel(steps);
            }
        }
        uint64_t t = now_us();
        float dt = (float)(unsigned)(t - last) / 1e6f; /* 32-bit: no libgcc for 64-bit floats */
        last = t;
        update(dt > 0.05f ? 0.05f : dt);
        if (g.state == S_OVER && g.winner != 3) g.dirty = 1; /* the pulsing four */
        if (g.dirty) {
            g.dirty = 0;
            draw();
            if (tests) write_state();
        }
        uint64_t spent = now_us() - t;
        if (spent < 16000) sleep_us((unsigned)(16000 - spent));
    }
    return 0;
}
