# Compiling

```bash
git clone https://github.com/SteppeChange/srt_test.git
cd srt_test
git submodule update --init --recursive
cd libant/thirdparty/srt
./configure
cd ../../
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
export OPENSSL_LIB_DIR=$(brew --prefix openssl)"/lib"
export OPENSSL_INCLUDE_DIR=$(brew --prefix openssl)"/include"
mkdir cmake-build-debug
cd cmake-build-debug
cmake -DENABLE_DEBUG=1 -DENABLE_HEAVY_LOGGING=1 -DOPENSSL_LIB_DIR=${OPENSSL_LIB_DIR} -DOPENSSL_LIBRARIES=${OPENSSL_LIB_DIR}/libcrypto.a ..
make
```

# How to reproduce the problem

1. run srt_test as server:
$ ./srt_test -vvv -l -s 3030
2. see at log and find message like this "libsrt bound to local 1.2.3.4:3031"
3. run srt_test as client:
$ ./srt_test -vvv -s 3020 -t 200 -b 50000 1.2.3.4:3031
4. wait for some time (usually, enough several seconds), if assertion not happens, stop client by Ctrl-C and re-try clause 3-4

# How it works

First client receives DATA packets and sends ACK reply.
Second client sends DATA packets with time stamp and receives ACK. Than calculate rtt. 

This exmaple sends 100kb per 200ms https://github.com/SteppeChange/srt_test/blob/master/SRTTest/SRTTestThread.mm#L25
But problem can be reproduced on other bitrate too. 

Normaly, the rtt is about 0.1 second for 100000 bytes packet. Assert indicates that rtt is huge.

