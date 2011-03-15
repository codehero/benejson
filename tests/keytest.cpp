#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

using BNJ::PullParser;

int main(int argc, const char* argv[]){
	if(argc < 3){
		fprintf(stderr, "Usage: %s buffer_size stack_depth\n", argv[0]);
		return 1;
	}

	/* Read arguments. */
	char* endptr;
	unsigned buffsize = strtol(argv[1], &endptr, 10);
	unsigned depth = strtol(argv[2], &endptr, 10);

	/* Read json from std input. */
	FD_Reader reader(0);
	uint32_t *pstack = new uint32_t[depth];
	BNJ::PullParser parser(depth, pstack);

	/* Initialize parsing session. buffer is I/O scratch space. */
	uint8_t *buffer = new uint8_t[buffsize];
	parser.Begin(buffer, buffsize, &reader);

	try{
		/* Verify top level map. */
		parser.Pull();
		BNJ::VerifyMap(parser);

		while(parser.Pull() != PullParser::ST_ASCEND_MAP){
			char b2[512];
			/* Key identifies the string index. */
			const bnj_val& v = parser.GetValue();
			bnj_stpkeycpy(b2, &v, parser.Buff());
			fprintf(stderr, "KEY %s\n", b2);

			/* Must be an unsigned integer. */
			unsigned value = strtol(b2, &endptr, 10);
			if(*endptr)
				throw BNJ::PullParser::invalid_value("Parser error", parser);

			/* Have to pull entire value before we know key. */
			parser.ChunkRead8(b2, 512);
		}
	}
	catch(const std::exception& e){
		fprintf(stderr, "Error at %s\n", e.what());
		return 1;
	}

	return 0;
}
