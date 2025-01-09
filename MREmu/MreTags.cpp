#include "MreTags.h"
#include <cstring>

bool MreTags::load(std::vector<unsigned char>& file)
{
	raw_tags.clear();
	raw_tags.resize(0x34);

	size_t file_size = file.size();

	if (file_size < 4 * 3)
		return false;

	tags_offset = *(uint32_t*)&file[file_size - 4 * 3];

	uint32_t id, tag_size, pos = tags_offset;

	do {
		if (pos + 8 >= file_size)
			return false;

		id = *(uint32_t*)&file[pos];
		tag_size = *(uint32_t*)&file[pos + 4];
		pos += 8;

		if (pos + tag_size >= file_size)
			return false;

		//if (id >= raw_tags.size())
		//	raw_tags.resize(id + 1);

		raw_tags[id].resize(tag_size);
		if (tag_size)
			memcpy(raw_tags[id].data(), &file[pos], tag_size);

		pos += tag_size;
	} while (id);

	return true;
}

bool MreTags::is_ads()
{
	if (raw_tags[0x21].size() == 4) {
		int t = *(int*)raw_tags[0x21].data();
		return (t == 0 || t == 1 || t == 5);
	}
	return false;
}
bool MreTags::is_simple_ads()
{
	if (raw_tags[0x21].size() == 4) {
		int t = *(int*)raw_tags[0x21].data();
		return (t == 5);
	}
	return false;
}

bool MreTags::is_zipped()
{
	if (raw_tags[0x22].size() == 4)
		return *(int*)raw_tags[0x22].data();
	return false;
}

unsigned int MreTags::get_ram()
{
	if (raw_tags[0x0F].size() == 4)
		return *(unsigned int*)raw_tags[0x0F].data();
	return false;
}

