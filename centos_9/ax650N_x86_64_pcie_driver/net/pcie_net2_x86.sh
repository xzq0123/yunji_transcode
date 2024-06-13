#!/bin/bash

action=$1
IP=192.168.1.2
# Function to get the list of current network interfaces
function get_interfaces() {
    ip link show | awk -F: '$0 !~ "lo|vir|wl|docker|^[^0-9]"{print $2;getline}'
}

function start_rc_net()
{
    echo "start pcie rc net..."

    echo 3 > /proc/sys/vm/drop_caches
    # Get the list of network interfaces before inserting the driver
    interfaces_before=$(get_interfaces)
    insmod ./out/ko/ax_pcie_net2.ko
    sleep 0.1
    # Get the list of network interfaces after inserting the driver
    interfaces_after=$(get_interfaces)
    
    # Find the new interface
    new_interface=$(comm -13 <(echo "$interfaces_before" | sort) <(echo "$interfaces_after" | sort))

    if [ -n "$new_interface" ]; then
	    echo "New network interface detected: $new_interface"
    
            # Assign IP address to the new network interface
            sudo ifconfig $new_interface 192.168.1.2 up
            echo "Assigned IP address 192.168.1.2 to $new_interface"
    else
            echo "No new network interface detected."
    fi
    echo "start sucess, IP: ${IP}"
}

function remove_rc_net()
{
    echo "remove pcie rc net..."

    ifconfig ax-net0 down
    rmmod ax_pcie_net2

    echo "remove sucess"
}

main()
{
    if [ "$action" == "up" ]; then
        start_rc_net
    elif [ "$action" == "down" ]; then
        remove_rc_net
    else
        start_rc_net
    fi
}

main
