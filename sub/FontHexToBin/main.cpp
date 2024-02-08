#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

unsigned char char_to_hbyte(char c) {
	if ('0' <= c && c <= '9')
		return c - '0';
	else if ('A' <= c && c <= 'F')
		return 10 + c - 'A';
	else
		abort();
}

unsigned char str_to_byte(const char* str) {
	return (char_to_hbyte(str[0]) << 4) | char_to_hbyte(str[1]);
}

string byte_to_stars(unsigned char c) {
	string str(8, ' ');
	for (int i = 0; i < 8; ++i)
		if ((c >> (7 - i)) & 1)
			str[i] = '*';
	return str;
}
int first_bite(unsigned char c, int def = 17) {
	for (int i = 0; i < 8; ++i)
		if ((c >> (7 - i)) & 1)
			return i;
	return def;
}
int last_bite(unsigned char c, int def = -1) {
	for (int i = 0; i < 8; ++i)
		if ((c >> i) & 1)
			return 7 - i;
	return def;
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		cout << "Wrong arguments\n";
		return 1;
	}

	fs::path path = argv[1];

	if (!fs::exists(path)) {
		cout << "Wrong file\n";
		return 1;
	}

	ifstream in(path);

	vector<pair<int, vector<unsigned char>>> arr;
	int arr_size = 0;

	while (true) {
		string line;
		getline(in, line);
		if (!line.length())
			break;
		//cout << line << "\n";
		if (line.length() < 5)
			abort();

		int code = (str_to_byte(&line[0]) << 8) | str_to_byte(&line[2]);

		string hexline = line.substr(5);
		int byte_for_line = hexline.size() / 16 / 2;

		vector<unsigned char> data(hexline.size() / 2 + 2);

		int first = 17;
		int last = -1;

		for (int i = 0; i < 16; ++i)
			for (int j = 0; j < byte_for_line; ++j) {
				unsigned char byte = str_to_byte(&hexline[(i * byte_for_line + j) * 2]);
				first = min(first, first_bite(byte) + j * 8);
				last = max(last, last_bite(byte) + j * 8);
				data[i * byte_for_line + j + 2] = byte;
			}

		if (first == 17)
			data[0] = byte_for_line * 8 - 1;
		else
			data[0] = (first << 4) | last;

		arr.push_back({ code, data });
		arr_size += data.size();

		/*cout << code << '\n';
		for (int i = 0; i < 16; ++i) {
			for (int j = 0; j < byte_for_line; ++j)
				cout << byte_to_stars(str_to_byte(&hexline[(i * byte_for_line + j) * 2]));
			cout << '\n';
		}*/
	}

	vector<unsigned char> final_data(0X10000 * 4 + arr_size, 0);
	int curr_offset = 0X10000 * 4;

	for (int i = 0; i < arr.size(); ++i) {
		((unsigned int*)final_data.data())[arr[i].first] = curr_offset;
		memcpy(final_data.data() + curr_offset, arr[i].second.data(), arr[i].second.size());
		curr_offset += arr[i].second.size();
	}

	fs::path out_path = path.stem();
	out_path += ".bin";

	ofstream out(out_path, ios_base::binary);
	if (!out.good())
		return 0;
	out.write((char*)final_data.data(), final_data.size());
	out.close();
}

