#define _GNU_SOURCE
#include <dlfcn.h>
int marker(void){return 11;}
int run_lookup(void){int(*next)(void)=dlsym(RTLD_NEXT,"marker");return next?next():-1;}
