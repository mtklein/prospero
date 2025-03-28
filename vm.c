#include "vm.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// TODO:
//    - loop-invariant hoisting
//    - common subexpression elimination
//    - constant propagation?
//    - strength reduction?
//    - dead code elimination?

#if defined(__wasm)
    typedef long     mask;  // Hey wasm, WTF?
#else
    typedef int32_t  mask;
#endif

typedef float __attribute__((vector_size(16))) Float;
typedef mask  __attribute__((vector_size(16))) Mask;
#define K (int)(sizeof(Float) / sizeof(float))

static Float sel(Mask m, Float t, Float f) {
    return (Float)( (m & (Mask)t) | (~m & (Mask)f) );
}

struct inst {
    void (*fn)(struct inst const *ip, int i, Float *r, Float const v[], float *dst);
    union {
        float        imm;
        float const *uni;
        struct { int x,y; };
    };
};

#define op(name) \
    static void op_##name(struct inst const *ip, int i, Float *r, Float const v[], float *dst)
#define next ip[1].fn(ip+1,i,r+1,v,dst); return

op(index) { *r = (Float){0,1,2,3} + (float)i; next; }
op(imm)   { *r = ((Float){0} + 1) *  ip->imm; next; }
op(uni)   { *r = ((Float){0} + 1) * *ip->uni; next; }

op(add) { *r = v[ip->x] + v[ip->y]; next; }
op(sub) { *r = v[ip->x] - v[ip->y]; next; }
op(mul) { *r = v[ip->x] * v[ip->y]; next; }

op(min) { *r = sel(v[ip->x] < v[ip->y], v[ip->x], v[ip->y]); next; }
op(max) { *r = sel(v[ip->x] > v[ip->y], v[ip->x], v[ip->y]); next; }

op(sqrt) {
    *r = (Float) {
        sqrtf(v[ip->x][0]),
        sqrtf(v[ip->x][1]),
        sqrtf(v[ip->x][2]),
        sqrtf(v[ip->x][3]),
    };
    next;
}

op(done) {
    (void)ip;
    (void)v;
    memcpy(dst+i, r-1, sizeof *r);
}

#undef op
#undef next

struct builder {
    int          insts,unused;
    struct inst *inst;
};

struct builder* builder(void) {
    struct builder *b = calloc(1, sizeof *b);
    return b;
}

static int push_(struct builder *b, struct inst inst) {
    if ((b->insts & (b->insts-1)) == 0) {
        int const grown = b->insts ? b->insts*2 : 1;
        b->inst = realloc(b->inst, (size_t)grown * sizeof *b->inst);
    }
    int const id = b->insts++;
    b->inst[id] = inst;
    return id;
}
#define push(b,...) push_(b, (struct inst){.fn=__VA_ARGS__})

int build_index(struct builder *b                  ) { return push(b, op_index        ); }
int build_imm  (struct builder *b, float        imm) { return push(b, op_imm, .imm=imm); }
int build_uni  (struct builder *b, float const *uni) { return push(b, op_uni, .uni=uni); }

int build_add (struct builder *b, int x, int y) { return push(b, op_add , .x=x, .y=y); }
int build_sub (struct builder *b, int x, int y) { return push(b, op_sub , .x=x, .y=y); }
int build_mul (struct builder *b, int x, int y) { return push(b, op_mul , .x=x, .y=y); }
int build_min (struct builder *b, int x, int y) { return push(b, op_min , .x=x, .y=y); }
int build_max (struct builder *b, int x, int y) { return push(b, op_max , .x=x, .y=y); }
int build_sqrt(struct builder *b, int x       ) { return push(b, op_sqrt, .x=x      ); }

struct program {
    int         insts,unused;
    struct inst inst[];
};

struct program* compile(struct builder *b) {
    push(b, op_done);

    size_t const inst_bytes = (size_t)b->insts * sizeof *b->inst;

    struct program *p = malloc(sizeof *p + inst_bytes);
    p->insts = b->insts;
    memcpy(p->inst, b->inst, inst_bytes);
    free(b);

    return p;
}
void run(struct program const *p, float *dst, int n) {
    #define STACK 8192
    Float stack[STACK];
    Float *val = p->insts <= STACK ? stack : calloc((size_t)p->insts, sizeof *val);

    struct inst const *ip = p->inst;

    int i = 0;
    for (; i < n/K*K; i += K) {
        ip->fn(ip,i,val,val,dst);
    }
    if (i < n) {
        float tmp[4];
        ip->fn(ip,i,val,val,tmp);
        memcpy(dst, tmp, (size_t)(n-i) * sizeof *dst);
    }

    if (val != stack) {
        free(val);
    }
}
