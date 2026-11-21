#include "AIService.h"

#ifndef __EMSCRIPTEN__

#include "HTTPClient.h"
#include "AIClient.h"
#include "platform/PlatformUtils.h"
#include "json/json.hpp"
#include <fstream>
#include <cstdlib>

// PipeTrack: teaches the assistant to turn piping isometric sketches, photos
// and BOMs into pipe_run() models. Appended to every system prompt.
static const char *pipeTrackGuide()
{
  return R"PT(

### PipeTrack: piping sketches, photos and BOMs -> 3D pipe
When the user attaches or describes a piping isometric (hand sketch, drawing, photo of pipe) or a
BOM, build the model with the PipeTrack library. Do NOT model pipe by hand with cylinders.

Start the file with:  use <pipetrack/pipe.scad>
Then one pipe_run() per pipeline:
pipe_run([
    ["N", 1050],            // leg: direction + length, WORK POINT to WORK POINT
    ["E", "10'-6 3/4\""],   // feet-inch strings are fine (escape the inch mark)
    ["U", 750],
    ["valve", "gate"],      // flanged valve: gate | globe | ball | check (adds its own length)
    ["U", 350],
    ["flange"],             // flanged joint mid-run
    ["reducer", 2],         // concentric reducer to NPS 2 (adds its own length)
    ["tee", [ ["S", 600], ["U", 300] ], "flange"],  // tee here, branch run, branch end
    ["fw"],                 // field weld between two straight legs (["sw"] = shop weld)
    ["ell", "N"]            // last item only: turn north with an elbow and END at the elbow
  ],
  nps=4, sch="40", material="CS", units="mm", start="flange", end="open",
  elbow="LR", line="LINE-NUMBER");
Elbows are automatic wherever the direction changes (any angle).
Directions: E W N S U D, combinations like "NE", "SU", "NEU" for rolling offsets, or a vector [x,y,z].
nps: 0.5 0.75 1 1.25 1.5 2 2.5 3 3.5 4 5 6 8 10 12 14 16 18 20 24.
sch: 5S 10S 40 STD 40S 80 XS 80S 160 XXS.  elbow: LR | SR | 3D | 5D.  start/end: open | flange.

BE DECISIVE - this matters more than being perfect:
- Think briefly. Read the drawing once, decide, write the code. Do not re-check the same thing twice.
- If a number is blurry or the math doesn't reconcile with the BOM, use your best reading, mark it CHECK
  in the review table, and move on. The user verifies the table - that is their job, not yours.
- Always finish with the code. An answer with CHECK marks is useful; no answer is not.

READING AN ISOMETRIC SKETCH
- Vertical lines = UP / DOWN. Horizontal pipe runs along the two diagonal iso axes.
- NORTH = the iso axis the north arrow is drawn along, toward the arrowhead. No arrow: up-right = N.
- EAST = the other diagonal axis, on the side you reach turning CLOCKWISE from north:
    north up-right -> east down-right;   north up-left -> east up-right.
  South and west are the opposites.
- Every straight horizontal leg drawn ALONG an iso axis is exactly one of N, S, E, W.
  Do NOT write NE / NW / SE / SW for those: diagonal names mean a 45-degree run in plan and are only
  for legs drawn between the iso axes (rolling offsets, 45s), which are usually marked with a 45.
- Each dimension is work point to work point: elbow centerlines, tee centers, flange faces, valve faces.
- Follow the pipe from one end to the other and write the legs in that order.
- A dimension that spans several items (overall) is NOT a leg: use the individual dimensions.
- Bow-tie symbol = valve (gate unless marked; a filled dot in the bow-tie = globe; a circle = ball;
  arrow/flap = check). Two short parallel lines across the pipe = flange. A circle on a branch = tee.
- Units: numbers like 1050 or 750 with no marks are usually mm; numbers with ' or " are feet/inches.
  Keep the drawing's units: if it says 8'-8 15/16" write units="in" and ["U", "8'-8 15/16\""].
  Never convert feet-inches to mm.
- Stainless schedules end in the letter S: 5S, 10S, 40S, 80S (not 105 / 405).
- Never invent a number. If a dimension is unreadable or missing, put your best estimate and mark it
  in the review table with "CHECK".

FABRICATION ISOS (Isogen / MSUITE / Spoolgen style)
- The NORTH ARROW is usually top-left; lines parallel to it are N/S. Use it over the default axes.
- A dimension marked with * that "includes F/Trim allowance" is the work-point dimension to the
  field-trim end; model it as-is with that end "open" and note the trim in the review table.
- A dimension in (parentheses) next to another is the CUT length, not a work-point dimension.
  Use it only to check your model: cut = WP dim - fitting take-outs - weld gaps.
- Circled letters (A, B, ...) are BOM item numbers. FW28, FW29 in a bubble = numbered field welds.
  "CONT. ON C# 248057" = the line continues on another drawing (the run ends there).
- A run that ends AT an elbow (into a field weld or "CONT. ON"): finish with ["ell", dir] and end="fw".
  NEVER invent a stub length for a leg that has no dimension.
- TP / tie point = where the spool connects in the field.
- Shop isos usually use weld_gap = 1/16 (inches); if the BOM cut matches WP - take-out - 1/16, set it.
Example (2" Sch 10S 304/304L, one LR elbow, field-trim end, ends in FW at the elbow):
  pipe_run([ ["U", "8'-8 15/16\""], ["ell", "N"] ], nps=2, sch="10S", material="SS",
           units="in", start="open", end="fw", weld_gap=1/16, line="2-PAr-320");
  -> cut 8'-5 7/8" (matches the BOM).

READING A BOM / MATERIAL LIST
- Take NPS, schedule, rating and material from the BOM when one is given.
- material codes: A106/A53/API 5L = "CS"; A312/A358 TP304/316(L) = "SS"; A335 P11/P22/P91 = "CHROME";
  A790/A928 duplex = "DUPLEX"; B241/B210 aluminum = "AL"; B88/B42 copper = "CU"; B466/B467 Cu-Ni = "CUNI";
  B165/B444/B622 (Monel/Inconel/Hastelloy) = "NI"; B861/B862 titanium = "TI"; galvanized = "GALV".
- Count fittings from the BOM and make sure the model has the same number of elbows, tees, flanges
  and valves; mention any mismatch.

CLEAN ISO DRAWING
- If the user asks for a clean iso, drawing, redraw, PDF or spool sheet, build the same pipe_run()
  and add drawing=true. It draws a 2D isometric with dimensions, symbols, weld marks and the cut list;
  the user exports it with File > Export > PDF / SVG / DXF.

ALWAYS put a review table at the top of the file so the user can check what you read:
// READ FROM SKETCH  (verify before fabrication)
// #  item          length       note
// 1  N              1050 mm
// 2  E               800 mm     CHECK - digit unclear
Then call set_editor_code and trigger_preview, and in chat tell the user which values to double-check.
)PT";
}

static std::string getAISettingsPath()
{
  std::string configPath = PlatformUtils::userConfigPath();
  if (configPath.empty()) {
    char *home = std::getenv("HOME");
    if (home) {
      configPath = std::string(home) + "/.openscad";
    } else {
      configPath = ".";
    }
  }
  return configPath + "/ai_settings.json";
}

static bool loadActiveProfile(AIProfileConfig& config, std::string& error_msg)
{
  std::ifstream file(getAISettingsPath());
  if (!file.is_open()) {
    error_msg = "Could not open ai_settings.json config file.";
    return false;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const std::exception& e) {
    error_msg = std::string("JSON parsing error: ") + e.what();
    return false;
  }

  if (!j.contains("activeProfile") || !j["activeProfile"].is_string()) {
    error_msg = "ai_settings.json is missing 'activeProfile' string.";
    return false;
  }
  std::string active_profile_name = j["activeProfile"].get<std::string>();

  if (!j.contains("profiles") || !j["profiles"].is_object()) {
    error_msg = "ai_settings.json is missing 'profiles' object.";
    return false;
  }
  auto& profiles = j["profiles"];

  if (!profiles.contains(active_profile_name) || !profiles[active_profile_name].is_object()) {
    error_msg = "Active profile '" + active_profile_name + "' was not found in 'profiles'.";
    return false;
  }
  auto& profile = profiles[active_profile_name];

  config.endpoint = profile.value("endpoint", "");
  if (config.endpoint.empty()) {
    error_msg = "API endpoint is empty. Please configure a valid API URL in Preferences -> AI tab.";
    return false;
  }
  if (config.endpoint.rfind("http://", 0) != 0 && config.endpoint.rfind("https://", 0) != 0) {
    error_msg =
      "Invalid API endpoint: '" + config.endpoint + "'. Must start with 'http://' or 'https://'";
    return false;
  }
  config.apiKey = profile.value("apiKey", "");

  if (profile.contains("params") && profile["params"].is_object()) {
    auto& params = profile["params"];
    config.model = params.value("model", "");
    config.parameters = params;
  } else {
    config.model = "";
    config.parameters = nlohmann::json::object();
  }

  return true;
}

class AIService::Impl
{
public:
  std::unique_ptr<HTTPClient> http_client;
  std::unique_ptr<AIClient> ai_client;
  ToolExecutor tool_executor = nullptr;
  HistoryDrainCallback history_drain_callback = nullptr;

  Impl()
  {
    http_client = std::make_unique<HTTPClient>();
    ai_client = std::make_unique<AIClient>(std::shared_ptr<HTTPClient>(http_client.release()));
  }
  ~Impl() = default;

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;
};

AIService::AIService() : impl(std::make_unique<Impl>())
{
}

AIService::~AIService() = default;

AIService::AIService(AIService&&) noexcept = default;
AIService& AIService::operator=(AIService&&) noexcept = default;

void AIService::registerToolExecutor(ToolExecutor executor)
{
  impl->tool_executor = std::move(executor);
}

void AIService::registerHistoryDrainCallback(HistoryDrainCallback callback)
{
  impl->history_drain_callback = std::move(callback);
}

void AIService::chatCompletionStream(std::vector<ChatMessage>& history, ChunkCallback on_chunk,
                                     ErrorCallback on_error, CompleteCallback on_complete,
                                     bool interactiveMode, int current_auto_turn)
{
  AIProfileConfig config;
  std::string error_msg;
  if (!loadActiveProfile(config, error_msg)) {
    if (on_error) {
      on_error(error_msg);
    }
    return;
  }
  if (!param_overrides_json.empty()) {
    auto overrides = nlohmann::json::parse(param_overrides_json, nullptr, false);
    if (overrides.is_object()) {
      if (!config.parameters.is_object()) config.parameters = nlohmann::json::object();
      for (auto& el : overrides.items()) config.parameters[el.key()] = el.value();
    }
  }

  int max_auto_turns = 5;
  if (config.parameters.contains("max_auto_turns") &&
      config.parameters["max_auto_turns"].is_number_integer()) {
    max_auto_turns = config.parameters["max_auto_turns"].get<int>();
    if (max_auto_turns < 1) max_auto_turns = 1;
  }

  std::vector<AIChatMessage> ai_history;
  std::string sys_prompt =
    "You are the OpenSCAD Expert Assistant. You provide high-quality, surgical, and logical OpenSCAD "
    "code fixes.\n\n"
    "### YOUR CORE RULES:\n"
    "1. **Surgical Excellence**: If the user has a minor error (missing semicolon, wrong bracket), fix "
    "ONLY that specific line. Do NOT rewrite the entire script, do NOT rename variables, and do NOT "
    "change the overall logic unless explicitly asked.\n"
    "2. **OpenSCAD Syntax Mastery**:\n"
    "   - **Modifiers**: `color()`, `rotate()`, `translate()`, etc., are MODIFIERS. They apply to the "
    "next child or block. NEVER assign them to variables like `c = color(\"red\");`. Instead, use "
    "`color(\"red\") cube(10);`.\n"
    "   - **Semicolons**: Every assignment (e.g., `x = 5;`) and every module instantiation (e.g., "
    "`cube(10);`) MUST end with a semicolon. Semicolons are NOT used after module definitions `module "
    "name() { ... }` or after blocks `{ ... }`.\n"
    "3. **Tool Workflow**:\n"
    "   - YOU MUST USE `set_editor_code` to apply code changes. Behaviour differs by mode (see below).\n"
    "   - You can output code blocks in markdown format in the chat for explanation, but you MUST also "
    "call `set_editor_code` to update the script.\n"
    "   - Use `get_editor_code()` if you need to see the latest script state.\n"
    "   - Use `trigger_preview()` after setting code to validate compilation and check for errors.\n"
    "   - Use `get_viewport()` and `set_camera(...)` to inspect or adjust 3D view camera angles.\n"
    "4. **Response and Engagement**: Explain the reasoning behind your proposed code changes. Output "
    "standard text to explain your thoughts and keep the user engaged while proposing code changes via "
    "tools.\n"
    "5. **Formatting**: Use ACTUAL NEWLINES in your code output. Never use literal '\\n' sequences.\n"
    "6. **Tone**: Technical, concise, and helpful. Avoid long conversational filler.";

  if (config.parameters.contains("system_prompt") && config.parameters["system_prompt"].is_string()) {
    sys_prompt = config.parameters["system_prompt"].get<std::string>();
  }

  // Append mode-specific instructions so the model understands its operating context
  if (interactiveMode) {
    sys_prompt +=
      "\n\n### Current Operating Mode: INTERACTIVE REVIEW\n"
      "You are in interactive review mode. Follow these rules strictly:\n"
      "- Read the script with `get_editor_code()` if needed, then call `set_editor_code` ONCE with "
      "your proposed changes.\n"
      "- After calling `set_editor_code`, STOP. Do NOT call `trigger_preview` yourself. "
      "The user will review your diff and the preview will fire automatically upon acceptance.\n"
      "- Do NOT loop or call further tools after proposing code.";
  } else {
    sys_prompt +=
      "\n\n### Current Operating Mode: AUTONOMOUS AGENTIC\n"
      "You are in autonomous mode. Follow these rules:\n"
      "- Apply code changes directly with `set_editor_code`, then immediately call `trigger_preview` "
      "to validate the result.\n"
      "- If `trigger_preview` reports errors, read the error messages and call `set_editor_code` again "
      "with a fix. Repeat until the code compiles cleanly or you exhaust your turn limit.";
  }

  int context_limit = 10;
  if (config.parameters.contains("context_limit") &&
      config.parameters["context_limit"].is_number_integer()) {
    context_limit = config.parameters["context_limit"].get<int>();
    if (context_limit < 1) context_limit = 1;
  }

  bool already_has_system = false;
  if (!history.empty() && history[0].role == "system") {
    already_has_system = true;
  }
  if (!already_has_system) {
    // Append hidden OpenSCAD reference documentation to the system prompt
    std::string ref_doc =
      "\n\n### OpenSCAD Quick Reference\n"
      "**3D Primitives**: cube([x,y,z], center), sphere(r, $fn), cylinder(h, r1, r2, center, $fn)\n"
      "**2D Primitives**: circle(r, $fn), square([x,y], center), polygon(points, paths), text(t, size, "
      "font, halign, valign)\n"
      "**Transforms**: translate([x,y,z]), rotate([x,y,z]), scale([x,y,z]), mirror([x,y,z]), "
      "multmatrix(m), color(c, alpha), offset(r|delta, chamfer), hull(), minkowski()\n"
      "**Boolean Ops**: union(), difference(), intersection()\n"
      "**Extrusion**: linear_extrude(height, center, twist, slices, scale, $fn), "
      "rotate_extrude(angle, $fn)\n"
      "**Modules**: module name(params) { body } -- NO semicolon after definition. "
      "Call with: name(args);\n"
      "**Functions**: function name(params) = expression;\n"
      "**Control**: for(i=[start:step:end]), if(cond), let(assignments), each, "
      "assert(cond, msg), echo(values)\n"
      "**Math**: sin, cos, tan, asin, acos, atan, atan2, abs, ceil, floor, round, "
      "sqrt, pow, exp, ln, log, min, max, norm, cross\n"
      "**List Ops**: concat, len, lookup, str, chr, ord, search, flatten\n"
      "**Special Vars**: $fn (fragments), $fa (fragment angle), $fs (fragment size), "
      "$t (animation), $vpr/$vpt/$vpd/$vpf (viewport)\n"
      "**Import/Use**: use <file.scad> (functions/modules only), include <file.scad> (full), "
      "import(\"file.stl\")\n\n"
      "### Available Tools\n"
      "- **get_editor_code()**: Returns the current contents of the active editor tab.\n"
      "- **set_editor_code({\"code\": \"...\"})**: Applies code changes directly to active editor.\n"
      "- **trigger_preview()**: Renders current editor code and returns compile errors/warnings.\n"
      "- **get_viewport()**: Returns 3D viewport camera parameters (vpt, vpr, vpd, vpf, projection).\n"
      "- **set_camera({\"vpt\": [x,y,z], \"vpr\": [x,y,z], ...})**: Sets 3D view camera vectors.";
    ai_history.push_back({"system", sys_prompt + ref_doc + pipeTrackGuide()});
  }

  // Apply sliding window: keep last context_limit turn pairs (a turn starts with a "user" message)
  size_t window_start = 0;
  {
    std::vector<size_t> turn_starts;
    for (size_t i = 0; i < history.size(); ++i) {
      if (history[i].role == "user") {
        turn_starts.push_back(i);
      }
    }
    if (static_cast<int>(turn_starts.size()) > context_limit) {
      window_start = turn_starts[turn_starts.size() - context_limit];
    }
  }

  for (size_t i = window_start; i < history.size(); ++i) {
    const auto& msg = history[i];
    AIChatMessage am;
    am.role = msg.role;
    am.content = msg.content;
    am.tool_call_id = msg.tool_call_id;
    for (const auto& img : msg.images) {
      am.images.push_back({img.mime_type, img.base64_data});
    }
    if (!msg.tool_calls.empty()) {
      try {
        auto tcs_json = nlohmann::json::parse(msg.tool_calls);
        if (tcs_json.is_array()) {
          for (auto& tc_json : tcs_json) {
            AIToolCall tc;
            if (tc_json.contains("id")) tc.id = tc_json["id"].get<std::string>();
            if (tc_json.contains("type")) tc.type = tc_json["type"].get<std::string>();
            if (tc_json.contains("function") && tc_json["function"].is_object()) {
              auto& fn = tc_json["function"];
              if (fn.contains("name")) tc.name = fn["name"].get<std::string>();
              if (fn.contains("arguments")) {
                if (fn["arguments"].is_string()) {
                  tc.arguments = fn["arguments"].get<std::string>();
                } else {
                  tc.arguments = fn["arguments"].dump();
                }
              }
            }
            am.tool_calls.push_back(tc);
          }
        }
      } catch (...) {
      }
    }
    ai_history.push_back(am);
  }

  // In interactive mode the model gets at most 2 LLM calls: one to call tools
  // (e.g. get_editor_code then set_editor_code) and one final response after
  // seeing those results. We enforce this by capping the effective turn limit.
  const int effective_max_turns = interactiveMode ? std::min(max_auto_turns, 2) : max_auto_turns;

  auto on_complete_wrapper = [this, config, &history, on_chunk, on_error, on_complete, current_auto_turn,
                              effective_max_turns,
                              interactiveMode](const std::vector<AIToolCall>& tool_calls) {
    if (tool_calls.empty()) {
      if (on_complete) {
        on_complete();
      }
      return;
    }

    ChatMessage assistant_msg;
    assistant_msg.role = "assistant";
    assistant_msg.content = "";
    nlohmann::json tcs_arr = nlohmann::json::array();
    for (const auto& tc : tool_calls) {
      nlohmann::json t = nlohmann::json::object();
      t["id"] = tc.id;
      t["type"] = "function";
      nlohmann::json fn = nlohmann::json::object();
      fn["name"] = tc.name;
      fn["arguments"] = tc.arguments;
      t["function"] = fn;
      tcs_arr.push_back(t);
    }
    assistant_msg.tool_calls = tcs_arr.dump();
    history.push_back(assistant_msg);

    for (const auto& tc : tool_calls) {
      std::string result;
      if (impl->tool_executor) {
        result = impl->tool_executor(tc.name, tc.arguments);
      } else {
        result = "Error: No tool executor registered.";
      }
      ChatMessage tool_msg;
      tool_msg.role = "tool";
      tool_msg.content = result;
      tool_msg.tool_call_id = tc.id;
      history.push_back(tool_msg);
    }

    // Allow the UI layer to inject additional context messages (e.g. a
    // viewport snapshot captured after trigger_preview) into history before
    // the next recursive LLM call.
    if (impl->history_drain_callback) {
      impl->history_drain_callback(history);
    }

    if (current_auto_turn + 1 >= effective_max_turns) {
      if (on_complete) {
        on_complete();
      }
      return;
    }

    chatCompletionStream(history, on_chunk, on_error, on_complete, interactiveMode,
                         current_auto_turn + 1);
  };

  impl->ai_client->sendChatCompletionStream(config, ai_history, on_chunk, on_error, on_complete_wrapper);
}

void AIService::chatCompletion(const std::vector<ChatMessage>& history, ResponseCallback on_response,
                               ErrorCallback on_error)
{
  AIProfileConfig config;
  std::string error_msg;
  if (!loadActiveProfile(config, error_msg)) {
    if (on_error) {
      on_error(error_msg);
    }
    return;
  }
  if (!param_overrides_json.empty()) {
    auto overrides = nlohmann::json::parse(param_overrides_json, nullptr, false);
    if (overrides.is_object()) {
      if (!config.parameters.is_object()) config.parameters = nlohmann::json::object();
      for (auto& el : overrides.items()) config.parameters[el.key()] = el.value();
    }
  }

  std::vector<AIChatMessage> ai_history;
  std::string sys_prompt =
    "You are the OpenSCAD Expert Assistant. You provide high-quality, surgical, and logical OpenSCAD "
    "code fixes.\n\n"
    "### YOUR CORE RULES:\n"
    "1. **Surgical Excellence**: If the user has a minor error (missing semicolon, wrong bracket), fix "
    "ONLY that specific line. Do NOT rewrite the entire script, do NOT rename variables, and do NOT "
    "change the overall logic unless explicitly asked.\n"
    "2. **OpenSCAD Syntax Mastery**:\n"
    "   - **Modifiers**: `color()`, `rotate()`, `translate()`, etc., are MODIFIERS. They apply to the "
    "next child or block. NEVER assign them to variables like `c = color(\"red\");`. Instead, use "
    "`color(\"red\") cube(10);`.\n"
    "   - **Semicolons**: Every assignment (e.g., `x = 5;`) and every module instantiation (e.g., "
    "`cube(10);`) MUST end with a semicolon. Semicolons are NOT used after module definitions `module "
    "name() { ... }` or after blocks `{ ... }`.\n"
    "3. **Tool Workflow**:\n"
    "   - YOU MUST USE `set_editor_code` to propose any code changes so they are set for user review.\n"
    "   - You can output code blocks in markdown format in the chat for explanation, but you MUST also "
    "call the `set_editor_code` tool so the changes are set for review and can be applied "
    "automatically.\n"
    "   - Use `get_editor_code()` if you need to see the latest script state.\n"
    "   - Use `trigger_preview()` once after setting the code to validate the result.\n"
    "4. **Response and Engagement**: Explain the reasoning behind your proposed code changes. Output "
    "standard text to explain your thoughts and keep the user engaged while proposing code changes via "
    "tools.\n"
    "5. **Formatting**: Use ACTUAL NEWLINES in your code output. Never use literal '\\n' sequences.\n"
    "6. **Tone**: Technical, concise, and helpful. Avoid long conversational filler.";
  if (config.parameters.contains("system_prompt") && config.parameters["system_prompt"].is_string()) {
    sys_prompt = config.parameters["system_prompt"].get<std::string>();
  }

  bool already_has_system = false;
  if (!history.empty() && history[0].role == "system") {
    already_has_system = true;
  }
  if (!already_has_system) {
    // Append hidden OpenSCAD reference documentation to the system prompt
    std::string ref_doc =
      "\n\n### OpenSCAD Quick Reference\n"
      "**3D Primitives**: cube([x,y,z], center), sphere(r, $fn), cylinder(h, r1, r2, center, $fn)\n"
      "**2D Primitives**: circle(r, $fn), square([x,y], center), polygon(points, paths), text(t, size, "
      "font, halign, valign)\n"
      "**Transforms**: translate([x,y,z]), rotate([x,y,z]), scale([x,y,z]), mirror([x,y,z]), "
      "multmatrix(m), color(c, alpha), offset(r|delta, chamfer), hull(), minkowski()\n"
      "**Boolean Ops**: union(), difference(), intersection()\n"
      "**Extrusion**: linear_extrude(height, center, twist, slices, scale, $fn), "
      "rotate_extrude(angle, $fn)\n"
      "**Modules**: module name(params) { body } -- NO semicolon after definition. "
      "Call with: name(args);\n"
      "**Functions**: function name(params) = expression;\n"
      "**Control**: for(i=[start:step:end]), if(cond), let(assignments), each, "
      "assert(cond, msg), echo(values)\n"
      "**Math**: sin, cos, tan, asin, acos, atan, atan2, abs, ceil, floor, round, "
      "sqrt, pow, exp, ln, log, min, max, norm, cross\n"
      "**List Ops**: concat, len, lookup, str, chr, ord, search, flatten\n"
      "**Special Vars**: $fn (fragments), $fa (fragment angle), $fs (fragment size), "
      "$t (animation), $vpr/$vpt/$vpd/$vpf (viewport)\n"
      "**Import/Use**: use <file.scad> (functions/modules only), include <file.scad> (full), "
      "import(\"file.stl\")\n\n"
      "### Available Tools\n"
      "- **get_editor_code()**: Returns the current contents of the active editor tab.\n"
      "- **set_editor_code({\"code\": \"...\"})**:  Proposes code changes for user review in a "
      "side-by-side diff dialog. "
      "Always use this instead of pasting code in chat.\n"
      "- **trigger_preview()**: Renders the current editor code in the 3D viewport. "
      "Automatically postponed if code changes are pending review.";
    ai_history.push_back({"system", sys_prompt + ref_doc + pipeTrackGuide()});
  }

  for (const auto& msg : history) {
    AIChatMessage am;
    am.role = msg.role;
    am.content = msg.content;
    am.tool_call_id = msg.tool_call_id;
    for (const auto& img : msg.images) {
      am.images.push_back({img.mime_type, img.base64_data});
    }
    if (!msg.tool_calls.empty()) {
      try {
        auto tcs_json = nlohmann::json::parse(msg.tool_calls);
        if (tcs_json.is_array()) {
          for (auto& tc_json : tcs_json) {
            AIToolCall tc;
            if (tc_json.contains("id")) tc.id = tc_json["id"].get<std::string>();
            if (tc_json.contains("type")) tc.type = tc_json["type"].get<std::string>();
            if (tc_json.contains("function") && tc_json["function"].is_object()) {
              auto& fn = tc_json["function"];
              if (fn.contains("name")) tc.name = fn["name"].get<std::string>();
              if (fn.contains("arguments")) {
                if (fn["arguments"].is_string()) {
                  tc.arguments = fn["arguments"].get<std::string>();
                } else {
                  tc.arguments = fn["arguments"].dump();
                }
              }
            }
            am.tool_calls.push_back(tc);
          }
        }
      } catch (...) {
      }
    }
    ai_history.push_back(am);
  }

  impl->ai_client->sendChatCompletion(
    config, ai_history,
    [on_response](const std::string& response, const std::vector<AIToolCall>&) {
      if (on_response) {
        on_response(response);
      }
    },
    on_error);
}

std::string AIService::getDefaultPrompt() const
{
  AIProfileConfig config;
  std::string error_msg;
  if (loadActiveProfile(config, error_msg)) {
    if (config.parameters.contains("default_prompt") &&
        config.parameters["default_prompt"].is_string()) {
      std::string prompt = config.parameters["default_prompt"].get<std::string>();
      if (!prompt.empty()) {
        return prompt;
      }
    }
  }
  return "Create a sphere with radius 10 and detail level $fn=50.";
}

void AIService::cancelPendingRequests()
{
  impl->ai_client->cancelPendingRequests();
}

int AIService::getPayloadLimit() const
{
  AIProfileConfig config;
  std::string error_msg;
  if (loadActiveProfile(config, error_msg)) {
    if (config.parameters.contains("payload_limit") &&
        config.parameters["payload_limit"].is_number_integer()) {
      return config.parameters["payload_limit"].get<int>();
    }
  }
  return 50000;
}

bool AIService::getAutoAttachViewport() const
{
  AIProfileConfig config;
  std::string error_msg;
  if (loadActiveProfile(config, error_msg)) {
    if (config.parameters.contains("auto_attach_viewport") &&
        config.parameters["auto_attach_viewport"].is_boolean()) {
      return config.parameters["auto_attach_viewport"].get<bool>();
    }
  }
  return false;
}

int AIService::getMaxAutoTurns() const
{
  AIProfileConfig config;
  std::string error_msg;
  if (loadActiveProfile(config, error_msg)) {
    if (config.parameters.contains("max_auto_turns") &&
        config.parameters["max_auto_turns"].is_number_integer()) {
      int val = config.parameters["max_auto_turns"].get<int>();
      if (val >= 1) return val;
    }
  }
  return 5;
}

#else  // __EMSCRIPTEN__

class AIService::Impl
{
};

AIService::AIService() : impl(std::make_unique<Impl>())
{
}
AIService::~AIService() = default;

AIService::AIService(AIService&&) noexcept = default;
AIService& AIService::operator=(AIService&&) noexcept = default;

void AIService::chatCompletionStream(std::vector<ChatMessage>&, ChunkCallback, ErrorCallback on_error,
                                     CompleteCallback, bool, int)
{
  if (on_error) {
    on_error("AI service is not supported on WebAssembly.");
  }
}

void AIService::chatCompletion(const std::vector<ChatMessage>&, ResponseCallback, ErrorCallback on_error)
{
  if (on_error) {
    on_error("AI service is not supported on WebAssembly.");
  }
}

std::string AIService::getDefaultPrompt() const
{
  return "";
}

void AIService::cancelPendingRequests()
{
}

int AIService::getPayloadLimit() const
{
  return 50000;
}

bool AIService::getAutoAttachViewport() const
{
  return false;
}

int AIService::getMaxAutoTurns() const
{
  return 5;
}

#endif  // __EMSCRIPTEN__
