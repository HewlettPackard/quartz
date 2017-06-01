#include "fam_atomic.h"

int64_t global;

int main()
{
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_fence();
}
