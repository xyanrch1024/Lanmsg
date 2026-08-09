#!/usr/bin/env python3
"""Decode QLanMsg TCP frames from a raw byte stream.

Frame layout (all little-endian):
    [total u32][msgType u32][jsonLen u32][json bytes][body bytes]

Usage:
    # capture a session with tcpdump, then decode
    tcpdump -i en0 -X port 24261 -w dump.pcap
    # extract payload bytes with tshark, or feed a raw file:
    python3 protocol_dump.py frames.bin
    # or pipe hex:
    echo -n "0c0000000200000000000000..." | xxd -r -p | python3 protocol_dump.py
"""
import json
import struct
import sys

MSG_TYPES = {
    1: "Hello", 2: "Chat", 3: "FileOffer", 4: "FileAccept", 5: "FileDecline",
    6: "FileChunk", 7: "FileDone", 8: "FileCancel", 9: "RcRequest",
    10: "RcAccept", 11: "RcDecline", 12: "RcFrame", 13: "RcInput",
    14: "RcStop", 15: "RcPing", 16: "RcPong",
}


def decode(data: bytes):
    buf = bytearray(data)
    n = 0
    while len(buf) >= 12:
        total, mtype, json_len = struct.unpack_from("<III", buf, 0)
        if total < 12:
            print(f"  ! malformed total={total}, dropping rest")
            return
        if len(buf) < total:
            print(f"  (waiting: have {len(buf)}, need {total})")
            return
        frame = bytes(buf[:total])
        del buf[:total]
        payload = frame[12:12 + json_len]
        body = frame[12 + json_len:]
        name = MSG_TYPES.get(mtype, f"Unknown({mtype})")
        try:
            obj = json.loads(payload) if payload else {}
        except json.JSONDecodeError as e:
            obj = {"!json_error": str(e), "raw": payload[:64].decode("latin1", "replace")}
        brief = obj
        if name in ("FileChunk", "RcFrame"):
            brief = dict(obj)
            brief["body_bytes"] = len(body)
        if name == "Chat":
            brief = dict(obj)
            brief["text"] = brief.get("text", "")[:60]
        print(f"#{n:03d} {name:<12} len={total:<6} json={json.dumps(brief, ensure_ascii=False)}")
        n += 1
    if buf:
        print(f"  (trailing {len(buf)} bytes)")


def main():
    data = sys.stdin.buffer.read()
    if not data:
        print("no input")
        sys.exit(1)
    print(f"decoding {len(data)} bytes")
    decode(data)


if __name__ == "__main__":
    main()
