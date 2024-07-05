import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib

DATA_PATH_DEFAULT = 'Traces.csv'
COMMENT_CHAR = '#'
# NUM_OPS = 10**7
NUM_OPS = 10**7

DROP_MEASUREMENTS = 5

PALETTE_ORDER = [
    'default',
    'malloc',
    'jemalloc',
    'vmem',
    'vmmalloc',
    'memkind',
    'pmemobj_alloc',
    'make_persistent_atomic',
    'pmem',
]

TRACE_FILES = [
    'Alloc_All.csv',
    'Alloc_NoExtremes.csv',
    'Write_All.csv',
    # 'Write_Volatile.csv',
    'Write_Persistence.csv',
    # 'Read_All.csv',
]

# On Read_All.csv, change NUM_OPS to 10**8


def drop_measurements(df):
    '''
    drop the first DROP_MEASUREMENTS measurements of each experiment
    '''
    experiment_names = df['experiments'].unique().tolist()
    dropList = []
    for name in experiment_names:
        dropList += df.index[
            df['experiments'] == name].tolist()[:DROP_MEASUREMENTS]
    return df.drop(dropList)


def to_throughput(df, headers):
    '''
    Convert runtime to throughput
    '''
    throughput_header = 'Mops/second'
    num_million_ops = NUM_OPS / 10**6
    seconds = df['milliseconds'] / 1000
    df[throughput_header] = num_million_ops / seconds
    headers[2] = throughput_header
    return df, headers


def pre_process(data_path):
    df = pd.read_csv(data_path, skipinitialspace=True, comment=COMMENT_CHAR)
    title = pathlib.Path(data_path).stem
    headers = df.columns.to_list()
    # df = drop_measurements(df)
    df, headers = to_throughput(df, headers)
    return df, title, headers


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('data_path', nargs='?', default=DATA_PATH_DEFAULT)
    parser.add_argument('plot_type', nargs='?', choices=[
                        'barplot', 'pointplot'], default='pointplot')
    args = parser.parse_args()
    return args.data_path, args.plot_type


def generate(data_path, plot_type):
    df, title, headers = pre_process(data_path)
    sns.set_theme(context='paper', style='whitegrid', font_scale=1.2)
    fig, ax = plt.subplots()

    if plot_type == 'barplot':
        sns.barplot(
            data=df, x=headers[0], y=headers[1], hue=headers[0], ax=ax)
    elif plot_type == 'pointplot':
        palette_dict = {name: col for name, col in zip(
            PALETTE_ORDER, sns.color_palette())}
        sns.pointplot(data=df, hue=headers[0], x=headers[1], y=headers[2],
                      palette=palette_dict, native_scale=True, ax=ax)
        ax.set_xticks(df['threads'].unique())
    ax.set_title(title)
    fig.savefig(title + '.pdf', bbox_inches='tight', pad_inches=0.02)


def main():
    # data_path, plot_type = parse_args()
    # generate(data_path, plot_type)
    for trace in TRACE_FILES:
        generate(trace, 'pointplot')


if __name__ == '__main__':
    main()
