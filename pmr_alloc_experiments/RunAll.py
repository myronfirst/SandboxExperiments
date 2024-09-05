import subprocess
import numpy as np
from scipy import stats
import math

from pathlib import Path

ALLOCATORS = ['NewDeleteAllocator']


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

UNIT = 'milliseconds'

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
    def get_duration_avg():
        operations = None
        durations = np.array([], dtype=float)
        for i in range(MAX_RUNS):
            completed_proc = subprocess.run(args, capture_output=True)
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
    name = Path(args[0]).name
    threads = args[1]
    return name, threads, duration_avg, operations


def main():
    print(f'ALLOCATORS: {ALLOCATORS}')
    print(f'N_THREADS: {N_THREADS}')
    with open(f'AllocatorTraces.csv', 'w') as f:
        f.write(f'name,threads,{UNIT},operations\n')
        for allocator in ALLOCATORS:
            for n in N_THREADS:
                bench = f'build/{allocator}_{n}'
                subprocess.run(
                    ['make', 'DEBUG=0', f'PARAMS=-DPARAM_N_THREADS={n} -DPARAM_ALLOCATOR={allocator}'])
                subprocess.run(['mv', f'build/main', f'{bench}'])
                name, threads, duration, operations = run_repeatedly(
                    [bench, n])
                f.write(f'{name},{threads},{duration},{operations}\n')
                f.flush()
    print(f'MAX_RUNS_CASES:')
    for case in MAX_RUNS_CASES:
        print(f'{case}')


if __name__ == '__main__':
    main()
