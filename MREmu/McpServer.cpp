#include "McpServer.h"

#include <algorithm>
#include <iostream>
#include <sstream>

// On Windows, dup2 of fd 1 needs these.
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
// Emscripten provides POSIX fileno/dup/fdopen/close.
#elif defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

// A small hand-rolled JSON layer. The project already vendors spdlog/fmt but not a
// JSON library, and MCP messages are simple enough that a minimal
// serialiser/parser keeps this dependency-free.

namespace {

std::string json_escape(const std::string& in) {
	std::string out;
	for (unsigned char c : in) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) {
				char b[8];
				snprintf(b, sizeof(b), "\\u%04x", c);
				out += b;
			}
			else out += (char)c;
		}
	}
	return out;
}

std::string jstr(const std::string& v) { return "\"" + json_escape(v) + "\""; }
std::string jnum(long long v) { return std::to_string(v); }
std::string jbool(bool v) { return v ? "true" : "false"; }

// ---- minimal JSON parser: enough for MCP request objects ----
struct JValue;
using JPtr = std::shared_ptr<JValue>;

struct JValue {
	enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
	bool b = false;
	double num = 0;
	std::string str;
	std::vector<JPtr> arr;
	std::vector<std::pair<std::string, JPtr>> obj;

	const JPtr find(const std::string& k) const {
		for (auto& kv : obj)
			if (kv.first == k) return kv.second;
		return nullptr;
	}
	std::string get_str(const std::string& k, const std::string& def = "") const {
		auto v = find(k);
		return (v && v->kind == Str) ? v->str : def;
	}
	long long get_num(const std::string& k, long long def = 0) const {
		auto v = find(k);
		return (v && v->kind == Num) ? (long long)v->num : def;
	}
	bool get_bool(const std::string& k, bool def = false) const {
		auto v = find(k);
		return (v && v->kind == Bool) ? v->b : def;
	}
};

struct JParser {
	const std::string& s;
	size_t i = 0;
	explicit JParser(const std::string& src) : s(src) {}

	void ws() {
		while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
			i++;
	}

	JPtr parse() {
		ws();
		if (i >= s.size()) return nullptr;
		char c = s[i];
		if (c == '{') return parse_obj();
		if (c == '[') return parse_arr();
		if (c == '"') {
			auto v = std::make_shared<JValue>();
			v->kind = JValue::Str;
			v->str = parse_str();
			return v;
		}
		if (s.compare(i, 4, "true") == 0) { i += 4; auto v = std::make_shared<JValue>(); v->kind = JValue::Bool; v->b = true; return v; }
		if (s.compare(i, 5, "false") == 0) { i += 5; auto v = std::make_shared<JValue>(); v->kind = JValue::Bool; v->b = false; return v; }
		if (s.compare(i, 4, "null") == 0) { i += 4; return std::make_shared<JValue>(); }
		auto v = std::make_shared<JValue>();
		v->kind = JValue::Num;
		size_t st = i;
		while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '+' ||
			s[i] == '.' || s[i] == 'e' || s[i] == 'E')) i++;
		v->num = atof(s.substr(st, i - st).c_str());
		return v;
	}

	std::string parse_str() {
		std::string out;
		if (i < s.size() && s[i] == '"') i++;
		while (i < s.size() && s[i] != '"') {
			if (s[i] == '\\' && i + 1 < s.size()) {
				i++;
				char e = s[i];
				switch (e) {
				case 'n': out += '\n'; break;
				case 't': out += '\t'; break;
				case 'r': out += '\r'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'u': {
					// Decode \uXXXX to UTF-8 (BMP only; surrogates pass through).
					if (i + 4 < s.size()) {
						unsigned cp = (unsigned)strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
						i += 4;
						if (cp < 0x80) out += (char)cp;
						else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
						else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
					}
					break;
				}
				default: out += e;
				}
				i++;
			}
			else out += s[i++];
		}
		if (i < s.size() && s[i] == '"') i++;
		return out;
	}

	JPtr parse_arr() {
		auto v = std::make_shared<JValue>();
		v->kind = JValue::Arr;
		i++; // [
		ws();
		if (i < s.size() && s[i] == ']') { i++; return v; }
		while (i < s.size()) {
			v->arr.push_back(parse());
			ws();
			if (i < s.size() && s[i] == ',') { i++; ws(); continue; }
			if (i < s.size() && s[i] == ']') { i++; break; }
			break;
		}
		return v;
	}

	JPtr parse_obj() {
		auto v = std::make_shared<JValue>();
		v->kind = JValue::Obj;
		i++; // {
		ws();
		if (i < s.size() && s[i] == '}') { i++; return v; }
		while (i < s.size()) {
			ws();
			if (i >= s.size() || s[i] != '"') break;
			std::string k = parse_str();
			ws();
			if (i < s.size() && s[i] == ':') i++;
			auto val = parse();
			v->obj.emplace_back(k, val);
			ws();
			if (i < s.size() && s[i] == ',') { i++; continue; }
			if (i < s.size() && s[i] == '}') { i++; break; }
			break;
		}
		return v;
	}
};

std::string hex32(unsigned v) {
	char b[16];
	snprintf(b, sizeof(b), "0x%08X", v);
	return b;
}

} // namespace

namespace Mcp {

void Control::publish(const Snapshot& s) {
	std::lock_guard<std::mutex> lk(snap_mtx_);
	snap_ = s;
}

Snapshot Control::snapshot() const {
	std::lock_guard<std::mutex> lk(snap_mtx_);
	return snap_;
}

void Control::push(Command c) {
	std::lock_guard<std::mutex> lk(cmd_mtx_);
	cmds_.push_back(std::move(c));
}

bool Control::take(Command& out) {
	std::lock_guard<std::mutex> lk(cmd_mtx_);
	if (cmds_.empty()) return false;
	out = std::move(cmds_.front());
	cmds_.pop_front();
	return true;
}

std::vector<Command> Control::pending() {
	std::lock_guard<std::mutex> lk(cmd_mtx_);
	return std::vector<Command>(cmds_.begin(), cmds_.end());
}

bool Control::take_trace_request(bool& out) {
	out = trace_request_.exchange(false);
	return out;
}

void Control::note_error(const std::string& e) {
	std::lock_guard<std::mutex> lk(err_mtx_);
	if (errs_.size() < 1000) errs_.push_back(e);
}

std::vector<std::string> Control::errors() const {
	std::lock_guard<std::mutex> lk(err_mtx_);
	return std::vector<std::string>(errs_.begin(), errs_.end());
}

void Control::clear_errors() {
	std::lock_guard<std::mutex> lk(err_mtx_);
	errs_.clear();
}

// ---------------------------------------------------------------- tool table

namespace {

struct ToolCtx {
	Control* ctl = nullptr;
	const HostApi* host = nullptr;
} g_ctx;

} // namespace

namespace {

// streambuf that forwards to a C FILE*. Needed because MSVC's std::ostream does
// not accept a FILE* constructor, and this is how the protocol stream reaches the
// real stdout after fd 1 has been redirected to stderr.
class FileBuf : public std::streambuf {
public:
	explicit FileBuf(FILE* f) : f_(f) {}
protected:
	int_type overflow(int_type ch) override {
		if (ch != traits_type::eof() && f_) {
			char c = (char)ch;
			fwrite(&c, 1, 1, f_);
			fflush(f_);
		}
		return ch;
	}
	std::streamsize xsputn(const char* s, std::streamsize n) override {
		if (!f_) return 0;
		std::streamsize w = fwrite(s, 1, (size_t)n, f_);
		fflush(f_);
		return w;
	}
	int sync() override { return f_ ? (fflush(f_) == 0 ? 0 : -1) : -1; }
private:
	FILE* f_;
};

} // namespace

std::ostream* claim_stdout_for_protocol() {
#ifdef _WIN32
	// Duplicate the real stdout first, then point fd 1 at stderr. Everything that
	// logs to fd 1 (spdlog's stdout sink, printf, SFML) now lands on stderr, while
	// the returned stream keeps writing to the genuine stdout for JSON-RPC.
	fflush(stdout);
	int saved = _dup(_fileno(stdout));
	if (_dup2(_fileno(stderr), _fileno(stdout)) == -1 || saved < 0) {
		if (saved >= 0) _close(saved);
		return nullptr;
	}
	FILE* f = _fdopen(saved, "w");
	if (!f) { _close(saved); return nullptr; }
	// Both objects are deliberately never destroyed: the protocol stream must
	// outlive every call here, and closing the fd would end the session.
	static FileBuf buf(f);
	static std::ostream proto(&buf);
	return &proto;
#else
	fflush(stdout);
	if (FILE* f = fdopen(dup(fileno(stdout)), "w")) {
		static FileBuf buf(f);
		static std::ostream proto(&buf);
		return &proto;
	}
	return nullptr;
#endif
}

void set_host(const HostApi* api) { g_ctx.host = api; }

namespace {

std::string tool_result_text(const std::string& text) {
	return "{\"content\":[{\"type\":\"text\",\"text\":" + jstr(text) + "}]}";
}

std::string tool_result_json(const std::string& json) {
	return "{\"content\":[{\"type\":\"text\",\"text\":" + jstr(json) + "}]}";
}

// Tool schemas are emitted as one line. MCP is JSON-RPC over a line-delimited
// stream, so a literal newline anywhere in a reply would split one response into
// two unparseable frames.
const char* TOOLS = R"JSON([{"name":"emu_status","description":"Current state: running app, run_state, VM call count, stepped instructions, PC, screen size and last error."},{"name":"emu_list_apps","description":"List every loaded app with index, name and run state."},{"name":"emu_load_vxp","description":"Load a VXP file into the emulator by absolute path."},{"name":"emu_close_app","description":"Close a loaded app by index."},{"name":"emu_regs","description":"Read all CPU registers plus CPSR flags."},{"name":"emu_set_reg","description":"Write one CPU register by name (r0-r12, sp, lr, pc, cpsr)."},{"name":"emu_read_mem","description":"Read memory from the emulated address space. Parameters: address (int or hex string), size (int, default 64, max 4096)."},{"name":"emu_step","description":"Run at most N instructions, then yield. Makes pending app timers due first, so an otherwise-idle app actually runs."},{"name":"emu_pause","description":"Pause or resume. Paused apps still receive timer ticks but yield at each slice instead of executing."},{"name":"emu_screen","description":"Get the emulated screen as a PNG, optionally downscaled."},{"name":"emu_errors","description":"Read errors the emulator recorded since the last clear."},{"name":"emu_trace_control","description":"Enable, disable, clear or query the VM API call tracer."},{"name":"emu_trace_dump","description":"Read recorded VM API calls: name, PC, arguments and return value. Parameters: offset (int), limit (int), filter (string), tail (bool)."},{"name":"emu_key","description":"Inject a key event (press, release, or click) by VM keycode."},{"name":"emu_touch","description":"Inject a touch/pen event at (x, y) coordinates. Parameters: x (int), y (int), action ('click', 'down', 'up', 'move', or int)."}])JSON";

} // namespace

static std::string handle_tool(Control& ctl, const std::string& name, const JPtr& args) {
	auto arg_s = [&](const char* k) { return args ? args->get_str(k) : std::string(); };
	auto arg_n = [&](const char* k, long long d = 0) { return args ? args->get_num(k, d) : d; };

	// Host entry points are optional so a transport-only build still serves
	// tools/list and reports which tools are unavailable, instead of crashing.
	const HostApi* H = g_ctx.host;
	auto need = [&](const char* what) {
		if (!H) return true;
		return false;
	};
	(void)need;

	if (name == "emu_status") {
		Snapshot s = ctl.snapshot();
		std::string j = "{";
		j += "\"app_running\":" + jbool(s.app_running) + ",";
		j += "\"app_name\":" + jstr(s.app_name) + ",";
		j += "\"run_state\":" + jstr(s.run_state) + ",";
		j += "\"vm_calls\":" + jnum((long long)s.vm_calls) + ",";
		j += "\"stepped_instructions\":" + jnum((long long)s.stepped_instructions) + ",";
		j += "\"step_budget\":" + jnum((long long)s.step_budget) + ",";
		j += "\"timers_made_due\":" + jnum(s.timers_made_due) + ",";
		j += "\"faulted\":" + jbool(s.faulted) + ",";
		j += "\"pc\":" + jstr(hex32(s.pc)) + ",";
		j += "\"screen_width\":" + jnum(s.screen_w) + ",";
		j += "\"screen_height\":" + jnum(s.screen_h) + ",";
		j += "\"apps_loaded\":" + jnum((long long)s.apps_loaded) + ",";
		j += "\"last_error\":" + jstr(s.last_error) + ",";
		j += "\"trace_ready\":" + jbool(s.trace_ready) + ",";
		j += "\"pending_commands\":" + jnum((long long)ctl.pending().size());
		j += "}";
		return tool_result_json(j);
	}

	if (name == "emu_list_apps") {
		auto v = (H && H->list_apps) ? H->list_apps() : std::vector<std::string>{};
		std::string j = "{\"apps\":[";
		for (size_t i = 0; i < v.size(); i++) {
			if (i) j += ",";
			j += jstr(v[i]);
		}
		j += "]}";
		return tool_result_json(j);
	}

	if (name == "emu_load_vxp") {
		std::string p = arg_s("path");
		if (p.empty()) return tool_result_text("error: path is required");
		if (!H || !H->load_vxp) return tool_result_text("error: load not supported in this build");
		return tool_result_text(H->load_vxp(p));
	}

	if (name == "emu_close_app") {
		long long idx = arg_n("index", -1);
		if (!H || !H->close_app) return tool_result_text("error: close not supported");
		return tool_result_text(H->close_app((int)idx) ? "closed" : "error: no such app index");
	}

	if (name == "emu_regs") {
		Snapshot s = ctl.snapshot();
		static const char* names[] = { "r0","r1","r2","r3","r4","r5","r6","r7",
			"r8","r9","r10","r11","r12","sp","lr","pc" };
		std::string j = "{\"registers\":{";
		for (int i = 0; i < 16; i++) {
			if (i) j += ",";
			j += jstr(names[i]) + ":" + jstr(hex32(s.regs[i]));
		}
		j += "},\"cpsr\":" + jstr(hex32(s.cpsr));
		j += ",\"flags\":{";
		j += "\"N\":" + jbool(s.cpsr & (1u << 31)) + ",";
		j += "\"Z\":" + jbool(s.cpsr & (1u << 30)) + ",";
		j += "\"C\":" + jbool(s.cpsr & (1u << 29)) + ",";
		j += "\"V\":" + jbool(s.cpsr & (1u << 28)) + ",";
		j += "\"thumb\":" + jbool(s.cpsr & (1u << 5));
		j += "},\"apps\":" + jnum((long long)s.apps_loaded);
		j += "}";
		return tool_result_json(j);
	}

	if (name == "emu_set_reg") {
		std::string r = arg_s("register");
		long long v = arg_n("value");
		if (!H || !H->write_reg) return tool_result_text("error: register write not supported");
		return tool_result_text(H->write_reg(r, (unsigned)v)
			? ("wrote " + r + "=" + hex32((unsigned)v))
			: ("error: unknown register " + r));
	}

	if (name == "emu_read_mem") {
		long long addr = 0;
		std::string s_addr = arg_s("address");
		if (!s_addr.empty()) {
			addr = strtoll(s_addr.c_str(), nullptr, 0);
		} else {
			addr = arg_n("address", 0);
		}
		size_t size = (size_t)arg_n("size", 64);
		if (!H || !H->read_mem) return tool_result_text("error: read_mem not supported");
		return tool_result_text(H->read_mem((uint32_t)addr, size));
	}

	if (name == "emu_step") {
		long long n = arg_n("instructions", 1000);
		if (n < 0) n = 0;
		if (!H || !H->step) return tool_result_text("error: stepping not supported");
		return tool_result_text(H->step((uint64_t)n) ? "stepped" : "error: could not step");
	}

	if (name == "emu_pause") {
		bool on = args ? args->get_bool("paused", true) : true;
		ctl.request_pause(on);
		if (H && H->set_pause) H->set_pause(on);
		return tool_result_text(on
			? "paused: the app yields at its next slice; timers keep firing"
			: "resumed: the app continues on its next timer");
	}

	if (name == "emu_screen") {
		long long max_side = arg_n("max_side", 0);
		if (!H || !H->screen_png_base64) return tool_result_text("error: screen capture not supported");
		std::string b64 = H->screen_png_base64((int)max_side);
		if (b64.empty()) return tool_result_text("error: no screen available yet");
		return "{\"content\":[{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":" + jstr(b64) +
			"},{\"type\":\"text\",\"text\":" + jstr("emulated screen") + "}]}";
	}

	if (name == "emu_errors") {
		auto e = ctl.errors();
		std::string j = "{\"errors\":[";
		for (size_t i = 0; i < e.size(); i++) { if (i) j += ","; j += jstr(e[i]); }
		j += "]}";
		return tool_result_json(j);
	}

	if (name == "emu_trace_control") {
		std::string action = arg_s("action");
		if (action == "clear") {
			if (H && H->trace_clear) H->trace_clear();
			return tool_result_text("trace cleared");
		}
		if (action == "status") {
			size_t c = (H && H->trace_count) ? H->trace_count() : 0;
			return tool_result_text("recorded calls: " + std::to_string(c));
		}
		bool on = (action != "disable" && action != "off");
		ctl.set_trace_request(on);
		if (H && H->set_trace) H->set_trace(on);
		return tool_result_text(on ? "trace enable requested" : "trace disable requested");
	}

	if (name == "emu_trace_dump") {
		size_t off = (size_t)arg_n("offset", 0);
		size_t lim = (size_t)arg_n("limit", 50);
		std::string filter = arg_s("filter");
		bool tail = args ? args->get_bool("tail", false) : false;
		if (!H || !H->trace_dump) return tool_result_text("error: tracing not supported");
		return tool_result_text(H->trace_dump(off, lim, filter, tail));
	}

	if (name == "emu_key") {
		long long key = arg_n("key", 0);
		std::string action = arg_s("action");
		if (!H || !H->inject_key) return tool_result_text("error: key injection not supported");
		if (action == "up") {
			H->inject_key((int)key, false);
		} else if (action == "down") {
			H->inject_key((int)key, true);
		} else {
			H->inject_key((int)key, true);
			H->inject_key((int)key, false);
		}
		return tool_result_text("key event sent");
	}

	if (name == "emu_touch") {
		long long x = arg_n("x", -1);
		long long y = arg_n("y", -1);
		std::string action = arg_s("action");
		if (x < 0 || y < 0) return tool_result_text("error: x and y coordinates are required");
		if (!H || !H->inject_touch) return tool_result_text("error: touch injection not supported");

		if (action == "down" || action == "press") {
			H->inject_touch((int)x, (int)y, 1); // VM_PEN_EVENT_TAP
		} else if (action == "up" || action == "release") {
			H->inject_touch((int)x, (int)y, 2); // VM_PEN_EVENT_RELEASE
		} else if (action == "move") {
			H->inject_touch((int)x, (int)y, 3); // VM_PEN_EVENT_MOVE
		} else if (action == "long" || action == "long_tap") {
			H->inject_touch((int)x, (int)y, 4); // VM_PEN_EVENT_LONG_TAP
		} else if (action == "double" || action == "double_click") {
			H->inject_touch((int)x, (int)y, 5); // VM_PEN_EVENT_DOUBLE_CLICK
		} else {
			// default: click (tap then release)
			H->inject_touch((int)x, (int)y, 1);
			H->inject_touch((int)x, (int)y, 2);
		}
		return tool_result_text("touch event sent");
	}

	return "{\"content\":[{\"type\":\"text\",\"text\":" + jstr("error: unknown tool " + name) + "}],\"isError\":true}";
}

int serve(std::istream& in, std::ostream& out, Control& ctl) {
	g_ctx.ctl = &ctl;

	std::string line;
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		JParser p(line);
		auto req = p.parse();
		if (!req || req->kind != JValue::Obj) continue;

		std::string method = req->get_str("method");
		JPtr idv = req->find("id");
		// JSON-RPC ids are integers; read through a double here, so keep the exact
		// value rather than the double's std::to_string rendering ("1.000000").
		long long id_num = 0;
		bool has_id = false;
		if (idv && idv->kind == JValue::Num) {
			id_num = (long long)idv->num;
			has_id = true;
		}
		else if (idv && idv->kind == JValue::Str) {
			id_num = strtoll(idv->str.c_str(), nullptr, 10);
			has_id = true;
		}

		auto reply = [&](const std::string& result) {
			if (!has_id) return; // notification: no response
			out << "{\"jsonrpc\":\"2.0\",\"id\":" << std::to_string(id_num) << ",\"result\":" << result << "}\n";
			out.flush();
		};
		auto reply_err = [&](int code, const std::string& msg) {
			if (!has_id) return;
			out << "{\"jsonrpc\":\"2.0\",\"id\":" << std::to_string(id_num)
				<< ",\"error\":{\"code\":" << code << ",\"message\":" << jstr(msg) << "}}\n";
			out.flush();
		};

		if (method == "initialize") {
			reply("{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
				  "\"serverInfo\":{\"name\":\"mremu\",\"version\":\"1.0\"}}");
		}
		else if (method == "notifications/initialized" || method == "initialized") {
			// notification, nothing to reply to
		}
		else if (method == "ping") {
			reply("{}");
		}
		else if (method == "tools/list") {
			reply("{\"tools\":" + std::string(TOOLS) + "}");
		}
		else if (method == "tools/call") {
			auto params = req->find("params");
			std::string tname = params ? params->get_str("name") : "";
			auto targs = params ? params->find("arguments") : nullptr;
			try {
				reply(handle_tool(ctl, tname, targs));
			}
			catch (const std::exception& e) {
				reply_err(-32603, std::string("tool threw: ") + e.what());
			}
		}
		else {
			reply_err(-32601, "method not found: " + method);
		}
	}
	return 0;
}

} // namespace Mcp