# app.py
import os
import pickle
from contextlib import nullcontext
import argparse
import torch
import tiktoken
from flask import Flask, render_template, request, jsonify, Response, stream_with_context
import json
import torch.nn.functional as F

# import your GPT model & config
from model import GPTConfig, GPT

"""
# ---------------------------------------------------------------------
# CONFIG / defaults
DEFAULT_OUT_DIR = 'out'
DEFAULT_INIT_FROM = 'resume'
DEFAULT_LINES = 10
DEFAULT_SAMPLES_PER_LINE = 5 
DEFAULT_TOKENS_PER_SAMPLE = 30
SEPARATOR = "<eot>"
DEFAULT_TEMPERATURE = 0.8
DEFAULT_TOP_K = 200
# ---------------------------------------------------------------------
"""

# for debug
DEFAULT_OUT_DIR = 'out'
DEFAULT_INIT_FROM = 'resume'
DEFAULT_LINES = 1 # give 1
DEFAULT_SAMPLES_PER_LINE = 1 # give 1 
DEFAULT_TOKENS_PER_SAMPLE = 30
SEPARATOR = "<eot>"
DEFAULT_TEMPERATURE = 0.8
DEFAULT_TOP_K = 200
##########################

parser = argparse.ArgumentParser()
parser.add_argument("--out_dir", type=str, default=DEFAULT_OUT_DIR)
parser.add_argument("--init_from", type=str, default=DEFAULT_INIT_FROM)
parser.add_argument("--device", type=str, default="cpu", help="cuda or cpu")
parser.add_argument("--no_compile", action="store_true", help="don't torch.compile even if enabled")
parser.add_argument("--port", type=int, default=5000)
args = parser.parse_args()

out_dir = args.out_dir
init_from = args.init_from
device_arg = args.device
device = torch.device("cpu" if "cpu" in device_arg else "cuda")
use_compile = False if args.no_compile else False

# load model (same as before)
print("Loading model...")
if init_from == 'resume':
    ckpt_path = os.path.join(out_dir, 'ckpt.pt')
    if not os.path.exists(ckpt_path):
        raise FileNotFoundError(f"Checkpoint not found: {ckpt_path}")
    checkpoint = torch.load(ckpt_path, map_location=device)
    gptconf = GPTConfig(**checkpoint['model_args'])
    model = GPT(gptconf)
    state_dict = checkpoint['model']
    unwanted_prefix = '_orig_mod.'
    for k in list(state_dict.keys()):
        if k.startswith(unwanted_prefix):
            state_dict[k[len(unwanted_prefix):]] = state_dict.pop(k)
    model.load_state_dict(state_dict)
elif init_from.startswith('gpt2'):
    model = GPT.from_pretrained(init_from, dict(dropout=0.0))
else:
    raise ValueError("init_from must be 'resume' or start with 'gpt2'")

model.eval()
model.to(device)
if use_compile:
    try:
        model = torch.compile(model)
    except Exception as e:
        print("torch.compile failed, continuing without compile:", e)

# tokenizer/load meta (same as before)
load_meta = False
if init_from == 'resume' and 'config' in checkpoint and 'dataset' in checkpoint['config']:
    meta_path = os.path.join('data', checkpoint['config']['dataset'], 'meta.pkl')
    load_meta = os.path.exists(meta_path)

if load_meta:
    with open(meta_path, 'rb') as f:
        meta = pickle.load(f)
    stoi, itos = meta['stoi'], meta['itos']
    encode = lambda s: [stoi.get(c, 0) for c in s]
    decode = lambda l: ''.join([itos[i] for i in l])
else:
    enc = tiktoken.get_encoding("gpt2")
    encode = lambda s: enc.encode(s, allowed_special={"<|endoftext|>"})
    decode = lambda l: enc.decode(l)

# autocast context like original
device_type = 'cuda' if device.type == 'cuda' else 'cpu'
dtype = 'float16' if (device.type == 'cuda' and torch.cuda.is_available()) else 'float32'
ptdtype = {'float32': torch.float32, 'float16': torch.float16}[dtype]
ctx = nullcontext() if device_type == 'cpu' else torch.amp.autocast(device_type=device_type, dtype=ptdtype)

print("Model ready.")

app = Flask(__name__)

@app.route("/")
def index():
    try:
        return render_template("index.html")
    except Exception:
        return "<p>No index.html found. Use /api/generate.</p>"

def sse_pack(obj):
    """Return a single SSE data frame for the JSON-able obj"""
    return "data: " + json.dumps(obj, default=str) + "\n\n"

@app.route("/api/generate", methods=["POST"])
def api_generate():
    """
    Accepts JSON:
    { prompt, lines, samples, tokens, temperature, top_k, mode }
    mode: 'batch' (blocking JSON), 'line' (SSE per-line), 'token' (SSE per-token)
    """
    data = request.get_json(force=True)
    prompt = data.get("prompt", "")
    if prompt is None or prompt.strip() == "":
        return jsonify({"error": "empty prompt"}), 400

    lines = int(data.get("lines", DEFAULT_LINES))
    samples = int(data.get("samples", DEFAULT_SAMPLES_PER_LINE))
    tokens = int(data.get("tokens", DEFAULT_TOKENS_PER_SAMPLE))
    temperature = float(data.get("temperature", DEFAULT_TEMPERATURE))
    top_k = int(data.get("top_k", DEFAULT_TOP_K))
    mode = data.get("mode", "batch")  # 'batch' | 'line' | 'token'

    start_ids = encode(prompt)
    x = torch.tensor(start_ids, dtype=torch.long, device=device)[None, ...]
    prompt_len = x.shape[1]

    # Batch mode: existing behavior (blocking, returns JSON)
    if mode == "batch":
        completions_all = []
        try:
            with torch.no_grad():
                with ctx:
                    for _ in range(lines):
                        xb = x.repeat(samples, 1)
                        y = model.generate(xb, tokens, temperature=temperature, top_k=top_k)
                        for i in range(samples):
                            full_ids = y[i].tolist()
                            new_ids = full_ids[prompt_len:]
                            completion = decode(new_ids)
                            completions_all.append(prompt + completion)
        except Exception as e:
            return jsonify({"error": f"generation failed: {str(e)}"}), 500

        results = []
        idx = 0
        for _ in range(lines):
            row = SEPARATOR.join(completions_all[idx:idx + samples])
            results.append(row)
            idx += samples
        return jsonify({"results": results})

    # Otherwise we stream SSE frames (line or token mode)
    def event_stream():
        # Keep context and torch.no_grad() active for duration of stream
        try:
            with torch.no_grad():
                with ctx:
                    completions_all = []
                    # initial log
                    yield sse_pack({"type": "log", "msg": f"Starting generation: mode={mode} lines={lines} samples={samples} tokens={tokens}"})

                    for line_idx in range(lines):
                        yield sse_pack({"type": "log", "msg": f"Starting line {line_idx+1}/{lines}", "progress": {"lines_done": line_idx, "lines_total": lines}})
                        xb = x.repeat(samples, 1).to(device)

                        if mode == "line":
                            # generate full tokens for the line as before
                            y = model.generate(xb, tokens, temperature=temperature, top_k=top_k)
                            # build joined row text
                            parts = []
                            for i in range(samples):
                                full_ids = y[i].tolist()
                                new_ids = full_ids[prompt_len:]
                                completion = decode(new_ids)
                                parts.append(prompt + completion)
                                completions_all.append(prompt + completion)
                            row_text = SEPARATOR.join(parts)
                            # send line event
                            yield sse_pack({"type": "line", "line_idx": line_idx, "text": row_text, "progress": {"lines_done": line_idx+1, "lines_total": lines}})
                        elif mode == "token":
                            # token-by-token generation, streaming per token step
                            seq = xb.clone()  # seq shape: (samples, cur_len)
                            for step in range(tokens):
                                # crop context if necessary
                                if seq.size(1) <= model.config.block_size:
                                    idx_cond = seq
                                else:
                                    idx_cond = seq[:, -model.config.block_size:]

                                logits, _ = model(idx_cond)  # logits shape (B,1,V)
                                logits_step = logits[:, -1, :] / (temperature if temperature != 0 else 1e-9)

                                # apply top_k mask for sampling
                                if top_k is not None:
                                    v, _ = torch.topk(logits_step, min(top_k, logits_step.size(-1)))
                                    logits_masked = logits_step.clone()
                                    logits_masked[logits_masked < v[:, [-1]]] = -float('Inf')
                                    probs = F.softmax(logits_masked, dim=-1)
                                else:
                                    probs = F.softmax(logits_step, dim=-1)

                                idx_next = torch.multinomial(probs, num_samples=1)
                                seq = torch.cat((seq, idx_next.to(seq.device)), dim=1)

                                # prepare partial decoded strings for each sample
                                partials = []
                                for i in range(samples):
                                    new_ids = seq[i].tolist()[prompt_len:]
                                    partials.append(decode(new_ids))

                                # send token event
                                yield sse_pack({
                                    "type": "token",
                                    "line_idx": line_idx,
                                    "step": step,
                                    "partials": partials,
                                    "progress": {"lines_done": line_idx, "lines_total": lines, "step_in_line": step+1, "steps_total": tokens}
                                })

                            # after finishing token loop, add final completions for this line
                            for i in range(samples):
                                full_ids = seq[i].tolist()
                                new_ids = full_ids[prompt_len:]
                                completion = decode(new_ids)
                                completions_all.append(prompt + completion)

                            # also yield a line-complete event
                            parts = [prompt + decode(seq[i].tolist()[prompt_len:]) for i in range(samples)]
                            row_text = SEPARATOR.join(parts)
                            yield sse_pack({"type": "line", "line_idx": line_idx, "text": row_text, "progress": {"lines_done": line_idx+1, "lines_total": lines}})
                        else:
                            yield sse_pack({"type": "error", "msg": f"Unknown mode: {mode}"})
                    # final done
                    # assemble rows (same ordering as CLI)
                    results = []
                    idx = 0
                    for _ in range(lines):
                        row = SEPARATOR.join(completions_all[idx:idx + samples])
                        results.append(row)
                        idx += samples

                    yield sse_pack({"type": "done", "results": results, "msg": "Generation finished"})
        except Exception as e:
            yield sse_pack({"type": "error", "msg": str(e)})

    # Stream response: text/event-stream
    return Response(stream_with_context(event_stream()), mimetype="text/event-stream")

if __name__ == "__main__":
    # single-process Flask; don't use reloader (would load model twice)
    app.run(host="0.0.0.0", port=args.port, debug=False, use_reloader=False)