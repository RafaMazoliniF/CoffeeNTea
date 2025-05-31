#!/bin/bash

PROC_PATH="/proc/mod2"

show_help() {
    cat << EOF
Usage: $0 [OPTION]

Options:
  --show         Display process behavior score table
  --help         Show this help message and exit
EOF
}

if [ $# -eq 0 ]; then
    show_help
    exit 1
fi

for arg in "$@"; do
    case "$arg" in
        --show)
            if [ -f "$PROC_PATH" ]; then
                cat "$PROC_PATH"
            else
                echo "Error: $PROC_PATH not found. Is the module loaded?"
                exit 1
            fi
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Invalid option: $arg"
            show_help
            exit 1
            ;;
    esac
done
