#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

void OutputValue(BNJ::PullParser& parser){
	const bnj_val& val = parser.GetValue();

	if(val.key_length){
		char key[512];
		BNJ::GetKey(key, 1024, parser);
		fprintf(stdout, "key: '%s'\n", key);
	}

	switch(bnj_val_type(&val)){
		case BNJ_NUMERIC:
			if(val.exp_val){
				double f;
				BNJ::Get(f, parser);
				fprintf(stdout, "double: %g\n", f);
			}
			else{
				if(val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND){
					int v;
					BNJ::Get(v, parser);
					fprintf(stdout, "integer: %d\n", v);
				}
				else{
					unsigned v;
					BNJ::Get(v, parser);
					fprintf(stdout, "unsigned: %d\n", v);
				}
			}
			break;

		case BNJ_SPECIAL:
			switch(val.significand_val){
				case BNJ_SPC_FALSE:
					fprintf(stdout, "bool: false\n");
					break;
				case BNJ_SPC_TRUE:
					fprintf(stdout, "bool: true\n");
					break;
				case BNJ_SPC_NULL:
					fprintf(stdout, "null\n");
					break;
				case BNJ_SPC_NAN:
					fprintf(stdout, "float: NaN\n");
					break;
				case BNJ_SPC_INFINITY:
					{
						char c = (val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? '-' : ' ';
						fprintf(stdout, "float: %cInfinity\n", c);
					}
					break;
				default:
					break;
			}
			break;

		case BNJ_ARR_BEGIN:
			break;

		case BNJ_OBJ_BEGIN:
			break;

		case BNJ_STRING:
			{
				/* Can read the length of the string fragment before consuming
				unsigned outlen = bnj_strlen8(&parser.GetValue());
				unsigned outlen = bnj_strlen16(&parser.GetValue());
				unsigned outlen = bnj_strlen32&parser.GetValue());
				*/

				unsigned len;
				char buffer[1024];

				/* Consume/Write loop */
				fprintf(stdout, "string: '");
				while((len = parser.Consume8(buffer, 1024)))
					fwrite(buffer, 1, len, stdout);
				fprintf(stdout, "'\n");
			}
			break;

		default:
			break;
	}
}

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

	int ret = 0;
	try{
		/* Begin a tree walk. */
		bool running = true;
		while(running){
			switch(parser.Pull()){
				/* Stop when there is no more data. */
				case BNJ::PullParser::ST_NO_DATA:
					running = false;
					break;

				/* Parser pulled a datum. */
				case BNJ::PullParser::ST_DATUM:
					OutputValue(parser);
					break;

				case BNJ::PullParser::ST_MAP:
    			fprintf(stdout, "map open '{'\n");
					break;

				case BNJ::PullParser::ST_LIST:
    			fprintf(stdout, "array open '['\n");
					break;

				case BNJ::PullParser::ST_ASCEND_MAP:
					fprintf(stdout, "map close '}'\n");
					break;

				case BNJ::PullParser::ST_ASCEND_LIST:
					fprintf(stdout, "array close ']'\n");
					break;

				default:
					break;
			}
		}

	}
	catch(const std::exception& e){
		fprintf(stderr, "Runtime Error: %s\n", e.what());
		ret = 1;
	}
	delete [] buffer;
	delete [] pstack;

	return ret;
}
