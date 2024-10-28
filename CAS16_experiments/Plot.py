import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib

DATA_PATH_DEFAULT = 'Traces.csv'
TRACE_FILES = [
    'CAS2Traces.csv',
]
COMMENT_CHAR = '#'
UNIT = {
    'milliseconds': 10**3,
    'microseconds': 10**6,
}

DROP_MEASUREMENTS = 5

PALETTE_ORDER = [
    'SynchBuiltin',
    'AssemblySynch',
    'LibAtomic',
]


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
    num_million_ops = df['operations'] / 10**6
    seconds = df[headers[2]] / UNIT[headers[2]]
    df[throughput_header] = num_million_ops / seconds
    headers = [headers[0], headers[1], throughput_header]
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
