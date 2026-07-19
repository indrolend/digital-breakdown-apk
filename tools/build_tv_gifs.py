"""Download the explicitly curated TV GIF allowlist and bake 12x8 RGB565 clips."""
from io import BytesIO
from pathlib import Path
import struct
import urllib.request
from PIL import Image, ImageOps, ImageSequence

URLS = [
    "https://web.archive.org/web/20091026070943im_/http://geocities.com/boss_be_99/ab41.gif",
    "https://web.archive.org/web/20091027075929im_/http://geocities.com/pnmngray/rabbit_in_grass_lg_blk.gif",
    "https://blob.gifcities.org/gifcities/N7SOZRWIRQIMOFSA4UHVSOPM3S3EMFQ7.gif",
    "https://blob.gifcities.org/gifcities/Z4OZQWQDWEDPFHRTRPNWKFXWS5QCMGS7.gif",
    "https://web.archive.org/web/20091027113902if_/http://geocities.com/al_birdie_2000/gif/disco4.gif",
    "https://web.archive.org/web/20091027134026im_/http://geocities.com/lpcsti/email8.gif",
    "https://web.archive.org/web/20091027083753im_/http://www.geocities.com/djmechelen/music.gif",
    "https://web.archive.org/web/20091027075730im_/http://geocities.com/SoHo/Easel/1267/intro.gif",
    "https://blob.gifcities.org/gifcities/6U27Q2KD4GODHO3J2E7XXPCLK5P6273Q.gif",
    "https://blob.gifcities.org/gifcities/CARRZPVWSW2XR3UDM5FBQEBGCU7VOLE5.gif",
    "https://blob.gifcities.org/gifcities/3AWIVVMMZWUA6KPXNEFBXLXZFEA4P3LO.gif",
    "https://blob.gifcities.org/gifcities/FHNLETW4TJP6RA6KG6XT2J6WMFQON33K.gif",
    "https://blob.gifcities.org/gifcities/S4O5KHO6NZN6QCKT5RHOEPSH4HXH4TZT.gif",
    "https://blob.gifcities.org/gifcities/4RSQVOL63HX5YMB6LOCB5IJYFHOKDF6Y.gif",
    "https://blob.gifcities.org/gifcities/GP36RATT3AHTDCMJVZA3CXV2BXCIZAHR.gif",
]

root = Path(__file__).resolve().parents[1]
output = root / "native-tv-gifs"
output.mkdir(exist_ok=True)
for index, url in enumerate(URLS, 1):
    target = output / f"tv{index:02d}.dbgif"
    target.unlink(missing_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "DigitalBreakdown-TV-Asset-Baker/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            image = Image.open(BytesIO(response.read()))
    except Exception as error:
        print(f"tv{index:02d}: unavailable ({error})")
        continue
    frames = []
    for frame_index, frame in enumerate(ImageSequence.Iterator(image)):
        if frame_index >= 120:
            break
        rgb = ImageOps.fit(frame.convert("RGB"), (12, 8), method=Image.Resampling.LANCZOS)
        delay = max(20, min(2000, int(frame.info.get("duration", image.info.get("duration", 100)) or 100)))
        pixels = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in rgb.getdata()]
        frames.append((delay, pixels))
    with target.open("wb") as stream:
        stream.write(struct.pack("<4sHHH", b"DBGF", 12, 8, len(frames)))
        for delay, pixels in frames:
            stream.write(struct.pack("<H", delay))
            stream.write(struct.pack(f"<{len(pixels)}H", *pixels))
    print(f"tv{index:02d}: {len(frames)} frames")
