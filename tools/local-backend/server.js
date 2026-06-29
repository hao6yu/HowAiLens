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

function latestImageInfo() {
  if (!fs.existsSync(latestImagePath)) {
    return {
      exists: false,
      size: 0,
      updatedAt: null,
    };
  }

  const stat = fs.statSync(latestImagePath);

  return {
    exists: true,
    size: stat.size,
    updatedAt: stat.mtime.toISOString(),
  };
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

function sendText(res, status, body) {
  res.writeHead(status, {
    "Content-Type": "text/plain; charset=utf-8",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  res.end(body);
}

function handleHealth(req, res) {
  sendJson(res, 200, {
    ok: true,
    service: "haolens-local-backend",
    version: "0.5.1",
    maxUploadBytes: maxBytes,
    latestImage: latestImageInfo(),
  });
}

function handleAnalyze(req, res) {
  const chunks = [];
  let totalBytes = 0;
  let rejected = false;

  req.on("data", (chunk) => {
    if (rejected) {
      return;
    }

    totalBytes += chunk.length;

    if (totalBytes > maxBytes) {
      rejected = true;
      sendJson(res, 413, {
        ok: false,
        text: "Too large",
        error: "Upload too large",
        maxBytes,
      });
      req.resume();
      return;
    }

    chunks.push(chunk);
  });

  req.on("end", () => {
    if (rejected) {
      return;
    }

    const image = Buffer.concat(chunks);

    if (image.length === 0) {
      sendJson(res, 400, {
        ok: false,
        text: "No image",
        error: "Request body was empty",
      });
      return;
    }

    fs.writeFileSync(latestImagePath, image);

    console.log(`Saved ${image.length} bytes to ${latestImagePath}`);

    sendJson(res, 200, {
      ok: true,
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
    sendText(res, 404, "No image uploaded yet");
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
  const latest = latestImageInfo();
  const firmwareUrls = addresses.map((address) => `http://${address}:${port}/analyze`);
  const healthUrls = addresses.map((address) => `http://${address}:${port}/health`);
  const cacheBust = latest.updatedAt ? encodeURIComponent(latest.updatedAt) : Date.now();
  const latestHtml = latest.exists
    ? `<p><a href="/latest.jpg">latest.jpg</a> (${latest.size} bytes, ${latest.updatedAt})</p>
       <img src="/latest.jpg?t=${cacheBust}" alt="Latest HaoLens upload" style="max-width: min(100%, 720px); border: 1px solid #ccc;">`
    : "<p>No image uploaded yet.</p>";

  const body = `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HaoLens Local Backend</title>
  <style>
    body { font-family: system-ui, sans-serif; line-height: 1.4; margin: 24px; max-width: 900px; }
    code { background: #f3f3f3; padding: 2px 4px; border-radius: 4px; }
    li { margin: 4px 0; }
  </style>
</head>
<body>
  <h1>HaoLens Local Backend</h1>
  <p>Status: <strong>OK</strong></p>

  <h2>Firmware URL</h2>
  <p>Use one of these in <code>include/secrets.h</code>:</p>
  <ul>${firmwareUrls.map((url) => `<li><code>${url}</code></li>`).join("")}</ul>

  <h2>Health</h2>
  <ul>${healthUrls.map((url) => `<li><a href="${url}">${url}</a></li>`).join("")}</ul>

  <h2>Latest Upload</h2>
  ${latestHtml}
</body>
</html>`;

  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
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

  if (req.method === "GET" && req.url === "/health") {
    handleHealth(req, res);
    return;
  }

  if (req.method === "GET" && req.url.split("?")[0] === "/latest.jpg") {
    handleLatest(req, res);
    return;
  }

  if (req.method === "GET" && req.url === "/") {
    handleHome(req, res);
    return;
  }

  sendText(res, 404, "Not found");
});

server.listen(port, host, () => {
  console.log(`HaoLens local backend listening on http://localhost:${port}`);
  console.log(`Health check: http://localhost:${port}/health`);
  for (const address of localAddresses()) {
    console.log(`Use in firmware: http://${address}:${port}/analyze`);
  }
});
