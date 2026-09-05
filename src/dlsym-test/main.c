#include <assert.h>
#include <stdio.h>
extern int run_lookup(void);
int main(void){assert(run_lookup()==22);puts("PASS: unrelated RTLD_NEXT caller scope preserved");}
