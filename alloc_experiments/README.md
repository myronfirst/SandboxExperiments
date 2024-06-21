# AllocExperiments

## Dependencies

### PMDK
- libpmem
- libpmemobj
- libpmemobj C++ bindings
```
sudo apt install libpmem-dev  libpmemobj-dev libpmemobj-cpp-dev
sudo apt install libpmem1-debug  libpmemobj1-debug
sudo apt install libpmemobj-cpp-doc
```

### libvmmem
- libvmmem
- libvmmalloc

- Build from source. Follow instructions in repo
```
git clone https://github.com/pmem/vmem
cd vmem
git checkout tags/1.8
make
sudo make install
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/lib"
```
- Then add `-lvmmalloc` linker flag to Makefile. This replaces the default memory allocator for the binary.
