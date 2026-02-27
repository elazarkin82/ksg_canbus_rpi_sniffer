#!/bin/bash

IFACE="vcan0"

# Check if module is loaded
if ! lsmod | grep -q "^vcan"; then
    echo "Loading vcan module..."
    sudo modprobe vcan
fi

# Check if interface exists
if ! ip link show "$IFACE" > /dev/null 2>&1; then
    echo "Creating $IFACE..."
    sudo ip link add dev "$IFACE" type vcan
fi

echo "Bringing up $IFACE..."
sudo ip link set up "$IFACE"
echo "Done."
