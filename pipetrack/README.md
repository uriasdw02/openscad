# PipeTrack: sketches and photos to 3D pipe

This is step 1: **pipe.scad**, the library that turns a pipe run into a 3D spool with real sizes.
Later steps (sketch reader, BOM reader, PCF/IFC export, phone camera) all produce or read the
same **pipe run** format described below.

## Quick start
Open `examples/sketch_1050_800.scad` in OpenSCAD and press F5. The Customizer lets you change
NPS, schedule and material. The console prints the **cut list** and **BOM**.

## Pipe run format (what the AI will output)

```
pipe_run([
    ["N", 1050],          // leg: direction + length, work point to work point
    ["E",  800],
    ["U",  750],
    ["valve", "gate"],    // inline flanged valve: gate | globe | ball | check
    ["U",  350],
    ["flange"]            // optional flanged joint mid-run
  ],
  nps=4, sch="40", material="CS", units="mm",
  start="flange", end="flange", elbow="LR");
```

| Setting | Values |
|---|---|
| direction | `E W N S U D`, combos `NE`, `SU`, `NEU`..., or a vector `[1,1,0]` |
| nps | 0.5 to 24 |
| sch | `5S 10S 40 STD 40S 80 XS 80S 160 XXS` |
| material | `CS SS CHROME AL CU CUNI NI TI DUPLEX GALV` (more can be added) |
| elbow | `LR`, `SR`, or bend radius `3D`, `5D`... |
| start / end | `open` or `flange` |
| units | `mm` or `in` |

JSON form (same data, for the AI and for exports):
```json
{ "nps": 4, "sch": "40", "material": "CS", "units": "mm",
  "start": "flange", "end": "flange", "elbow": "LR",
  "run": [["N",1050],["E",800],["U",750],["valve","gate"],["U",350]] }
```

## Data sources
Nominal values from ASME B36.10M / B36.19M (pipe), B16.9 (elbows), B16.5 class 150 (WN flanges)
and B16.10 (valve face-to-face). Meant for modeling; verify against spec sheets before fabrication.

## Coming next
Tees and reducers, class 300+ flanges, socket weld / threaded, BOM upload (material and specs),
sketch reading through a local vision model, PCF / IFC export.
