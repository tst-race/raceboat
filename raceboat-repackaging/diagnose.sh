#!/bin/bash

set -e

echo -e "\n\e[1;36m=== Raceboat Diagnostic Script ===\e[0m"

if ! command -v ldd &> /dev/null; then
    echo -e "\e[1;31m[ERROR]\e[0m 'ldd' not found. This script requires basic ELF tools (ldd, file, optionally strace)."
    exit 1
fi

echo -e "\n\e[1;34m[INFO]\e[0m Scanning for ELF binaries in ./bin and ./lib...\n"

targets=$(find bin lib -type f 2>/dev/null | while read -r f; do
    if file "$f" | grep -q "ELF"; then
        echo "$f"
    fi
done)

if [[ -z "$targets" ]]; then
    echo -e "\e[1;33m[WARN]\e[0m No ELF binaries found in bin/ or lib/."
    exit 0
fi

missing_deps=()

for f in $targets; do
    echo -e "\e[1;32m-- Checking $f --\e[0m"
    file "$f"
    
    echo -e "\e[1;34mldd output:\e[0m"
    ldd "$f" | tee /tmp/ldd.tmp
    echo ""

    if grep -q "not found" /tmp/ldd.tmp; then
        echo -e "\e[1;31m[ERROR]\e[0m Missing dependencies found for $f!"
        missing_deps+=("$f")
    fi
done

echo -e "\n\e[1;36m=== Summary ===\e[0m"

if [[ ${#missing_deps[@]} -eq 0 ]]; then
    echo -e "\e[1;32mAll binaries have their dependencies resolved.\e[0m"
else
    echo -e "\e[1;31mThe following binaries have missing dependencies:\e[0m"
    for f in "${missing_deps[@]}"; do
        echo -e " - $f"
    done
fi

if command -v strace &> /dev/null; then
    echo -e "\n\e[1;34m[INFO]\e[0m Optional runtime test with strace? (y/n)"
    read -r answer
    if [[ "$answer" == "y" ]]; then
        for f in $targets; do
            echo -e "\e[1;36mRunning strace on $f (output in /tmp/strace-$(basename "$f").log)\e[0m"
            strace -f -o "/tmp/strace-$(basename "$f").log" "$f" || echo -e "\e[1;33m[WARN]\e[0m $f failed to execute"
        done
    fi
fi
