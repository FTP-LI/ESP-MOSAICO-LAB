"""Serve this project's synthetic UI prototype on loopback only."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Trader-Lab 界面原型（合成数据）")
    parser.add_argument("--port", type=int, default=8770)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("端口应为 1–65535")
    root = Path(__file__).resolve().parents[1]
    handler = partial(SimpleHTTPRequestHandler, directory=str(root))
    with ThreadingHTTPServer(("127.0.0.1", args.port), handler) as server:
        print(f"Trader-Lab prototype: http://127.0.0.1:{args.port}/prototype/", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
