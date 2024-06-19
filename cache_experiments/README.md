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

# Workflow
- Initialize data in persistency
- Config REPETITIONS, WORKLOAD_SIZE
- Pick Experiment in main
- Instrumenter callbacks: AppendCSV
- Clear Traces.csv after trial runs
- `.bazelrc` build optimized

## Main
- NoWrapper
## Main Invalidate
- NoWrapper, InvalidateCachePrepare
## Main Terminate
- NoWrapper
- 1 rep, use sh file
## Main Terminate Invalidate
- NoWrapper, InvalidateCachePrepare
- 1 rep, use sh file

## Thread
- ThreadWrapper
## Thread Invalidate
- ThreadWrapper, InvalidateCachePrepare
## Thread Terminate
- ThreadWrapper
- 1 rep, use sh file
## Thread Terminate Invalidate
- ThreadWrapper, InvalidateCachePrepare
- 1 rep, use sh file

## System Call
- SystemCallWrapper
- system_call_main.cpp manages parameters other than REPETITIONS

## Fork
- ForkWrapper
