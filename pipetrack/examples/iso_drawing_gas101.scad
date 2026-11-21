// Clean iso drawing of the GAS-101 test spool.
// F5 to preview, F6 to render, then File > Export > Export as PDF (or SVG / DXF).
use <../pipe.scad>

pipe_run([
    ["W", 400], ["valve", "gate"], ["W", 1000], ["S", 600], ["D", 250], ["S", 900],
    ["tee", [ ["D", 800], ["valve", "gate"] ], "flange"],
    ["S", 1100], ["E", 1000], ["D", 300], ["E", 600], ["U", 300], ["valve", "gate"], ["U", 200]
  ],
  nps = 2, sch = "40", material = "CS", units = "mm",
  start = "flange", end = "flange", line = "GAS-101",
  drawing = true);
