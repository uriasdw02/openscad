// ANSWER KEY for tests/gas101_iso.png  (the AI's model should match this)
//
// Expected console cut list (2" Sch 40 CS, mm):
//   S1: 273.0
//   S2: 860.3  447.6  97.6  760.3  960.3  847.6  147.6  447.6  160.3   branch: 673.0
//   S3: 73.0
//   24 welds, 3 gate valves, 1 tee, 7 elbows 90 LR

use <../pipe.scad>

pipe_run([
    ["W", 400],
    ["valve", "gate"],
    ["W", 1000],
    ["S", 600],
    ["D", 250],
    ["S", 900],
    ["tee", [
        ["D", 800],
        ["valve", "gate"]
      ], "flange"],
    ["S", 1100],
    ["E", 1000],
    ["D", 300],
    ["E", 600],
    ["U", 300],
    ["valve", "gate"],
    ["U", 200]
  ],
  nps = 2, sch = "40", material = "CS", rating = 150, units = "mm",
  start = "flange", end = "flange", elbow = "LR", line = "GAS-101");
