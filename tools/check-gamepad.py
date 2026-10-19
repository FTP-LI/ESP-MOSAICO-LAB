"""Check the exported EDA connectivity against the shared H2 input contract."""
import csv
import json
import re
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
    assert components[r["pullup"]]["props"]["Name"] == "10k"
    assert components[r["series"]]["props"]["Name"] == "1k"
assert all(not net("J1", n) for n in range(10, 19))
assert net("J1", 19) == "OUT_3V3" and net("J1", 20) == "GND"

source = (root / "firmware/game-console/main/diagnostics.c").read_text(encoding="utf-8")
gpio_block = source.split("key_pins[] = {", 1)[1].split("};", 1)[0]
assert re.findall(r"GPIO_NUM_(\d+)", gpio_block) == [r["gpio"] for r in rows]
names_block = source.split("key_names[] = {", 1)[1].split("};", 1)[0]
assert re.findall(r'"([A-Z]+)"', names_block)[:9] == [r["action"] for r in rows]
assert len([r for r in components if r and re.fullmatch(r"(?:J|R|SW)\d+", r)]) == 28
print("PASS: 9 keys, pull-ups, series resistors, reserved pins, supply and firmware mapping")
