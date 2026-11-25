/* Host tests of the rules and the computer player: logic_test */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "c4.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* plays a string of columns ('1'-'7'), alternating P1 and P2; the last row played */
static int play(Board *b, const char *moves, int *last_col) {
    int row = -1;
    for (int i = 0; moves[i]; ++i) row = board_drop(b, moves[i] - '1', i % 2 ? P2 : P1), *last_col = moves[i] - '1';
    return row;
}

static int wins_after(const char *moves) {
    Board b; board_clear(&b);
    int col, row = play(&b, moves, &col), line[4][2];
    return board_wins(&b, row, col, line);
}

static double secs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

/* one game between two levels (0-2) or a random player (-1); the winner, 0 a draw */
static int match(int l1, int l2) {
    Board b; board_clear(&b);
    for (int who = P1;; who = 3 - who) {
        int lv = who == P1 ? l1 : l2, col;
        if (lv < 0) do col = rnd() % COLS; while (!board_can(&b, col));
        else col = ai_move(&b, who, lv);
        int row = board_drop(&b, col, who);
        if (row < 0) return -9;
        if (board_wins(&b, row, col, 0)) return who;
        if (board_full(&b)) return 0;
    }
}

int main(void) {
    rnd_seed(12345);
    /* wins */
    CHECK(wins_after("1121314"), "horizontal");
    CHECK(!wins_after("112131"), "three isn't four");
    CHECK(wins_after("1212121"), "vertical");
    CHECK(wins_after("12233434454"), "diagonal up");
    CHECK(wins_after("76655454434"), "diagonal down");
    CHECK(!wins_after("1234567"), "no win");
    { /* the four given back include the new piece, in a five-run filled in the middle */
        Board b; board_clear(&b);
        int col, line[4][2];
        play(&b, "1122446655", &col);
        int row = board_drop(&b, 2, P1);
        CHECK(board_wins(&b, row, 2, line), "five-run");
        int has = 0;
        for (int k = 0; k < 4; ++k) {
            has |= line[k][0] == 0 && line[k][1] == 2;
            CHECK(b.cell[line[k][0]][line[k][1]] == P1, "line on P1");
        }
        CHECK(has, "line has the new piece");
    }
    { /* full column */
        Board b; board_clear(&b);
        for (int i = 0; i < ROWS; ++i) board_drop(&b, 0, 1 + i % 2);
        CHECK(!board_can(&b, 0) && board_drop(&b, 0, P1) < 0, "full column");
        board_undo(&b, 0);
        CHECK(board_can(&b, 0) && b.moves == 5, "undo");
    }
    /* the computer takes a win and blocks one (Medium and Hard always) */
    for (int lv = MEDIUM; lv <= HARD; ++lv) {
        Board b; board_clear(&b); int col;
        play(&b, "112233", &col); /* P1 to play: 4 wins */
        CHECK(ai_move(&b, P1, lv) == 3, "level %d takes the win", lv);
        play(&b, "7", &col); /* now P2 to move must block at 4 */
        CHECK(ai_move(&b, P2, lv) == 3, "level %d blocks", lv);
    }
    { /* Hard sees the open-ended three: P1 with 2 and 3 in the bottom row, 1-4 and 5 empty */
        Board b; board_clear(&b); int col;
        play(&b, "2737", &col); /* P1: cols 2,3; P2: 7,7 */
        int c = ai_move(&b, P1, HARD);
        CHECK(c == 3 || c == 0, "hard makes the double threat (played %d)", c + 1);
    }
    /* strengths */
    int hard_v_rand = 0, med_v_rand = 0, easy_v_rand = 0, hard_v_easy = 0, hard_v_med = 0, med_v_easy = 0;
    double t0 = secs();
    for (int i = 0; i < 40; ++i) {
        hard_v_rand += match(i % 2 ? -1 : HARD, i % 2 ? HARD : -1) == (i % 2 ? P2 : P1);
        med_v_rand += match(i % 2 ? -1 : MEDIUM, i % 2 ? MEDIUM : -1) == (i % 2 ? P2 : P1);
        easy_v_rand += match(i % 2 ? -1 : EASY, i % 2 ? EASY : -1) == (i % 2 ? P2 : P1);
        hard_v_easy += match(i % 2 ? EASY : HARD, i % 2 ? HARD : EASY) == (i % 2 ? P2 : P1);
        hard_v_med += match(i % 2 ? MEDIUM : HARD, i % 2 ? HARD : MEDIUM) == (i % 2 ? P2 : P1);
        med_v_easy += match(i % 2 ? EASY : MEDIUM, i % 2 ? MEDIUM : EASY) == (i % 2 ? P2 : P1);
    }
    printf("wins of 40: hard-random %d, medium-random %d, easy-random %d, hard-easy %d, hard-medium %d, medium-easy %d (%.1f s)\n",
           hard_v_rand, med_v_rand, easy_v_rand, hard_v_easy, hard_v_med, med_v_easy, secs() - t0);
    CHECK(hard_v_rand >= 39 && med_v_rand >= 36, "strong levels beat random");
    CHECK(hard_v_easy >= 36 && hard_v_med >= 26 && med_v_easy >= 28, "levels in order");
    /* the slowest Hard move: the empty board and a few openings */
    double worst = 0; long worst_nodes = 0;
    const char *openings[] = { "", "4", "44", "434", "3443", "445566" };
    for (int i = 0; i < 6; ++i) {
        Board b; board_clear(&b); int col;
        play(&b, openings[i], &col);
        double t = secs();
        ai_move(&b, 1 + b.moves % 2, HARD);
        t = secs() - t;
        if (t > worst) worst = t, worst_nodes = ai_nodes();
    }
    printf("hard: slowest move %.0f ms here, %ld positions\n", worst * 1000, worst_nodes);
    printf(fails ? "logic: %d failed\n" : "logic: ok\n", fails);
    return fails != 0;
}
