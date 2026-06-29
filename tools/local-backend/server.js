const http = require("http");
const fs = require("fs");
const os = require("os");
const path = require("path");

const host = "0.0.0.0";
const port = Number(process.env.HAOLENS_BACKEND_PORT || 8787);
const maxBytes = Number(process.env.HAOLENS_MAX_UPLOAD_BYTES || 2 * 1024 * 1024);
const uploadDir = path.join(__dirname, "uploads");
const latestImagePath = path.join(uploadDir, "latest.jpg");

fs.mkdirSync(uploadDir, { recursive: true });

function localAddresses() {
  const interfaces = os.networkInterfaces();
  const addresses = [];

  for (const values of Object.values(interfaces)) {
    for (const item of values || []) {
      if (item.family === "IPv4" && !item.internal) {
        addresses.push(item.address);
      }
    }
  }

  return addresses;
}

function sendJson(res, status, body) {
  const json = JSON.stringify(body);

  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(json),
    "Cache-Control": "no-store",
  });
  res.end(json);
}

function handleAnalyze(req, res) {
  const chunks = [];
  let totalBytes = 0;

  req.on("data", (chunk) => {
    totalBytes += chunk.length;

    if (totalBytes > maxBytes) {
      res.writeHead(413, { "Content-Type": "text/plain" });
      res.end("Upload too large");
      req.destroy();
      return;
    }

    chunks.push(chunk);
  });

  req.on("end", () => {
    const image = Buffer.concat(chunks);
    fs.writeFileSync(latestImagePath, image);

    console.log(`Saved ${image.length} bytes to ${latestImagePath}`);

    sendJson(res, 200, {
      text: "Image OK",
      bytes: image.length,
      savedAs: latestImagePath,
    });
  });

  req.on("error", (error) => {
    console.error("Request error:", error.message);
  });
}

function handleLatest(req, res) {
  if (!fs.existsSync(latestImagePath)) {
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("No image uploaded yet");
    return;
  }

  const image = fs.readFileSync(latestImagePath);
  res.writeHead(200, {
    "Content-Type": "image/jpeg",
    "Content-Length": image.length,
    "Cache-Control": "no-store",
  });
  res.end(image);
}

function handleHome(req, res) {
  const addresses = localAddresses();
  const body = [
    "HaoLens local backend",
    "",
    "POST /analyze with image/jpeg",
    "GET  /latest.jpg to view the last upload",
    "",
    "Use one of these URLs in include/secrets.h:",
    ...addresses.map((address) => `http://${address}:${port}/analyze`),
    "",
  ].join("\n");

  res.writeHead(200, {
    "Content-Type": "text/plain",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  res.end(body);
}

const server = http.createServer((req, res) => {
  if (req.method === "POST" && req.url === "/analyze") {
    handleAnalyze(req, res);
    return;
  }

  if (req.method === "GET" && req.url === "/latest.jpg") {
    handleLatest(req, res);
    return;
  }

  if (req.method === "GET" && req.url === "/") {
    handleHome(req, res);
    return;
  }

  res.writeHead(404, { "Content-Type": "text/plain" });
  res.end("Not found");
});

server.listen(port, host, () => {
  console.log(`HaoLens local backend listening on http://localhost:${port}`);
  for (const address of localAddresses()) {
    console.log(`Use in firmware: http://${address}:${port}/analyze`);
  }
});
