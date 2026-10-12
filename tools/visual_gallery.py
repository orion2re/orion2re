#!/usr/bin/env python3
"""Build a local comparison gallery; requires Pillow and scaler-compare."""
import argparse
import csv
import io
import json
from pathlib import Path
import subprocess

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path, help="Directory of logical-frame BMPs")
    parser.add_argument("output", type=Path)
    parser.add_argument("--scaler-tool", type=Path, required=True)
    parser.add_argument("--gpu-tool", type=Path, help="Optional Vulkan parity and timing tool")
    parser.add_argument("--sequence", action="store_true", help="Add frame playback; use one CPU pass per frame instead of benchmarking")
    args = parser.parse_args()
    frames = sorted(args.captures.glob("*.bmp"))
    if not frames:
        parser.error("No BMP captures found")
    args.output.mkdir(parents=True, exist_ok=True)
    results = {}
    gpu_results = {}
    for frame in frames:
        directory = args.output / frame.stem
        command = [str(args.scaler_tool.resolve()), str(frame), str(directory)]
        if args.sequence:
            command.append("--single-pass")
        run = subprocess.run(command,
                             check=True, capture_output=True, text=True)
        (directory / "timings.csv").write_text(run.stdout)
        results[frame.stem] = list(csv.DictReader(io.StringIO(run.stdout)))
        directories = [directory]
        if args.gpu_tool:
            gpu_directory = directory / "gpu"
            gpu_run = subprocess.run([str(args.gpu_tool.resolve()), str(frame), str(gpu_directory)],
                                     check=True, capture_output=True, text=True)
            (gpu_directory / "timings.csv").write_text(gpu_run.stdout)
            gpu_results[frame.stem] = list(csv.DictReader(io.StringIO(gpu_run.stdout)))
            directories.append(gpu_directory)
        for image_directory in directories:
            for row in results[frame.stem]:
                name = row["scaler"]
                with Image.open(image_directory / f"{name}.bmp") as bitmap:
                    image = bitmap.convert("RGB")
                    image.save(image_directory / f"{name}.png")
                    # Presentation previews are approximations of SDL's output sampling.
                    sampling = Image.Resampling.NEAREST if name == "pixelart" else Image.Resampling.BILINEAR
                    for height in (() if args.sequence else (1080, 1440)):
                        image.resize((height * 4 // 3, height), sampling).save(
                            image_directory / f"{name}-{height}.png")
                if args.sequence:
                    (image_directory / f"{name}.bmp").unlink()
    metadata = json.dumps(results).replace("<", "\\u003c")
    html = r'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orion2re graphics baseline</title><style>
body{background:#11171e;color:#e0e7ef;font:16px system-ui;margin:24px}
h1{font-size:26px}label{display:inline-block;margin:8px 18px 8px 0}
select{font:inherit;background:#243342;color:inherit;padding:7px}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}
figure{margin:0;min-width:0}figcaption{margin:12px 0}img{width:100%;display:block}
.scroll{overflow:auto;max-height:72vh;background:#000}.actual img{width:auto;max-width:none}
table{border-collapse:collapse;margin-top:20px}td,th{padding:8px 16px;border-bottom:1px solid #34404e;text-align:right}th:first-child,td:first-child{text-align:left}
.note{color:#b3bfcd;max-width:1000px;line-height:1.5}a{color:#82c8ff}
@media(max-width:850px){.pair{grid-template-columns:1fr}}
</style><h1>Orion2re graphics baseline</h1>
<p class="note">Same lossless logical frame, processed by the production scalers. Images contain locally supplied game artwork; keep this gallery local. No AI enhancement has been applied.</p>
<label>Screen <select id="screen"></select></label>
<label>Output <select id="resolution"><option value="1080">1080p — 1440×1080 viewport</option><option value="1440" selected>1440p — 1920×1440 viewport</option><option value="native">Native scaler output</option></select></label>
<label>Left <select id="left"></select></label><label>Right <select id="right"></select></label>
<label><input type="checkbox" id="gpu"> Use Vulkan shaders on the right</label>
<label><input type="checkbox" id="actual"> Inspect at actual pixel size</label>
<div id="sequence-controls" hidden><label><input type="checkbox" id="play"> Play sequence</label>
<label>Preview frames/second <input id="rate" type="number" min="1" max="60" value="18"></label>
<p class="note">Playback advances both views through the same numbered captures. The selected rate is a review aid; capture disk I/O changes timing, so this does not reproduce game frame pacing. CPU timings use one pass without warmups in sequence mode.</p></div>
<div class="pair"><figure><figcaption id="left-caption"></figcaption><div class="scroll"><img id="left-image" alt="Left scaler comparison"></div></figure>
<figure><figcaption id="right-caption"></figcaption><div class="scroll"><img id="right-image" alt="Right scaler comparison"></div></figure></div>
<p class="note">1080p/1440p previews retain 4:3 proportions. Pixelart uses nearest-neighbor preview sampling; other modes use bilinear sampling after their native scaler output. These previews approximate presentation and do not reproduce SDL's pixel-art sampler exactly. Select native output to inspect the unmodified scaler result.</p>
<table><thead><tr><th>Scaler</th><th>Full-frame CPU ms</th><th>Unchanged-frame CPU ms</th><th>Output buffer MiB</th><th>GPU submission and wait ms</th></tr></thead><tbody id="timings"></tbody></table>
<p class="note">Timings: __CPU_SCOPE__ on this machine. Full-frame measurements invalidate the entire image each time; unchanged-frame measurements exercise dirty detection. GPU upload, display sampling, gameplay, and presentation are excluded. This is not an end-to-end frame-rate benchmark.</p>
<p class="note">When Vulkan results are available, each scaler has passed a pixel-for-pixel GPU readback comparison against the CPU output. GPU times include upload, command submission, execution, and an idle wait, averaged over 10 iterations after five warmups. GPU and CPU columns measure different scopes; neither includes window presentation.</p>
<script>const data=__DATA__,gpuData=__GPU_DATA__;
const el=id=>document.getElementById(id);
function option(select,value){const item=document.createElement('option');item.value=value;item.textContent=value;select.append(item)}
Object.keys(data).forEach(name=>option(el('screen'),name));
data[Object.keys(data)[0]].forEach(row=>['left','right'].forEach(id=>option(el(id),row.scaler)));
el('left').value='pixelart';el('right').value='mmpx2';el('screen').value='research' in data?'research':Object.keys(data)[0];
el('gpu').disabled=Object.keys(gpuData).length===0;
if(__SEQUENCE__){el('resolution').value='native';Array.from(el('resolution').options).forEach(option=>{if(option.value!=='native')option.remove()})}
function refresh(){const screen=el('screen').value,res=el('resolution').value;
document.querySelector('.pair').classList.toggle('actual',el('actual').checked);
for(const side of ['left','right']){const mode=el(side).value,useGpu=side==='right'&&el('gpu').checked&&screen in gpuData;el(side+'-caption').textContent=mode+' — '+screen+(useGpu?' — Vulkan shader':' — CPU');
el(side+'-image').src=encodeURIComponent(screen)+'/'+(useGpu?'gpu/':'')+mode+(res==='native'?'':'-'+res)+'.png'}
el('timings').replaceChildren();for(const row of data[screen]){const tr=document.createElement('tr');
const gpuRow=(gpuData[screen]||[]).find(item=>item.scaler===row.scaler);
for(const value of [row.scaler,Number(row.full_frame_ms).toFixed(2),Number(row.idle_frame_ms).toFixed(3),row.output_mib,gpuRow?Number(gpuRow.submission_and_gpu_wait_ms).toFixed(2):'—']){const td=document.createElement('td');td.textContent=value;tr.append(td)}el('timings').append(tr)}}
document.querySelectorAll('select,input').forEach(control=>control.addEventListener('change',refresh));refresh();
el('sequence-controls').hidden=!__SEQUENCE__;
let timer=null;
function playback(){clearInterval(timer);if(!el('play').checked)return;
const rate=Math.min(60,Math.max(1,Number(el('rate').value)||18));
timer=setInterval(()=>{const screen=el('screen');screen.selectedIndex=(screen.selectedIndex+1)%screen.options.length;refresh()},1000/rate)}
el('play').addEventListener('change',playback);el('rate').addEventListener('change',playback);
</script></html>'''
    gpu_metadata = json.dumps(gpu_results).replace("<", "\\u003c")
    scope = "one iteration without warmups" if args.sequence else "mean of 30 iterations after 5 warmups"
    (args.output / "index.html").write_text(html.replace("__DATA__", metadata).replace("__GPU_DATA__", gpu_metadata)
                                         .replace("__SEQUENCE__", json.dumps(args.sequence)).replace("__CPU_SCOPE__", scope))
    (args.output / "timings.json").write_text(json.dumps(results, indent=2))
    (args.output / "gpu-timings.json").write_text(json.dumps(gpu_results, indent=2))
    print(args.output / "index.html")


if __name__ == "__main__":
    main()
