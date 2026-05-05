/* E1 — MESI step-machine for two cores + false-sharing layout check.
 *
 * Implement (see ../EXERCISES.md):
 *   typedef enum { I=0, S, E, M } mesi_t;
 *   typedef struct { mesi_t st; int v; } cline_t;
 *   void rd(int who);            // 0 or 1
 *   void wr(int who, int v);
 *
 * Print state after each step: "C0:S(7) C1:I(0) mem:7" form.
 *
 * Layout check:
 *   struct bad  { int a; int b; };
 *   struct good { int a; char pad[60]; int b; };
 *   show whether &a and &b share a 64B line.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { I = 0, S, E, M } mesi_t;
typedef struct { mesi_t st; int v; } cline_t;

static cline_t c0, c1;
static int mem = 100;

/* TODO: rd, wr, show */

int main(void) {
    /* TODO: drive 5-step sequence, then layout check */
    return 0;
}
