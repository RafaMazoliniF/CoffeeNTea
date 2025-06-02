#!/bin/bash

KFETCH_RELEASE=1
KFETCH_CPU_MODEL=2
KFETCH_NUM_CPUS=4
KFETCH_MEM=8
KFETCH_NUM_PROCS=16
KFETCH_UPTIME=32

MASK=0

show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
  --release       Include kernel release information
  --cpu_model     Include CPU model information
  --num_cpus      Include number of CPUs information
  --mem           Include memory usage information
  --num_procs     Include number of processes information
  --uptime        Include system uptime information
  --help          Show this help message and exit

You can combine multiple options. Example:
  $0 --release --mem --uptime
EOF
}


# If 0 flags, use default (0)
if [ $# -eq 0 ]; then
    MASK=0
fi


# Defines flags
for arg in "$@"; do
    case "$arg" in
        --release)
            MASK=$((MASK | KFETCH_RELEASE))
            ;;
        --cpu_model)
            MASK=$((MASK | KFETCH_CPU_MODEL))
            ;;
        --num_cpus)
            MASK=$((MASK | KFETCH_NUM_CPUS))
            ;;
        --mem)
            MASK=$((MASK | KFETCH_MEM))
            ;;
        --num_procs)
            MASK=$((MASK | KFETCH_NUM_PROCS))
            ;;
        --uptime)
            MASK=$((MASK | KFETCH_UPTIME))
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Invalid flag: $arg"
            echo "Use --help for usage information."
            exit 1
            ;;
    esac
done

# Write the mask to the device (must be run with sudo)
echo "$MASK" > /dev/kfetch

# Read and display the output from the device
cat /dev/kfetch
