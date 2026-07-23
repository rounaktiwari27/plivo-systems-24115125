# Architecture Notes

The graded playout delay should be evaluated at **200 ms**.

The system utilizes a stateless N-1 Piggyback protocol on the sender and a strict event-driven Jitter Buffer on the receiver. To achieve zero Round-Trip-Time (RTT) recovery, the sender duplicates the previous frame's payload into every packet, allowing immediate recovery of isolated network drops. To strictly comply with the 2.00x bandwidth cap, the sender intentionally omits the redundant payload on every 30th packet, exploiting the 1.00% error budget to solve the mathematical bandwidth constraint. The receiver utilizes a non-blocking `poll()` event loop that drains the network socket completely before evaluating strict playback deadlines against `gettimeofday()`. 

The primary vulnerability of this design is burst packet loss. Because redundancy is limited to N-1, if the network drops two sequential packets, the first packet is permanently lost. A delay of 200ms was selected to absorb severe jitter spikes, ensuring the remaining burst-drop misses stay mathematically below the 1.00% failure cap on hostile profiles.