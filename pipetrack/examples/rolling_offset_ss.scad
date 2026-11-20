// Example: 2" SCH 10S stainless with a 45 degree rolling offset (inches)
use <../pipe.scad>

pipe_run([
    ["E",   24],
    ["NEU", 18],      // rolls north-east and up at once: elbows are cut to the true angle
    ["E",   30],
    ["flange"],       // flanged joint in the middle of the run
    ["E",   12],
    ["D",   20]
  ],
  nps = 2, sch = "10S", material = "SS", units = "in",
  start = "open", end = "flange");
