#!/bin/bash
# QzRAM Setup - Automated zRAM setup and optimization script
# Must be run with root privileges (sudo)

set -e

# Calculate size: half of the total available physical RAM
TOTAL_MEM=$(free -m | awk '/^Mem:/{print $2}')
ZRAM_SIZE=$(( TOTAL_MEM / 2 ))

# Load required kernel modules
modprobe zram || { echo "[-] Error: zram module is not available"; exit 1; }
modprobe qzram || { echo "[-] Error: qzram module is not loaded. Please install it first."; exit 1; }

# Find a free zram device
ZRAM_DEV=$(zramctl -f 2>/dev/null)
if [ -z "$ZRAM_DEV" ]; then
    # Fallback: manually attempt to hot-add zram0 if none are free
    ZRAM_DEV=zram0
    if [ ! -b /dev/zram0 ]; then
        echo 1 > /sys/class/zram-control/hot_add 2>/dev/null || { echo "[-] Error: Failed to manually create zram device"; exit 1; }
    fi
fi
echo "[+] Active device selected: /dev/${ZRAM_DEV}"

# Assign qzram as the compression algorithm
echo qzram > /sys/block/${ZRAM_DEV}/comp_algorithm || { echo "[-] Error: Failed to set compression algorithm. Is qzram module loaded?"; exit 1; }

# Set the disk size to half of physical RAM
echo ${ZRAM_SIZE}M > /sys/block/${ZRAM_DEV}/disksize

# Format device as swap and activate with highest priority
mkswap /dev/${ZRAM_DEV} > /dev/null
swapon -p 32767 /dev/${ZRAM_DEV}

echo "[==>] Success! QzRAM swap activated on /dev/${ZRAM_DEV} with size ${ZRAM_SIZE}MB and priority 32767"
