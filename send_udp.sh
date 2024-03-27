#!/bin/bash

# Configuration for UDP communication
HOST="localhost"
PORT="4567"

# Function to convert hex input to bytes
function hex_to_bytes {
    local hex_msg="${1:2}" # Remove '0x' prefix
    local byte_str=""
    for ((i=0; i<${#hex_msg}; i+=2)); do
        local byte_pair="${hex_msg:$i:2}"
        byte_str+="\\x$byte_pair"
    done
    echo -ne "$byte_str"
}

# Function to send a message
function send_message {
    local msg="$1"
    if [[ $msg =~ ^0x[0-9A-Fa-f]+ ]]; then
        # If the message is a hex string, convert and send as bytes
        send=$(hex_to_bytes "$msg")
        echo $send | nc -u -w1 $HOST $PORT
    else
        # For any other input, replace spaces with null bytes and send as bytes
        msg=${msg// /\\x00}
        echo -ne "$msg" | nc -u -w1 $HOST $PORT
    fi
}

# Main loop
while true; do
    # Wait for the user to input a message
    read -p "Enter message (hex format with '0x' or any text, 'exit' to quit): " user_input

    # If the user wants to exit the script
    if [[ "$user_input" == "exit" ]]; then
        # Exit the loop
        exit 0
    fi

    # Send the user's message
    send_message "$user_input"
done
