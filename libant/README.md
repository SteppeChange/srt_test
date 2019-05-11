# libant
Distributed transport library
  
Dependences:  
no
    
sudo apt-get install build-essential git  
sudo apt-get install cmake  
git clone https://github.com/SteppeChange/libant.git  
cd libant/  
git submodule update --init --recursive  
mkdir build  
cd build/  
cmake ..  
make  

# kad_test
run node with 2 bootstrap nodes  
$ ./kad_test -vv -b dev.boot1.peeramid.video:6883 -b dev.boot2.peeramid.video:6883  

# utp_test
run server at port 3010 (by default):  
$ ./utp_test -vvv -l -T 1000  
run client at port 3011 with sending random buffer size of 10240 (option -b) every 10 ms (option -t):  
$ ./utp_test -vvv -s 3011 -b 10240 -t 10 -T 60 192.168.88.187:3010  

run client at port 3011 with sending buffer loaded from file:  
$ ./utp_test -vvv -s 3011 -b 10240 -f CMakeCache.txt -t 10 -T 60 192.168.88.187:3010  

# ant_test
run broadcaster:  
./ant_test -vvv -s 3010 -t 15CF4179D95337A5617CA266CD29825CE7C54550 -T 1000 -B 4000  
run listener:  
./ant_test -vvv -s 3011 -l 15CF4179D95337A5617CA266CD29825CE7C54550 -T 1000  

run listener with boot node:  
./ant_test -vvv -s 3011 -b ec2-54-204-226-84.compute-1.amazonaws.com:6883 -l 15CF4179D95337A5617CA266CD29825CE7C54550 -T 1000  

run listener with relay server:
./ant_test -vvv -s 3011 -r 34.207.207.185:3010 -l 15CF4179D95337A5617CA266CD29825CE7C54550 -T 1000  
