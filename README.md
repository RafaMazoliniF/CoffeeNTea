# ☕ CoffeeNTea 🍵

This repository was created for me and my awesome group to develop our final project for the college course "Operating System Kernel Implementation."
In this project, our goal is to build two Linux kernel modules: **Kfetch** (in Portuguese, the pronunciation of “Kfetch” sounds like the word for “coffee”) and **Tea** (because it's just cool):

1. ☕**Kfetch:** A module that implements a writable/readable **character device** at /dev/kfetch.
When read, it fetches and returns useful system information.
When written to, it selects which information will be fetched.

2. 🍵**Tea**: A module that implements a **procfs** file that displays useful statistics of the processes running on the system.
It asserts a score of how "dangerous" or abnormal is a process behaviour, and use it to classify that process risk as "high", "medium" and "low".

Each of the modules has it's own directory with the source code and more detailed information.

### Execution and Compilation of the Module
# To use the Module 1, follow the instructions below:
KFetch is a linux kernel module that returns you yours system information, like memory usage, CPU model, uptime etc...  

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

## To use the Module 2, follow the instructions below:
1 - Download and set up Ubuntu/Linux on VirtualBox.
2 - Install Kernel Headers, run the following command in the terminal:
```sh
sudo apt-get install linux-headers-$(uname -r)
```
3 - Create a Makefile
Create a file named Makefile in the same directory as your module with the following content:
```sh
obj-m += tea.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

## Execute the Module:
This will create a file named tea.ko:
```sh
make
```
Load the Module
```sh
sudo insmod tea.ko
```
View Kernel Log
```sh
dmesg | tail
``` 
Execute the Module with an auxiliary code
```sh
python3 monitoramento
```
Remove the Module
```sh
sudo rmmod tea.ko
```
## Features:
In this module you will see some information about running processes.
Based on those informations, the module assigns a risk level: High, Medium, or Low.
If the risk is classefied as High, the module will create a file containing the process information.
The information displayed (both in the output and in the file) includes:
```sh
CPU Usage
```
```sh
System Calls
```
```sh
I/O Activity
```
```sh
Network Traffic
```
```sh
Priority
```

### Technologies Used
![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white) ![VSCode](https://img.shields.io/badge/VSCode-0078D4?style=for-the-badge&logo=visual%20studio%20code&logoColor=white) ![virtualbox](https://img.shields.io/badge/VirtualBox-183A61?logo=virtualbox&logoColor=white&style=for-the-badge) ![Python](https://img.shields.io/badge/python-3670A0?style=for-the-badge&logo=python&logoColor=ffdd54)


## People
[@andrecostamarques](https://github.com/andrecostamarques)\
[@danchih](https://github.com/danchih)\
[@flaviamedeiross](https://github.com/flaviamedeiross)\
[@giovanaalarcon](https://github.com/giovanaalarcon)\
[@Yamakasuki](https://github.com/Yamakasuki)\
[@RafaMazoliniF](https://github.com/RafaMazoliniF)\
[@VinizAA](https://github.com/VinizAA)
