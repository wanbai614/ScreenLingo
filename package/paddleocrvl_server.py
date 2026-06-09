"""PaddleOCR-VL JSON-line server for ScreenLingo (Ollama backend).

Same pattern as glmocr_server.py — reads JSON from stdin, calls Ollama, outputs JSON to stdout.
Uses OpenAI-compatible /v1/chat/completions with image_url format.
"""

import json, sys, base64, os, io, re

os.environ["http_proxy"] = ""
os.environ["https_proxy"] = ""
os.environ["HTTP_PROXY"] = ""
os.environ["HTTPS_PROXY"] = ""

OLLAMA_URL = os.environ.get("OLLAMA_BASE_URL", "http://localhost:11434")
MODEL_NAME = os.environ.get("PADDLEOCR_VL_MODEL", "MedAIBase/PaddleOCR-VL:0.9b")

sys.stderr.write(f"PaddleOCR-VL server init: url={OLLAMA_URL} model={MODEL_NAME}\n")
sys.stderr.flush()

try:
    import requests
except ImportError:
    import urllib.request, urllib.error
    requests = None

# --- helpers ---

def ollama_chat(image_b64: str) -> dict:
    # PaddleOCR-VL uses task-prefix prompts (OCR:, Table Recognition:, etc.)
    # Temperature 0 for deterministic output
    prompt = "OCR:"
    data_uri = f"data:image/png;base64,{image_b64}"
    payload = {
        "model": MODEL_NAME,
        "messages": [{"role": "user", "content": [
            {"type": "image_url", "image_url": {"url": data_uri}},
            {"type": "text", "text": prompt},
        ]}],
        "stream": False,
        "temperature": 0,
    }
    url = OLLAMA_URL.rstrip("/") + "/v1/chat/completions"

    if requests:
        resp = requests.post(url, json=payload, timeout=120)
        resp.raise_for_status()
        result = resp.json()
    else:
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=120) as resp:
            result = json.loads(resp.read().decode("utf-8"))

    choices = result.get("choices", [])
    content = choices[0].get("message", {}).get("content", "") if choices else ""
    return {"message": {"content": content}}


def extract_text_and_boxes(content: str):
    content = re.sub(r"<\s*think\s*>[\s\S]*?<\s*/\s*think\s*>", "", content,
                     flags=re.IGNORECASE).strip()

    def try_parse(s):
        try: return json.loads(s)
        except (json.JSONDecodeError, ValueError): return None

    def items_to_result(items):
        texts, bx = [], []
        for item in items:
            if not isinstance(item, dict): continue
            t = str(item.get("text", "")).strip()
            if not t: continue
            texts.append(t)
            bb = item.get("bbox", item.get("bounding_box", []))
            if isinstance(bb, list) and len(bb) >= 4:
                bx.append({"text": t, "x": int(bb[0]), "y": int(bb[1]),
                           "w": int(bb[2]), "h": int(bb[3])})
            else:
                bx.append({"text": t, "x": 0, "y": 0, "w": 100, "h": 20})
        return "\n".join(texts), bx

    parsed = try_parse(content)

    if parsed is None:
        for m in re.finditer(r"\[[\s\S]*?\]", content):
            p = try_parse(m.group(0))
            if isinstance(p, list): parsed = p; break

    if parsed is None:
        m = re.search(r"```(?:json)?\s*([\s\S]*?)```", content)
        if m: parsed = try_parse(m.group(1).strip())

    if parsed is None:
        for m in re.finditer(r"\{[\s\S]*?\}", content):
            obj = try_parse(m.group(0))
            if isinstance(obj, dict):
                for k in ("texts", "results", "items", "elements"):
                    if k in obj and isinstance(obj[k], list):
                        parsed = obj[k]; break
                if parsed is not None: break

    if isinstance(parsed, list): return items_to_result(parsed)
    if isinstance(parsed, dict):
        for k in ("texts", "results", "items", "elements"):
            if k in parsed and isinstance(parsed[k], list):
                return items_to_result(parsed[k])

    lines = [l.strip() for l in content.split("\n") if l.strip()]
    lines = [l for l in lines if not l.startswith("```") and not l.startswith("{")
             and not l.startswith("[") and not l.startswith("Here") and not l.startswith("The ")]
    if lines:
        bx = [{"text": l, "x": 0, "y": 0, "w": 100, "h": 20} for l in lines]
        return "\n".join(lines), bx
    return content, []


# --- main loop ---

sys.stderr.write("PaddleOCR-VL server ready\n")
sys.stderr.flush()

for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        req = json.loads(line)
        img_b64 = req.get("image", "")
        if not img_b64:
            print(json.dumps({"boxes": [], "fullText": "", "error": "no image"}))
            sys.stdout.flush()
            continue

        raw = base64.b64decode(img_b64)
        img_b64 = base64.b64encode(raw).decode("ascii")

        response = ollama_chat(img_b64)
        content = response.get("message", {}).get("content", "")
        full_text, boxes = extract_text_and_boxes(content)

        print(json.dumps({"boxes": boxes, "fullText": full_text, "error": None}))
        sys.stdout.flush()
    except Exception as e:
        print(json.dumps({"boxes": [], "fullText": "", "error": str(e)}))
        sys.stdout.flush()
