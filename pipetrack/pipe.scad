// =====================================================================
// pipe.scad - PipeTrack pipe run library for OpenSCAD
//
// Turn an isometric-style list of legs into a 3D pipe spool with real
// ASME sizes: pipe (B36.10M / B36.19M), butt-weld elbows at any angle
// (B16.9 LR / SR or pipe bends), class 150 weld-neck flanges (B16.5) and
// flanged valves (B16.10 face-to-face). Also prints a cut list and BOM.
//
// Use it:
//   use <pipe.scad>
//   pipe_run([ ["E",1050], ["N",800], ["U",750], ["valve","gate"], ["U",350] ],
//            nps=4, sch="40", units="mm", start="flange", end="flange",
//            material="CS");
//
// Legs:   [direction, length]  measured WORK POINT to WORK POINT, the way an
//         iso is dimensioned (to elbow centerlines and flange faces).
//         direction: "E" "W" "N" "S" "U" "D", combos like "NE", "SU", "NEU",
//         or any vector like [1, 1, 0].
// Inline: ["valve", "gate"|"globe"|"ball"|"check"]  flanged valve, adds its
//         face-to-face length in the current direction.
//         ["flange"]  a flanged joint (pair of WN flanges) in the run.
//
// Dimensions are nominal catalog values for modeling. Verify against the
// spec / manufacturer data before cutting pipe.
// =====================================================================

// ---------------- Pipe OD (inches) by NPS ----------------
PIPE_OD = [
  [0.125, 0.405], [0.25, 0.540], [0.375, 0.675], [0.5, 0.840], [0.75, 1.050],
  [1, 1.315], [1.25, 1.660], [1.5, 1.900], [2, 2.375], [2.5, 2.875],
  [3, 3.500], [3.5, 4.000], [4, 4.500], [5, 5.563], [6, 6.625],
  [8, 8.625], [10, 10.750], [12, 12.750], [14, 14.0], [16, 16.0],
  [18, 18.0], [20, 20.0], [24, 24.0]
];

// ---------------- Wall thickness (inches) ----------------
// columns: NPS, 5S, 10S, 40, 80, 160, XXS   (0 = not made)
PIPE_WALL = [
  [0.5,  0.065, 0.083, 0.109, 0.147, 0.188, 0.294],
  [0.75, 0.065, 0.083, 0.113, 0.154, 0.219, 0.308],
  [1,    0.065, 0.109, 0.133, 0.179, 0.250, 0.358],
  [1.25, 0.065, 0.109, 0.140, 0.191, 0.250, 0.382],
  [1.5,  0.065, 0.109, 0.145, 0.200, 0.281, 0.400],
  [2,    0.065, 0.109, 0.154, 0.218, 0.344, 0.436],
  [2.5,  0.083, 0.120, 0.203, 0.276, 0.375, 0.552],
  [3,    0.083, 0.120, 0.216, 0.300, 0.438, 0.600],
  [3.5,  0.083, 0.120, 0.226, 0.318, 0,     0.636],
  [4,    0.083, 0.120, 0.237, 0.337, 0.531, 0.674],
  [5,    0.109, 0.134, 0.258, 0.375, 0.625, 0.750],
  [6,    0.109, 0.134, 0.280, 0.432, 0.719, 0.864],
  [8,    0.109, 0.148, 0.322, 0.500, 0.906, 0.875],
  [10,   0.134, 0.165, 0.365, 0.594, 1.125, 1.000],
  [12,   0.156, 0.180, 0.406, 0.688, 1.312, 1.000],
  [14,   0.156, 0.188, 0.438, 0.750, 1.406, 0],
  [16,   0.165, 0.188, 0.500, 0.844, 1.594, 0],
  [18,   0.165, 0.188, 0.562, 0.938, 1.781, 0],
  [20,   0.188, 0.218, 0.594, 1.031, 1.969, 0],
  [24,   0.218, 0.250, 0.688, 1.219, 2.344, 0]
];

// ---------------- B16.9 tee center-to-end (inches) ----------------
TEE_C = [
  [0.5, 1.0], [0.75, 1.125], [1, 1.5], [1.25, 1.875], [1.5, 2.25], [2, 2.5],
  [2.5, 3.0], [3, 3.375], [3.5, 3.75], [4, 4.125], [5, 4.875], [6, 5.625],
  [8, 7.0], [10, 8.5], [12, 10.0], [14, 11.0], [16, 12.0], [18, 13.5],
  [20, 15.0], [24, 17.0]
];

// ---------------- B16.5 Class 150 weld-neck flange (inches) ----------------
// NPS, flange OD, thickness, length through hub (Y), raised face dia,
// bolt circle, number of bolts, bolt hole dia
FLANGE_150 = [
  [0.5,  3.50, 0.44, 1.88,  1.38,  2.38,  4, 0.62],
  [0.75, 3.88, 0.50, 2.06,  1.69,  2.75,  4, 0.62],
  [1,    4.25, 0.56, 2.19,  2.00,  3.12,  4, 0.62],
  [1.25, 4.62, 0.62, 2.25,  2.50,  3.50,  4, 0.62],
  [1.5,  5.00, 0.69, 2.44,  2.88,  3.88,  4, 0.62],
  [2,    6.00, 0.75, 2.50,  3.62,  4.75,  4, 0.75],
  [2.5,  7.00, 0.88, 2.75,  4.12,  5.50,  4, 0.75],
  [3,    7.50, 0.94, 2.75,  5.00,  6.00,  4, 0.75],
  [3.5,  8.50, 0.94, 2.81,  5.50,  7.00,  8, 0.75],
  [4,    9.00, 0.94, 3.00,  6.19,  7.50,  8, 0.75],
  [5,   10.00, 0.94, 3.50,  7.31,  8.50,  8, 0.88],
  [6,   11.00, 1.00, 3.50,  8.50,  9.50,  8, 0.88],
  [8,   13.50, 1.12, 4.00, 10.62, 11.75,  8, 0.88],
  [10,  16.00, 1.19, 4.00, 12.75, 14.25, 12, 1.00],
  [12,  19.00, 1.25, 4.50, 15.00, 17.00, 12, 1.00],
  [14,  21.00, 1.38, 5.00, 16.25, 18.75, 12, 1.12],
  [16,  23.50, 1.44, 5.00, 18.50, 21.25, 16, 1.12],
  [18,  25.00, 1.56, 5.50, 21.00, 22.75, 16, 1.25],
  [20,  27.50, 1.69, 5.69, 23.00, 25.00, 20, 1.25],
  [24,  32.00, 1.88, 6.00, 27.25, 29.50, 20, 1.38]
];

// ---------------- B16.10 Class 150 valve face-to-face (inches) ----------------
// NPS, gate/ball, globe/check
VALVE_FF = [
  [0.5,  4.25, 4.25], [0.75, 4.62, 4.62], [1, 5.00, 5.00], [1.25, 5.50, 5.50],
  [1.5,  6.50, 6.50], [2, 7.00, 8.00], [2.5, 7.50, 8.50], [3, 8.00, 9.50],
  [4,    9.00, 11.50], [5, 10.00, 14.00], [6, 10.50, 16.00], [8, 11.50, 19.50],
  [10,  13.00, 24.50], [12, 14.00, 27.50], [14, 15.00, 31.00], [16, 16.00, 36.00],
  [18,  17.00, 38.50], [20, 18.00, 38.50], [24, 20.00, 51.00]
];

// ---------------- Materials ----------------
// code, display color
MATERIALS = [
  ["CS",     [0.30, 0.31, 0.33]],   // carbon steel
  ["SS",     [0.78, 0.80, 0.82]],   // stainless 304/316
  ["CHROME", [0.42, 0.40, 0.38]],   // chrome-moly (P11/P22/P91)
  ["AL",     [0.86, 0.88, 0.90]],   // aluminum
  ["CU",     [0.80, 0.50, 0.25]],   // copper
  ["CUNI",   [0.70, 0.62, 0.50]],   // copper-nickel
  ["NI",     [0.60, 0.62, 0.58]],   // nickel alloys (Inconel, Monel, Hastelloy)
  ["TI",     [0.62, 0.64, 0.70]],   // titanium
  ["DUPLEX", [0.70, 0.72, 0.76]],   // duplex / super duplex
  ["GALV",   [0.66, 0.70, 0.72]]    // galvanized
];

// ======================= lookups =======================

function _row(table, nps) =
  let(i = search([nps], table, 1, 0)[0])
  assert(i != [] && i != undef, str("pipe.scad: NPS ", nps, " is not in the table"))
  table[i];

function pipe_od_in(nps) = _row(PIPE_OD, nps)[1];

// Schedule names: 5S 10S 40 STD 40S 80 XS 80S 160 XXS
function pipe_wall_in(nps, sch) =
  let(r = _row(PIPE_WALL, nps),
      w = sch == "5S"  ? r[1]
        : sch == "10S" ? r[2]
        : sch == "40"  ? r[3]
        : (sch == "STD" || sch == "40S") ? (nps >= 12 ? 0.375 : r[3])
        : sch == "80"  ? r[4]
        : (sch == "XS" || sch == "80S")  ? (nps >= 10 ? 0.500 : r[4])
        : sch == "160" ? r[5]
        : sch == "XXS" ? r[6]
        : undef)
  assert(w != undef, str("pipe.scad: unknown schedule '", sch, "'"))
  assert(w > 0, str("pipe.scad: schedule ", sch, " is not made in NPS ", nps))
  w;

// Elbow / bend centerline radius (inches)
//   "LR" long radius (1.5 x NPS), "SR" short radius (1 x NPS),
//   "3D", "5D" ... pipe bends (n x NPS)
function bend_radius_in(nps, type="LR") =
  type == "LR" ? (nps <= 0.5 ? 1.5 : 1.5 * nps)
  : type == "SR" ? max(nps, 1)
  : let(n = _num(type)) assert(n != undef, str("pipe.scad: unknown elbow type '", type, "'")) n * nps;

// "3D" -> 3, "5D" -> 5, "10D" -> 10
function _num(s, i=0, acc=0, seen=false) =
  i >= len(s) ? (seen ? acc : undef)
  : let(c = ord(s[i]))
    (c >= 48 && c <= 57) ? _num(s, i + 1, acc * 10 + c - 48, true) : _num(s, i + 1, acc, seen);

function flange_150(nps) = _row(FLANGE_150, nps);
function valve_ff_in(nps, type) =
  let(r = _row(VALVE_FF, nps)) (type == "globe" || type == "check") ? r[2] : r[1];

function material_color(m) =
  let(i = search([m], MATERIALS, 1, 0)[0])
  (i == [] || i == undef) ? [0.5, 0.5, 0.52] : MATERIALS[i][1];

// ======================= vector helpers =======================

function _unit(v) = v / norm(v);
function _dir(d) =
  is_list(d) ? _unit(d)
  : let(v = [ for (ch = d) ch == "E" ? [1,0,0] : ch == "W" ? [-1,0,0]
                          : ch == "N" ? [0,1,0] : ch == "S" ? [0,-1,0]
                          : ch == "U" ? [0,0,1] : ch == "D" ? [0,0,-1]
                          : assert(false, str("pipe.scad: bad direction '", d, "'")) [0,0,0] ],
        s = [for (x = v) 1] * v)   // sum of the letter vectors
    _unit(s);

function _is_leg(it) =
  is_num(it[1]) && (is_list(it[0]) ||
    (is_string(it[0]) && it[0] != "valve" && it[0] != "flange"));

function _angle(a, b) = acos(max(-1, min(1, a * b)));
function _check_turns(segs) =
  [for (k = [0 : len(segs) - 2]) assert(_angle(segs[k][3], segs[k+1][3]) < 179,
     str("pipe.scad: leg ", k + 2, " doubles straight back on leg ", k + 1)) 0];

// Direction of the first leg at or after index i (for valves at the start).
function _next_dir(run, i) =
  i >= len(run) ? [1,0,0] : _is_leg(run[i]) ? _dir(run[i][0]) : _next_dir(run, i + 1);

// ======================= run -> segments =======================
// segment = [start point, end point, kind, unit direction, valve type]
//   kind: "pipe", "valve", "joint" (zero-length flanged joint)

function _segments(run, u, nps, i=0, pos=[0,0,0], last=undef, acc=[]) =
  i >= len(run) ? acc
  : let(it = run[i])
    _is_leg(it)
      ? let(d = _dir(it[0]), p = pos + d * it[1])
        _segments(run, u, nps, i+1, p, d, concat(acc, [[pos, p, "pipe", d, ""]]))
    : it[0] == "valve"
      ? let(d = last == undef ? _next_dir(run, i+1) : last,
            t = len(it) > 1 ? it[1] : "gate",
            p = pos + d * valve_ff_in(nps, t) * u)
        _segments(run, u, nps, i+1, p, d, concat(acc, [[pos, p, "valve", d, t]]))
    : it[0] == "flange"
      ? let(d = last == undef ? _next_dir(run, i+1) : last)
        _segments(run, u, nps, i+1, pos, d, concat(acc, [[pos, pos, "joint", d, ""]]))
    : assert(false, str("pipe.scad: don't understand run item ", it)) acc;

// ======================= geometry modules =======================

module _align(p, d) {
  translate(p) rotate([0, acos(max(-1, min(1, d[2] / norm(d)))), atan2(d[1], d[0])]) children();
}

module _tube(a, b, od, id=0) {
  L = norm(b - a);
  if (L > 1e-6)
    _align(a, b - a)
      difference() {
        cylinder(h=L, d=od);
        if (id > 0) translate([0, 0, -0.01]) cylinder(h=L + 0.02, d=id);
      }
}

// Elbow of bend angle `ang` at work point `wp`, from direction d1 to d2.
module _elbow(wp, d1, d2, R, od) {
  ang = _angle(d1, d2);
  if (ang > 0.5 && ang < 179.5) {
    e = R * tan(ang / 2);
    n = _unit(cross(d1, d2));
    uin = _unit(cross(n, d1));      // toward the inside of the bend
    S = wp - d1 * e;               // start of the elbow (weld end)
    C = S + uin * R;               // bend center
    x = -uin; y = d1; z = n;
    multmatrix([[x[0], y[0], z[0], C[0]],
                [x[1], y[1], z[1], C[1]],
                [x[2], y[2], z[2], C[2]],
                [0, 0, 0, 1]])
      rotate_extrude(angle=ang) translate([R, 0]) circle(d=od);
  }
}

// Weld-neck flange: face at `p`, facing direction `out`, hub runs back into the pipe.
module _wn_flange(p, out, nps, u, od) {
  f = flange_150(nps);
  O = f[1]*u; T = f[2]*u; Y = f[3]*u; RF = f[4]*u; BC = f[5]*u; nb = f[6]; hole = f[7]*u;
  rf = 0.0625 * u;
  X = min(od * 1.1 + 0.6 * u, O * 0.8);   // hub base (approx.)
  _align(p, out) {
    translate([0, 0, -rf]) cylinder(h=rf, d=RF);                       // raised face
    translate([0, 0, -rf - T])
      difference() {
        cylinder(h=T, d=O);
        for (k = [0 : nb - 1]) rotate([0, 0, 360 * k / nb + 180 / nb])
          translate([BC / 2, 0, -0.01]) cylinder(h=T + 0.02, d=hole);
      }
    translate([0, 0, -Y]) cylinder(h=Y - rf - T, d1=od, d2=X);         // hub
  }
}

// Flanged valve body between faces a -> b (local Z along the run).
module _valve(a, b, type, nps, u, od, stem_up) {
  f = flange_150(nps);
  O = f[1]*u; T = f[2]*u;
  L = norm(b - a);
  d = _unit(b - a);
  // spin the valve so the stem points as close to `stem_up` as possible
  side = abs(d * stem_up) > 0.9 ? [1, 0, 0] : stem_up;
  _align(a, d) {
    for (z = [0, L - T]) translate([0, 0, z]) cylinder(h=T, d=O);        // end flanges
    translate([0, 0, T]) cylinder(h=L - 2*T, d=od * 1.25);               // body
  }
  // bonnet + stem + operator, built in world space
  mid = (a + b) / 2;
  s = _unit(side - d * (side * d));
  body = od * 1.3;
  if (type == "check") {
    _align(mid, s) cylinder(h=od * 0.9, d=od * 0.9);
  } else if (type == "ball") {
    _align(mid, s) { cylinder(h=od * 0.9, d=od * 0.35); translate([0, 0, od * 0.9]) rotate([0, 90, 0]) cylinder(h=od * 2.2, d=od * 0.15); }
  } else {   // gate / globe: bonnet, stem, handwheel
    _align(mid, s) {
      cylinder(h=body * (type == "globe" ? 1.2 : 1.6), d=od * 0.8);
      cylinder(h=body * 2.4, d=od * 0.12);
      translate([0, 0, body * 2.4]) rotate_extrude() translate([O * 0.42, 0]) circle(d=od * 0.1);
      translate([0, 0, body * 2.4]) for (k = [0:2]) rotate([0, 0, 60 * k]) rotate([0, 90, 0]) cylinder(h=O * 0.84, d=od * 0.06, center=true);
    }
  }
}

module _weld_bead(p, d, od) {
  _align(p, d) rotate_extrude() translate([od / 2, 0]) circle(d=od * 0.06);
}

// ======================= main module =======================

module pipe_run(run, nps=4, sch="40", units="in", material="CS",
                elbow="LR", start="open", end="open",
                hollow=false, show_welds=true, stem_up=[0, 0, 1],
                report=true, $fn=48) {
  u = units == "mm" ? 25.4 : 1;
  od = pipe_od_in(nps) * u;
  id = od - 2 * pipe_wall_in(nps, sch) * u;
  R = bend_radius_in(nps, elbow) * u;
  Y = flange_150(nps)[3] * u;
  segs = _segments(run, u, nps);
  n = len(segs);
  _ok = len(segs) > 1 ? _check_turns(segs) : [];
  col = material_color(material);

  // does a flange sit at the start / end of segment k?
  function fl_start(k) = (k == 0 && start == "flange") || (k > 0 && segs[k-1][2] != "pipe");
  function fl_end(k)   = (k == n-1 && end == "flange") || (k < n-1 && segs[k+1][2] != "pipe");
  function trim_start(k) =
    fl_start(k) ? Y : k > 0 ? R * tan(_angle(segs[k-1][3], segs[k][3]) / 2) : 0;
  function trim_end(k) =
    fl_end(k) ? Y : k < n-1 ? R * tan(_angle(segs[k][3], segs[k+1][3]) / 2) : 0;

  color(col) {
    for (k = [0 : n - 1]) {
      s = segs[k];
      if (s[2] == "pipe") {
        a = s[0] + s[3] * trim_start(k);
        b = s[1] - s[3] * trim_end(k);
        _tube(a, b, od, hollow ? id : 0);
        if (fl_start(k)) _wn_flange(s[0], -s[3], nps, u, od);
        if (fl_end(k))   _wn_flange(s[1],  s[3], nps, u, od);
        if (k < n - 1 && segs[k+1][2] == "pipe")
          _elbow(s[1], s[3], segs[k+1][3], R, od);
      } else if (s[2] == "valve") {
        _valve(s[0], s[1], s[4], nps, u, od, stem_up);
      }
    }
  }

  if (show_welds)
    color([0.15, 0.15, 0.15])
      for (k = [0 : n - 1]) if (segs[k][2] == "pipe") {
        s = segs[k];
        a = s[0] + s[3] * trim_start(k);
        b = s[1] - s[3] * trim_end(k);
        if (norm(b - a) > 1e-6) { _weld_bead(a, s[3], od * 1.02); _weld_bead(b, s[3], od * 1.02); }
      }

  if (report) {
    unit = units == "mm" ? "mm" : "in";
    echo(str("PIPE RUN: NPS ", nps, " SCH ", sch, " ", material, "  OD ", od, unit,
             "  elbows ", elbow, " (R=", R, unit, ")"));
    for (k = [0 : n - 1]) if (segs[k][2] == "pipe") {
      cut = norm(segs[k][1] - segs[k][0]) - trim_start(k) - trim_end(k);
      echo(str("  CUT ", k + 1, ": ", round(cut * 10) / 10, unit,
               cut <= 0 ? "   <-- TOO SHORT for the fittings at its ends!" : ""));
    }
    nelb = len([for (k = [0 : n - 2]) if (segs[k][2] == "pipe" && segs[k+1][2] == "pipe"
                                            && _angle(segs[k][3], segs[k+1][3]) > 0.5) 1]);
    nfl = len([for (k = [0 : n - 1]) if (segs[k][2] == "pipe") for (e = [fl_start(k), fl_end(k)]) if (e) 1]);
    nv = len([for (s = segs) if (s[2] == "valve") 1]);
    total = [for (k = [0 : n - 1]) if (segs[k][2] == "pipe")
               max(0, norm(segs[k][1] - segs[k][0]) - trim_start(k) - trim_end(k))] * [for (k = [0 : n - 1]) if (segs[k][2] == "pipe") 1];
    echo(str("  BOM: pipe ", round(total * 10) / 10, unit, ", ", nelb, " elbow(s) ", elbow,
             ", ", nfl, " WN flange(s) CL150, ", nv, " valve(s)"));
  }
}
