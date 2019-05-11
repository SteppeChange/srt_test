#include <cerrno>
#include <string>
#include <cstring>
#ifdef ANT_UNIT_TESTS
# include <gtest/gtest.h>
#endif

#include "bencode.h"

#define MAJOR_VER 1
#define MINOR_VER 2

#define MAX_MSG_LEN 10000000
#define MAX_BYTE_STRING_LEN 10000000

#ifdef ANT_UNIT_TESTS

namespace ant {

void bencode_parse_byte_string_test();
void bencode_parse_integer_test();

char const *proto_test_00 = ""; // ok

char const *proto_test_01_00 = "i24ei1ei2ei600e4:data5:HELLO"; // ok
char const *proto_test_01_01 = "i23ei1ei2ei600e4:data5:HELLO"; // EPROTO

char const *proto_test_02_01 = "i24ei1ei2"; // EAGAIN
char const *proto_test_02_02 = "ei600e4:dat"; // EAGAIN
char const *proto_test_02_03 = "a5:HELLO"; // ok

char const *proto_test_03_01 = "i24"; // EAGAIN
char const *proto_test_03_02 = "ei1ei2ei600e4:dat"; // EAGAIN
char const *proto_test_03_03 = "a5:HELLOi24ei1ei2ei601e4:data5:"; // ok
char const *proto_test_03_04 = "OLLEH"; // ok

char const *proto_test_04_01 = "i25ei1ei2ei600e4:data6:HELLO1"; // ok
char const *proto_test_04_02 = "i25ei1ei2ei601e4:data6:HELLO2"; // ok
char const *proto_test_04_03 = "i25ei1ei2ei602e4:data6:HELLO3"; // ok

char const *proto_test_05_01 = "i24ei1ei2ei600"; // EAGAIN
//char const *proto_test_05_02 = "e4:data5:HELLO"; // lost segment
char const *proto_test_05_03 = "i25ei1ei2ei600e4:data6:HELLO1"; // ok

char const *proto_test_data = "Hello, world";

TEST(Protocol, bencodeChecking)
{
    ant::Signaling_protocol protocol;
    int rc;

    rc = atoi("0");
    ASSERT_TRUE(rc == 0);
    rc = atoi("0123456789");
    ASSERT_TRUE(rc == 123456789);
    rc = atoi("-0123456789");
    ASSERT_TRUE(rc == -123456789);
    rc = atoi("0e");
    ASSERT_TRUE(rc == 0);
    rc = atoi("0ee");
    ASSERT_TRUE(rc == 0);
    rc = atoi("0ei1e4:data12:Hello, world");
    ASSERT_TRUE(rc == 0);

    bencode_parse_integer_test();
    bencode_parse_byte_string_test();
    protocol.clear();

    std::string command;
    Sign_message data;


    ant::Sign_message out_msg;
    protocol.encode_DATA(out_msg, (uint8_t const *)proto_test_data, strlen(proto_test_data));
    protocol.eat_frame(out_msg.data(), out_msg.size());
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && command == DataCommand && data.size() == strlen(proto_test_data));

    ant::Sign_message out_msg2;
    protocol.encode_ALLOCATE(out_msg2, "test_id");
    protocol.eat_frame(out_msg2.data(), out_msg2.size());
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && command == AllocateCommand && data.size() == strlen("test_id"));

    ant::Sign_message out_msg3;
    protocol.encode_ALLOCATED(out_msg3, 0);
    protocol.eat_frame(out_msg3.data(), out_msg3.size());
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && command == AllocatedCommand && data.size() == 1 && data[0] == '0');

    ant::Sign_message out_msg4;
    protocol.encode_LINKED(out_msg4);
    protocol.eat_frame(out_msg4.data(), out_msg4.size());
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && command == LinkedCommand && data.size() == 1 && data[0] == '0');


    rc = protocol.eat_frame((uint8_t const *)proto_test_00, strlen(proto_test_00));
    ASSERT_EQ(rc, 0);

    rc = protocol.eat_frame((uint8_t const *)proto_test_01_00, strlen(proto_test_01_00));
    ASSERT_EQ(rc, strlen(proto_test_01_00));
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && data.size() == 5);
    ASSERT_EQ(command, DataCommand);

    rc = protocol.eat_frame((uint8_t const *)proto_test_01_01, strlen(proto_test_01_01));
    rc = protocol.ready_message(command, data);
    ASSERT_EQ(rc, EPROTO);

    rc = protocol.eat_frame((uint8_t const *)proto_test_02_01, strlen(proto_test_02_01));
    ASSERT_EQ(rc, strlen(proto_test_02_01));
    rc = protocol.eat_frame((uint8_t const *)proto_test_02_02, strlen(proto_test_02_02));
    ASSERT_EQ(rc, strlen(proto_test_02_01) + strlen(proto_test_02_02));
    rc = protocol.eat_frame((uint8_t const *)proto_test_02_03, strlen(proto_test_02_03));
    ASSERT_EQ(rc, strlen(proto_test_02_01) + strlen(proto_test_02_02) + strlen(proto_test_02_03));
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && data.size() == 5);

    protocol.eat_frame((uint8_t const *)proto_test_03_01, strlen(proto_test_03_01));
    protocol.eat_frame((uint8_t const *)proto_test_03_02, strlen(proto_test_03_02));
    protocol.eat_frame((uint8_t const *)proto_test_03_03, strlen(proto_test_03_03));
    protocol.eat_frame((uint8_t const *)proto_test_03_04, strlen(proto_test_03_04));
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && data.size() == 5);
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && data.size() == 5);
    rc = protocol.ready_message(command, data);
    ASSERT_EQ(rc, EAGAIN);

    protocol.eat_frame((uint8_t const *)proto_test_04_01, strlen(proto_test_04_01));
    protocol.eat_frame((uint8_t const *)proto_test_04_02, strlen(proto_test_04_02));
    protocol.eat_frame((uint8_t const *)proto_test_04_03, strlen(proto_test_04_03));
    int count = 0;
    while (!(rc = protocol.ready_message(command, data))) {
        ASSERT_EQ(data.size(), 6);
        ++count;
    }
    ASSERT_EQ(rc, EAGAIN);
    ASSERT_EQ(count, 3);

    protocol.eat_frame((uint8_t const *)proto_test_05_01, strlen(proto_test_05_01));
    //eat_frame((uint8_t const *)proto_test_05_02, strlen(proto_test_05_02));//lost
    protocol.eat_frame((uint8_t const *)proto_test_05_03, strlen(proto_test_05_03));
    rc = protocol.ready_message(command, data);
    ASSERT_EQ(rc, EINVAL);
    protocol.clear();
    rc = protocol.ready_message(command, data);
    ASSERT_EQ(rc, EAGAIN);

    rc = protocol.eat_frame((uint8_t const *)proto_test_01_00, strlen(proto_test_01_00));
    ASSERT_EQ(rc, strlen(proto_test_01_00));
    rc = protocol.ready_message(command, data);
    ASSERT_TRUE(rc == 0 && data.size() == 5 && command == DataCommand);
}

} // namespace ant

#endif // ANT_UNIT_TESTS

ant::Signaling_protocol::Signaling_protocol()
{
    assert(DataCommandLen == strlen(DataCommand));
    assert(AllocateCommandLen == strlen(AllocateCommand));
    assert(AllocatedCommandLen == strlen(AllocatedCommand));
    assert(LinkedCommandLen == strlen(LinkedCommand));
    assert(OpenCommandLen == strlen(OpenCommand));
}

// ANT message format: [MSG_SIZE][MAJOR_VER][MINOR_VER][MARK][COMMAND][APP_DATA]
// ANT message sample: i61e      i1e        i1e        i1e   4:data   43:xx..xx
void ant::Signaling_protocol::encode_DATA(Sign_message& out_msg, uint8_t const *data, size_t data_len, int mark)
{
    bencode_encode_byte_string(out_msg, data, data_len);
    bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(DataCommand), DataCommandLen); // to do string название не очень хорошее для bytes
    bencode_encode_integer(out_msg, mark);
    bencode_encode_integer(out_msg, MINOR_VER);
    bencode_encode_integer(out_msg, MAJOR_VER);
    bencode_encode_integer(out_msg, out_msg.size());
}

void ant::Signaling_protocol::encode_ALLOCATE(Sign_message& out_msg, std::string conn_id)
{
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const * const>(conn_id.c_str()), conn_id.size());
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(AllocateCommand), AllocateCommandLen); // to do string название не очень хорошее для bytes
	bencode_encode_integer(out_msg, 1);
    bencode_encode_integer(out_msg, MINOR_VER);
    bencode_encode_integer(out_msg, MAJOR_VER);
	bencode_encode_integer(out_msg, out_msg.size());
}

void ant::Signaling_protocol::encode_ALLOCATED(Sign_message& out_msg, int code)
{
	std::string str_code = std::to_string(code);
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(str_code.c_str()), str_code.size()); // to do string название не очень хорошее для bytes
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(AllocatedCommand), AllocatedCommandLen); // to do string название не очень хорошее для bytes
	bencode_encode_integer(out_msg, 1);
    bencode_encode_integer(out_msg, MINOR_VER);
    bencode_encode_integer(out_msg, MAJOR_VER);
	bencode_encode_integer(out_msg, out_msg.size());
}

void ant::Signaling_protocol::encode_LINKED(Sign_message& out_msg)
{
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>("0"),  1); // fake arg
	bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(LinkedCommand), LinkedCommandLen); // to do string название не очень хорошее для bytes
	bencode_encode_integer(out_msg, 1);
    bencode_encode_integer(out_msg, MINOR_VER);
    bencode_encode_integer(out_msg, MAJOR_VER);
	bencode_encode_integer(out_msg, out_msg.size());
}

void ant::Signaling_protocol::encode_OPEN(Sign_message& out_msg)
{
    bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>("0"),  1); // fake arg
    bencode_encode_byte_string(out_msg, reinterpret_cast<uint8_t const *>(OpenCommand), OpenCommandLen); // to do string название не очень хорошее для bytes
    bencode_encode_integer(out_msg, 1);
    bencode_encode_integer(out_msg, MINOR_VER);
    bencode_encode_integer(out_msg, MAJOR_VER);
    bencode_encode_integer(out_msg, out_msg.size());
}

// return 0 of ready message
// return EAGAIN if need more data
// return EINVAL broken stream
// return EPROTO for protocol error
int ant::Signaling_protocol::ready_message(std::string &command, Sign_message &data, int* message_mark, int *major_ver, int *minor_ver, int *expects_bytes)
{
	int rc;
    uint8_t const *ptr = in_buf.data();
    size_t rest_bytes = in_buf.size();
    size_t size_bytes = 0;
    size_t body_size = 0;

	// read message size
	int msg_size = 0;
	rc = bencode_parse_integer(ptr, rest_bytes, &size_bytes, &msg_size);
	if (rc != 0)
		return rc;
	if (in_buf.size() < msg_size + size_bytes) {
	    if(expects_bytes)
            *expects_bytes = msg_size + size_bytes;
        return EAGAIN;
    }
    ptr += size_bytes;
    rest_bytes -= size_bytes;

    if (msg_size > MAX_MSG_LEN)
        return EPROTO;

    // read major version number
    int major = 0;
    rc = bencode_parse_integer(ptr, rest_bytes, &size_bytes, &major);
    if (rc)
        return rc;
    ptr += size_bytes;
    rest_bytes -= size_bytes;
    body_size += size_bytes;
    if (major_ver != nullptr)
        *major_ver = major;

    // read minor version number
    int minor = 0;
    rc = bencode_parse_integer(ptr, rest_bytes, &size_bytes, &minor);
    if (rc)
        return rc;
    ptr += size_bytes;
    rest_bytes -= size_bytes;
    body_size += size_bytes;
    if(minor_ver != nullptr)
        *minor_ver = minor;

    // read message mark
	int mark = 0;
	rc = bencode_parse_integer(ptr, rest_bytes, &size_bytes, &mark);
	if (rc)
		return rc;
    ptr += size_bytes;
    rest_bytes -= size_bytes;
    body_size += size_bytes;
    if(message_mark != nullptr) {
        *message_mark = mark;
    }

	// read command
	std::vector<uint8_t> command_vec;
	rc = bencode_parse_byte_string(ptr, rest_bytes, &size_bytes, command_vec);
	if (rc)
		return rc;
    if (command_vec.size()>COMMAND_MAX_SIZE)
        return EINVAL;
    ptr += size_bytes;
    rest_bytes -= size_bytes;
    body_size += size_bytes;
    command = std::string(command_vec.begin(), command_vec.end());

	// read data
	std::vector<uint8_t> data_vec;
	rc = bencode_parse_byte_string(ptr, rest_bytes, &size_bytes, data_vec);
	if (rc)
		return rc;
    ptr += size_bytes;
    rest_bytes -= size_bytes;
    body_size += size_bytes;
	data = data_vec; // to do можно оптимизировать без выделения памяти

	// remove parsed data from incoming buffer
	in_buf.erase(in_buf.begin(), in_buf.begin() + (ptr - in_buf.data()));

	if(in_buf.size()>0)
	    assert(in_buf[0]=='i');

    if ((size_t)msg_size != body_size)
        return EPROTO;
	return 0;
}

size_t ant::Signaling_protocol::eat_frame(uint8_t const *data, size_t data_len)
{
    in_buf.insert(in_buf.end(), data, data + data_len);
    return in_buf.size();
}

///////////////////////////////////////////////////////////////////////////////

size_t ant::bencode_encode_integer(std::vector<uint8_t> &out_vec, int value)
{
    std::string str = std::to_string(value);
    ant::Sign_message tmp_buf;
    tmp_buf.emplace_back('i');
    tmp_buf.insert(tmp_buf.end(), std::begin(str), std::end(str));
    tmp_buf.emplace_back('e');
    out_vec.insert(out_vec.begin(), tmp_buf.begin(), tmp_buf.end());
    return tmp_buf.size();
}

size_t ant::bencode_encode_byte_string(std::vector<uint8_t> &out_vec, uint8_t const * const data, size_t data_len)
{
    std::string str = std::to_string(data_len);
    ant::Sign_message tmp_buf;
    tmp_buf.insert(tmp_buf.end(), std::begin(str), std::end(str));
    tmp_buf.emplace_back(':');
    tmp_buf.insert(tmp_buf.end(), data, data+data_len);
    out_vec.insert(out_vec.begin(), tmp_buf.begin(), tmp_buf.end());
    return tmp_buf.size();
}

// return 0 if successfully eats eat_bytes
// return EAGAIN if need more data
// return EINVAL broken data
// return another error if can't eats data
/*
 * A byte string (a sequence of bytes, not necessarily characters) is encoded as <length>:<contents>.
 * The length is encoded in base 10, like integers, but must be non-negative (zero is allowed); the contents
 * are just the bytes that make up the string. The string "spam" would be encoded as 4:spam. The specification
 * does not deal with encoding of characters outside the ASCII set; to mitigate this, some BitTorrent applications explicitly
 * communicate the encoding (most commonly UTF-8) in various non-standard ways. This is identical to how netstrings work, except
 * that netstrings additionally append a comma suffix after the byte sequence.
 */
int ant::bencode_parse_byte_string(uint8_t const *data, size_t data_len, size_t *eat_bytes, std::vector<uint8_t> &out_vec)
{
	if (data_len == 0)
		return EAGAIN;

	char const *cdata = reinterpret_cast<char const*>(data);

	size_t str_len = atoi(&cdata[0]);
	if (str_len == 0 && errno)
		return EINVAL;
    if (str_len > MAX_BYTE_STRING_LEN)
        return EPROTO;

    char const *ptr = (char const *)memchr(cdata, ':', data_len);
    if (ptr) {
        size_t n = ptr - cdata;
        size_t full_sz = 1 + n + str_len;
        if (data_len < full_sz) {
            *eat_bytes = 0;
            return EAGAIN;
        }
        out_vec = std::vector<uint8_t>(data + n + 1, data + n + 1 + str_len); // to do without copying
        *eat_bytes = full_sz;
        return 0;
    } else {
        *eat_bytes = 0;
        return EAGAIN;
    }
}

#ifdef ANT_UNIT_TESTS

namespace ant {

char const *byte_string_test_00 = ""; // EAGAIN
char const *byte_string_test_01 = "10"; // EAGAIN
char const *byte_string_test_02 = "10:"; // EAGAIN
char const *byte_string_test_03 = "10:012345678"; // EAGAIN
char const *byte_string_test_04 = "20:0123456789abcdefghij"; // ok
char const *byte_string_test_05 = "20:0123456789abcdefghij0"; // ok
char const *byte_string_test_06 = "10000001:0123456789abcdefghij"; // EPROTO

void bencode_parse_byte_string_test()
{
    int rc;
    size_t eat_bytes;
    std::vector<uint8_t> out_vec;

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_00, strlen(byte_string_test_00), &eat_bytes, out_vec);
    assert(rc == EAGAIN);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_01, strlen(byte_string_test_01), &eat_bytes, out_vec);
    assert(rc == EAGAIN);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_02, strlen(byte_string_test_02), &eat_bytes, out_vec);
    assert(rc == EAGAIN);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_03, strlen(byte_string_test_03), &eat_bytes, out_vec);
    assert(rc == EAGAIN);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_04, strlen(byte_string_test_04), &eat_bytes, out_vec);
    assert(rc == 0 && eat_bytes == strlen(byte_string_test_04) && out_vec.size() == 20);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_05, strlen(byte_string_test_05), &eat_bytes, out_vec);
    assert(rc == 0 && eat_bytes == strlen(byte_string_test_05)-1 && out_vec.size() == 20);

    rc = ant::bencode_parse_byte_string((uint8_t const *)byte_string_test_06, strlen(byte_string_test_06), &eat_bytes, out_vec);
    assert(rc == EPROTO);
}

}
#endif

// return 0 if successfully eats *eat_bytes
// return EAGAIN if need more data
// return EINVAL if data broken
// return another error if can't eats data
/*
 * An integer is encoded as i<integer encoded in base ten ASCII>e.
 * Leading zeros are not allowed (although the number zero is still represented as "0").
 * Negative values are encoded by prefixing the number with a minus sign.
 * The number 42 would thus be encoded as i42e, 0 as i0e, and -42 as i-42e. Negative zero is not permitted.
 */
int ant::bencode_parse_integer(uint8_t const *data, size_t data_len, size_t *eat_bytes, int *value)
{
	if (data_len == 0)
		return EAGAIN;

	if (data[0] != 'i')
		return EINVAL;

	uint8_t const *ptr = reinterpret_cast<uint8_t const *>(memchr(data, 'e', data_len));

	if (ptr == data+1)
		return EINVAL;

    if (ptr) {
		const char * digits = reinterpret_cast<char const *>(&data[1]);
        *value = atoi(digits);
        *eat_bytes = ptr - data + 1; // 'e'+1
        if(*value == 0) {
			if(*digits=='-') digits++;
        	while(*(digits++)=='0');
			digits--;
        	if(*digits=='e')
				return 0;
			else
				return EINVAL;
		}
		if(*value == -1 && digits[0] != '-') // overflow
			return EINVAL;
        return 0;
    } else {
        *eat_bytes = 0;
        return EAGAIN;
    }
}

#ifdef ANT_UNIT_TESTS

namespace ant {

char const *test_data_00 = ""; // EAGAIN
char const *test_data_01 = "i0e"; // ok
char const *test_data_02 = "i-1e"; // ok
char const *test_data_03 = "i-1234567890e"; // ok
char const *test_data_04 = "i1e"; // ok
char const *test_data_05 = "i1234567890e"; // ok
char const *test_data_06 = "i123456789012345678901234567890e"; // overflow
char const *test_data_07 = "i1234567890123456789012345678901e"; // overflow
char const *test_data_08 = "i--1e"; // bad
char const *test_data_09 = "i+1e"; // ok
char const *test_data_10 = "iie"; // bad
char const *test_data_11 = "iee"; // bad
char const *test_data_12 = "0123456789i32e"; // ok
char const *test_data_13 = "abcdefghiji32e"; // ok
char const *test_data_14 = "01234iiiiii32e"; // ok
char const *test_data_15 = "i"; // EAGAIN
char const *test_data_16 = "i1"; // EAGAIN
char const *test_data_17 = "i-"; // EAGAIN
char const *test_data_18 = "i-1"; // EAGAIN
char const *test_data_19 = "ie"; // EINVAL
char const *test_data_20 = "i0eHello"; // ok
char const *test_data_21 = "i000eHello"; // ok
char const *test_data_22 = "i-000eHello"; // ok

void bencode_parse_integer_test()
{
    enum {
        INITIAL = 0xabcdef
    };

    int rc;
    size_t eat_bytes;
    int value = INITIAL;


	rc = ant::bencode_parse_integer((uint8_t const *)test_data_00, strlen(test_data_00), &eat_bytes, &value);
    ASSERT_EQ(rc, EAGAIN);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_01, strlen(test_data_01), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == strlen(test_data_01) && value == 0);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_02, strlen(test_data_02), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == strlen(test_data_02) && value == -1);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_03, strlen(test_data_03), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == strlen(test_data_03) && value == -1234567890);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_04, strlen(test_data_04), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == strlen(test_data_04) && value == 1);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_05, strlen(test_data_05), &eat_bytes, &value);
    assert(rc == 0 && eat_bytes == strlen(test_data_05) && value == 1234567890);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_06, strlen(test_data_06), &eat_bytes, &value);
    ASSERT_TRUE(rc == EINVAL && eat_bytes == strlen(test_data_06) && value == -1);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_07, strlen(test_data_07), &eat_bytes, &value);
    assert(rc == EINVAL && eat_bytes == strlen(test_data_07) && value == -1);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_08, strlen(test_data_08), &eat_bytes, &value);
    ASSERT_TRUE(rc == EINVAL && eat_bytes == strlen(test_data_08));

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_09, strlen(test_data_09), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == strlen(test_data_09) && value == 1);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_10, strlen(test_data_10), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_11, strlen(test_data_11), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_12, strlen(test_data_12), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_13, strlen(test_data_13), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

    value = INITIAL;
    rc = ant::bencode_parse_integer((uint8_t const *)test_data_14, strlen(test_data_14), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

    rc = ant::bencode_parse_integer((uint8_t const *)test_data_15, strlen(test_data_15), &eat_bytes, &value);
    ASSERT_EQ(rc, EAGAIN);

    rc = ant::bencode_parse_integer((uint8_t const *)test_data_16, strlen(test_data_16), &eat_bytes, &value);
    ASSERT_EQ(rc, EAGAIN);

    rc = ant::bencode_parse_integer((uint8_t const *)test_data_17, strlen(test_data_17), &eat_bytes, &value);
    ASSERT_EQ(rc, EAGAIN);

    rc = ant::bencode_parse_integer((uint8_t const *)test_data_18, strlen(test_data_18), &eat_bytes, &value);
    ASSERT_EQ(rc, EAGAIN);

    rc = ant::bencode_parse_integer((uint8_t const *)test_data_19, strlen(test_data_19), &eat_bytes, &value);
    ASSERT_EQ(rc, EINVAL);

	rc = ant::bencode_parse_integer((uint8_t const *)test_data_20, strlen(test_data_20), &eat_bytes, &value);
	ASSERT_TRUE(rc == 0 && eat_bytes == 3 && value == 0);

	rc = ant::bencode_parse_integer((uint8_t const *)test_data_21, strlen(test_data_21), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == 5 && value == 0);

	rc = ant::bencode_parse_integer((uint8_t const *)test_data_22, strlen(test_data_22), &eat_bytes, &value);
    ASSERT_TRUE(rc == 0 && eat_bytes == 6 && value == 0);
}

}  // namespace ant

#endif
