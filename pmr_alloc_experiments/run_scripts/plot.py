#! /bin/python3
import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

columns = 'Mops/s'
rows = '# of Threads'
title = 'Allocator Tests'

# Function to plot a single line from a CSV file
def plot_line_from_csv(file):
    # Read the CSV file, skip the first row (line name), and parse the coordinates
    data = pd.read_csv(file, skiprows=1)

    # Strip any leading or trailing whitespace from the column names
    data.columns = data.columns.str.strip()

    # Print the column names to debug
    print(f"Columns in {file}: {data.columns}")

    # Open the file again to get the line name
    with open(file, 'r') as f:
        line_name = f.readline().strip()  # Read the first line as the name

    # Plot the line
    plt.plot(data[rows], data[columns], marker='o', label=line_name)

# Get a list of CSV files to plot
csv_files = glob.glob(os.path.join("CSVs", "*.csv"))  # Modify the pattern to match your files

# Create a figure with a specific size and resolution
plt.figure(figsize=(19.2, 10.8), dpi=100)  # 1920x1080 resolution

# Plot each CSV file
for file in csv_files:
    plot_line_from_csv(file)

# Add labels and title
plt.xlabel(rows)
plt.ylabel(columns)
plt.title(title)

# Add a legend to distinguish the lines
plt.legend()

# Save the plot to a file with high resolution
output_file = os.path.join("CSVs", "allocator_tests_plot_1920x1080.png")  # Specify the path and filename
plt.savefig('fig.png', dpi=100)  # dpi=100 ensures 1920x1080 resolution with the figsize set earlier

# Close the plot to avoid displaying it
plt.close()
