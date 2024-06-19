import subprocess


def main():
    subprocess.run(['make', 'clean'])
    subprocess.run('make')
    with open('out.txt', 'w') as f:
        subprocess.run('ls', stdout=f)


if __name__ == '__main__':
    main()
