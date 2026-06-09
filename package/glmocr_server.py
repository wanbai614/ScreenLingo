"""GLM-OCR JSON-line server for ScreenLingo (local Ollama mode)."""
import os, sys

# Use the bundled glmocr SDK from the project directory
_script_dir = os.path.dirname(os.path.abspath(__file__))
_glmocr_dir = os.path.join(_script_dir, "glmocr")
if _glmocr_dir not in sys.path:
    sys.path.insert(0, _script_dir)  # package/ contains glmocr/

os.environ["http_proxy"] = ""
os.environ["https_proxy"] = ""

sys.stderr.write("GLM-OCR server init...\n"); sys.stderr.flush()

import json, base64, io
from PIL import Image
from glmocr.pipeline import Pipeline
from glmocr.config import load_config

config = load_config()
pipeline = Pipeline(config.pipeline)

sys.stderr.write("GLM-OCR server ready\n"); sys.stderr.flush()

for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        req = json.loads(line)
        img = Image.open(io.BytesIO(base64.b64decode(req["image"])))
        if img.mode != 'RGB': img = img.convert('RGB')

        # Save to temp file for data URI (GLM-OCR needs a URL or path)
        tmp_path = os.path.join(os.environ.get("TEMP", "/tmp"), "glmocr_tmp.png")
        img.save(tmp_path, "PNG")
        data_uri = f"data:image/png;base64,{base64.b64encode(open(tmp_path, 'rb').read()).decode()}"

        messages = [{"role": "user", "content": [
            {"type": "image_url", "image_url": {"url": data_uri}}
        ]}]
        results = list(pipeline.process({"messages": messages}, save_layout_visualization=False))

        if results:
            r = results[0]
            md = r.markdown_result or ""
            # Try to extract bounding boxes from json_result
            boxes = []
            jr = r.json_result
            if jr and isinstance(jr, dict):
                for item in jr.get("items", jr.get("elements", [])):
                    bbox = item.get("bbox", item.get("bounding_box", None))
                    text = item.get("text", item.get("content", ""))
                    if bbox and text:
                        boxes.append({
                            "text": str(text).strip(),
                            "x": int(bbox[0]), "y": int(bbox[1]),
                            "w": int(bbox[2] - bbox[0]), "h": int(bbox[3] - bbox[1])
                        })

            print(json.dumps({"boxes": boxes, "fullText": md, "error": None}), flush=True)
        else:
            print(json.dumps({"boxes": [], "fullText": "", "error": "empty response"}), flush=True)

    except Exception as e:
        print(json.dumps({"boxes": [], "fullText": "", "error": str(e)}), flush=True)
