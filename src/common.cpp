#include <cstdlib>
#include <cassert>
#include "common.h"

using namespace v8;

bool str_eq(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

