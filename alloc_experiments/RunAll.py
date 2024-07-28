import subprocess
import os

from pathlib import Path

BENCHES = [
    './build/bench_default',
    './build/bench_malloc',
    './build/bench_jemalloc',
    './build/bench_vmem',
    './build/bench_vmmalloc',
    './build/bench_memkind',
    './build/bench_pmemobj_alloc',
    './build/bench_make_persistent_atomic',
    './build/bench_pmem',
]
# run Read operation with LOGN_OPS = '6', then change NUM_OPS=10**6 in Plot.py

OPERATIONS = [
    ['None', 'Alloc'],
    ['Block', 'Read'],
    ['Block', 'Write'],
    ['Sparse', 'Read'],
    ['Sparse', 'Write'],
]
N_THREADS = [
    '1',
    '2',
    '4',
    '8',
    '12',
    '16',
    '20',
    '24',
]

MIN_RUNS = 3
MAX_RUNS = 10

UNIT = 'microseconds'


def RepeatRun(args):
    '''
    Run up until MAX_RUNS times or until error margin of MIN_RUNS consecutive times is less than 2%
    '''
    my_env = os.environ.copy()
    my_env['VMMALLOC_POOL_DIR'] = '/mnt/pmem0/myrontsa'
    my_env['VMMALLOC_POOL_SIZE'] = f'{1024 *1024*1024*32}'  # 32GB
    stats = {'dur': [], 'ops': []}
    retDurAvg = 0
    retOpsAvg = 0
    for i in range(MAX_RUNS):
        completedProc = subprocess.run(args,
                                       capture_output=True, env=my_env)
        lines = completedProc.stdout.splitlines()
        dur, ops = int(lines[0]), int(lines[1])
        stats['dur'].append(dur)
        stats['ops'].append(ops)
        if i < MIN_RUNS:
            continue
        durWindow = [dur for dur in stats['dur'][i-MIN_RUNS:]]
        durAvg = sum(durWindow) / len(durWindow)
        errorMargin = sum(durWindow) / len(durWindow)
        if (errorMargin < 0.02):
            retDurAvg = durAvg
            break
    durWindowAll = [dur for dur in stats['dur']]
    if retDurAvg == 0:
        retDurAvg = sum(durWindowAll) / len(durWindowAll)
    allocator = Path(args[0]).name.replace('bench_', '')
    threads = args[1]
    duration = retDurAvg
    # operations = int(lines[1])
    return allocator, threads, duration, operations


def main():

    subprocess.run(['make', 'clean'])
    subprocess.run(['make', '-j'])
    print(f'OPERATIONS: {OPERATIONS}')
    print(f'N_THREADS: {N_THREADS}')
    for op in OPERATIONS:
        with open(f'{op[0]}_{op[1]}.csv', 'w') as f:
            f.write(f'allocator,threads,{UNIT},operations\n')
            for bench in BENCHES:
                if op[1] == 'Alloc' and bench == './build/bench_pmem':
                    continue  # do not run Alloc operation with bench_pmem, it is erroneous
                for n in N_THREADS:
                    allocator, threads, duration, operations = RepeatRun(
                        [bench, n, op[0], op[1]])
                    f.write(f'{allocator},{threads},{duration},{operations}\n')
                    f.flush()


if __name__ == '__main__':
    main()
