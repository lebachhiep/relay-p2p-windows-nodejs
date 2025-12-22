# Relay Leaf Node.js SDK for Windows
Node.js SDK for P2P relay connections on Windows x64.

---

## Project Structure
```
.
├── relay_leaf.node          # Node.js addon
├── relay_leaf_win64.dll     # Required DLL
└── index.js                 # Demo
```

---

## Setup

### Download Files
Download from:
```
https://github.com/lebachhiep/relay-leaf-library/releases/
```

Required files:
- `relay_leaf.node`
- `relay_leaf_win64.dll`

Place them in your project root.

### Run Demo
```bash
node index.js
```

---

## Basic Usage
```javascript
const relay = require('./relay_leaf.node');

// Create and start client
const client = new relay.RelayClient({
    discoveryUrl: "https://api.prx.network/public/relay/nodes",
    partnerId: "",
    proxies: [],
    verbose: false
});

client.start();

// Monitor connection
setInterval(() => {
    const stats = client.stats();
    console.log(
        `Connected: ${stats.connected} | ` +
        `Nodes: ${stats.connectedNodes} | ` +
        `Streams: ${stats.activeStreams}`
    );
}, 2000);

// Graceful shutdown
process.on('SIGINT', () => {
    client.stop();
    process.exit(0);
});
```

---

## Configuration

### Options
```javascript
{
    discoveryUrl: string,  // API endpoint (optional)
    partnerId: string,     // Partner ID (optional)
    proxies: string[],     // Proxy URLs (optional)
    verbose: boolean       // Enable logs (optional)
}
```

### Partner ID (Optional)
```javascript
const client = new relay.RelayClient({
    partnerId: "your-partner-id"
});
```

### Proxy Configuration

#### Single Proxy
```javascript
const client = new relay.RelayClient({
    proxies: ["socks5://127.0.0.1:1080"]
});
```

#### Multiple Proxies
```javascript
const client = new relay.RelayClient({
    proxies: [
        "socks5://127.0.0.1:1080",
        "http://127.0.0.1:8080"
    ]
});
```

#### Proxy with Authentication
```javascript
const client = new relay.RelayClient({
    proxies: [
        "socks5://user:pass@127.0.0.1:1080",
        "http://user:pass@127.0.0.1:8080"
    ]
});
```

### Supported Proxy Formats
- HTTP: `http://host:port`
- HTTP with auth: `http://user:pass@host:port`
- SOCKS5: `socks5://host:port`
- SOCKS5 with auth: `socks5://user:pass@host:port`

### Full Configuration Example
```javascript
const client = new relay.RelayClient({
    discoveryUrl: "https://api.prx.network/public/relay/nodes",
    partnerId: "your-partner-id",  // Optional
    proxies: [                      // Optional
        "socks5://user:pass@127.0.0.1:1080",
        "http://127.0.0.1:8080"
    ],
    verbose: true
});
```

---

## API Methods

### Create Client
```javascript
const relay = require('./relay_leaf.node');

const client = new relay.RelayClient({
    partnerId: "your-partner-id",
    proxies: ["socks5://127.0.0.1:1080"]
});
```

### Start/Stop
```javascript
client.start()    // Start P2P connection
client.stop()     // Stop and cleanup
```

### Get Stats
```javascript
const stats = client.stats();

// Stats object:
stats.connected          // boolean
stats.connectedNodes     // number
stats.activeStreams      // number
stats.bytesSent          // number/BigInt
stats.bytesReceived      // number/BigInt
```

### Get Version
```javascript
const version = relay.version();
```

---

## License
See LICENSE file.
