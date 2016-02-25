#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"


using BNJ::PullParser;

int main(int argc, const char* argv[]){

	try{
		/* Read json from std input. */
		FD_Reader reader(0);

		/* Deal with metadata depths up to 64 */
		uint32_t pstack[64];
		PullParser parser(64, pstack);

		uint8_t buffer[1024];
		parser.Begin(buffer, 1024, &reader);

		parser.Pull();
		BNJ::VerifyList(parser);

		unsigned count = 0;
		while(parser.Pull() != PullParser::ST_ASCEND_LIST){
			double val;
			BNJ::Get(val, parser);
			if(count == 4261)
				printf("BALDFAS");
			if(val >= 0)
				throw PullParser::invalid_value("Expected negative value!", parser);
			++count;
		}

		return 0;
	}
	catch(const std::exception& e){
		fprintf(stderr, "Runtime Error: %s\n", e.what());
	}

	return 1;
}
