"""Measure the supplied Mosaico frame STL; never modify the source mesh.

Requires numpy, trimesh, networkx and matplotlib. Output uses raw STL axes.
Millimetres are inferred by comparing the side-opening pitch with H2's 2.54 mm.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import trimesh
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("stl", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
mesh = trimesh.load_mesh(args.stl)
args.output.mkdir(parents=True, exist_ok=True)


def section(axis, value):
    normal = np.eye(3)[axis]
    path = mesh.section(plane_origin=normal * value, plane_normal=normal)
    return [] if path is None else list(path.discrete)


# Fixed section planes belong to this supplied frame, not to arbitrary STL files.
sides = {}
for x in (-21, 21):
    holes = []
    for loop in section(0, x):
        extent = np.ptp(loop, axis=0)
        if np.allclose(extent[1:], [1, 1], atol=0.002):
            holes.append((loop.min(axis=0) + loop.max(axis=0)) / 2)
    if len(holes) != 20:
        raise ValueError(f"Expected 20 one-unit side openings at X={x}; found {len(holes)}")
    centers = np.array(sorted(holes, key=lambda p: (round(p[2], 3), p[1])))
    ys = np.unique(np.round(centers[:, 1], 5))
    zs = np.unique(np.round(centers[:, 2], 5))
    pitch = np.diff(ys)
    if len(ys) != 10 or len(zs) != 2 or not np.allclose(pitch, 2.54, atol=0.002):
        raise ValueError("Side-hole pattern does not match the known H2 pitch")
    if not np.isclose(zs[1] - zs[0], 2.54, atol=0.002):
        raise ValueError("Unexpected row spacing")
    sides[str(x)] = {"section_x": x, "centers": centers.tolist(),
                     "column_pitch": float(np.mean(pitch)),
                     "row_pitch": float(zs[1] - zs[0]),
                     "first_to_last": float(ys[-1] - ys[0]),
                     "opening_size": [1, 1]}

mounts = []
for loop in section(2, -3):
    xy = loop[:-1, :2]
    extent = np.ptp(xy, axis=0)
    if np.all((extent > 1.7) & (extent < 1.9)):
        solution = np.linalg.lstsq(np.column_stack((2 * xy, np.ones(len(xy)))),
                                   (xy ** 2).sum(axis=1), rcond=None)[0]
        center = solution[:2]
        radius = np.sqrt(solution[2] + (center ** 2).sum())
        mounts.append({"center_xy": center.tolist(), "section_z": -3,
                       "diameter_approx": float(2 * radius)})
if len(mounts) != 4:
    raise ValueError(f"Expected four circular mounting holes; found {len(mounts)}")

result = {
    "filename": args.stl.name,
    "sha256": hashlib.sha256(args.stl.read_bytes()).hexdigest(),
    "triangles": len(mesh.faces), "vertices": len(mesh.vertices),
    "watertight": bool(mesh.is_watertight),
    "winding_consistent": bool(mesh.is_winding_consistent),
    "stl_units": mesh.units, "interpreted_units": "mm",
    "unit_basis": "Both side opening grids agree with the manufacturer's 2.54 mm pitch and row spacing",
    "bounds": mesh.bounds.tolist(), "extents": mesh.extents.tolist(),
    "side_openings": sides, "mounting_holes": mounts,
    "limits": ["Frame only, not complete assembled host thickness",
               "Raw XYZ axes; no electrical pin numbers or H2/H1 side assignment inferred",
               "Mesh dimensions are nominal geometry, not manufacturing tolerances",
               "Hole circle fits do not identify thread or screw size"],
}
(args.output / "host-frame-measurements.json").write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

plt.rcParams["font.family"] = "DejaVu Sans"
fig, axes = plt.subplots(1, 2, figsize=(12, 6), layout="constrained")
for line in section(2, -3):
    axes[0].plot(line[:, 0], line[:, 1], color="#26688b", linewidth=0.8)
for h in mounts:
    x, y = h["center_xy"]
    axes[0].annotate(f"({x:.2f}, {y:.2f})", (x, y), xytext=(x * 0.62, y),
                     fontsize=7, ha="center", arrowprops={"arrowstyle": "-", "lw": 0.5})
axes[0].set(title="Frame XY at Z=-3 | 44.90 x 44.90 mm", xlabel="Raw X (mm)", ylabel="Raw Y (mm)")
for line in section(0, -21):
    axes[1].plot(line[:, 1], line[:, 2], color="#26688b", linewidth=0.8)
axes[1].set(xlabel="Raw Y (mm)", ylabel="Raw Z (mm)")
axes[1].set_title("Side openings at X=-21 | frame depth 11.10 mm\n"
                  "2 x 10 openings | pitch / row spacing 2.54\n"
                  "Y = 11.0075 ... 33.8675 | Z = -6.0300 / -8.5700", fontsize=10)
for ax in axes:
    ax.set_aspect("equal")
    ax.grid(alpha=0.2)
fig.suptitle("ESP-MOSAICO.stl | nominal mesh measurements, not a fabrication drawing", fontsize=12)
fig.savefig(args.output / "host-frame-dimensions.png", dpi=170)
plt.close(fig)
print(json.dumps({"extents_mm": result["extents"], "mounting_holes": mounts}, indent=2))
