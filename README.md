# compiling

```bash
https://github.com/SteppeChange/srt_test.git
cd srt_test
git submodule update --init --recursive
cd libant/thirdparty/srt
./configure
cd ../../../
XCODE open SRTTest.xcworkspace
```

# How to reproduce the problem

![alt text](https://i.gyazo.com/67cf8f6027646429831819d277efe657.jpg "assert(rtt < 2.0)")

1. run srt_test on first iPhone at local WiFi network
1. Start Server
1. Check logs for local interface: ... [INF] [ANT] libsrt bound to local 192.168.1.50:3011
1. run srt_test on second iPhone (the same network)
1. type first addr
1. Start Client
1. wait for 20 minutes

 
