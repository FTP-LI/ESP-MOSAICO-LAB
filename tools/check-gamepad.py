"""Check the exported EDA connectivity against the shared H2 input contract."""
import csv
import json
import re
import zipfile
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parents[1]
board = root / "hardware/gamepad-board/rev-0.1"
netlist = json.loads((board / "eda/gamepad-board-rev-0.1.net").read_text(encoding="utf-8-sig"))
components = {c["props"].get("Designator"): c for c in netlist["components"].values()}
rows = list(csv.DictReader((board / "pin-map.csv").open(encoding="utf-8-sig", newline="")))

def net(ref, pin):
    return components[ref]["pinInfoMap"][str(pin)]["net"]

assert len(rows) == 9
assert len({r["gpio"] for r in rows}) == 9
for r in rows:
    switch_net = "SW_" + r["action"]
    assert net("J1", r["host_pin"]) == r["net"]
    assert net(r["series"], 1) == r["net"]
    assert net(r["series"], 2) == switch_net
    assert net(r["pullup"], 1) == net(r["switch"], 1) == switch_net
    assert net(r["pullup"], 2) == "OUT_3V3"
    assert net(r["switch"], 2) == "GND"
    pullup = components[r["pullup"]]["props"]
    series = components[r["series"]]["props"]
    switch = components[r["switch"]]["props"]
    assert pullup["Name"] == pullup["Value"] == "10kΩ"
    assert series["Name"] == series["Value"] == "1kΩ"
    assert (pullup["Manufacturer Part"], pullup["Supplier Part"]) == ("0603WAF1002T5E", "C25804")
    assert (series["Manufacturer Part"], series["Supplier Part"]) == ("0603WAF1001T5E", "C21190")
    assert switch["Name"] == switch["Manufacturer Part"] == "KAN0444-0201B/102"
    assert switch["Supplier Part"] == "C231644" and switch["GameAction"] == r["action"]
assert all(not net("J1", n) for n in range(10, 19))
assert net("J1", 19) == "OUT_3V3" and net("J1", 20) == "GND"

source = (root / "firmware/game-console/main/diagnostics.c").read_text(encoding="utf-8")
gpio_block = source.split("key_pins[] = {", 1)[1].split("};", 1)[0]
assert re.findall(r"GPIO_NUM_(\d+)", gpio_block) == [r["gpio"] for r in rows]
names_block = source.split("key_names[] = {", 1)[1].split("};", 1)[0]
assert re.findall(r'"([A-Z]+)"', names_block)[:9] == [r["action"] for r in rows]
physical = {ref: c["props"] for ref, c in components.items() if ref and re.fullmatch(r"(?:J|R|SW)\d+", ref)}
assert len(physical) == 28
assert all(p["Add into BOM"] == p["Convert to PCB"] == "yes" for p in physical.values())
assert physical["J1"]["Name"] == physical["J1"]["Manufacturer Part"] == "PZ254V-12-20P"
assert physical["J1"]["Supplier Part"] == "C492427"


def records(text):
    """Read EDA source records, including empty deletion records."""
    for line in text.splitlines():
        if "||" in line:
            header, data = line.split("||", 1)
            data = data.rstrip("|")
            yield json.loads(header), json.loads(data) if data else None


def check_net_names(page):
    names = Counter(d["parentId"] for h, d in page
                    if d and h["type"] == "ATTR" and d.get("key") == "NET")
    assert all(count == 1 for count in names.values()), "Duplicate NET attributes in a wire group"


def normalize(value):
    """Ignore archive bookkeeping and normalize floating-point roundoff."""
    if isinstance(value, dict):
        return {k: normalize(v) for k, v in value.items()
                if k != "zIndex" and not (k == "strikeout" and v is False)}
    if isinstance(value, list):
        return [normalize(v) for v in value]
    if isinstance(value, float):
        return round(value, 8)
    return value


def primitives(page):
    return {(h["type"], h.get("id")): normalize(d) for h, d in page
            if d is not None and h["type"] not in ("DOCHEAD", "META", "ELE_PLACEHOLDER", "CANVAS")}


page = list(records((board / "eda/gamepad-board-rev-0.1.esch").read_text(encoding="utf-8")))
check_net_names(page)
assert sum(h["type"] == "WIRE" for h, d in page if d is not None) == 47
with zipfile.ZipFile(board / "eda/gamepad-board-rev-0.1.epro2") as archive:
    assert archive.testzip() is None
    documents = {}
    for header, data in records(archive.read("ESP-MOSAICO-LAB.epru").decode("utf-8")):
        if header["type"] == "DOCHEAD":
            current = data["uuid"]
            documents[current] = {"type": data["docType"], "records": []}
        else:
            documents[current]["records"].append((header, data))
    assert sum(d["type"] == "SCH" for d in documents.values()) == 1
    assert sum(d["type"] == "SCH_PAGE" for d in documents.values()) == 1
    saved_page = documents["43d12f3fc0a5c859"]["records"]
    assert primitives(saved_page) == primitives(page), "Native project and page source differ"
    pcb = documents["a9cfdddb6baec258"]["records"]
    assert next(d["title"] for h, d in pcb if h["type"] == "META") == "GamePlay V1.0"
assert json.loads((board / "eda/drc.json").read_text(encoding="utf-8")) == []
print("PASS: 9-key connectivity, 28 BOM parts, firmware mapping, unique NET names, native export and PCB name")
print("Archived DRC result is empty; this script does not run live EDA DRC or verify physical hardware.")
