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
    # './build/bench_pmemobj_alloc',
    # './build/bench_make_persistent_atomic',
]

N_THREADS = [
    '1',
    '2',
    '4',
    '8',
    '16',
    '24',
    '32',
    '40',
    '48',
]

LOGN_OPS = '7'


def main():
    my_env = os.environ.copy()
    my_env['VMMALLOC_POOL_DIR'] = '/mnt/pmem0/myrontsa'
    my_env['VMMALLOC_POOL_SIZE'] = f'{1024 *1024*1024*16}'  # 16GB

    subprocess.run(['make', 'clean'])
    subprocess.run(['make', '-j'])
    print(f'LOGN_OPS: {LOGN_OPS}')
    print(f'N_THREADS: {N_THREADS}')
    with open('Traces.csv', 'w') as f:
        f.write('allocator,threads,milliseconds\n')
        for bench in BENCHES:
            for n in N_THREADS:
                completedProc = subprocess.run([bench, n, LOGN_OPS],
                                               capture_output=True, env=my_env)
                allocator = Path(bench).name.replace('bench_', '')
                millis = int(completedProc.stdout.strip())
                f.write(f'{allocator},{n},{millis}\n')


if __name__ == '__main__':
    main()
