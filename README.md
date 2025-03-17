# NetProbe - Network Performance Testing Tool
NetProbe is a comprehensive network performance testing toolkit designed for measuring, analyzing, and optimizing TCP/UDP network performance. It offers a flexible client-server architecture with support for both HTTP and HTTPS protocols, making it ideal for diagnosing network issues, benchmarking network performance, and validating network configurations.<br><br>

## Features
**Protocol Support:** TCP, UDP, HTTP, and HTTPS<br>

**Testing Modes:** <br>
Send Mode: Measure throughput by sending data<br>
Receive Mode: Measure throughput by receiving data<br>
Response Mode: Measure round-trip time and jitter<br>
HTTP/HTTPS Mode: Test web server performance<br>

**Metric Collection:** <br>
Throughput (bandwidth) measurement<br>
Packet loss statistics<br>
Latency (round-trip time)<br>
Jitter (variation in latency)<br>

**Advanced Capabilities:**<br>
Configurable packet size and sending rate<br>
Adjustable buffer sizes<br>
Persistent and non-persistent connections<br>
Customizable TCP congestion control algorithms<br>
Dynamic thread pool for efficient connection handling<br>

**Cross-Platform Support**: Works on both Linux and Windows<br>
**HTTPS Security:** Certificate validation and SSL/TLS support<br>

## Design Principles
**Modularity:** Clear separation of components with well-defined interfaces
**Resource Efficiency:** Proper resource management using RAII principles
**Thread Safety:** Safe concurrent operations using mutexes and atomic variables
**Error Handling:** Comprehensive error detection and reporting
**Cross-Platform Compatibility:** Works seamlessly on both Linux and Windows
**Extensibility:** Easy to add new protocols or test modes


# Server Options<br>

| Option | Description | Default |
|--------|-------------|---------|
| --lhost | Local host address to bind to | IN_ADDR_ANY |
| --lport | Main listening port for TCP/UDP connections | 4180 |
| --lhttpport | HTTP server port | 4080 |
| --lhttpsport | HTTPS server port | 4081 |
| --sbufsize | Send buffer size (bytes) | 0 (system default) |
| --rbufsize | Receive buffer size (bytes) | 0 (system default) |
| --tcpcca | TCP congestion control algorithm | System default |
| --poolsize | Thread pool size | 8 |

<br>

# Client Options<br>
| Option | Description | Default |
|--------|-------------|---------|
| --send | Send mode | N/A |
| --recv | Receive mode | N/A |
| --response | Response mode | N/A |
| --http URL | HTTP/HTTPS mode with URL | N/A |
| --stat | Statistics reporting interval (ms) | 500 |
| --rhost | Remote host address | localhost |
| --rport | Remote port | 4180 |
| --proto | Protocol (TCP or UDP) | UDP |
| --pktsize | Packet size (bytes) | 1000 |
| --pktrate | Packet rate (bytes/second) | 1000 |
| --pktnum | Number of packets (0 for infinite) | 0 |
| --sbufsize | Send buffer size (bytes) | 0 (system default) |
| --rbufsize | Receive buffer size (bytes) | 0 (system default) |
| --presist | Connection persistence (Yes or No) | No |
| --file | Output file for HTTP responses | /dev/null |