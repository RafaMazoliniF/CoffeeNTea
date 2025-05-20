# Kfetch - Get your system information
KFetch is a linux kernel module that returns you yours system information, like memory usage, CPU model, uptime etc...

## How to use it
It is simple. Follow this guide and everything will be ok :)

### Prerequisites
- Linux operating system installed on your host or VM;
- I only tested on **Ubuntu** based distros with kernel **6.8.0-59-generic** (you can get your kernel version with `uname -r`);
- Packages **build_essential** and **kmod** installed;

### Compile and Run the Module
This project has only four files: 
- `kfetch.c`: the source code of the module;
- `kfetch.h`: header file + some helper functions;
- `Makefile`: compiles the module;
- `kfetch.sh`: helper bash script that writes in and reads the character device with some usage flags;

#### Compile
```bash
$ make
```
#### Run
```bash
$ sudo insmod kfetch.ko && sudo ./kfetch.sh
```
#### Stop
```bash
$ sudo rmmod kfetch && make clean
```
