strace -f -o out.txt ./server A 10 5000 &
sleep .2
strace -f -o out2.txt ./client B 5000