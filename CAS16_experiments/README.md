# CAS2 experiments

- We compare ways to achive CAS2 functionality

- CAS2_BUILTIN is using sync_val_compare_swap. It is the slowest.

- CAS2_KALLIM is uing the CAS128 assembly emission in synch framework. This is the fastest. Although memory ordering semantics cannot be well defined this way

- CAS2_LIBATOMIC has C++ emit a call to atomic_compare_swap_16. It is slow in seq_cst ordering. A lot faster in relaxed ordering, though not as fast as CAS2_KALLIM.

TODO:
- Find out whether the load I perform after the CAS operation is what slows things down.
- Examine Titan output in hotspot. Make sure we are spending most of our time during CAS, NOT during the load afterwards or to any other place.
- In the future, make a plot of these.
