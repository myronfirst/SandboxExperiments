import numpy as np
import pandas as pd

import matplotlib as mpl
import matplotlib.pyplot as plt

import seaborn as sns
import seaborn.objects as so

# pointplot #useful for thread plots

DATA_PATH = './Traces.csv'
PIXEL_SIZE = (1920, 1080)
MY_DPI = 96


def drop_measurements(data_frame):
    # drop the first 5 measurements of each experiment
    experiment_names = data_frame['experiments'].unique().tolist()
    dropList = []
    for name in experiment_names:
        dropList += data_frame.index[
            data_frame['experiments'] == name].tolist()[:5]
    return data_frame.drop(dropList)


def barplot(data_path, x_label, y_label):
    df = pd.read_csv(data_path)
    df = drop_measurements(df)

    # print(df)
    plt.figure(figsize=(PIXEL_SIZE[0]/MY_DPI,
               PIXEL_SIZE[1]/MY_DPI), dpi=MY_DPI)
    sns.set_theme()
    sns.barplot(data=df, x=x_label, y=y_label, hue=x_label)
    # bottom, top = plt.ylim()
    # top = 15
    # plt.ylim(bottom, top)
    plt.savefig('figure.pdf', dpi=MY_DPI, format='pdf')
    # plt.show()


def main():
    headers = pd.read_csv(DATA_PATH, nrows=0).columns.tolist()
    barplot(DATA_PATH, headers[0], headers[1])


if __name__ == "__main__":
    main()
