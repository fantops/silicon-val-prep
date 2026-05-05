/* E1 — Debug-log triage classifier.
 *
 * Implement (see ../EXERCISES.md):
 *   const char *class_name(class_t c);
 *   class_t triage(double repro_rate, int unique_units, int corner_dependent);
 *
 * Driver: print a table over the 5 sample reports listed in EXERCISES.md.
 */
#include <stdio.h>
#include <string.h>

typedef enum {
    CLASS_FUNCTIONAL, CLASS_RACE, CLASS_CORNER, CLASS_DEFECT, CLASS_UNKNOWN
} class_t;

typedef struct {
    const char *name;
    double      repro;
    int         units;
    int         corner;
} report_t;

/* TODO: class_name, triage */

int main(void) {
    /* TODO: define report[] and print: name | repro% | units | corner | class */
    return 0;
}
