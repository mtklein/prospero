#include "vm.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(__ARM_NEON)
    #include <arm_neon.h>
#endif

// TODO:
//    [ ] loop-invariant hoisting
//        [x] simple constant hoisting
//        [ ] hoist uniform math too
//    [ ] common subexpression elimination
//    [ ] constant propagation?
//    [ ] strength reduction?
//    [~] dead code elimination?

#if defined(__wasm)
    typedef long     mask;  // Hey wasm, WTF?
#else
    typedef int32_t  mask;
#endif

typedef float __attribute__((vector_size(128))) Float;
typedef mask  __attribute__((vector_size(128))) Mask;
#define K (int)(sizeof(Float) / sizeof(float))


struct inst {
    void (*fn)(struct inst const *ip, int i, Float *r, Float const v[], float *dst);
    union {
        float const *uni;
        float        imm;
    };
    int x,y,z;
    int reordered;
};

#define op(name) \
    static void op_##name(struct inst const *ip, int i, Float *r, Float const v[], float *dst)
#define next ip[1].fn(ip+1,i,r+1,v,dst); return

op(index) {
    _Static_assert(K == 32, "");
    *r = (float)i
       + (Float){ 0, 1, 2, 3,  4, 5, 6, 7,  8, 9,10,11, 12,13,14,15,
                 16,17,18,19, 20,21,22,23, 24,25,26,27, 28,29,30,31};
    next;
}

op(imm) { *r = ((Float){0} + 1) *  ip->imm; next; }
op(uni) { *r = ((Float){0} + 1) * *ip->uni; next; }

op(add) { *r = v[ip->x] + v[ip->y]; next; }
op(sub) { *r = v[ip->x] - v[ip->y]; next; }
op(mul) { *r = v[ip->x] * v[ip->y]; next; }

#if 1 && defined(__ARM_NEON)
    op(min) {
        _Static_assert(K == 32, "");
        union { Float vec; float32x4_t part[8]; } x = {v[ip->x]}, y = {v[ip->y]};
        x.part[0] = vminq_f32(x.part[0], y.part[0]);
        x.part[1] = vminq_f32(x.part[1], y.part[1]);
        x.part[2] = vminq_f32(x.part[2], y.part[2]);
        x.part[3] = vminq_f32(x.part[3], y.part[3]);
        x.part[4] = vminq_f32(x.part[4], y.part[4]);
        x.part[5] = vminq_f32(x.part[5], y.part[5]);
        x.part[6] = vminq_f32(x.part[6], y.part[6]);
        x.part[7] = vminq_f32(x.part[7], y.part[7]);
        *r = x.vec;
        next;
    }
    op(max) {
        _Static_assert(K == 32, "");
        union { Float vec; float32x4_t part[8]; } x = {v[ip->x]}, y = {v[ip->y]};
        x.part[0] = vmaxq_f32(x.part[0], y.part[0]);
        x.part[1] = vmaxq_f32(x.part[1], y.part[1]);
        x.part[2] = vmaxq_f32(x.part[2], y.part[2]);
        x.part[3] = vmaxq_f32(x.part[3], y.part[3]);
        x.part[4] = vmaxq_f32(x.part[4], y.part[4]);
        x.part[5] = vmaxq_f32(x.part[5], y.part[5]);
        x.part[6] = vmaxq_f32(x.part[6], y.part[6]);
        x.part[7] = vmaxq_f32(x.part[7], y.part[7]);
        *r = x.vec;
        next;
    }
#else
    static Float sel(Mask m, Float t, Float f) {
        return (Float)( (m & (Mask)t) | (~m & (Mask)f) );
    }
    op(min) { *r = sel(v[ip->x] < v[ip->y], v[ip->x], v[ip->y]); next; }
    op(max) { *r = sel(v[ip->x] > v[ip->y], v[ip->x], v[ip->y]); next; }
#endif

op(sqrt) {
    _Static_assert(K == 32, "");
#if 1 && defined(__ARM_NEON)
    union { Float vec; float32x4_t part[8]; } x = {v[ip->x]};
    x.part[0] = vsqrtq_f32(x.part[0]);
    x.part[1] = vsqrtq_f32(x.part[1]);
    x.part[2] = vsqrtq_f32(x.part[2]);
    x.part[3] = vsqrtq_f32(x.part[3]);
    x.part[4] = vsqrtq_f32(x.part[4]);
    x.part[5] = vsqrtq_f32(x.part[5]);
    x.part[6] = vsqrtq_f32(x.part[6]);
    x.part[7] = vsqrtq_f32(x.part[7]);
    *r = x.vec;
#else
    *r = (Float) {
        sqrtf(v[ip->x][ 0]), sqrtf(v[ip->x][ 1]), sqrtf(v[ip->x][ 2]), sqrtf(v[ip->x][ 3]),
        sqrtf(v[ip->x][ 4]), sqrtf(v[ip->x][ 5]), sqrtf(v[ip->x][ 6]), sqrtf(v[ip->x][ 7]),
        sqrtf(v[ip->x][ 8]), sqrtf(v[ip->x][ 9]), sqrtf(v[ip->x][10]), sqrtf(v[ip->x][11]),
        sqrtf(v[ip->x][12]), sqrtf(v[ip->x][13]), sqrtf(v[ip->x][14]), sqrtf(v[ip->x][15]),
        sqrtf(v[ip->x][16]), sqrtf(v[ip->x][17]), sqrtf(v[ip->x][18]), sqrtf(v[ip->x][19]),
        sqrtf(v[ip->x][20]), sqrtf(v[ip->x][21]), sqrtf(v[ip->x][22]), sqrtf(v[ip->x][23]),
        sqrtf(v[ip->x][24]), sqrtf(v[ip->x][25]), sqrtf(v[ip->x][26]), sqrtf(v[ip->x][27]),
        sqrtf(v[ip->x][28]), sqrtf(v[ip->x][29]), sqrtf(v[ip->x][30]), sqrtf(v[ip->x][31]),
    };
#endif
    next;
}

op(done) {
    (void)r;
    memcpy(dst+i, v+ip->x, sizeof v[ip->x]);
}

#undef op
#undef next

struct builder {
    int          insts,unused;
    struct inst *inst;
};

struct builder* builder(void) {
    struct builder *b = calloc(1, sizeof *b);
    (void)build_imm(b, 0.0f);  // Handy for default x,y args to point to a known zero constant.
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
    int         insts,loop;
    struct inst inst[];
};

struct program* compile(struct builder *b) {
    push(b, op_done, .x=b->insts-1);

    struct program *p = malloc(sizeof *p + (size_t)b->insts * sizeof *b->inst);
    p->insts = 0;

    for (int varying = 0; varying < 2; varying++) {
        if (varying) {
            p->loop = p->insts;
        }
        for (int i = 0; i < b->insts; i++) {
            struct inst *inst = b->inst+i;
            if ((inst->fn != op_imm) == varying) {
                inst->x = b->inst[inst->x].reordered;
                inst->y = b->inst[inst->y].reordered;
                inst->z = b->inst[inst->z].reordered;
                p->inst[p->insts] = *inst;
                inst->reordered = p->insts++;
            }
        }
    }
    free(b);
    return p;
}

void run(struct program const *p, float *dst, int n) {
    #define STACK 8192
    Float stack[STACK];
    Float *val = p->insts <= STACK ? stack : calloc((size_t)p->insts, sizeof *val);

    struct inst const *ip = p->inst,  *loop = ip + p->loop;
    Float             *v  =     val, *vloop =  v + p->loop;

    int i = 0;
    for (; i < n/K*K; i += K) {
        ip->fn(ip,i,v,val,dst);
        ip =  loop;
        v  = vloop;
    }
    if (i < n) {
        float tmp[K];
        ip->fn(ip,i,v,val,tmp);
        memcpy(dst, tmp, (size_t)(n-i) * sizeof *dst);
    }

    if (val != stack) {
        free(val);
    }
}
