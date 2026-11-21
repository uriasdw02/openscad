# PipeTrack: sketches and photos to 3D pipe

This is step 1: **pipe.scad**, the library that turns a pipe run into a 3D spool with real sizes.
Later steps (sketch reader, BOM reader, PCF/IFC export, phone camera) all produce or read the
same **pipe run** format described below.

## Quick start
After rebuilding OpenSCAD, any model anywhere can start with `use <pipetrack/pipe.scad>`.
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
    ["flange"],           // flanged joint (gasket + bolts in BOM)
    ["reducer", 2],       // concentric reducer to 2"
    ["tee", [ ["N", 500] ], "flange"],   // tee with a branch run
    ["fw"],               // field weld (splits spools); ["sw"] = shop weld
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
| rating | flange / valve class: `150` or `300` |
| start / end | `open` or `flange` |
| units | `mm` or `in` (lengths can also be feet-inch strings like `"10'-6 3/4\""` in any units) |
| line | line number for reports and PCF, e.g. `"CW-6-101"` |
| show_labels | `true` shows weld numbers in 3D (red = field weld) |

JSON form (same data, for the AI and for exports):
```json
{ "nps": 4, "sch": "40", "material": "CS", "units": "mm",
  "start": "flange", "end": "flange", "elbow": "LR",
  "run": [["N",1050],["E",800],["U",750],["valve","gate"],["U",350]] }
```

## What the console shows (Window > Console after F5)
- **Cut list by spool** (S1, S2...; spools break at field welds, flanged joints and valves)
- **Weld map**: W1, W2... shop welds and FW field welds with spool and location
- **Elbow cuts** for odd angles: outside / centerline / inside arc marks to cut from a 90, plus take-out
- **BOM** per size: pipe length, elbows (90 / 45 / cut), tees, WN flanges, gaskets, stud bolts, valves, reducers

## Reading sketches and BOMs with AI
Turn on the AI panel (Edit > Preferences > Features > ai-features, then Window > AI), point it at your local
model in Preferences > AI (Ollama: `http://localhost:11434/v1`, LM Studio: `http://localhost:1234/v1`),
attach a photo of the iso sketch and/or the BOM and ask: "model this spool". The assistant knows the
PipeTrack format, writes the pipe_run() with a review table of every dimension it read, and previews it.

## PCF export (for Spoolgen, Plant 3D, Isogen...)
In the MSYS2 window:
```
./pipetrack/export-pcf.sh pipetrack/examples/tee_reducer_fieldweld.scad
```
writes `tee_reducer_fieldweld.pcf` next to the model.

## Data sources
Nominal values from ASME B36.10M / B36.19M (pipe), B16.9 (elbows), B16.5 class 150 (WN flanges)
and B16.10 (valve face-to-face). Meant for modeling; verify against spec sheets before fabrication.

## Coming next
Pipe panel (point-and-click), sketch + BOM reading through a local vision model, printable spool
sheets, class 600+ flanges, socket weld / threaded, reducing tees, O-lets, elbow cut marks, IFC export.
