// Real-world check: Air Liquide line 2" PAr 320 (A01SC1SX), iso TP 320
//
// From the drawing:
//   A  2" Sch 10S (.109) A312 TP304/304L pipe, PE x BE, cut 8'-5 7/8"
//   B  2" 90 LR elbow Sch 10S, A403 WP304/304L
//   *8'-8 15/16" = field-trim end to elbow B work point (includes 6" F/Trim)
//   Welds: 1 x 2" BW (pipe to elbow), FW28 (elbow to the line continued on C# 248057)
//
// The console cut for pipe A should read 8'-5 7/8" - same as the BOM:
//   8'-8 15/16"  - 3" LR take-out  - 1/16" weld gap

use <../pipe.scad>

pipe_run([
    ["U", "8'-8 15/16\""],   // F/Trim end up to elbow B
    ["N", 3]                  // elbow B take-out, ends at FW28
  ],
  nps = 2, sch = "10S", material = "SS", units = "in",
  start = "open", end = "fw", weld_gap = 1/16,
  line = "2-PAr-320", show_labels = true);
