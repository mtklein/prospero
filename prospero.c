#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IMG_WH          1024
#define MAX_LINE_LENGTH 1024
#define MAX_INSTS       1024*1024

int main(int argc, char* argv[]) {
    FILE *in  = fopen(argc > 1 ? argv[1] : "prospero.vm", "r");

    struct builder *b = builder();
    static int val[MAX_INSTS];

    // x: -1 + (IMG_WH-1)*step ~~> +1
    // y: +1 - (IMG_WH-1)*step ~~> -1
    float const step = 2.0f / (IMG_WH-1);
    float Y;

    char line[MAX_LINE_LENGTH] = {0};
    while (fgets(line, sizeof line, in)) {
        char const *c = line;
        if (*c == '#') {
            continue;
        }

        unsigned id;
        int      skip;
        sscanf(c, "_%x %n", &id, &skip);
        c += skip;

        float imm;
        if (1 == sscanf(c, "const %f\n", &imm)) {
            val[id] = build_imm(b, imm);
            continue;
        }

        if (0 == strcmp(c, "var-x\n")) {
            // x = -1 + index * step
            val[id] = build_add(b, build_mul(b, build_index(b)
                                              , build_imm(b, step))
                                 , build_imm(b, -1.0f));
            continue;
        }
        if (0 == strcmp(c, "var-y\n")) {
            val[id] = build_uni(b, &Y);
            continue;
        }

        unsigned x,y;
        if (2 == sscanf(c, "add _%x _%x\n", &x,&y)) {
            val[id] = build_add(b, val[x], val[y]);
            continue;
        }
        if (2 == sscanf(c, "sub _%x _%x\n", &x,&y)) {
            val[id] = build_sub(b, val[x], val[y]);
            continue;
        }
        if (2 == sscanf(c, "mul _%x _%x\n", &x,&y)) {
            val[id] = build_mul(b, val[x], val[y]);
            continue;
        }
        if (2 == sscanf(c, "min _%x _%x\n", &x,&y)) {
            val[id] = build_min(b, val[x], val[y]);
            continue;
        }
        if (2 == sscanf(c, "max _%x _%x\n", &x,&y)) {
            val[id] = build_max(b, val[x], val[y]);
            continue;
        }

        if (1 == sscanf(c, "square _%x\n", &x)) {
            val[id] = build_mul(b, val[x], val[x]);
            continue;
        }

        if (1 == sscanf(c, "neg _%x\n", &x)) {
            val[id] = build_sub(b, build_imm(b, 0.0f), val[x]);
            continue;
        }
        if (1 == sscanf(c, "sqrt _%x\n", &x)) {
            val[id] = build_sqrt(b, val[x]);
            continue;
        }
    }
    fclose(in);

    struct program *p = compile(b);

    dprintf(1, "P5\n%d %d\n255\n", IMG_WH, IMG_WH);

    for (int j = 0; j < IMG_WH; j++) {
        Y = +1.0f - (float)j*step;

        static float row[IMG_WH];
        run(p, row, IMG_WH);

        static char lt_zero[IMG_WH];
        for (int i = 0; i < IMG_WH; i++) {
            lt_zero[i] = row[i] < 0 ? 0xff : 0x00;
        }
        write(1, lt_zero, sizeof lt_zero);
    }

    free(p);
    return 0;
}
