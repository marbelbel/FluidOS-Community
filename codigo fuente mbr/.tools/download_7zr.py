import hashlib
import pathlib
import urllib.request


url = "https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe"
expected = "9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7"
size = 602624
destination = pathlib.Path(__file__).parent / "7zr.exe"
chunk_size = 1024 * 1024
with destination.open("wb") as output:
    for start in range(0, size, chunk_size):
        end = min(start + chunk_size - 1, size - 1)
        request = urllib.request.Request(
            url,
            headers={
                "User-Agent": "VOS-local-build-tools/1.0",
                "Range": f"bytes={start}-{end}",
            },
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            content_range = response.headers.get("Content-Range", "")
            if response.status != 206 or content_range != f"bytes {start}-{end}/{size}":
                raise SystemExit(f"Unexpected response: {response.status} {content_range}")
            data = response.read()
        if len(data) != end - start + 1:
            raise SystemExit(f"Incomplete range {start}-{end}")
        output.write(data)
actual = hashlib.sha256(destination.read_bytes()).hexdigest()
print(f"SHA256 {actual}")
if actual != expected:
    destination.unlink(missing_ok=True)
    raise SystemExit("7zr SHA256 mismatch")
