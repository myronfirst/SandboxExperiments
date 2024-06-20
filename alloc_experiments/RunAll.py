import subprocess
import os


def main():

    my_env = os.environ.copy()
    my_env['VMMALLOC_POOL_DIR'] = '/mnt/pmem0/myrontsa'
    my_env['VMMALLOC_POOL_SIZE'] = '17179869184'  # 16GB

    subprocess.run(['make', 'clean'])
    subprocess.run('make')
    with open('out.txt', 'w') as f:
        subprocess.run('ls', stdout=f, env=my_env)


if __name__ == '__main__':
    main()
