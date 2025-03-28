#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__ARM_NEON)
    #include <arm_neon.h>
#endif

#define IMG_WH          1024
#define MAX_LINE_LENGTH 1024
#define MAX_INSTS       1024*1024

#if defined(__wasm)
    typedef long     mask;  // Hey wasm, WTF?
#else
    typedef int32_t  mask;
#endif

typedef float __attribute__((vector_size(16))) Float;
typedef mask  __attribute__((vector_size(16))) Mask;
#define K (int)(sizeof(Float) / sizeof(float))

static struct inst {
    void (*fn)(struct inst const *ip, Float *v, Float const val[], Float x, float y);
    union {
        float imm;
        struct { int a,b; };
        FILE *file;
    };
} program[MAX_INSTS];

#define next ip[1].fn(ip+1,v+1,val,x,y); return
#define inst(name) \
    static void inst_##name(struct inst const *ip, Float *v, Float const val[], Float x, float y)

inst(x)     { *v =                            x; next; }
inst(y)     { *v = (((Float){0}) + 1) *       y; next; }
inst(const) { *v = (((Float){0}) + 1) * ip->imm; next; }

inst(add) { *v =  val[ip->a] + val[ip->b]; next; }
inst(sub) { *v =  val[ip->a] - val[ip->b]; next; }
inst(mul) { *v =  val[ip->a] * val[ip->b]; next; }
inst(neg) { *v = -val[ip->a]             ; next; }

#if defined(__ARM_NEON)
    inst(min) { *v = vminq_f32(val[ip->a], val[ip->b]); next; }
    inst(max) { *v = vmaxq_f32(val[ip->a], val[ip->b]); next; }
#else
    static Float sel(Mask m, Float t, Float f) {
        return (Float)( (m & (Mask)t) | (~m & (Mask)f) );
    }
    inst(min) { *v = sel(val[ip->a] < val[ip->b], val[ip->a], val[ip->b]); next; }
    inst(max) { *v = sel(val[ip->a] > val[ip->b], val[ip->a], val[ip->b]); next; }
#endif

inst(sqrt) {
#if defined(__ARM_NEON)
    *v = vsqrtq_f32(val[ip->a]);
#else
    *v = (Float){
        sqrtf(val[ip->a][0]),
        sqrtf(val[ip->a][1]),
        sqrtf(val[ip->a][2]),
        sqrtf(val[ip->a][3]),
    };
#endif
    next;
}

inst(done) {
    typedef char __attribute__((vector_size(K))) Char;
    Char lt_zero = __builtin_convertvector(v[-1] < 0, Char);
    write(1, &lt_zero, sizeof lt_zero);

    (void)ip;
    (void)val;
    (void)x;
    (void)y;
    return;
}

#undef next
#undef inst

int main(int argc, char* argv[]) {
    FILE *in  = fopen(argc > 1 ? argv[1] : "prospero.vm", "r");

    char line[MAX_LINE_LENGTH] = {0};
    struct inst *ip = program;

    while (fgets(line, sizeof line, in)) {
        char const *c = line;
        if (*c == '#') {
            continue;
        }

        unsigned id;
        int      skip;
        sscanf(c, "_%x %n", &id, &skip);
        assert((int)id == (int)(ip - program));
        c += skip;

        if (0 == strcmp(c, "var-x\n")) { *ip++ = (struct inst){ .fn=inst_x }; continue; }
        if (0 == strcmp(c, "var-y\n")) { *ip++ = (struct inst){ .fn=inst_y }; continue; }

        float imm;
        if (1 == sscanf(c, "const %f\n", &imm)) {
            *ip++ = (struct inst){ .fn=inst_const, .imm=imm };
            continue;
        }

        unsigned a,b;
        if (2 == sscanf(c, "add _%x _%x\n", &a,&b)) {
            *ip++ = (struct inst){ .fn = inst_add, .a=(int)a, .b=(int)b };
            continue;
        }
        if (2 == sscanf(c, "sub _%x _%x\n", &a,&b)) {
            *ip++ = (struct inst){ .fn = inst_sub, .a=(int)a, .b=(int)b };
            continue;
        }
        if (2 == sscanf(c, "mul _%x _%x\n", &a,&b)) {
            *ip++ = (struct inst){ .fn = inst_mul, .a=(int)a, .b=(int)b };
            continue;
        }
        if (2 == sscanf(c, "max _%x _%x\n", &a,&b)) {
            *ip++ = (struct inst){ .fn = inst_max, .a=(int)a, .b=(int)b };
            continue;
        }
        if (2 == sscanf(c, "min _%x _%x\n", &a,&b)) {
            *ip++ = (struct inst){ .fn = inst_min, .a=(int)a, .b=(int)b };
            continue;
        }

        if (1 == sscanf(c, "square _%x\n", &a)) {
            *ip++ = (struct inst){ .fn = inst_mul, .a=(int)a, .b=(int)a };
            continue;
        }

        if (1 == sscanf(c, "neg _%x\n", &a)) {
            *ip++ = (struct inst){ .fn = inst_neg, .a=(int)a };
            continue;
        }
        if (1 == sscanf(c, "sqrt _%x\n", &a)) {
            *ip++ = (struct inst){ .fn = inst_sqrt, .a=(int)a };
            continue;
        }

        dprintf(2, "didn't understand '%s'\n", c);
        assert(0);
    }
    *ip = (struct inst){.fn = inst_done};
    fclose(in);


    dprintf(1, "P5\n%d %d\n255\n", IMG_WH, IMG_WH);

    // x: -1 + (IMG_WH-1)*step ~~> +1
    // y: +1 - (IMG_WH-1)*step ~~> -1
    float const step = 2.0f / (IMG_WH-1);

    assert(K == 4);
    Float const iota = {0,1,2,3};

    for (int j = 0; j < IMG_WH; j += 1)
    for (int i = 0; i < IMG_WH; i += K) {
        float x0 = -1 + (float)i * step,
              y  = +1 - (float)j * step;
        Float x  = x0 + iota * step;

        static Float val[MAX_INSTS];
        program->fn(program,val,val,x,y);
    }

    return 0;
}
