/* The board and the computer player.
 *
 * The computer looks ahead with negamax and alpha-beta pruning, scoring the positions at the end
 * of its look-ahead by the four-in-a-row "windows" each side still has open. The levels differ in
 * how far it looks and how often it lets a good move go:
 *
 *   Easy    one move: takes a win most of the time, blocks yours about half the time, otherwise
 *           plays a random column (leaning to the middle).
 *   Medium  four moves ahead: always takes a win and blocks, but about one move in three it
 *           plays any column that doesn't lose at once, instead of the best one.
 *   Hard    six moves ahead, every move its best. Strong, but it can still be beaten. */
#include "c4.h"

void board_clear(Board *b) {
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c) b->cell[r][c] = EMPTY;
    for (int c = 0; c < COLS; ++c) b->height[c] = 0;
    b->moves = 0;
}

int board_can(const Board *b, int col) { return col >= 0 && col < COLS && b->height[col] < ROWS; }

int board_drop(Board *b, int col, int who) {
    if (!board_can(b, col)) return -1;
    int row = b->height[col]++;
    b->cell[row][col] = (uint8_t)who;
    b->moves++;
    return row;
}

void board_undo(Board *b, int col) {
    if (col < 0 || col >= COLS || !b->height[col]) return;
    b->cell[--b->height[col]][col] = EMPTY;
    b->moves--;
}

int board_full(const Board *b) { return b->moves >= ROWS * COLS; }

int board_wins(const Board *b, int row, int col, int line[4][2]) {
    static const int dir[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } }; /* (dr, dc) */
    int who = b->cell[row][col];
    if (!who) return 0;
    for (int d = 0; d < 4; ++d) {
        int dr = dir[d][0], dc = dir[d][1];
        /* walk back to the first of the run, then count forwards */
        int r0 = row, c0 = col;
        while (r0 - dr >= 0 && r0 - dr < ROWS && c0 - dc >= 0 && c0 - dc < COLS && b->cell[r0 - dr][c0 - dc] == who)
            r0 -= dr, c0 -= dc;
        int n = 0, r = r0, c = c0;
        while (r >= 0 && r < ROWS && c >= 0 && c < COLS && b->cell[r][c] == who) ++n, r += dr, c += dc;
        if (n >= 4) {
            if (line) {
                /* the four that include the new piece, from the start of the run */
                int k, along = dr ? (row - r0) / dr : (col - c0) / dc; /* the new piece's place in the run */
                int first = along - 3 < 0 ? 0 : along - 3;
                for (k = 0; k < 4; ++k) line[k][0] = r0 + dr * (first + k), line[k][1] = c0 + dc * (first + k);
            }
            return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * Random numbers: xorshift32 */

static unsigned seed = 0x9e3779b9u;
void rnd_seed(unsigned s) { seed = s ? s : 0x9e3779b9u; }
unsigned rnd(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
static int chance(int percent) { return (int)(rnd() % 100) < percent; }

/* ---------------------------------------------------------------------------------------------
 * Look-ahead */

#define WIN 1000000
#define INF 0x3fffffff
static const int order[COLS] = { 3, 2, 4, 1, 5, 0, 6 }; /* the middle first: better cuts */
static const int centre_weight[COLS] = { 1, 2, 3, 5, 3, 2, 1 };

static int windows[69][4][2]; /* every line of four on the board */
static int n_windows;

static void windows_init(void) {
    if (n_windows) return;
    static const int dir[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c)
            for (int d = 0; d < 4; ++d) {
                int er = r + 3 * dir[d][0], ec = c + 3 * dir[d][1];
                if (er < 0 || er >= ROWS || ec < 0 || ec >= COLS) continue;
                for (int k = 0; k < 4; ++k) windows[n_windows][k][0] = r + k * dir[d][0], windows[n_windows][k][1] = c + k * dir[d][1];
                ++n_windows;
            }
}

/* the position as it looks to `who` */
static int evaluate(const Board *b, int who) {
    int other = 3 - who, score = 0;
    for (int w = 0; w < n_windows; ++w) {
        int mine = 0, theirs = 0;
        for (int k = 0; k < 4; ++k) {
            int v = b->cell[windows[w][k][0]][windows[w][k][1]];
            mine += v == who, theirs += v == other;
        }
        if (mine && theirs) continue;
        if (mine == 3) score += 6;
        else if (mine == 2) score += 2;
        else if (theirs == 3) score -= 7;
        else if (theirs == 2) score -= 2;
    }
    for (int r = 0; r < ROWS; ++r) {
        int v = b->cell[r][3];
        score += v == who ? 3 : v == other ? -3 : 0;
    }
    return score;
}

static long nodes;

/* the best `who` can do with `depth` moves to look at (theirs and the other side's) */
static int negamax(Board *b, int depth, int alpha, int beta, int who) {
    int best = -INF, any = 0;
    ++nodes;
    for (int i = 0; i < COLS; ++i) {
        int col = order[i];
        if (!board_can(b, col)) continue;
        any = 1;
        int row = board_drop(b, col, who), score;
        if (board_wins(b, row, col, 0)) score = WIN + depth; /* sooner is better */
        else if (board_full(b)) score = 0;
        else if (depth <= 1) score = evaluate(b, who);
        else score = -negamax(b, depth - 1, -beta, -alpha, 3 - who);
        board_undo(b, col);
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return any ? best : 0;
}

/* each legal column's score for `who`, looking `depth` moves ahead (illegal: -INF) */
static void score_moves(Board *b, int who, int depth, int score[COLS]) {
    for (int col = 0; col < COLS; ++col) {
        score[col] = -INF;
        if (!board_can(b, col)) continue;
        int row = board_drop(b, col, who);
        if (board_wins(b, row, col, 0)) score[col] = WIN + depth;
        else if (board_full(b)) score[col] = 0;
        else if (depth <= 1) score[col] = evaluate(b, who);
        else score[col] = -negamax(b, depth - 1, -INF, INF, 3 - who);
        board_undo(b, col);
    }
}

/* a random legal column, the middle ones likelier; only from `allowed` if given */
static int random_move(const Board *b, const int *allowed) {
    int total = 0;
    for (int c = 0; c < COLS; ++c)
        if (board_can(b, c) && (!allowed || allowed[c])) total += centre_weight[c];
    if (!total) return -1;
    int pick = (int)(rnd() % (unsigned)total);
    for (int c = 0; c < COLS; ++c)
        if (board_can(b, c) && (!allowed || allowed[c])) {
            if (pick < centre_weight[c]) return c;
            pick -= centre_weight[c];
        }
    return -1;
}

/* the column where `who` would win at once, or -1 */
static int winning_move(Board *b, int who) {
    for (int c = 0; c < COLS; ++c) {
        if (!board_can(b, c)) continue;
        int row = board_drop(b, c, who), won = board_wins(b, row, c, 0);
        board_undo(b, c);
        if (won) return c;
    }
    return -1;
}

static int best_of(const int score[COLS]) {
    int best = -INF, n = 0, pick[COLS];
    for (int c = 0; c < COLS; ++c)
        if (score[c] > best) best = score[c], n = 0, pick[n++] = c;
        else if (score[c] == best && best > -INF) pick[n++] = c;
    return n ? pick[rnd() % (unsigned)n] : -1;
}

int ai_move(Board *b, int who, int level) {
    windows_init();
    nodes = 0;
    if (level == EASY) {
        int c = winning_move(b, who);
        if (c >= 0 && chance(85)) return c;
        c = winning_move(b, 3 - who);
        if (c >= 0 && chance(55)) return c;
        return random_move(b, 0);
    }
    int score[COLS];
    if (level == MEDIUM) {
        score_moves(b, who, 4, score);
        int best = best_of(score);
        if (best < 0 || score[best] >= WIN / 2) return best; /* a win in sight: take it */
        if (chance(33)) {
            /* a slip: any column that doesn't hand over a win the next move */
            int ok[COLS], any = 0;
            for (int c = 0; c < COLS; ++c) ok[c] = score[c] > -WIN / 2 && score[c] > -INF, any |= ok[c];
            int c = any ? random_move(b, ok) : -1;
            if (c >= 0) return c;
        }
        return best;
    }
    score_moves(b, who, 6, score);
    return best_of(score);
}

long ai_nodes(void) { return nodes; }
