#pragma once

struct builder* builder(void);

int build_index(struct builder*);
int build_imm  (struct builder*, float);
int build_uni  (struct builder*, float const*);

int build_add (struct builder*, int, int);
int build_sub (struct builder*, int, int);
int build_mul (struct builder*, int, int);
int build_min (struct builder*, int, int);
int build_max (struct builder*, int, int);
int build_sqrt(struct builder*, int);

struct program* compile(struct builder*);
void run(struct program const*, float *dst, int n);
