#include "GDB.h"

#include <SFML/Network.hpp>
#include <unicorn/unicorn.h>
#include "AppManager.h"

#include <iostream>
#include <string_view>

using namespace std::string_literals;

extern uc_engine* uc;
extern AppManager* g_appManager;

int cpu_state = 2;

namespace GDB {
	sf::TcpListener tcpListner;
	sf::TcpSocket sock;

	const uint32_t inbuf_size = 8 * 1024 * 1024;
	uint8_t inbuf[inbuf_size];
	uint32_t inbuf_pos = 0;


	const uint32_t outbuf_size = 8 * 1024 * 1024;
	uint8_t outbuf[outbuf_size];
	uint32_t outbuf_pos = 0;

	bool process_input();

	void wait() {
		tcpListner.listen(12342);
		tcpListner.accept(sock);
		sock.setBlocking(false);
	}

	void update() {
		size_t received = 0;
		do {
			received = 0;
			sock.receive(inbuf + inbuf_pos, inbuf_size - inbuf_pos, received);
			inbuf_pos += received;

			while (process_input());

			if (outbuf_pos) {
				size_t sended = 0;
				sock.send(outbuf, outbuf_pos, sended);
				if (sended != outbuf_pos) {
					memmove(outbuf, outbuf + sended, outbuf_pos - sended);
				}
				outbuf_pos -= sended;
			}
		} while (received);
	}

	void put_to_out(const uint8_t* data, uint32_t size) {
		if (size == 0)
			return;
		memcpy(outbuf + outbuf_pos, data, size);
		outbuf_pos += size;
	}

	void make_answer(const uint8_t* data, uint32_t size) {
		uint32_t checksum = 0;
		for (int i = 0; i < size; ++i)
			checksum = (checksum + data[i]) % 256;
		char checksum_str[20];
		sprintf(checksum_str, "#%02hhx", (uint8_t)checksum);

		put_to_out((const uint8_t*)"$", 1);
		put_to_out(data, size);
		put_to_out((const uint8_t*)checksum_str, 3);
	}

	void make_answer(std::string_view str) {
		make_answer((const uint8_t*)str.data(), str.size());
	}

	uint32_t byteswap(uint32_t val) {
		uint32_t res = 0;
		for (int i = 0; i < 4; ++i) {
			res = (res << 8) | (val & 0xFF);
			val = (val >> 8);
		}
		return res;
	}

	struct cpu_reg {
		int uc_reg = UC_ARM_REG_INVALID, size = 4;
	};

	const cpu_reg cpu_reg_list[17] =//26
	{
		{UC_ARM_REG_R0},
		{UC_ARM_REG_R1},
		{UC_ARM_REG_R2},
		{UC_ARM_REG_R3},
		{UC_ARM_REG_R4},
		{UC_ARM_REG_R5},
		{UC_ARM_REG_R6},
		{UC_ARM_REG_R7},
		{UC_ARM_REG_R8},
		{UC_ARM_REG_R9},
		{UC_ARM_REG_R10},
		{UC_ARM_REG_R11},
		{UC_ARM_REG_R12},
		{UC_ARM_REG_SP},
		{UC_ARM_REG_LR},
		{UC_ARM_REG_PC},
		//{UC_ARM_REG_INVALID, 12}, //f0
		//{UC_ARM_REG_INVALID, 12}, //f1
		//{UC_ARM_REG_INVALID, 12}, //f2
		//{UC_ARM_REG_INVALID, 12}, //f3
		//{UC_ARM_REG_INVALID, 12}, //f4
		//{UC_ARM_REG_INVALID, 12}, //f5
		//{UC_ARM_REG_INVALID, 12}, //f6
		//{UC_ARM_REG_INVALID, 12}, //f7
		//{UC_ARM_REG_INVALID, 4}, //fps
		{UC_ARM_REG_CPSR}, //cprs
	};

	std::string get_reg_hex(int id) {
		if (id < 0 || id >= 26)
			return "";

		cpu_reg reg_info = cpu_reg_list[id];

		if (reg_info.uc_reg == UC_ARM_REG_INVALID)
			return std::string(reg_info.size * 2, 'x');

		uint32_t reg_value = 0;
		uc_reg_read(uc, reg_info.uc_reg, &reg_value);

		uint32_t rreg_value = byteswap(reg_value);

		char str[64];
		sprintf(str, "%08x", rreg_value);

		return str;
	}

	std::string data_to_hex(uint8_t* data, int size) {
		std::string hex_data(size * 2, ' ');

		for (int i = 0; i < size; ++i)
			sprintf(hex_data.data() + i * 2, "%02hhx", data[i]);

		return hex_data;
	}

	std::string data_to_newdata(uint8_t* data, int size) {
		std::vector<uint8_t> ecdata;
		ecdata.reserve(size);

		for (int i = 0; i < size; ++i)
			if (data[i] == 0x23 || data[i] == 0x24 || data[i] == 0x2a || data[i] == 0x7d) {
				ecdata.push_back(0x7d);
				ecdata.push_back(data[i] ^ 0x20);
			}
			else
				ecdata.push_back(data[i]);

		return std::string((char*)ecdata.data(), ecdata.size());
	}

	void process_g(std::string_view name) {
		std::string regs_str;

		for (int i = 0; i < 17; ++i)
			regs_str.append(get_reg_hex(i));

		make_answer(regs_str);
	}

	void process_p(std::string_view name) {
		uint32_t id = 0;

		sscanf(name.data(), "p%x", &id);

		std::string regs_str = get_reg_hex(id);

		make_answer(regs_str);
	}

	void process_P(std::string_view name) {
		uint32_t id = 0, val = 0;

		sscanf(name.data(), "P%x=%x", &id, &val);

		cpu_reg reg_info = cpu_reg_list[id];

		if (reg_info.uc_reg == UC_ARM_REG_INVALID)
			return make_answer("");

		uint32_t rreg_value = byteswap(val);

		uc_reg_write(uc, reg_info.uc_reg, &rreg_value);

		make_answer("OK");
	}

	std::map<uint32_t, uc_hook> hooks;

	void hook_bracepoint(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
		uc_emu_stop(uc);

		cpu_state = 0;

		make_answer("S05");
	}

	void process_Z(std::string_view name) {
		uint32_t type, addr, kind;

		int c = sscanf(name.data(), "Z%x,%x,%x", &type, &addr, &kind);

		if(c!=3)
			return make_answer("");

		uc_hook uc_hu;
		uc_hook_add(uc, &uc_hu, UC_HOOK_CODE, hook_bracepoint, 0, addr, addr+1);

		hooks[addr] = uc_hu;

		make_answer("OK");
	}

	void process_z(std::string_view name) {
		uint32_t type, addr, kind;

		int c = sscanf(name.data(), "z%x,%x,%x", &type, &addr, &kind);

		if (c != 3)
			return make_answer("");

		if(!hooks.at(addr))
			return make_answer("");

		uc_hook_del(uc, hooks[addr]);

		hooks.erase(addr);

		make_answer("OK");
	}

	void process_qSupported(std::string_view parameters) {
		make_answer("PacketSize=47ff;qXfer:auxv:read+;exec-events+;QDisableRandomization-;BreakpointCommands+;swbreak+;hwbreak+;qXfer:exec-file:read+;vContSupported+;no-resumed+;QNonStop+");
	}

	void process_qAttached(std::string_view parameters) {
		make_answer("OK");
	}

	void process_qfThreadInfo(std::string_view parameters) {
		make_answer("m1");
	}

	void process_qsThreadInfo(std::string_view parameters) {
		make_answer("l");
	}

	void process_qC(std::string_view parameters) {
		make_answer("QC1");
	}

	void process_qXfer_exec_file(std::string_view parameters) {
		auto app = g_appManager->get_active_app();
		if (!app)
			return;

		auto path = app->real_path.string();

		make_answer("l" + path);
	}

	std::map<std::string, void(*)(std::string_view parameters)> packets_qXfer_callbacks =
	{
		{"exec-file", process_qXfer_exec_file}
	};

	void process_qXfer(std::string_view parameters_in) {
		size_t colon_pos = parameters_in.find(':');

		std::string_view name;
		std::string_view parameters;

		if (colon_pos != parameters_in.npos) {
			name = parameters_in.substr(0, colon_pos);
			parameters = parameters_in.substr(colon_pos + 1);
		}
		else {
			name = parameters_in;
			parameters = "";
		}

		auto it = packets_qXfer_callbacks.find(std::string(name));
		if (it == packets_qXfer_callbacks.end())
			make_answer("");
		else
			it->second(parameters);
	}


	std::map<std::string, void(*)(std::string_view parameters)> packets_q_callbacks =
	{
		{"qSupported", process_qSupported},
		{"qAttached", process_qAttached},
		{"qfThreadInfo", process_qfThreadInfo},
		{"qsThreadInfo", process_qsThreadInfo},
		{"qC", process_qC},
		{"qXfer", process_qXfer},
	};

	void process_q(std::string_view packet) {
		size_t colon_pos = packet.find(':');

		std::string_view name;
		std::string_view parameters;

		if (colon_pos != packet.npos) {
			name = packet.substr(0, colon_pos);
			parameters = packet.substr(colon_pos + 1);
		}
		else {
			name = packet;
			parameters = "";
		}

		auto it = packets_q_callbacks.find(std::string(name));
		if (it == packets_q_callbacks.end())
			make_answer("");
		else
			it->second(parameters);
	}

	void process_vRun(std::string_view parameters) {
		size_t path_hex_size = std::min(parameters.size(), parameters.find(';'));

		int path_size = path_hex_size / 2;
		std::string path(path_size, ' ');

		for (int i = 0; i < path_size; ++i)
			if (sscanf(parameters.data() + i * 2, "%02hhx", path.data() + i) != 1) {
				make_answer("");
				return;
			}

		std::cout << "Path: " << path << '\n';

		g_appManager->add_app_for_launch(path, 0);
		make_answer("S00");
	}

	void process_vCont_request(std::string_view parameters) {
		make_answer("vCont;s;c;r;C");
	}

	void process_vCont(std::string_view parameters) {
		if (parameters == "s"s) {
			cpu_state = 1;
			make_answer("S05");
		} else if (parameters == "c"s) {
			cpu_state = 2;
		}
	}

	void process_vFile_open(std::string_view parameters) {
		make_answer("F1");
	}

	void process_vFile_pread(std::string_view parameters) {
		uint32_t fd, count, offset;

		int c = sscanf(parameters.data(), "%x,%x,%x", &fd, &count, &offset);
		if (c != 3)
			return;

		auto app = g_appManager->get_active_app();
		if (!app)
			return;

		auto& context = app->file_context;

		if (offset + count >= context.size()) {
			make_answer("F0;");
			return;
		}

		//auto hex_data = data_to_hex(context.data() + offset, count);
		auto hex_data = data_to_newdata(context.data() + offset, count);

		char tmp[100] = "";

		sprintf(tmp, "F%x;", count);

		make_answer(tmp + hex_data);
	}

	void process_vFile_close(std::string_view parameters) {
		make_answer("F0");
	}

	std::map<std::string, void(*)(std::string_view parameters)> packets_vFile_callbacks =
	{
		{"open", process_vFile_open},
		{"pread", process_vFile_pread},
		{"close", process_vFile_close},
	};

	void process_vFile(std::string_view parameters_in) {
		size_t colon_pos = parameters_in.find(':');

		std::string_view name;
		std::string_view parameters;

		if (colon_pos != parameters_in.npos) {
			name = parameters_in.substr(0, colon_pos);
			parameters = parameters_in.substr(colon_pos + 1);
		}
		else {
			name = parameters_in;
			parameters = "";
		}

		auto it = packets_vFile_callbacks.find(std::string(name));
		if (it == packets_vFile_callbacks.end())
			make_answer("");
		else
			it->second(parameters);
	}

	std::map<std::string, void(*)(std::string_view parameters)> packets_v_callbacks =
	{
		{"vRun", process_vRun},
		{"vCont?", process_vCont_request},
		{"vCont", process_vCont},
		{"vFile", process_vFile},
	};

	void process_v(std::string_view packet) {
		size_t colon_pos = packet.find(':');
		size_t semicolon_pos = packet.find(';');

		size_t name_size = std::min(std::min(colon_pos, semicolon_pos), packet.length());

		std::string_view name = packet.substr(0, name_size);
		std::string_view parameters = name_size < packet.length()
			? packet.substr(name_size + 1) : "";

		auto it = packets_v_callbacks.find(std::string(name));
		if (it == packets_v_callbacks.end())
			make_answer("");
		else
			it->second(parameters);
	}

	void process_m(std::string_view name) {
		uint32_t addr = 0, size = 0;

		int c = sscanf(name.data() + 1, "%x,%x", &addr, &size);
		if (c != 2) {
			make_answer("");
			return;
		}

		std::vector<uint8_t> data(size);

		uc_err ret = uc_mem_read(uc, addr, data.data(), size);

		if (ret != uc_err::UC_ERR_OK) {
			make_answer("");
			return;
		}

		make_answer(data_to_hex(data.data(), size));
	}


	void process_packet(std::string_view packet) {
		if (packet.length() == 0)
			return;

		std::cout << "GDB: " << packet << '\n';

		switch (packet[0]) {
		case 'q':
			process_q(packet);
			break;
		case 'v':
			process_v(packet);
			break;
		case '?':
			make_answer("OK");
			break;
		case 'g':
			process_g(packet);
			break;
		case 'p':
			process_p(packet);
			break;
		case 'P':
			process_P(packet);
			break;
		case 'm':
			process_m(packet);
			break;
		case 'H':
			make_answer("E01");
			break;
		case 'Z':
			process_Z(packet);
			break;
		case 'z':
			process_z(packet);
			break;
		case 'k':
			//exit(0);//TODO
			break;
		case '!':
			make_answer("OK");
			break;
		default:
			std::cout << "GDB: Unknow " << packet << '\n';
			make_answer("");
			break;
		}
	}

	int check_packet(const uint8_t* data, uint32_t size) {
		int sharp_pos = -1;
		uint32_t c_checksum = 0;

		for (int i = 1; i < size; ++i)
			if (inbuf[i] == '#') {
				sharp_pos = i;
				break;
			}
			else
				c_checksum = (c_checksum + inbuf[i]) % 256;

		if (sharp_pos == -1 || sharp_pos + 2 >= size)
			return 0;

		{
			uint8_t checksum = 0;
			int scanf_ret = sscanf((char*)inbuf + sharp_pos + 1, "%02hhx", &checksum);
			if (scanf_ret != 1 || checksum != c_checksum) {
				printf("GDB: Fatal\n");
				return 0;
			}
		}
		put_to_out((const uint8_t*)"+", 1);

		process_packet(std::string_view((char*)inbuf + 1, sharp_pos - 1));

		return sharp_pos + 3;
	}

	bool process_input() {
		if (inbuf_pos == 0)
			return false;

		int packet_len = 0;

		switch (inbuf[0]) {
		case '+':
			packet_len = 1;
			break;
		case '-':
			packet_len = 1;
			printf("GDB: Some went wrong\n");
			break;
		case '$':
			packet_len = check_packet(inbuf, inbuf_pos);
			break;
		default:
			printf("GDB: Unknown packet %c\n", inbuf[0]);
			break;

		}

		memmove(inbuf, inbuf + packet_len, inbuf_pos - packet_len);
		inbuf_pos -= packet_len;

		return packet_len != 0;
	}
}