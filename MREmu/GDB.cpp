#include "GDB.h"

#include <SFML/Network.hpp>
#include <unicorn/unicorn.h>
#include "AppManager.h"

#include <iostream>
#include <string_view>

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

	void process_packet(std::string_view packet) {
		if (packet.length() == 0)
			return;

		std::cout << "GDB: " << packet << '\n';
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
				printf("Fatal\n");
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