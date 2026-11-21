// Example: 6" header with a tee branch, a field weld, and a 6x4 reducer.
// Dimensions in feet-inches, the way they're written in the field.
//
// Console (Window > Console) shows the cut list by spool, weld map and BOM.
// Turn on show_labels to see weld numbers in 3D (red = field weld).

use <../pipe.scad>

show_labels = false;

pipe_run([
    ["E", "4'"],
    ["tee", [                      // branch off the tee, going north
        ["N", "3'-6\""],
        ["E", "2'"]
      ], "flange"],                // branch ends in a flange
    ["E", "3'"],
    ["fw"],                        // field weld: spool S1 | spool S2
    ["E", "2'-6\""],
    ["reducer", 4],                // 6" x 4" concentric reducer
    ["E", "1'-6\""],
    ["D", "2'"]
  ],
  nps = 6, sch = "40", material = "CS", units = "in",
  start = "flange", end = "open", line = "CW-6-101",
  show_labels = show_labels);
