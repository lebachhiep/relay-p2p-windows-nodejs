/**
 * Relay Leaf Node.js SDK - Example (Windows x64)
 *
 * Requirements:
 *  - relay_leaf.node (in root directory)
 *  - relay_leaf_win64.dll (if needed, also in root)
 *
 * Notes:
 *  - partnerId and proxies are OPTIONAL.
 *  - If proxies is empty, the client will connect directly (no proxy).
 *  - Proxy format must be a valid URL string. Supported schemes: http, socks5.
 */

const relay = require("./relay_leaf.node");

console.log("Relay Leaf version:", relay.version());

/**
 * Proxy URL formats supported by Relay Leaf:
 *
 * 1) HTTP proxy (no auth)
 *    "http://HOST:PORT"
 *    Example: "http://127.0.0.1:8080"
 *
 * 2) HTTP proxy (basic auth)
 *    "http://USER:PASS@HOST:PORT"
 *    Example: "http://user:pass@203.0.113.10:8080"
 *
 * 3) SOCKS5 proxy (no auth)
 *    "socks5://HOST:PORT"
 *    Example: "socks5://127.0.0.1:1080"
 *
 * 4) SOCKS5 proxy (auth)
 *    "socks5://USER:PASS@HOST:PORT"
 *    Example: "socks5://user:pass@203.0.113.10:1080"
 *
 * Rules:
 *  - Scheme must be lowercase: "http" or "socks5"
 *  - HOST can be an IP or a domain
 *  - PORT must be a number
 *  - If USER/PASS contain special characters, URL-encode them
 *    Example: pass "p@ss:word" -> "p%40ss%3Aword"
 */
const proxiesExample = [
  // HTTP proxy without authentication
  "http://127.0.0.1:8080",

  // HTTP proxy with authentication (username:password)
  // "http://user:pass@203.0.113.10:8080",

  // SOCKS5 proxy without authentication
  "socks5://127.0.0.1:1080",

  // SOCKS5 proxy with authentication
  // "socks5://user:pass@203.0.113.10:1080",
];

/**
 * Create a RelayClient instance.
 *
 * Options:
 *  - discoveryUrl (string): OPTIONAL
 *      If provided, the client fetches relay nodes from this API endpoint.
 *      If not provided, the library may use its own internal default.
 *
 *  - partnerId (string): OPTIONAL
 *      If empty, no partner ID is sent.
 *
 *  - proxies (string[]): OPTIONAL
 *      If empty, the client uses a direct connection (no proxy).
 *      If provided, you can add one or multiple proxy URLs.
 *
 *  - verbose (boolean): OPTIONAL
 *      Enables verbose logging in the native library.
 */
const client = new relay.RelayClient({
  discoveryUrl: "https://api.prx.network/public/relay/nodes", // correct endpoint
  partnerId: "", // OPTIONAL: e.g. "my-partner-id"
  proxies: proxiesExample, // OPTIONAL: [] for direct connection
  verbose: false,
});

// Start relay (non-blocking)
client.start();
console.log("Relay client started");

// Print stats every 2 seconds
setInterval(() => {
  try {
    const stats = client.stats();

    /**
     * stats fields:
     *  - connected (bool)
     *  - connectedNodes (number)
     *  - uptimeSeconds (BigInt or number depending on addon)
     *  - activeStreams (number)
     *  - totalStreams (BigInt or number depending on addon)
     *  - bytesSent / bytesReceived (BigInt or number depending on addon)
     *  - lastError (string)
     *  - exitPointsJson (string)
     *  - nodeAddressesJson (string)
     */

    // If your addon returns BigInt, convert safely for display:
    const uptime = typeof stats.uptimeSeconds === "bigint" ? Number(stats.uptimeSeconds) : stats.uptimeSeconds;

    console.log(
      `Connected=${stats.connected} | Nodes=${stats.connectedNodes} | ` +
        `Uptime=${uptime}s | Streams=${stats.activeStreams}/${stats.totalStreams} | ` +
        `Sent=${stats.bytesSent} | Recv=${stats.bytesReceived}`
    );

    // Optional: log debug info when disconnected
    if (!stats.connected && stats.lastError) {
      console.log("LastError:", stats.lastError);
    }
  } catch (e) {
    console.error("Stats error:", e);
  }
}, 2000);

/**
 * Graceful shutdown:
 *  - Ctrl+C triggers SIGINT
 *  - Stop the relay client, then exit process
 *
 * Note:
 *  If you want "run forever", just do not call client.stop() unless you actually want to stop it.
 */
process.on("SIGINT", () => {
  console.log("Stopping relay...");
  try {
    client.stop();
  } catch {}
  process.exit(0);
});