Pod::Spec.new do |s|
  s.name         = "LibANT"
  s.version      = "1.14"
  s.summary      = "LibANT"
  s.description  = <<-DESC
  PeerScope .
  DESC
  s.homepage     = "https://steppechange.com/"
  s.author       = {
    "Aleksei Dorofeev" => "aleksei.dorofeev@steppechange.com",
    "Oleg Golosovskiy" => "oleggl@steppechange.com",
  }
  
  s.ios.deployment_target  = '9.0'
  s.osx.deployment_target  = '10.12'
  #s.tvos.deployment_target = '9.0'
  s.source       = { :path => './' }
  
  s.default_subspec = 'ant'
  
  s.xcconfig = {
    'GCC_WARN_64_TO_32_BIT_CONVERSION' => 'NO',
    'GCC_PREPROCESSOR_DEFINITIONS[config=Debug]' => '_DEBUG=1 DEBUG=1',
    'GCC_PREPROCESSOR_DEFINITIONS[config=Release]' => 'NDEBUG NS_BLOCK_ASSERTIONS'
  }
  
  s.subspec 'ant' do |ant|
    ant.source_files = "src/*.{h,hpp,cpp}", "include/**/*.h"
    ant.public_header_files = "include/**/*.h"
    
    ant.dependency 'LibANT/libsrt'
    ant.dependency 'LibANT/libbtdht'
    
    ant.compiler_flags = "-Wall -g -fPIC -DPOSIX"
  end
  
  s.subspec 'libsrt' do |libsrt|
      libsrt.source_files =
      "thirdparty/srt/version.h",
      "thirdparty/srt/srtcore/buffer.h",
      "thirdparty/srt/srtcore/common.cpp",
      "thirdparty/srt/srtcore/crypto.h",
      "thirdparty/srt/srtcore/handshake.h",
      "thirdparty/srt/srtcore/md5.cpp",
      "thirdparty/srt/srtcore/platform_sys.h",
      "thirdparty/srt/srtcore/srt.h",
      "thirdparty/srt/srtcore/threadname.h",
      "thirdparty/srt/srtcore/window.h",
      "thirdparty/srt/srtcore/cache.cpp",
      "thirdparty/srt/srtcore/common.h",
      "thirdparty/srt/srtcore/epoll.cpp",
      "thirdparty/srt/srtcore/list.cpp",
      "thirdparty/srt/srtcore/md5.h",
      "thirdparty/srt/srtcore/queue.cpp",
      "thirdparty/srt/srtcore/srt4udt.h",
      "thirdparty/srt/srtcore/udt.h",
      "thirdparty/srt/srtcore/api.cpp",
      "thirdparty/srt/srtcore/cache.h",
      "thirdparty/srt/srtcore/core.cpp",
      "thirdparty/srt/srtcore/epoll.h",
      "thirdparty/srt/srtcore/list.h",
      "thirdparty/srt/srtcore/netinet_any.h",
      "thirdparty/srt/srtcore/queue.h",
      "thirdparty/srt/srtcore/srt_c_api.cpp",
      "thirdparty/srt/srtcore/utilities.h",
      "thirdparty/srt/srtcore/api.h",
      "thirdparty/srt/srtcore/channel.cpp",
      "thirdparty/srt/srtcore/core.h",
      "thirdparty/srt/srtcore/logging.h",
      "thirdparty/srt/srtcore/packet.cpp",
      "thirdparty/srt/srtcore/smoother.cpp",
      "thirdparty/srt/srtcore/srt_compat.c",
      "thirdparty/srt/srtcore/buffer.cpp",
      "thirdparty/srt/srtcore/channel.h",
      "thirdparty/srt/srtcore/crypto.cpp",
      "thirdparty/srt/srtcore/handshake.cpp",
      "thirdparty/srt/srtcore/logging_api.h",
      "thirdparty/srt/srtcore/packet.h",
      "thirdparty/srt/srtcore/smoother.h",
      "thirdparty/srt/srtcore/srt_compat.h",
      "thirdparty/srt/srtcore/window.cpp",
      
      "thirdparty/srt/haicrypt/haicrypt_log.cpp",
      "thirdparty/srt/haicrypt/hc_openssl_aes.c",
      #"thirdparty/srt/haicrypt/hcrypt-gnutls.c",
      "thirdparty/srt/haicrypt/hcrypt.c",
      "thirdparty/srt/haicrypt/hcrypt.h",
      "thirdparty/srt/haicrypt/hcrypt_ctx_rx.c",
      "thirdparty/srt/haicrypt/hcrypt_rx.c",
      #"thirdparty/srt/haicrypt/hcrypt_ut.c",
      "thirdparty/srt/haicrypt/haicrypt_log.h",
      "thirdparty/srt/haicrypt/hc_openssl_evp_cbc.c",
      #"thirdparty/srt/haicrypt/hcrypt-gnutls.h",
      "thirdparty/srt/haicrypt/hcrypt_ctx_tx.c",
      "thirdparty/srt/haicrypt/hcrypt_sa.c",
      "thirdparty/srt/haicrypt/hcrypt_xpt_srt.c",
      "thirdparty/srt/haicrypt/haicrypt.h",
      #"thirdparty/srt/haicrypt/hc_nettle_aes.c",
      "thirdparty/srt/haicrypt/hc_openssl_evp_ctr.c",
      "thirdparty/srt/haicrypt/hcrypt-openssl.h",
      "thirdparty/srt/haicrypt/hcrypt_ctx.h",
      "thirdparty/srt/haicrypt/hcrypt_msg.h",
      "thirdparty/srt/haicrypt/hcrypt_tx.c",
      "thirdparty/srt/haicrypt/hcrypt_xpt_sta.c"
      
      libsrt.private_header_files = "**/*.{h,hpp}"
     libsrt.compiler_flags = "-DSRT_VERSION='\"1.3.1\"'", "-DHAICRYPT_USE_OPENSSL_AES", "-DHAICRYPT_USE_OPENSSL_EVP=1", "-DHAI_ENABLE_SRT=1", "-DHAI_PATCH=1", "-DOSX=1", "-DSRT_DYNAMIC", "-DSRT_EXPORTS", "-D_GNU_SOURCE", "-DENABLE_LOGGING=1", "-DENABLE_HEAVY_LOGGING=1", "-Wall -Wextra -g -fPIC"
     #      libsrt.compiler_flags = "-DSRT_VERSION='\"1.3.1\"'", "-DHAICRYPT_USE_OPENSSL_AES", "-DHAICRYPT_USE_OPENSSL_EVP=1", "-DHAI_ENABLE_SRT=1", "-DHAI_PATCH=1", "-DOSX=1", "-DSRT_DYNAMIC", "-DSRT_EXPORTS", "-D_GNU_SOURCE", "-Wall -Wextra -g -fPIC"

      libsrt.ios.pod_target_xcconfig = {
          'HEADER_SEARCH_PATHS' => '/usr/local/opt/openssl-ios/include',
          'LIBRARY_SEARCH_PATHS' => '/usr/local/opt/openssl-ios/lib',
      }

      libsrt.osx.pod_target_xcconfig = {
        'HEADER_SEARCH_PATHS' => '/usr/local/opt/openssl/include',
        'LIBRARY_SEARCH_PATHS' => '/usr/local/opt/openssl/lib',
      }

      libsrt.pod_target_xcconfig = {
        'OTHER_LDFLAGS' => '-lssl -lcrypto'
      }

      ##  -std=gnu++11
  end
  
  s.subspec 'libbtdht' do |libbtdht|
    libbtdht.source_files = "thirdparty/libbtdht/libbtutils/src/bench-json.h",
    "thirdparty/libbtdht/libbtutils/src/bencoding.cpp",
    "thirdparty/libbtdht/libbtutils/src/bencoding.h",
    "thirdparty/libbtdht/libbtutils/src/bencparser.cpp",
    "thirdparty/libbtdht/libbtutils/src/bencparser.h",
    "thirdparty/libbtdht/libbtutils/src/bitfield.cpp",
    "thirdparty/libbtdht/libbtutils/src/bitfield.h",
    "thirdparty/libbtdht/libbtutils/src/bloom_filter.cpp",
    "thirdparty/libbtdht/libbtutils/src/bloom_filter.h",
    "thirdparty/libbtdht/libbtutils/src/DecodeEncodedString.cpp",
    "thirdparty/libbtdht/libbtutils/src/DecodeEncodedString.h",
    "thirdparty/libbtdht/libbtutils/src/endian_utils.h",
    "thirdparty/libbtdht/libbtutils/src/enumtype.h",
    "thirdparty/libbtdht/libbtutils/src/get_microseconds.cpp",
    "thirdparty/libbtdht/libbtutils/src/get_microseconds.h",
    "thirdparty/libbtdht/libbtutils/src/inet_ntop.cpp",
    "thirdparty/libbtdht/libbtutils/src/inet_ntop.h",
    "thirdparty/libbtdht/libbtutils/src/interlock.cpp",
    "thirdparty/libbtdht/libbtutils/src/interlock.h",
    "thirdparty/libbtdht/libbtutils/src/invariant_check.hpp",
    "thirdparty/libbtdht/libbtutils/src/RefBase.cpp",
    "thirdparty/libbtdht/libbtutils/src/RefBase.h",
    "thirdparty/libbtdht/libbtutils/src/sha1_hash.h",
    "thirdparty/libbtdht/libbtutils/src/smart_ptr.h",
    "thirdparty/libbtdht/libbtutils/src/snprintf.cpp",
    "thirdparty/libbtdht/libbtutils/src/snprintf.h",
    "thirdparty/libbtdht/libbtutils/src/sockaddr.cpp",
    "thirdparty/libbtdht/libbtutils/src/sockaddr.h",
    "thirdparty/libbtdht/libbtutils/src/tailqueue.h",
    "thirdparty/libbtdht/libbtutils/src/target.h",
    "thirdparty/libbtdht/libbtutils/src/udp_utils.cpp",
    "thirdparty/libbtdht/libbtutils/src/udp_utils.h",
    "thirdparty/libbtdht/libbtutils/src/utypes.h",
    "thirdparty/libbtdht/src/blockallocator.cpp",
    "thirdparty/libbtdht/src/blockallocator.h",
    "thirdparty/libbtdht/src/Buffer.h",
    "thirdparty/libbtdht/src/crc32c.cpp",
    "thirdparty/libbtdht/src/dht.cpp",
    "thirdparty/libbtdht/src/dht.h",
    "thirdparty/libbtdht/src/DhtImpl.cpp",
    "thirdparty/libbtdht/src/DhtImpl.h",
    "thirdparty/libbtdht/src/DHTMessage.cpp",
    "thirdparty/libbtdht/src/DHTMessage.h",
    "thirdparty/libbtdht/src/ExternalIPCounter.cpp",
    "thirdparty/libbtdht/src/ExternalIPCounter.h",
    "thirdparty/libbtdht/src/logger.h"
    libbtdht.private_header_files = "**/*.{h,hpp}"
    libbtdht.compiler_flags = "-Wall -g -fPIC -DPOSIX"
  end
end
