import subprocess
import glob
from colorama import Fore, Style
from attr import dataclass


@dataclass
class BazelTest:
    repository: str
    target: str


BAZEL_BINS: list[BazelTest] = [
    BazelTest('', 'cache_warm')
]
BUILD_DIR: str = 'build/bazel-bin'

# THREADS_TOTAL = 24
# THREADS_STEP = 4
# THREADS = list(range(THREADS_STEP, THREADS_TOTAL + 1, THREADS_STEP))
# THREADS = [48, 8, 24]


def build_bins() -> list[str]:
    print(f'{Fore.YELLOW}Building binaries{Style.RESET_ALL}')
    subprocess.run(['bazel', 'clean'], check=True)
    executables: list[str] = []
    for bazel_bin in BAZEL_BINS:
            subprocess.run(['bazel', 'build', '--config=release', f'//{bazel_bin.repository}:{bazel_bin.target}'], check=True)
            subprocess.run(['mv', '-f', f'{BUILD_DIR}/{bazel_bin.repository}/{bazel_bin.target}',
                            f'{BUILD_DIR}/{bazel_bin.repository}/{bazel_bin.target}'], check=True)
            executables.append(f'{BUILD_DIR}/{bazel_bin.repository}/{bazel_bin.target}')
    return executables


def run_bins(executables: list[str]):
    print(f'{Fore.YELLOW}Executing binaries{Style.RESET_ALL}')
    for executable in executables:
        subprocess.run(['rm', '-f', *glob.glob('/mnt/pmem0/myrontsa/*')], check=True)
        print(f'{Fore.YELLOW}Executing{executable}{Style.RESET_ALL}')
        subprocess.run(
            ['stdbuf', '-o0', '-e0'] +
            [executable], check=True)


def main():
    executables = build_bins()
    run_bins(executables)


if __name__ == '__main__':
    main()
