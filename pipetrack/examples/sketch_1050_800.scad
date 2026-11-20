// Example: the "3D piping sketch drawing" iso
//
//   flange -> 1050 -> elbow -> 800 -> elbow -> up 750 -> gate valve -> up 350 -> flange
//
// Lengths are millimetres, work point to work point (elbow centerlines,
// flange faces, valve faces), exactly as written on the sketch.
// Change nps / sch / material and the whole spool rebuilds with real sizes.

use <../pipe.scad>

nps      = 4;      // [0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10, 12]
sch      = "40";   // [5S, 10S, 40, STD, 80, XS, 160]
material = "CS";   // [CS, SS, CHROME, AL, CU, CUNI, NI, TI, DUPLEX, GALV]

pipe_run([
    ["N",  1050],
    ["E",   800],
    ["U",   750],
    ["valve", "gate"],
    ["U",   350]
  ],
  nps = nps, sch = sch, material = material, units = "mm",
  start = "flange", end = "flange", elbow = "LR");
