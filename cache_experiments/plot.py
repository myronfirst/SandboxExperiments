import argparse

import pandas as pd

import matplotlib.pyplot as plt

import seaborn as sns

DATA_PATH = 'Traces.csv'
NUM_OPS = 2 * 10**7

DROP_MEASUREMENTS = 5


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


def to_throughput(df, headers, num_ops):
    '''
    Convert runtime to throughput
    '''
    throughput_header = 'Mops/second'
    num_million_ops = num_ops / 10**6
    seconds = df['milliseconds'] / 1000
    df[throughput_header] = num_million_ops / seconds
    headers[2] = throughput_header
    return df, headers


def barplot(df, x_label, y_label):
    sns.set_theme()
    sns.set_context('paper')
    sns.barplot(data=df, x=x_label, y=y_label, hue=x_label)
    plt.savefig('figure.pdf', format='pdf')
    # plt.show()


def pointplot(df, name_label, x_label, y_label):
    sns.set_theme()
    sns.set_context('paper')
    ax = sns.pointplot(data=df, x=x_label, y=y_label,
                       hue=name_label, native_scale=True)
    plt.xticks(df['threads'].unique())
    plt.savefig('figure.pdf', format='pdf')
    # plt.show()


def pre_process(data_path):
    df = pd.read_csv(data_path, skipinitialspace=True, comment='#')
    headers = df.columns.to_list()
    # df = drop_measurements(df)
    df, headers = to_throughput(df, headers, NUM_OPS)
    return df, headers


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('plot_type', nargs='?', choices=[
                        'barplot', 'pointplot'], default='barplot')
    args = parser.parse_args()
    return args.plot_type


def main():
    plot_type = parse_args()
    df, headers = pre_process(DATA_PATH)
    if plot_type == 'barplot':
        barplot(df, headers[0], headers[1])
    elif plot_type == 'pointplot':
        pointplot(df, headers[0], headers[1], headers[2])


if __name__ == '__main__':
    main()
