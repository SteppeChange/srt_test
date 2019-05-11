#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace ant {

	char const* const DataCommand = "data";
	size_t const DataCommandLen = 4;
	char const* const AllocateCommand = "allocate";
    size_t const AllocateCommandLen = 8;
	char const* const AllocatedCommand = "allocated";
    size_t const AllocatedCommandLen = 9;
	char const* const LinkedCommand = "linked";
    size_t const LinkedCommandLen = 6;
    char const* const OpenCommand = "open";
    size_t const OpenCommandLen = 4;

    #define COMMAND_MAX_SIZE 9 // strlen(AllocatedCommand)

	/*
	allocate(connection_id) - завершает аллокацию соединения между клиентом и  сервером.
	allocated(error) - подтверждает алокацию или отказывает с кодом ошибки. Коды:
		0 - success
		1 - duplicate
		2 - oncoming connection
	linked() - подтверждает появление соединения с второй стороной
	 */

    size_t bencode_encode_integer(std::vector<uint8_t> &out_vec, int value);
    size_t bencode_encode_byte_string(std::vector<uint8_t> &out_vec, uint8_t const *data, size_t data_len);
    int bencode_parse_byte_string(uint8_t const *data, size_t data_len, size_t *eat_bytes, std::vector<uint8_t> &out_vec);
    int bencode_parse_integer(uint8_t const *data, size_t data_len, size_t *eat_bytes, int *value);

    using Sign_message = std::vector<uint8_t>;

    class Signaling_protocol {
    public:
        Signaling_protocol();

        void encode_DATA(Sign_message& out_msg, uint8_t const *data, size_t data_len, int mark = 1);
		void encode_ALLOCATE(Sign_message& out_msg, std::string conn_id);
		void encode_ALLOCATED(Sign_message& out_msg, int code);
		void encode_LINKED(Sign_message& out_msg);
        void encode_OPEN(Sign_message& out_msg);

        // return buffer size
        size_t eat_frame(uint8_t const *data, size_t data_len);
        // return 0 of ready message
        // return EAGAIN if need more data
        int ready_message(std::string &command, Sign_message &data,
                          int* mark = nullptr, int *major = nullptr, int *minor = nullptr, int *expects_bytes = nullptr);
        // clear buffer
        inline void clear() {
            in_buf.clear();
        }

        size_t size() const {
			return in_buf.size();
        }

    private:
        Sign_message in_buf;
    };

}
