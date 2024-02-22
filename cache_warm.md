- Titan: (1024 * 2014 * 4.66) of 8-byte elements in persistence, ~37.28MB
<!-- - pc-myron: (1024 * 2014 * 1) million of 8-byte elements in persistence, 32MB -->
- Conducted by newly created Thread vs Conducted bt newly forked Process
- Repeat each case 30 times
- Drop the first 5 entries for each case
- Plot on bars the average runtime on each case

Timed Experiments
- Read Persistence: Read CACHE_SIZE array from persistence to volatile number
- Write Persistence: Write to CACHE_SIZE array in persistence

Preparation
- None
- Read persistent array to prefetch to cache
- Write to volatile array 2*CACHE_SIZE, to fill cache with useless info

Wrappers
- Conduct experiment by new worker thread, main thread waits
- Conduct experiment by new child process, parent process waits

On process based experiments we observe almost half the runtime, including the line above. Explanation:
- `fork() Copy-On-Write` mechanism kicks in after we access the persistence domain. The mechanism copies the volatile memory array from the parent process memory domain to the child and then performs the write of the persistent block to the copy.
- We warm our caches, bringing us performance

## Thread, Fork, WBINVD experiments
- Initialize data in persistency
- Config REPETITIONS, CACHE_SIZE
- Pick Experiment in main
- Intrumenter callbacks: PrefixCSV, AppendCSV

## Main Thread (Mallis) experiment
- Initialize data in persistency
- Config CACHE_SIZE
- Pick Experiment in main, with 1 repetition
- Config REPETITIONS in .sh script
- Intrumenter callbacks: PrefixCSVIfEmpty, AppendCSV
