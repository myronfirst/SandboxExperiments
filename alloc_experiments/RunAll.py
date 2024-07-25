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

LOGN_OPS = '6'
UNIT = 'milliseconds'


def main():
    my_env = os.environ.copy()
    my_env['VMMALLOC_POOL_DIR'] = '/mnt/pmem0/myrontsa'
    my_env['VMMALLOC_POOL_SIZE'] = f'{1024 *1024*1024*32}'  # 32GB

    subprocess.run(['make', 'clean'])
    subprocess.run(['make', '-j'])
    print(f'OPERATIONS: {OPERATIONS}')
    print(f'LOGN_OPS: {LOGN_OPS}')
    print(f'N_THREADS: {N_THREADS}')
    for op in OPERATIONS:
        with open(f'{op[0]}_{op[1]}.csv', 'w') as f:
            f.write(f'allocator,threads,{UNIT}\n')
            for bench in BENCHES:
                if op[1] == 'Alloc' and bench == './build/bench_pmem':
                    continue  # do not run Alloc operation with bench_pmem, it is erroneous
                for n in N_THREADS:
                    completedProc = subprocess.run([bench, n, LOGN_OPS, op[0], op[1]],
                                                   capture_output=True, env=my_env)
                    allocator = Path(bench).name.replace('bench_', '')
                    millis = int(completedProc.stdout.strip())
                    f.write(f'{allocator},{n},{millis}\n')
                    f.flush()


if __name__ == '__main__':
    main()
