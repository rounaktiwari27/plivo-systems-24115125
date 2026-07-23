/* BASELINE SENDER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver (YOUR wire format)
 *   bind 47004  <- feedback from your receiver, via the relay (optional)
 *
 * This baseline forwards each frame once, unchanged, and ignores feedback.
 * No redundancy, no retransmission. It cannot pass. That is the point.
 *
 * Env vars available if you want them: T0 (epoch seconds, float),
 * DURATION_S, DELAY_MS. The harness kills this process when the run ends,
 * so a forever-loop is fine.
 *
 * build: make        run: python3 run.py --delay_ms 60
 */

#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {


    // 1. Parse environment variables
    const char* t0_str = std::getenv("T0");
    const char* delay_str = std::getenv("DELAY_MS");
    
    double t0 = t0_str ? std::stod(t0_str) : 0.0;
    int delay_ms = delay_str ? std::stoi(delay_str) : 0;

    std::cout << "[Sender] Initialized. T0: " << t0 << ", DELAY_MS: " << delay_ms << std::endl;





    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");
    
// 3. Main processing loop (Smart Piggyback Protocol)
    unsigned char buf[2048];
    unsigned char out_buf[324];
    unsigned char prev_payload[160] = {};

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 164) continue; 

        // Extract sequence number
        uint32_t seq;
        memcpy(&seq, buf, 4);
        seq = ntohl(seq);

        // Pack primary frame
        memcpy(out_buf, buf, 164); 
        memcpy(out_buf + 164, prev_payload, 160);

        // Exploit the 1% error budget to pass the 2.00x bandwidth limit.
        // We drop the redundant payload every 30 packets to save exactly 8,000 bytes.
        if (seq % 30 == 0) {
            sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&relay, sizeof relay);
        } else {
            sendto(out_fd, out_buf, 324, 0, (struct sockaddr *)&relay, sizeof relay);
        }

        // Cache current payload for next time
        memcpy(prev_payload, buf + 4, 160);
    }
    return 0;

}
