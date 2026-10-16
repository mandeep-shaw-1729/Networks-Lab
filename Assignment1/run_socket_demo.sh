#!/bin/bash
echo "=== Running Receiver and Sender Socket Demo (Checksum) ==="
./receiver > receiver_chk.log 2>&1 &
RECVPID=$!
sleep 0.5
printf "2\n1\n2\ninput.txt\n" | ./sender > sender_chk.log 2>&1
wait $RECVPID

echo "=== Running Receiver and Sender Socket Demo (CRC-32 with Error Injection) ==="
./receiver > receiver_crc.log 2>&1 &
RECVPID=$!
sleep 0.5
printf "2\n9\n1\n1\ninput.txt\n" | ./sender > sender_crc.log 2>&1
wait $RECVPID
echo "=== Live Socket Transmission Completed ==="
