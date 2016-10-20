#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

using BNJ::PullParser;

int main(int argc, const char* argv[]){
	/* Read json from std input. */
	uint32_t pstack[16];
	BNJ::PullParser parser(16, pstack);

	const char s_test_map[] = "{"
		"\"key1\":30"
		",\"key2\":12"
	"}";

	parser.Begin((const uint8_t*)s_test_map, sizeof(s_test_map)  - 1);

	try{

		unsigned multiple_of_10;
		unsigned lazy;

		/* Declare our assignment map. */
		const std::array<BNJ::KeyValue, 2> get_arr = {
			"key1", [&multiple_of_10](PullParser& parser)
			{
				BNJ::Get(multiple_of_10, parser);
				if(multiple_of_10 % 10)
					throw BNJ::PullParser::invalid_value("Expected multiple of 10!", parser);
			}

			,"key2", BNJ::KeyGetter(lazy)
		};

		assert(BNJ::CheckGetMap(get_arr));

		parser.Pull();
		BNJ::GetMap(get_arr, BNJ::Skip, parser);

		printf("%u %u\n", multiple_of_10, lazy);
	}
	catch(const std::exception& e){
		fprintf(stderr, "Error at %s\n", e.what());
		return 1;
	}

	return 0;
}
