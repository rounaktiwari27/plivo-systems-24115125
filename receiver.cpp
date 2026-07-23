/* BASELINE RECEIVER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender, via the relay (optional)
 *
 * This baseline forwards whatever arrives straight to the player: lost
 * frames stay lost, late frames stay late, duplicates are re-sent
 * harmlessly. All yours to fix — jitter buffer, reordering, recovery.
 *
 * Env vars available: T0, DURATION_S, DELAY_MS. Harness kills the process
 * at run end; a forever-loop is fine.
 */
#include <iostream>
#include <cstdlib>
#include <string>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <array>

// Get current epoch time in seconds
double get_now_s() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}

struct Frame {
    bool present = false;
    unsigned char payload[160];
};

int main(void) {
    const char* t0_str = std::getenv("T0");
    const char* delay_str = std::getenv("DELAY_MS");
    double t0 = t0_str ? std::stod(t0_str) : 0.0;
    int delay_ms = delay_str ? std::stoi(delay_str) : 0;

    std::cout << "[Receiver] Init. T0: " << t0 << ", DELAY_MS: " << delay_ms << std::endl;

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr);

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 1024 slots = ~20 seconds of buffer depth. Very safe.
    std::array<Frame, 1024> buffer;
    uint32_t expected_seq = 0;
    unsigned char in_buf[2048];
    unsigned char out_buf[164];

    for (;;) {
        // 1. DRAIN THE NETWORK COMPLETELY (0 timeout)
        while (true) {
            struct pollfd pfd = { in_fd, POLLIN, 0 };
            int p = poll(&pfd, 1, 0); 
            if (p > 0 && (pfd.revents & POLLIN)) {
                ssize_t n = recvfrom(in_fd, in_buf, sizeof in_buf, 0, NULL, NULL);
                if (n >= 164) {
                    uint32_t seq;
                    memcpy(&seq, in_buf, 4);
                    seq = ntohl(seq);

                    buffer[seq % 1024].present = true;
                    memcpy(buffer[seq % 1024].payload, in_buf + 4, 160);

                    // Recover N-1 frame if it's our 324-byte redundant packet
                    if (n == 324 && seq > 0) {
                        uint32_t prev = (seq - 1) % 1024;
                        if (!buffer[prev].present) {
                            buffer[prev].present = true;
                            memcpy(buffer[prev].payload, in_buf + 164, 160);
                        }
                    }
                }
            } else {
                break; // Socket is entirely empty
            }
        }

        // 2. PLAYOUT TIMED-OUT & READY FRAMES
        while (true) {
            double target_time_s = t0 + (delay_ms / 1000.0) + (expected_seq * 0.020);
            
            // We fire 2ms early to ensure transit time to the python player
            if (get_now_s() < (target_time_s - 0.002)) {
                break; 
            }

            uint32_t index = expected_seq % 1024;
            if (buffer[index].present) {
                uint32_t net_seq = htonl(expected_seq);
                memcpy(out_buf, &net_seq, 4);
                memcpy(out_buf + 4, buffer[index].payload, 160);
                
                sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof player);
                buffer[index].present = false; 
            }
            expected_seq++; 
        }

        // 3. SLEEP UNTIL NEXT EVENT
        double next_target = t0 + (delay_ms / 1000.0) + (expected_seq * 0.020);
        double wait_s = (next_target - 0.002) - get_now_s();
        int timeout_ms = (int)(wait_s * 1000.0);
        
        if (timeout_ms > 0) {
            struct pollfd pfd = { in_fd, POLLIN, 0 };
            poll(&pfd, 1, timeout_ms);
        }
    }
    return 0;
}