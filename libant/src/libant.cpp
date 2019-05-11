#include <iostream>
#include <sstream>
#include <random>
#include <ctype.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <utility>

#include "utils.hpp"
#include "channel_statistics.h"
#include "libsrt.h"

static const int PUNCH_TEST_TIMEOUT = 4;
static const int PUNCH_PROBE_COUNT = 3;

static const int PING_REQUEST_TIMEOUT = 4;
static const int PING_PROBE_COUNT = 2;


static const char *node_state_to_str(int state);
static const char *node_event_to_str(int event);

static void close_srt_connection_on_thread(ant::Srt::ptr srt, ant::Srt_connection_id const& conn_id)
{
    srt->close(conn_id);
}
