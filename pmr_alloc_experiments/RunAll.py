import subprocess
import numpy as np
from scipy import stats
import math

from pathlib import Path

ALLOCATORS = [
    'NewDeleteAllocator',
    'SyncPoolHeapAllocator',
    'SyncPoolBufferAllocator',
    'ArenaBufferAllocator',
    'ArenaPoolHeapAllocator',
    'ArenaPoolBufferAllocator',
    'JeMallocAllocator',
    # 'SynchPoolAllocator',
]
ALLOC_SIZES = [
    '{16}',
    '{ 16, 32, 64, 128, 256 }',
]


def alloc_size_str(val: str):
    return val.replace(' ', '').replace(',', '.').replace('{', '').replace('}', '')


ENABLE_DEALLOC_MODES = [
    'true',
    'false',
]

N_THREADS = [
    '1',
    '2',
    '4',
    '8',
    # '12',
    # '16',
    # '20',
    # '24',
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
            completed_proc.check_returncode()
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
    print(f'ENABLE_DEALLOC_MODES: {ENABLE_DEALLOC_MODES}')
    print(f'ALLOCATORS: {ALLOCATORS}')
    print(f'ALLOC_SIZES: {ALLOC_SIZES}')
    print(f'N_THREADS: {N_THREADS}')
    subprocess.run(['rm', '-rf', 'build']).check_returncode()
    for is_dealloc in ENABLE_DEALLOC_MODES:
        for alloc_size in ALLOC_SIZES:
            with open(f'AllocatorTraces_{alloc_size_str(alloc_size)}_Dealloc.{is_dealloc}.csv', 'w') as f:
                f.write(f'name,threads,{UNIT},operations\n')
                for allocator in ALLOCATORS:
                    for n in N_THREADS:
                        bench = f'build/{allocator}_{alloc_size_str(alloc_size)}_{n}'
                        subprocess.run(
                            ['make', 'DEBUG=0', f"""PARAMS=-DPARAM_N_THREADS={n} -DPARAM_ALLOCATOR={allocator} -DPARAM_ALLOC_SIZES='{alloc_size}' -DPARAM_ENABLE_DEALLOCATE={is_dealloc}"""]).check_returncode()
                        subprocess.run(
                            ['mv', f'build/main', f'{bench}']).check_returncode()
                        subprocess.run(['make', 'clean']).check_returncode()
                        name, threads, duration, operations = run_repeatedly(
                            [bench, n])
                        f.write(
                            f'{name[:name.find("_")]},{threads},{duration},{operations}\n')
                        f.flush()
    print(f'MAX_RUNS_CASES:')
    for case in MAX_RUNS_CASES:
        print(f'{case}')


if __name__ == '__main__':
    main()
