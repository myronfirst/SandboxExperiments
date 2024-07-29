import subprocess
import os
import numpy as np
from scipy import stats
import math

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

OPERATIONS = [
    ['None', 'Alloc'],
    ['Block', 'Read'],
    ['Block', 'Write'],
    # ['Sparse', 'Read'],
    # ['Sparse', 'Write'],
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

MIN_RUNS = 5
MAX_RUNS = 10

UNIT = 'microseconds'

MAX_RUNS_CASES = []


def get_error_margin_percentage(data):
    mean = np.mean(data)
    std_dev = np.std(data, ddof=1)  # Use ddof=1 for sample standard deviation
    n = len(data)
    standard_error = std_dev / math.sqrt(n)
    confidence_level = 0.95
    degrees_freedom = n - 1
    critical_value = stats.t.ppf((1 + confidence_level) / 2, degrees_freedom)
    error_margin = critical_value * standard_error
    error_margin_percentage = error_margin / mean
    return error_margin_percentage


def run_repeatedly(args):
    '''
    Run up until MAX_RUNS times, or until error margin of MIN_RUNS consecutive times is less than 2%
    Returns average of MAX_RUNS times minus min and max values, or average of consecutive MIN_RUNS times
    '''
    my_env = {}
    if args[0] == './build/bench_vmmalloc':
        my_env = os.environ.copy()
        my_env['VMMALLOC_POOL_DIR'] = '/mnt/pmem0/myrontsa'
        my_env['VMMALLOC_POOL_SIZE'] = f'{1024 *1024*1024*32}'  # 32GB

    def get_duration_avg():
        operations = None
        durations = np.array([], dtype=float)
        for i in range(MAX_RUNS):
            completed_proc = subprocess.run(
                args, capture_output=True, env=my_env)
            lines = completed_proc.stdout.splitlines()
            dur, ops = int(lines[0]), int(lines[1])
            if operations is None:
                operations = ops
            durations = np.append(durations, dur)
            if i < MIN_RUNS:
                print(f'[{i}]{args},{operations},{dur}')
                continue
            window = durations[-MIN_RUNS:]
            emp = get_error_margin_percentage(window)
            print(f'[{i}]{args},{operations},{dur},{emp}')
            if (emp < 0.02):
                return np.mean(window).astype(int), operations
        global MAX_RUNS_CASES
        MAX_RUNS_CASES.append(args)
        print(f'MAX_RUNS:{args}')
        return np.mean(np.delete(durations, [np.argmin(durations), np.argmax(durations)])).astype(int), operations

    duration_avg, operations = get_duration_avg()
    allocator = Path(args[0]).name.replace('bench_', '')
    threads = args[1]
    return allocator, threads, duration_avg, operations


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
                    allocator, threads, duration, operations = run_repeatedly(
                        [bench, n, op[0], op[1]])
                    f.write(f'{allocator},{threads},{duration},{operations}\n')
                    f.flush()
    print(f'MAX_RUNS_CASES:')
    for case in MAX_RUNS_CASES:
        print(f'{case}')


if __name__ == '__main__':
    main()
