I didn't have much time to refine the scripts.
Run all of them from this directory to avoid issues.

./test.lua will generate a lua file ('results.lua') with an entry for each allocator and 10 runtimes for each thread configuration

./format.lua will take results.lua and format its contents into a .csv file saved in the CSVs folder. Each .csv file corresponds to
a line in the final graph

./plot.py will take all .csv files and save the plot in ./fig.png

Example run: ./test.lua ; ./format.lua ; ./plot.py