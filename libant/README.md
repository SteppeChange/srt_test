# How to compile

```bash
git clone https://github.com/SteppeChange/srt_test.git
cd srt_test
git submodule update --init --recursive
cd libant
mkdir cmake-build-debug
cd cmake-build-debug
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
export OPENSSL_LIB_DIR=$(brew --prefix openssl)"/lib"
export OPENSSL_INCLUDE_DIR=$(brew --prefix openssl)"/include"
cmake -DENABLE_DEBUG=1 -DENABLE_HEAVY_LOGGING=1 -DOPENSSL_LIB_DIR=${OPENSSL_LIB_DIR} -DOPENSSL_LIBRARIES=${OPENSSL_LIB_DIR}/libcrypto.a ..
make
```

# How it works

Server receives DATA messages and sends ACK reply.
Client sends DATA messages with time stamp and receives ACK. Than calculate rtt. 

# Problem

Normaly, the rtt is about 0.1 second for 50000-100000 bytes message. Assert indicates that rtt is huge.

# How to reproduce the problem

1. run srt_test as server:
$ ./srt_test -vvv -l -s 3030
2. see log and find message like "libsrt bound to local 1.2.3.4:3031"
3. run srt_test as client, use address from log message:
$ ./srt_test -vvv -s 3020 -t 200 -b 50000 1.2.3.4:3031
4. wait for some time (usually, enough several seconds), if assertion not happens, stop client by Ctrl-C and re-try clause 3-4
Assertion text:
"Assertion failed: (rtt < 2.0), function srt_on_recv, file /Users/alekseydorofeev/src/srt_test/libant/tests/srt_test.cpp, line 455."

