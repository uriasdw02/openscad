// =====================================================================
// pipe.scad - PipeTrack pipe run library for OpenSCAD
//
// Turn an isometric-style list of legs into a 3D pipe spool with real
// ASME sizes, plus a cut list, weld map, spool numbers, BOM and PCF.
//
//   use <pipe.scad>
//   pipe_run([ ["N","3'-5 3/8\""], ["E",800], ["U",750], ["valve","gate"], ["U",350] ],
//            nps=4, sch="40", units="mm", start="flange", end="flange", material="CS");
//
// RUN ITEMS
//   [dir, length]             leg, WORK POINT to WORK POINT (elbow centerlines,
//                             tee centers, flange faces, valve faces).
//                             dir: "E" "W" "N" "S" "U" "D", combos "NE" "SU" "NEU",
//                             or a vector [1,1,0]. length: number in `units`, or a
//                             feet-inch string "10'-6 3/4\"", "6-1/2\"", "3'".
//   ["valve", type]           flanged valve: gate | globe | ball | check
//   ["flange"]                flanged joint (pair of WN flanges + gasket + bolts)
//   ["reducer", nps2]         concentric reducer to nps2 (adds its length)
//   ["tee", [branch run], end]  straight tee here; branch run starts at the tee
//                             center, end = "open" | "flange"
//   ["ell", dir]              turn with an elbow toward dir and STOP at the elbow's
//                             far end (a run that ends at an elbow, e.g. into a FW)
//   ["fw"] / ["sw"]           field weld / shop weld between two straight legs
//                             (a field weld splits spools)
//   start / end = "open" | "flange" | "fw"   ("fw": the run ends in a field weld,
//                             e.g. right at an elbow - give the last leg as the
//                             elbow take-out, LR take-out = 1.5 x NPS)
//   weld_gap = root gap per butt weld (e.g. 1/16 for inches, 1.6 for mm);
//                             cut lengths subtract it like the shop does
//
// Pipe (B36.10M/B36.19M), elbows (B16.9, any angle, LR/SR/nD bends), tees and
// reducers (B16.9), class 150 WN flanges (B16.5), valves (B16.10). Nominal
// catalog values for modeling - verify against specs before cutting pipe.
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

// ---------------- B16.5 Class 300 weld-neck flange (inches) ----------------
// same columns as FLANGE_150
FLANGE_300 = [
  [0.5,  3.75, 0.56, 2.06,  1.38,  2.62,  4, 0.62],
  [0.75, 4.62, 0.62, 2.25,  1.69,  3.25,  4, 0.75],
  [1,    4.88, 0.69, 2.44,  2.00,  3.50,  4, 0.75],
  [1.25, 5.25, 0.75, 2.56,  2.50,  3.88,  4, 0.75],
  [1.5,  6.12, 0.81, 2.69,  2.88,  4.50,  4, 0.88],
  [2,    6.50, 0.88, 2.75,  3.62,  5.00,  8, 0.75],
  [2.5,  7.50, 1.00, 3.00,  4.12,  5.88,  8, 0.88],
  [3,    8.25, 1.12, 3.12,  5.00,  6.62,  8, 0.88],
  [3.5,  9.00, 1.19, 3.19,  5.50,  7.25,  8, 0.88],
  [4,   10.00, 1.25, 3.38,  6.19,  7.88,  8, 0.88],
  [5,   11.00, 1.38, 3.88,  7.31,  9.25,  8, 0.88],
  [6,   12.50, 1.44, 3.88,  8.50, 10.62, 12, 0.88],
  [8,   15.00, 1.62, 4.38, 10.62, 13.00, 12, 1.00],
  [10,  17.50, 1.88, 4.62, 12.75, 15.25, 16, 1.12],
  [12,  20.50, 2.00, 5.12, 15.00, 17.75, 16, 1.25],
  [14,  23.00, 2.12, 5.62, 16.25, 20.25, 20, 1.25],
  [16,  25.50, 2.25, 5.75, 18.50, 22.50, 20, 1.38],
  [18,  28.00, 2.38, 6.25, 21.00, 24.75, 24, 1.38],
  [20,  30.50, 2.50, 6.38, 23.00, 27.00, 24, 1.38],
  [24,  36.00, 2.75, 6.62, 27.25, 32.00, 24, 1.62]
];

// ---------------- B16.10 Class 300 valve face-to-face (inches, RF, approx.) ----------------
// NPS, gate/ball, globe/check
VALVE_FF_300 = [
  [0.5,  5.50, 6.00], [0.75, 6.00, 7.00], [1, 6.50, 8.00], [1.25, 7.00, 8.50],
  [1.5,  7.50, 9.00], [2, 8.50, 10.50], [2.5, 9.50, 11.50], [3, 11.12, 12.50],
  [3.5, 11.50, 13.25], [4, 12.00, 14.00], [5, 15.00, 15.75], [6, 15.88, 17.50],
  [8,   16.50, 22.00], [10, 18.00, 24.50], [12, 19.75, 28.00], [14, 30.00, 33.00],
  [16,  33.00, 34.00], [18, 36.00, 38.50], [20, 39.00, 40.00], [24, 45.00, 53.00]
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

// ---------------- B16.9 concentric reducer length H (inches), by larger NPS ----------------
REDUCER_H = [
  [0.75, 1.5], [1, 2.0], [1.25, 2.0], [1.5, 2.5], [2, 3.0], [2.5, 3.5], [3, 3.5],
  [3.5, 4.0], [4, 4.0], [5, 5.0], [6, 5.5], [8, 6.0], [10, 7.0], [12, 8.0],
  [14, 13.0], [16, 14.0], [18, 15.0], [20, 20.0], [24, 20.0]
];

// ======================= lookups =======================

function _row(table, nps) =
  let(i = search([nps], table, 1, 0)[0])
  assert(is_num(i), str("pipe.scad: NPS ", nps, " is not in the table"))
  table[i];

function pipe_od_in(nps) = _row(PIPE_OD, nps)[1];

// Schedule names: 5S 10S 40 STD 40S 80 XS 80S 160 XXS
// Misreads from drawings ("105" for 10S, "405" for 40S...) are corrected.
function sch_norm(sch) =
  let(s = is_num(sch) ? str(sch) : sch)
  s == "105" ? "10S" : s == "55" ? "5S" : s == "405" ? "40S" : s == "805" ? "80S"
  : s == "10s" ? "10S" : s == "5s" ? "5S" : s == "40s" ? "40S" : s == "80s" ? "80S"
  : s == "std" ? "STD" : s == "xs" ? "XS" : s == "xxs" ? "XXS" : s;
function pipe_wall_in(nps, sch_in) =
  let(sch = sch_norm(sch_in), r = _row(PIPE_WALL, nps),
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

// Elbow / bend centerline radius (inches): "LR" 1.5 x NPS, "SR" 1 x NPS, "3D" "5D"... n x NPS
function bend_radius_in(nps, type="LR") =
  type == "LR" ? (nps <= 0.5 ? 1.5 : 1.5 * nps)
  : type == "SR" ? max(nps, 1)
  : let(n = _int(type)) assert(n != undef, str("pipe.scad: unknown elbow type '", type, "'")) n * nps;

// Flange class comes from pipe_run(rating=150|300) through $pipe_rating.
function _rating() = is_undef($pipe_rating) ? 150 : $pipe_rating;
function flange_150(nps) =
  let(r = _rating()) assert(r == 150 || r == 300, str("pipe.scad: rating ", r, " not available yet (150 or 300)"))
  _row(r == 300 ? FLANGE_300 : FLANGE_150, nps);
function valve_ff_in(nps, type) =
  let(r = _row(_rating() == 300 ? VALVE_FF_300 : VALVE_FF, nps)) (type == "globe" || type == "check") ? r[2] : r[1];
function tee_c_in(nps) = _row(TEE_C, nps)[1];
function reducer_h_in(n1, n2) = _row(REDUCER_H, max(n1, n2))[1];

function material_color(m) =
  let(i = search([m], MATERIALS, 1, 0)[0])
  is_num(i) ? MATERIALS[i][1] : [0.5, 0.5, 0.52];

// bolt diameter = hole - 1/8"
function bolt_dia_in(nps) = flange_150(nps)[7] - 0.125;

// ======================= text / number helpers =======================

function _find(s, ch, i=0) = i >= len(s) ? -1 : s[i] == ch ? i : _find(s, ch, i + 1);
function _sub(s, a, b) = a >= b ? "" : chr([for (i = [a : b - 1]) ord(s[i])]);
function _int(s, i=0, acc=0, seen=false) =   // first run of digits: "3D" -> 3
  i >= len(s) ? (seen ? acc : undef)
  : let(c = ord(s[i]))
    (c >= 48 && c <= 57) ? _int(s, i + 1, acc * 10 + c - 48, true)
    : seen ? acc : _int(s, i + 1, acc, seen);
function _digits(s, i, acc=0) =
  i >= len(s) ? acc : _digits(s, i + 1, acc * 10 + ord(s[i]) - 48);
function _dec(s) =   // "6", "6.75", ".5"
  let(p = _find(s, "."))
  p < 0 ? _digits(s, 0)
  : _digits(_sub(s, 0, p), 0) + (p + 1 >= len(s) ? 0
      : _digits(_sub(s, p + 1, len(s)), 0) / pow(10, len(s) - p - 1));
function _tok_val(t) =
  let(sl = _find(t, "/"))
  len(t) == 0 ? 0
  : sl < 0 ? _dec(t)
  : _dec(_sub(t, 0, sl)) / _dec(_sub(t, sl + 1, len(t)));
function _split_sp(s, i=0, cur="", acc=[]) =
  i >= len(s) ? (cur == "" ? acc : concat(acc, [cur]))
  : s[i] == " " ? _split_sp(s, i + 1, "", cur == "" ? acc : concat(acc, [cur]))
  : _split_sp(s, i + 1, str(cur, s[i]), acc);

// Feet-inch string -> inches.  "10'-6 3/4\""  "10' 6-3/4"  "6 1/2\""  "3/4\""  "126.5"  "3'"
function ftin_to_in(s) =
  let(fi = _find(s, "'"),
      feet = fi < 0 ? 0 : _dec(_strip(_sub(s, 0, fi))),
      rest = fi < 0 ? s : _sub(s, fi + 1, len(s)),
      clean = chr([for (c = rest) c == "\"" ? 32 : c == "-" ? 32 : ord(c)]),
      toks = _split_sp(clean))
  feet * 12 + ([for (t = toks) _tok_val(t)] * [for (t = toks) 1]);
function _strip(s) = chr([for (c = s) if (c != " ") ord(c)]);

function _gcd(a, b) = b == 0 ? a : _gcd(b, a % b);
// inches -> 10'-6 3/4"   (nearest 1/16)
function fmt_ftin(x) =
  let(t = round(abs(x) * 16) / 16,
      ft = floor(t / 12), rem = t - ft * 12,
      whole = floor(rem + 1e-9), n16 = round((rem - whole) * 16),
      g = n16 == 0 ? 1 : _gcd(n16, 16))
  str(x < 0 ? "-" : "", ft > 0 ? str(ft, "'-") : "", whole,
      n16 == 0 ? "" : str(" ", n16 / g, "/", 16 / g), "″");
function fmt_len(x, units) =
  units == "mm" ? str(round(x * 10) / 10, " mm") : fmt_ftin(x);

// length in run units from number or feet-inch string
function _L(x, u) = is_num(x) ? x : ftin_to_in(x) * u;

// ======================= vector helpers =======================

function _unit(v) = v / norm(v);
function _dir(d) =
  is_list(d) ? _unit(d)
  : let(v = [ for (ch = d) ch == "E" ? [1,0,0] : ch == "W" ? [-1,0,0]
                          : ch == "N" ? [0,1,0] : ch == "S" ? [0,-1,0]
                          : ch == "U" ? [0,0,1] : ch == "D" ? [0,0,-1]
                          : assert(false, str("pipe.scad: bad direction '", d, "'")) [0,0,0] ])
    _unit([for (x = v) 1] * v);
function _angle(a, b) = acos(max(-1, min(1, a * b)));
function _sum(v) = len(v) == 0 ? 0 : [for (x = v) 1] * v;
function _fmt3(p) = str(round(p[0] * 10) / 10, " ", round(p[1] * 10) / 10, " ", round(p[2] * 10) / 10);

function _is_leg(it) =
  len(it) == 2 && (is_num(it[1]) || is_string(it[1])) && (is_list(it[0]) ||
    (is_string(it[0]) && it[0] != "valve" && it[0] != "reducer" && it[0] != "tee" && it[0] != "ell"));

function _next_dir(run, i) =
  i >= len(run) ? [1,0,0] : _is_leg(run[i]) ? _dir(run[i][0]) : _next_dir(run, i + 1);

// ======================= run -> segments =======================
// segment = [a, b, kind, dir, info, nps]
//   kind "pipe" | "valve" (info=type) | "joint" | "reducer" (info=[n1,n2], nps=n1)
//        | "tee" (zero length at the tee center, info=[branch run, end]) | "fw" | "sw"
SEG_A = 0; SEG_B = 1; SEG_KIND = 2; SEG_DIR = 3; SEG_INFO = 4; SEG_NPS = 5;

function _segments(run, u, nps, pos, i=0, last=undef, acc=[]) =
  i >= len(run) ? acc
  : let(it = run[i], cur = last == undef ? _next_dir(run, i) : last)
    _is_leg(it)
      ? let(d = _dir(it[0]), p = pos + d * _L(it[1], u))
        _segments(run, u, nps, p, i + 1, d, concat(acc, [[pos, p, "pipe", d, "", nps]]))
    : it[0] == "ell"
      ? assert(last != undef, "pipe.scad: [\"ell\", dir] needs a leg before it")
        let(d = _dir(it[1]),
            el = is_undef($pipe_elbow) ? "LR" : $pipe_elbow,
            p = pos + d * bend_radius_in(nps, el) * u * tan(_angle(last, d) / 2))
        _segments(run, u, nps, p, i + 1, d, concat(acc, [[pos, p, "pipe", d, "", nps]]))
    : it[0] == "valve"
      ? let(t = len(it) > 1 ? it[1] : "gate", p = pos + cur * valve_ff_in(nps, t) * u)
        _segments(run, u, nps, p, i + 1, cur, concat(acc, [[pos, p, "valve", cur, t, nps]]))
    : it[0] == "flange"
      ? _segments(run, u, nps, pos, i + 1, cur, concat(acc, [[pos, pos, "joint", cur, "", nps]]))
    : it[0] == "reducer"
      ? let(n2 = it[1], p = pos + cur * reducer_h_in(nps, n2) * u)
        _segments(run, u, n2, p, i + 1, cur, concat(acc, [[pos, p, "reducer", cur, [nps, n2], nps]]))
    : it[0] == "tee"
      ? assert(len(acc) > 0, "pipe.scad: a run can't start with a tee")
        _segments(run, u, nps, pos, i + 1, cur,
                  concat(acc, [[pos, pos, "tee", cur, [it[1], len(it) > 2 ? it[2] : "open"], nps]]))
    : (it[0] == "fw" || it[0] == "sw")
      ? _segments(run, u, nps, pos, i + 1, cur, concat(acc, [[pos, pos, it[0], cur, "", nps]]))
    : assert(false, str("pipe.scad: don't understand run item ", it)) acc;

// ======================= chains (main run + tee branches) =======================
// chain = [start type, end type, segments, parent chain index, parent segment index]

function _chains(run, u, nps, pos, st, et, my, parent=-1, pseg=-1) =
  let(segs = _segments(run, u, nps, pos),
      tees = [for (k = [0 : len(segs) - 1]) if (segs[k][SEG_KIND] == "tee") k])
  concat([[st, et, segs, parent, pseg]], _branches(segs, tees, 0, u, my, my + 1));

function _branches(segs, tees, j, u, parent, next) =
  j >= len(tees) ? []
  : let(k = tees[j], s = segs[k],
        sub = _chains(s[SEG_INFO][0], u, s[SEG_NPS], s[SEG_A], "tee", s[SEG_INFO][1], next, parent, k))
    concat(sub, _branches(segs, tees, j + 1, u, parent, next + len(sub)));

// ======================= connections, trims, welds, spools =======================

function _Y(n, u) = flange_150(n)[3] * u;
function _C(n, u) = tee_c_in(n) * u;
function _R(n, el, u) = bend_radius_in(n, el) * u;

// what sits at the start / end of pipe segment k of chain ch
function _conn_s(ch, k) =
  let(segs = ch[2])
  k == 0 ? (ch[0] == "flange" ? "flange" : ch[0] == "tee" ? "tee" : ch[0] == "fw" ? "fw" : "open")
  : let(p = segs[k - 1][SEG_KIND])
    p == "pipe" ? (_angle(segs[k - 1][SEG_DIR], segs[k][SEG_DIR]) > 0.5 ? "elbow" : "butt")
    : (p == "valve" || p == "joint") ? "flange" : p;
function _conn_e(ch, k) =
  let(segs = ch[2])
  k == len(segs) - 1 ? (ch[1] == "flange" ? "flange" : ch[1] == "fw" ? "fw" : "open")
  : let(nx = segs[k + 1][SEG_KIND])
    nx == "pipe" ? (_angle(segs[k][SEG_DIR], segs[k + 1][SEG_DIR]) > 0.5 ? "elbow" : "butt")
    : (nx == "valve" || nx == "joint") ? "flange" : nx;
function _trim_s(ch, k, u, el) =
  let(c = _conn_s(ch, k), s = ch[2][k], n = s[SEG_NPS])
  c == "flange" ? _Y(n, u) : c == "tee" ? _C(n, u)
  : c == "elbow" ? _R(n, el, u) * tan(_angle(ch[2][k - 1][SEG_DIR], s[SEG_DIR]) / 2) : 0;
function _trim_e(ch, k, u, el) =
  let(c = _conn_e(ch, k), s = ch[2][k], n = s[SEG_NPS])
  c == "flange" ? _Y(n, u) : c == "tee" ? _C(n, u)
  : c == "elbow" ? _R(n, el, u) * tan(_angle(s[SEG_DIR], ch[2][k + 1][SEG_DIR]) / 2) : 0;
function _gap() = is_undef($pipe_gap) ? 0 : $pipe_gap;
function _gap_of(c) = c == "open" ? 0 : (c == "butt" || c == "fw" || c == "sw") ? _gap() / 2 : _gap();
function _cut(ch, k, u, el) =
  norm(ch[2][k][SEG_B] - ch[2][k][SEG_A]) - _trim_s(ch, k, u, el) - _trim_e(ch, k, u, el)
  - _gap_of(_conn_s(ch, k)) - _gap_of(_conn_e(ch, k));
// a leg that is only an elbow's take-out (no pipe), e.g. a run ending at an elbow
function _ell_only(ch, k, u, el) = abs(_cut(ch, k, u, el) + _gap_of(_conn_s(ch, k)) + _gap_of(_conn_e(ch, k))) < 0.02 * u;

// spool breaks: field weld, flanged joint, valve
function _is_break(s) = s[SEG_KIND] == "fw" || s[SEG_KIND] == "joint" || s[SEG_KIND] == "valve";
function _breaks_before(ch, k) = k <= 0 ? 0 : len([for (j = [0 : k - 1]) if (_is_break(ch[2][j])) 1]);
function _breaks(ch) = len([for (s = ch[2]) if (_is_break(s)) 1]);
function _spool(chains, ci, k) =
  let(b = k > 0 ? _breaks_before(chains[ci], k) : 0)
  b == 0 ? (ci == 0 ? 1 : _spool(chains, chains[ci][3], chains[ci][4]))
  : 1 + (ci == 0 ? 0 : _sum([for (c = [0 : ci - 1]) _breaks(chains[c])])) + b;

// all welds, numbered in order: [point, "SW"|"FW", dir, nps, spool]
function _welds(chains, u, el) = [
  for (ci = [0 : len(chains) - 1]) let(ch = chains[ci], segs = ch[2])
    for (k = [0 : len(segs) - 1]) if (segs[k][SEG_KIND] == "pipe" && (_cut(ch, k, u, el) > 0 || _ell_only(ch, k, u, el)))
      let(s = segs[k], cs = _ell_only(ch, k, u, el) ? "none" : _conn_s(ch, k), ce = _conn_e(ch, k),
          a = s[SEG_A] + s[SEG_DIR] * _trim_s(ch, k, u, el),
          b = s[SEG_B] - s[SEG_DIR] * _trim_e(ch, k, u, el),
          sp = _spool(chains, ci, k))
      for (w = [ (cs == "flange" || cs == "tee" || cs == "elbow" || cs == "reducer")
                   ? [a, "SW", s[SEG_DIR], s[SEG_NPS], sp] : undef,
                 ce != "open" ? [b, ce == "fw" ? "FW" : "SW", s[SEG_DIR], s[SEG_NPS], sp] : undef ])
        if (w != undef) w
];

// ======================= geometry =======================

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

module _elbow(wp, d1, d2, R, od) {
  ang = _angle(d1, d2);
  if (ang > 0.5 && ang < 179.5) {
    e = R * tan(ang / 2);
    n = _unit(cross(d1, d2));
    uin = _unit(cross(n, d1));
    S = wp - d1 * e;
    C = S + uin * R;
    x = -uin; y = d1; z = n;
    multmatrix([[x[0], y[0], z[0], C[0]],
                [x[1], y[1], z[1], C[1]],
                [x[2], y[2], z[2], C[2]],
                [0, 0, 0, 1]])
      rotate_extrude(angle=ang) translate([R, 0]) circle(d=od);
  }
}

module _wn_flange(p, out, nps, u) {
  f = flange_150(nps);
  od = pipe_od_in(nps) * u;
  O = f[1]*u; T = f[2]*u; Y = f[3]*u; RF = f[4]*u; BC = f[5]*u; nb = f[6]; hole = f[7]*u;
  rf = 0.0625 * u;
  X = min(od * 1.1 + 0.6 * u, O * 0.8);
  _align(p, out) {
    translate([0, 0, -rf]) cylinder(h=rf, d=RF);
    translate([0, 0, -rf - T])
      difference() {
        cylinder(h=T, d=O);
        for (k = [0 : nb - 1]) rotate([0, 0, 360 * k / nb + 180 / nb])
          translate([BC / 2, 0, -0.01]) cylinder(h=T + 0.02, d=hole);
      }
    translate([0, 0, -Y]) cylinder(h=Y - rf - T, d1=od, d2=X);
  }
}

module _valve(a, b, type, nps, u, stem_up) {
  f = flange_150(nps);
  od = pipe_od_in(nps) * u;
  O = f[1]*u; T = f[2]*u;
  L = norm(b - a);
  d = _unit(b - a);
  side = abs(d * stem_up) > 0.9 ? [1, 0, 0] : stem_up;
  _align(a, d) {
    for (z = [0, L - T]) translate([0, 0, z]) cylinder(h=T, d=O);
    translate([0, 0, T]) cylinder(h=L - 2*T, d=od * 1.25);
  }
  mid = (a + b) / 2;
  s = _unit(side - d * (side * d));
  body = od * 1.3;
  if (type == "check") {
    _align(mid, s) cylinder(h=od * 0.9, d=od * 0.9);
  } else if (type == "ball") {
    _align(mid, s) {
      cylinder(h=od * 0.9, d=od * 0.35);
      translate([0, 0, od * 0.9]) rotate([0, 90, 0]) cylinder(h=od * 2.2, d=od * 0.15);
    }
  } else {
    _align(mid, s) {
      cylinder(h=body * (type == "globe" ? 1.2 : 1.6), d=od * 0.8);
      cylinder(h=body * 2.4, d=od * 0.12);
      translate([0, 0, body * 2.4]) rotate_extrude() translate([O * 0.42, 0]) circle(d=od * 0.1);
      translate([0, 0, body * 2.4]) for (k = [0:2]) rotate([0, 0, 60 * k]) rotate([0, 90, 0])
        cylinder(h=O * 0.84, d=od * 0.06, center=true);
    }
  }
}

module _tee(wp, d, bd, nps, u) {
  od = pipe_od_in(nps) * u;
  C = _C(nps, u);
  _tube(wp - d * C, wp + d * C, od);
  _tube(wp, wp + bd * C, od);
  translate(wp) sphere(d=od);
}

module _reducer(a, b, n1, n2, u) {
  _align(a, b - a) cylinder(h=norm(b - a), d1=pipe_od_in(n1) * u, d2=pipe_od_in(n2) * u);
}

module _weld_bead(p, d, od) {
  _align(p, d) rotate_extrude() translate([od / 2, 0]) circle(d=od * 0.06);
}

module _label(p, txt, size) {
  translate(p) rotate([90, 0, 0]) linear_extrude(size * 0.08)
    text(txt, size=size, halign="center", valign="center");
}

// ======================= main module =======================

module pipe_run(run, nps=4, sch="40", units="in", material="CS", rating=150,
                elbow="LR", start="open", end="open", line="LINE-1", weld_gap=0,
                hollow=false, show_welds=true, show_labels=false, stem_up=[0, 0, 1],
                report=true, pcf=undef, $fn=48) {
  $pipe_rating = rating;
  $pipe_gap = weld_gap;
  $pipe_elbow = elbow;
  _sch_ok = pipe_wall_in(nps, sch);   // unknown schedule -> clear error now, not later
  u = units == "mm" ? 25.4 : 1;
  chains = _chains(run, u, nps, [0, 0, 0], start, end, 0);
  welds = _welds(chains, u, elbow);
  col = material_color(material);
  do_pcf = pcf != undef ? pcf : (is_undef($pipe_pcf) ? false : $pipe_pcf);

  // ---- 3D ----
  color(col)
    for (ci = [0 : len(chains) - 1]) {
      ch = chains[ci]; segs = ch[2]; n = len(segs);
      for (k = [0 : n - 1]) {
        s = segs[k]; kind = s[SEG_KIND]; sn = s[SEG_NPS];
        if (kind == "pipe") {
          a = s[SEG_A] + s[SEG_DIR] * _trim_s(ch, k, u, elbow);
          b = s[SEG_B] - s[SEG_DIR] * _trim_e(ch, k, u, elbow);
          if (_cut(ch, k, u, elbow) > 0) {
            od = pipe_od_in(sn) * u;
            _tube(a, b, od, hollow ? od - 2 * pipe_wall_in(sn, sch) * u : 0);
          }
          if (_conn_s(ch, k) == "flange") _wn_flange(s[SEG_A], -s[SEG_DIR], sn, u);
          if (_conn_e(ch, k) == "flange") _wn_flange(s[SEG_B],  s[SEG_DIR], sn, u);
          if (_conn_e(ch, k) == "elbow")
            _elbow(s[SEG_B], s[SEG_DIR], segs[k + 1][SEG_DIR], _R(sn, elbow, u), pipe_od_in(sn) * u);
        } else if (kind == "valve") {
          _valve(s[SEG_A], s[SEG_B], s[SEG_INFO], sn, u, stem_up);
        } else if (kind == "reducer") {
          _reducer(s[SEG_A], s[SEG_B], s[SEG_INFO][0], s[SEG_INFO][1], u);
        } else if (kind == "tee") {
          _tee(s[SEG_A], s[SEG_DIR], _dir(s[SEG_INFO][0][0][0]), sn, u);
        }
      }
    }

  if (show_welds)
    for (w = welds) color(w[1] == "FW" ? [0.85, 0.1, 0.1] : [0.12, 0.12, 0.12])
      _weld_bead(w[0], w[2], pipe_od_in(w[3]) * u * 1.03);

  if (show_labels && len(welds) > 0)
    color([0.1, 0.1, 0.6])
      for (i = [0 : len(welds) - 1]) let(w = welds[i], od = pipe_od_in(w[3]) * u)
        _label(w[0] + [0, 0, od * 1.2], str(w[1] == "FW" ? "FW" : "W", i + 1), od * 0.6);

  // ---- report ----
  ru = units == "mm" ? "mm" : "ftin";
  nspools = 1 + _sum([for (c = chains) _breaks(c)]);
  if (report) {
    echo(str("PIPE RUN ", line, ": NPS ", nps, " SCH ", sch_norm(sch), " ", material, ", CL", rating, ", elbows ", elbow));
    for (sp = [1 : nspools]) {
      cuts = [for (ci = [0 : len(chains) - 1]) let(ch = chains[ci])
                for (k = [0 : len(ch[2]) - 1])
                  if (ch[2][k][SEG_KIND] == "pipe" && _spool(chains, ci, k) == sp && !_ell_only(ch, k, u, elbow))
                    [_cut(ch, k, u, elbow), ch[2][k][SEG_NPS]]];
      if (len(cuts) > 0) {
        echo(str(" SPOOL S", sp, ":"));
        for (c = cuts)
          echo(str("   cut ", fmt_len(c[0], ru), "  (NPS ", c[1], ")",
                   c[0] <= 0 ? "   <-- TOO SHORT for the fittings at its ends!" : ""));
      }
    }
    nfw = len([for (w = welds) if (w[1] == "FW") 1]);
    echo(str(" WELDS: ", len(welds), " total (", len(welds) - nfw, " shop, ", nfw, " field)"));
    if (len(welds) > 0) for (i = [0 : len(welds) - 1])
      echo(str("   ", welds[i][1] == "FW" ? "FW" : "W", i + 1, "  NPS ", welds[i][3], "  spool S", welds[i][4],
               "  at ", _fmt3(welds[i][0])));

    // BOM
    all = [for (c = chains) for (k = [0 : len(c[2]) - 1]) [c, k]];
    sizes = [for (x = PIPE_OD) if (len([for (ck = all) if (ck[0][2][ck[1]][SEG_NPS] == x[0]) 1]) > 0) x[0]];
    echo(" BOM:");
    for (sz = sizes) {
      pipe_len = _sum([for (ck = all) let(ch = ck[0], k = ck[1], s = ch[2][k])
                        if (s[SEG_KIND] == "pipe" && s[SEG_NPS] == sz) max(0, _cut(ch, k, u, elbow))]);
      angs = [for (ck = all) let(ch = ck[0], k = ck[1], s = ch[2][k])
                if (s[SEG_KIND] == "pipe" && s[SEG_NPS] == sz && _conn_e(ch, k) == "elbow")
                  round(_angle(s[SEG_DIR], ch[2][k + 1][SEG_DIR]))];
      n90 = len([for (a = angs) if (a == 90) 1]);
      n45 = len([for (a = angs) if (a == 45) 1]);
      nfl = _sum([for (ck = all) let(ch = ck[0], k = ck[1], s = ch[2][k])
                    if (s[SEG_KIND] == "pipe" && s[SEG_NPS] == sz)
                      (_conn_s(ch, k) == "flange" ? 1 : 0) + (_conn_e(ch, k) == "flange" ? 1 : 0)]);
      ntee = len([for (ck = all) let(s = ck[0][2][ck[1]]) if (s[SEG_KIND] == "tee" && s[SEG_NPS] == sz) 1]);
      ngask = _sum([for (ck = all) let(s = ck[0][2][ck[1]])
                      if (s[SEG_NPS] == sz) (s[SEG_KIND] == "joint" ? 1 : s[SEG_KIND] == "valve" ? 2 : 0)]);
      echo(str("   NPS ", sz, ": pipe ", fmt_len(pipe_len, ru), " SCH ", sch_norm(sch),
               n90 > 0 ? str(", ", n90, "x 90 ", elbow, " ell") : "",
               n45 > 0 ? str(", ", n45, "x 45 ell") : "",
               len(angs) - n90 - n45 > 0 ? str(", ", len(angs) - n90 - n45, "x cut ell (other angle)") : "",
               ntee > 0 ? str(", ", ntee, "x straight tee") : "",
               nfl > 0 ? str(", ", nfl, "x WN flange CL", rating, " RF") : "",
               ngask > 0 ? str(", ", ngask, "x gasket + ", ngask, " bolt sets (",
                               flange_150(sz)[6], " x ", bolt_dia_in(sz), "″ studs each)") : ""));
    }
    for (s = [for (ck = all) let(s = ck[0][2][ck[1]]) if (s[SEG_KIND] == "valve" || s[SEG_KIND] == "reducer") s])
      echo(str("   ", s[SEG_KIND] == "valve" ? str("1x ", s[SEG_INFO], " valve CL", rating, " flanged NPS ", s[SEG_NPS])
                                            : str("1x concentric reducer ", s[SEG_INFO][0], " x ", s[SEG_INFO][1])));

    // Elbow cut marks for elbows that aren't a stock 90 or 45: cut from a 90.
    cutells = [for (ck = all) let(ch = ck[0], k = ck[1], s = ch[2][k])
                 if (s[SEG_KIND] == "pipe" && _conn_e(ch, k) == "elbow")
                   let(a = _angle(s[SEG_DIR], ch[2][k + 1][SEG_DIR]))
                     if (abs(a - 90) > 0.5 && abs(a - 45) > 0.5) [a, s[SEG_NPS]]];
    if (len(cutells) > 0) {
      echo(" ELBOW CUTS (cut from a 90, mark on the outside / centerline / inside):");
      for (c = cutells) let(a = c[0], n = c[1], R = _R(n, elbow, u), r = pipe_od_in(n) * u / 2)
        echo(str("   NPS ", n, " ", round(a * 10) / 10, " deg:  outside ", fmt_len((R + r) * a * PI / 180, ru),
                 ",  centerline ", fmt_len(R * a * PI / 180, ru), ",  inside ", fmt_len((R - r) * a * PI / 180, ru),
                 ",  take-out ", fmt_len(R * tan(a / 2), ru)));
    }
  }

  // ---- PCF (Piping Component File) ----
  if (do_pcf) {
    echo("PCF|ISOGEN-FILES ISOGEN.FLS");
    echo("PCF|UNITS-BORE INCH");
    echo(str("PCF|UNITS-CO-ORDS ", units == "mm" ? "MM" : "INCH"));
    echo("PCF|UNITS-WEIGHT LBS");
    echo("PCF|UNITS-BOLT-DIA INCH");
    echo("PCF|UNITS-BOLT-LENGTH INCH");
    echo(str("PCF|PIPELINE-REFERENCE ", line));
    echo(str("PCF|    PIPING-SPEC ", material, "-", sch_norm(sch), "-", rating));
    for (ci = [0 : len(chains) - 1]) {
      ch = chains[ci]; segs = ch[2];
      for (k = [0 : len(segs) - 1]) {
        s = segs[k]; kind = s[SEG_KIND]; sn = s[SEG_NPS];
        if (kind == "pipe") {
          ts = _trim_s(ch, k, u, elbow); te = _trim_e(ch, k, u, elbow);
          a = s[SEG_A] + s[SEG_DIR] * ts; b = s[SEG_B] - s[SEG_DIR] * te;
          if (_conn_s(ch, k) == "flange")
            _pcf_comp("FLANGE", [s[SEG_A], a], sn, "FLWN", str("WN FLANGE CL", rating, " RF ", material));
          if (_cut(ch, k, u, elbow) > 0)
            _pcf_comp("PIPE", [a, b], sn, "", str("PIPE SCH ", sch_norm(sch), " ", material),
                      str("    SPOOL-IDENTIFIER S", _spool(chains, ci, k)));
          if (_conn_e(ch, k) == "flange")
            _pcf_comp("FLANGE", [s[SEG_B], b], sn, "FLWN", str("WN FLANGE CL", rating, " RF ", material));
          if (_conn_e(ch, k) == "elbow") {
            nx = segs[k + 1];
            ang = _angle(s[SEG_DIR], nx[SEG_DIR]);
            e = _R(sn, elbow, u) * tan(ang / 2);
            _pcf_comp("ELBOW", [s[SEG_B] - s[SEG_DIR] * e, s[SEG_B] + nx[SEG_DIR] * e], sn, "ELBW",
                      str(round(ang), " DEG ELBOW ", elbow, " ", material), str("    CENTRE-POINT ", _fmt3(s[SEG_B])));
          }
          if (_conn_e(ch, k) == "fw")
            _pcf_comp("WELD", [b, b], sn, "WF", "FIELD WELD");
        } else if (kind == "valve") {
          sk = s[SEG_INFO] == "globe" ? "VGFL" : s[SEG_INFO] == "ball" ? "VBFL"
             : s[SEG_INFO] == "check" ? "VCFL" : "VVFL";
          _pcf_comp("VALVE", [s[SEG_A], s[SEG_B]], sn, sk, str(s[SEG_INFO], " VALVE CL", rating, " FLANGED"));
        } else if (kind == "reducer") {
          echo("PCF|REDUCER-CONCENTRIC");
          echo(str("PCF|    END-POINT ", _fmt3(s[SEG_A]), " ", s[SEG_INFO][0]));
          echo(str("PCF|    END-POINT ", _fmt3(s[SEG_B]), " ", s[SEG_INFO][1]));
          echo("PCF|    SKEY RCBW");
        } else if (kind == "tee") {
          C = _C(sn, u); bd = _dir(s[SEG_INFO][0][0][0]);
          _pcf_comp("TEE", [s[SEG_A] - s[SEG_DIR] * C, s[SEG_A] + s[SEG_DIR] * C], sn, "TEBW", "STRAIGHT TEE",
                    str("    CENTRE-POINT ", _fmt3(s[SEG_A])),
                    str("    BRANCH1-POINT ", _fmt3(s[SEG_A] + bd * C), " ", sn));
        }
      }
    }
  }
}

module _pcf_comp(type, ends, nps, skey, desc, extra1="", extra2="") {
  echo(str("PCF|", type));
  echo(str("PCF|    END-POINT ", _fmt3(ends[0]), " ", nps));
  echo(str("PCF|    END-POINT ", _fmt3(ends[1]), " ", nps));
  if (extra1 != "") echo(str("PCF|", extra1));
  if (extra2 != "") echo(str("PCF|", extra2));
  if (skey != "") echo(str("PCF|    SKEY ", skey));
  echo(str("PCF|    ITEM-DESCRIPTION ", desc));
}
