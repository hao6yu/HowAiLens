const http = require("http");
const fs = require("fs");
const os = require("os");
const path = require("path");

const repoRoot = path.resolve(__dirname, "../..");

loadDotEnv(path.join(repoRoot, ".env"));
loadDotEnv(path.join(__dirname, ".env"));

const host = "0.0.0.0";
const port = Number(process.env.HAOLENS_BACKEND_PORT || 8787);
const maxBytes = Number(process.env.HAOLENS_MAX_UPLOAD_BYTES || 2 * 1024 * 1024);
const uploadDir = path.join(__dirname, "uploads");
const latestImagePath = path.join(uploadDir, "latest.jpg");
const openaiApiKey = process.env.OPENAI_API_KEY || "";
const openaiModel = process.env.OPENAI_MODEL || "gpt-5.4-mini";
const openaiImageDetail = process.env.OPENAI_IMAGE_DETAIL || "low";
const openaiTimeoutMs = Number(process.env.OPENAI_TIMEOUT_MS || 30000);
const oledLineChars = Number(process.env.HAOLENS_OLED_LINE_CHARS || 10);
const oledMaxLines = Number(process.env.HAOLENS_OLED_MAX_LINES || 2);
const mockText = process.env.HAOLENS_AI_MOCK_TEXT || "AI off";
let lastAnalysis = null;

fs.mkdirSync(uploadDir, { recursive: true });

function loadDotEnv(filePath) {
  if (!fs.existsSync(filePath)) {
    return;
  }

  const lines = fs.readFileSync(filePath, "utf8").split(/\r?\n/);

  for (const line of lines) {
    const trimmed = line.trim();

    if (!trimmed || trimmed.startsWith("#")) {
      continue;
    }

    const equalsIndex = trimmed.indexOf("=");

    if (equalsIndex <= 0) {
      continue;
    }

    const key = trimmed.slice(0, equalsIndex).trim();
    let value = trimmed.slice(equalsIndex + 1).trim();

    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }

    if (!Object.prototype.hasOwnProperty.call(process.env, key)) {
      process.env[key] = value;
    }
  }
}

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
    version: "0.7.0",
    maxUploadBytes: maxBytes,
    ai: openaiStatus(),
    latestImage: latestImageInfo(),
  });
}

function openaiStatus() {
  return {
    configured: openaiApiKey.length > 0,
    model: openaiModel,
    imageDetail: openaiImageDetail,
    timeoutMs: openaiTimeoutMs,
    oledLineChars,
    oledMaxLines,
    mode: openaiApiKey.length > 0 ? "openai" : "mock",
  };
}

function sanitizeWord(word) {
  const cleaned = word
    .replace(/^[-*•"'`]+/, "")
    .replace(/["'`.,!?;:]+$/g, "")
    .trim();

  if (cleaned.length <= oledLineChars) {
    return cleaned;
  }

  return cleaned.slice(0, oledLineChars);
}

function cleanOledText(text) {
  const raw = String(text || "")
    .replace(/\r\n/g, "\n")
    .replace(/^["'`]+|["'`]+$/g, "")
    .trim();

  const words = wordsFromLine(raw);

  if (words.length === 0) {
    return "Not sure";
  }

  if (raw.includes("\n")) {
    const lines = [];

    for (const rawLine of raw.split(/\n+/)) {
      const wrappedLines = wrapWords(wordsFromLine(rawLine), oledMaxLines - lines.length);
      lines.push(...wrappedLines);

      if (lines.length >= oledMaxLines) {
        break;
      }
    }

    if (lines.length > 0) {
      return lines.slice(0, oledMaxLines).join("\n");
    }
  }

  if (!raw.includes("\n") && oledMaxLines >= 2) {
    const firstLine = takeWordsFromStart(words);
    const consumedWords = firstLine ? firstLine.split(/\s+/).length : 0;

    if (consumedWords < words.length) {
      const lastLine = trimLeadingArticle(takeWordsFromEnd(words.slice(consumedWords)));
      return [firstLine, lastLine].filter(Boolean).slice(0, oledMaxLines).join("\n");
    }
  }

  return wrapWords(words, oledMaxLines).join("\n");
}

function wordsFromLine(line) {
  return String(line || "")
    .split(/\s+/)
    .map(sanitizeWord)
    .filter(Boolean);
}

function wrapWords(words, maxLines) {
  const lines = [];
  let currentLine = "";

  for (const word of words) {
    if (!currentLine) {
      currentLine = word;
      continue;
    }

    if (currentLine.length + 1 + word.length <= oledLineChars) {
      currentLine += ` ${word}`;
      continue;
    }

    lines.push(currentLine);
    currentLine = word;

    if (lines.length >= maxLines) {
      break;
    }
  }

  if (lines.length < maxLines && currentLine) {
    lines.push(currentLine);
  }

  return lines.slice(0, maxLines);
}

function takeWordsFromStart(words) {
  let line = "";

  for (const word of words) {
    if (!line) {
      line = word;
      continue;
    }

    if (line.length + 1 + word.length > oledLineChars) {
      break;
    }

    line += ` ${word}`;
  }

  return line;
}

function takeWordsFromEnd(words) {
  let line = "";

  for (let i = words.length - 1; i >= 0; i--) {
    const word = words[i];

    if (!line) {
      line = word;
      continue;
    }

    if (word.length + 1 + line.length > oledLineChars) {
      break;
    }

    line = `${word} ${line}`;
  }

  return line;
}

function trimLeadingArticle(line) {
  return line.replace(/^(a|an|the)\s+/i, "").trim();
}

function extractResponseText(response) {
  if (typeof response.output_text === "string") {
    return response.output_text;
  }

  const parts = [];

  for (const item of response.output || []) {
    for (const content of item.content || []) {
      if (typeof content.text === "string") {
        parts.push(content.text);
      }
    }
  }

  return parts.join(" ");
}

function postJson(url, headers, body, timeoutMs) {
  if (typeof fetch !== "function") {
    throw new Error("OpenAI mode requires Node 18 or newer. Run `nvm use` first.");
  }

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);

  return fetch(url, {
    method: "POST",
    headers: {
      ...headers,
      "Content-Type": "application/json",
    },
    body: JSON.stringify(body),
    signal: controller.signal,
  })
    .then(async (response) => ({
      statusCode: response.status,
      body: await response.text(),
    }))
    .catch((error) => {
      if (error.name === "AbortError") {
        throw new Error("OpenAI request timed out");
      }

      throw error;
    })
    .finally(() => {
      clearTimeout(timeout);
    });
}

async function analyzeImageWithOpenAI(image) {
  if (!openaiApiKey) {
    return {
      ok: false,
      text: cleanOledText(mockText),
      mode: "mock",
      error: "OPENAI_API_KEY is not configured",
    };
  }

  const base64Image = image.toString("base64");
  const prompt = [
    "Describe this image for a tiny 64x48 OLED display.",
    `Return 1 or 2 lines. Each line must be ${oledLineChars} characters or fewer.`,
    "Prefer two short lines over one cramped phrase.",
    "Use simple concrete words. No markdown. No punctuation unless needed.",
    "Return only the display text.",
    "Good examples: Cable\\nnear wall | Red mug | Open door",
    "If unclear, return exactly: Not sure",
  ].join(" ");

  const requestBody = {
    model: openaiModel,
    input: [
      {
        role: "user",
        content: [
          { type: "input_text", text: prompt },
          {
            type: "input_image",
            image_url: `data:image/jpeg;base64,${base64Image}`,
            detail: openaiImageDetail,
          },
        ],
      },
    ],
    max_output_tokens: 40,
    store: false,
  };

  const response = await postJson(
    "https://api.openai.com/v1/responses",
    {
      Authorization: `Bearer ${openaiApiKey}`,
    },
    requestBody,
    openaiTimeoutMs
  );

  let parsed;

  try {
    parsed = JSON.parse(response.body);
  } catch (error) {
    throw new Error(`OpenAI returned non-JSON response: HTTP ${response.statusCode}`);
  }

  if (response.statusCode < 200 || response.statusCode >= 300) {
    const message = parsed.error && parsed.error.message ? parsed.error.message : response.body;
    throw new Error(`OpenAI HTTP ${response.statusCode}: ${message}`);
  }

  return {
    ok: true,
    text: cleanOledText(extractResponseText(parsed)),
    mode: "openai",
    model: openaiModel,
    imageDetail: openaiImageDetail,
    usage: parsed.usage || null,
  };
}

function handleAnalyze(req, res) {
  const chunks = [];
  let totalBytes = 0;
  let rejected = false;
  const startTime = Date.now();

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

  req.on("end", async () => {
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

    let analysis;

    try {
      analysis = await analyzeImageWithOpenAI(image);
    } catch (error) {
      console.error("OpenAI analyze failed:", error.message);
      analysis = {
        ok: false,
        text: "AI failed",
        mode: "openai",
        error: error.message,
      };
    }

    console.log(`Analysis result: ${analysis.text}`);
    if (analysis.usage) {
      console.log(`OpenAI tokens: ${JSON.stringify(analysis.usage)}`);
    }

    lastAnalysis = {
      ok: analysis.ok,
      text: analysis.text,
      bytes: image.length,
      mode: analysis.mode,
      model: analysis.model || openaiModel,
      imageDetail: analysis.imageDetail || openaiImageDetail,
      usage: analysis.usage || null,
      error: analysis.error || null,
      latencyMs: Date.now() - startTime,
      updatedAt: new Date().toISOString(),
    };

    sendJson(res, 200, {
      ok: analysis.ok,
      text: analysis.text,
      bytes: image.length,
      mode: analysis.mode,
      model: analysis.model || openaiModel,
    });
  });

  req.on("error", (error) => {
    console.error("Request error:", error.message);
  });
}

function handleLastAnalysis(req, res) {
  if (!lastAnalysis) {
    sendJson(res, 404, {
      ok: false,
      text: "No result",
      error: "No analysis has run yet",
    });
    return;
  }

  sendJson(res, 200, lastAnalysis);
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
  const ai = openaiStatus();
  const aiHtml = ai.configured
    ? `<p>Mode: <strong>OpenAI</strong></p>
       <p>Model: <code>${ai.model}</code></p>
       <p>Image detail: <code>${ai.imageDetail}</code></p>
       <p>OLED shape: <code>${ai.oledMaxLines} lines x ${ai.oledLineChars} chars</code></p>`
    : `<p>Mode: <strong>Mock</strong></p>
       <p>Add <code>OPENAI_API_KEY</code> to <code>.env</code> to enable AI vision.</p>`;
  const analysisHtml = lastAnalysis
    ? `<p>Last result:</p>
       <pre>${escapeHtml(lastAnalysis.text)}</pre>
       <p>${lastAnalysis.model}, ${lastAnalysis.latencyMs} ms, ${lastAnalysis.updatedAt}</p>`
    : "<p>No analysis yet.</p>";

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

  <h2>AI</h2>
  ${aiHtml}

  <h2>Last Analysis</h2>
  ${analysisHtml}

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

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
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

  if (req.method === "GET" && req.url === "/last-analysis") {
    handleLastAnalysis(req, res);
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
  console.log(`AI mode: ${openaiApiKey ? "OpenAI" : "mock"} (${openaiModel}, detail=${openaiImageDetail})`);
  if (openaiApiKey && typeof fetch !== "function") {
    console.warn("OpenAI mode needs Node 18 or newer. Run `nvm use` before starting the backend.");
  }
  for (const address of localAddresses()) {
    console.log(`Use in firmware: http://${address}:${port}/analyze`);
  }
});
