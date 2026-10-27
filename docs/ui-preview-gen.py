#!/usr/bin/env python3
"""Generates the README's UI preview image and prints its path.

Renders the SF Symbols the app uses with a small Cocoa helper (compiled with clang),
lays out a mockup in HTML and screenshots it with headless Chrome.
Keep content() in sync with the app when the UI changes.

Usage: python3 docs/ui-preview-gen.py
"""

import base64
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
WIDTH = 1040
SCALE = 2

# key: (SF Symbol, point size)
SYMBOLS = {
    "box": ("shippingbox", 15),
    "transition": ("arrow.triangle.2.circlepath", 15),
    "wifi": ("wifi", 15),
    "battery": ("battery.75percent", 15),
}


def main():
    if not Path(CHROME).exists():
        sys.exit(f"Needs Google Chrome at {CHROME}")
    out = Path(tempfile.mkdtemp(prefix="colimabar-ui-preview-"))
    html = out / "ui-preview.html"
    html.write_text(page(render_symbols(out)))
    png = out / "ui-preview.png"
    screenshot(html, png)
    print(png)


def content():
    running = item(sym("box"), txt("12"))
    busy = item(sym("box"), txt("12"), txt("288%", "orange"))
    idle = item(sym("box"), txt("0", "orange"))
    stopped = item(sym("box", "dim"))
    two_busy = item(sym("box"), txt("12"), txt("290%", "orange"))
    default_info = ["default · 6 CPU · 10 GiB", "On Mac: CPU 288% · RAM 10.0G"]
    groups = [
        group("k3d payments", "505%", "5.0G"),
        group("k3d search", "66%", "1.1G"),
        group("compose blog", "7%", "173M"),
        group("postgres-dev", "0%", "29M"),
    ]
    return f"""

<h2>Menu bar</h2>
<div class="states">
  {state(running, "Running", "Running containers, over all running profiles")}
  {state(busy, "Busy", "CPU of the VMs on the Mac above <code>busy_cpu</code>")}
  {state(idle, "Idle", "No containers, but a VM still holds its memory")}
  {state(stopped, "Stopped", "No profile running")}
  {state(item(sym("transition"), txt("starting")), "Starting",
         "<code>colima start</code> running, e.g. in a terminal; also <i>stopping</i>")}
</div>

<h2>Menu</h2>
<div class="menus">
  {menu(busy, [(default_info, groups)],
        "Containers grouped by k3d cluster and compose project, busiest first")}
  {menu(two_busy, [(default_info, groups), (["wso2-rosetta · 6 CPU · 10 GiB", "On Mac: CPU 2% · RAM 10.0G"], [])],
        "A section per running profile; wso2-rosetta is idle")}
  {menu(stopped, [(["No profile running"], [])], "Stopped, in dark mode", theme="dark")}
</div>
"""


# ---------------------------------------------------------------------------
# Mockup components
# ---------------------------------------------------------------------------
def state(colimabar, name, description):
    return f'<div class="state">{bar(colimabar)}<div><b>{name}</b><span class="sub">{description}</span></div></div>'


# sections: [(info lines, group rows)], one per running profile
def menu(colimabar, sections, caption, theme="light"):
    rows = "".join(
        "".join(f'<div class="mi dis">{line}</div>' for line in info)
        + (f'<div class="sep"></div>{"".join(groups)}' if groups else "")
        + '<div class="sep"></div>'
        for info, groups in sections)
    return f"""<div>{bar(colimabar, theme)}
    <div class="menu {theme}">
      {rows}
      <div class="mi">Quit<span class="key">⌘Q</span></div>
    </div>
    <p class="caption">{caption}</p></div>"""


def group(name, cpu, mem):
    return (f'<div class="mi row"><span class="name">{name}</span>'
            f'<span class="num">{cpu}</span><span class="num">{mem}</span></div>')


def bar(colimabar, theme="light"):
    return f'<div class="bar {theme}">{colimabar}<span class="native">{sym("wifi")}{sym("battery")}<span>Thu 11:42</span></span></div>'


def item(*parts):
    return f'<span class="item">{"".join(parts)}</span>'


def sym(key, cls=""):
    return f'<i class="sym s-{key} {cls}"></i>'


def txt(text, cls=""):
    return f'<span class="t {cls}">{text}</span>'


# ---------------------------------------------------------------------------
# Page
# ---------------------------------------------------------------------------
CSS = """
body { width: 960px; margin: 0; padding: 36px 40px 44px; font: 13px -apple-system, BlinkMacSystemFont, sans-serif; color: #1d1d1f; background: #f5f5f7; }
h2 { font-size: 15px; margin: 34px 0 12px; }
h2:first-child { margin-top: 0; }
p.caption { color: #6e6e73; margin: 10px 0 0; max-width: 860px; }
code { font: 12px ui-monospace, Menlo, monospace; background: rgba(0,0,0,.06); padding: 0 3px; border-radius: 3px; }

/* SF Symbols are alpha masks over the text color, like template images in the menu bar */
.sym { display: inline-block; background: currentColor; -webkit-mask: var(--m) center / contain no-repeat; mask: var(--m) center / contain no-repeat; }
.orange { color: #e8890c; } .dim { opacity: .35; }

.bar { display: flex; flex: none; align-items: center; justify-content: space-between; gap: 12px; width: 250px; height: 26px; padding: 0 10px; border-radius: 7px; }
.bar.light { background: rgba(255,255,255,.75); box-shadow: inset 0 0 0 .5px #c8c8cc; }
.bar.dark { background: #262626; color: #fff; }
.native { display: flex; align-items: center; gap: 13px; white-space: nowrap; }
.item { display: inline-flex; align-items: center; gap: 4px; padding: 2px 7px; border-radius: 5px; background: rgba(0,122,255,.13); white-space: nowrap; }
.bar.dark .item { background: rgba(255,255,255,.18); }
.t { font-variant-numeric: tabular-nums; }

.states { display: grid; grid-template-columns: 1fr 1fr; gap: 14px 32px; }
.state { display: flex; align-items: center; gap: 14px; }
.state b { display: block; }
.state .sub { color: #6e6e73; font-size: 12px; }

.menus { display: flex; gap: 26px; align-items: flex-start; }
.menus > div { flex: 1; min-width: 0; }
.menus .bar { width: auto; }
.menu { margin-top: 6px; padding: 5px; border-radius: 10px; box-shadow: 0 10px 28px rgba(0,0,0,.22), 0 0 0 .5px rgba(0,0,0,.18); }
.menu.light { background: rgba(246,246,246,.97); }
.menu.dark { background: rgba(40,40,40,.97); color: #eee; }
.mi { position: relative; padding: 3px 10px; border-radius: 5px; line-height: 16px; white-space: nowrap; }
.mi.dis { color: #8e8e93; }
.mi.row { display: flex; align-items: center; gap: 8px; }
.mi.row .name { flex: 1; }
.mi.row .num { width: 38px; text-align: right; font-variant-numeric: tabular-nums; }
.mi.row .num { color: #6e6e73; }
.key { position: absolute; right: 10px; color: #8e8e93; }
.sep { height: 1px; margin: 5px 10px; background: rgba(127,127,127,.3); }
"""


def page(symbols):
    symbol_css = "\n".join(
        f".s-{key} {{ width: {w:g}px; height: {h:g}px; --m: url(data:image/png;base64,{b64}); }}"
        for key, (b64, w, h) in symbols.items()
    )
    # The title reports the content height, so the screenshot can be cropped to it
    return f"""<!doctype html>
<meta charset="utf-8">
<title></title>
<style>{CSS}
{symbol_css}
</style>
{content()}
<script>document.title = document.documentElement.scrollHeight</script>
"""


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------
RENDER_SYMBOL_M = r"""
// render-symbol <name> <point size> <out.png>: SF Symbol as a 2x alpha mask
#import <Cocoa/Cocoa.h>
int main(int argc, char **argv) { @autoreleasepool {
    NSImage *image = [[NSImage imageWithSystemSymbolName:@(argv[1]) accessibilityDescription:nil]
        imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:atof(argv[2]) weight:NSFontWeightRegular]];
    if (!image) { fprintf(stderr, "Unknown SF Symbol %s\n", argv[1]); return 1; }
    NSRect rect = {NSZeroPoint, image.size};
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
        pixelsWide:rect.size.width * 2 pixelsHigh:rect.size.height * 2 bitsPerSample:8 samplesPerPixel:4
        hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    rep.size = rect.size;
    NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [image drawInRect:rect];
    return ![[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:@(argv[3]) atomically:YES];
}}
"""


# Returns {key: (base64 png, width pt, height pt)}
def render_symbols(out):
    helper = out / "render-symbol"
    subprocess.run(["clang", "-fobjc-arc", "-framework", "Cocoa", "-x", "objective-c", "-", "-o", helper],
                   input=RENDER_SYMBOL_M, text=True, check=True)
    symbols = {}
    for key, (name, size) in SYMBOLS.items():
        png = out / f"{key}.png"
        subprocess.run([helper, name, str(size), png], check=True)
        data = png.read_bytes()
        width, height = struct.unpack(">II", data[16:24])
        symbols[key] = (base64.b64encode(data).decode(), width / 2, height / 2)
    return symbols


def screenshot(html, png):
    dom = chrome("--dump-dom", f"--window-size={WIDTH},100", html.as_uri())
    height = int(re.search(r"<title>(\d+)</title>", dom)[1])
    chrome(f"--screenshot={png}", f"--window-size={WIDTH},{height}", f"--force-device-scale-factor={SCALE}", html.as_uri())


def chrome(*args):
    return subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars", *args],
                          capture_output=True, text=True, check=True).stdout


if __name__ == "__main__":
    main()
