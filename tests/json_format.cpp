#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

/* Reduce typing when dealing with PullParser members.
 * Not doing full BNJ namespace since Get() conversion methods should
 * remain within namespace. */
using BNJ::PullParser;

#define PREFIX_SIZE 128

char s_prefix[PREFIX_SIZE  + 1];

void print_json_string(const char* str){
	printf("%s",str);
}

void s_print_value(PullParser& parser, const char* prefix){
	if(parser.Descended()){
		unsigned count = 0;
		if(parser.InMap()){
			printf("{\n");
			while(parser.Pull() != PullParser::ST_ASCEND_MAP){
				const char* p = prefix - 1;

				char key[1024];
				BNJ::GetKey(key, 1024, parser);
				printf("%s", p);
				if(count)
					printf(",");
				printf("\"");
				print_json_string(key);
				printf("\":");
				s_print_value(parser, p);
				printf("\n");
				++count;
			}
			printf("%s}", prefix);
		}
		else{
			printf("[\n");
			while(parser.Pull() != PullParser::ST_ASCEND_LIST){
				const char* p = prefix - 1;
				printf("%s", p);
				if(count)
					printf(",");
				s_print_value(parser, prefix - 1);
				printf("\n");
				++count;
			}
			printf("%s]", prefix);
		}
	}
	else{
		const bnj_val& v = parser.GetValue();
		unsigned type = bnj_val_type(&v);
		switch(type){
			case BNJ_NUMERIC:
				/* If nonzero exponent, then float. */
				if(v.exp_val){
					double d;
					BNJ::Get(d, parser);
					printf("%lf", d);
				}
				else{
					if(v.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND){
						int d;
						BNJ::Get(d, parser);
						printf("%i", d);
					}
					else{
						unsigned d;
						BNJ::Get(d, parser);
						printf("%u", d);
					}
				}
				break;

			case BNJ_STRING:
				/* TODO loop to get entire string value..for now just output fixed chars. */
				{
					char buffer[1024];
					unsigned len;
					printf("\"");
					while(parser.ChunkRead8(buffer, 1024))
						print_json_string(buffer);
					printf("\"");
				}
				break;

			case BNJ_SPECIAL:
				{
					static const char* special[BNJ_COUNT_SPC] = {
						"false",
						"true",
						"null",
						"NaN",
						"Infinity"
					};
					unsigned idx = bnj_val_special(&v);
					if(BNJ_SPC_INFINITY == idx && (v.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND)){
						printf("-Infinity");
					}
					else{
						printf("%s", special[idx]);
					}
				}
				break;

			default:
				throw PullParser::invalid_value("Expecting simple value!", parser);
		}
	}
}

int main(int argc, const char* argv[]){

	try{

		/* Read json from std input. */
		FD_Reader reader(0);

		memset(s_prefix, '\t', sizeof(s_prefix));
		s_prefix[PREFIX_SIZE] = '\0';

		/* TODO allocate more depth! */
		/* Deal with data depths up to 64 */
		uint32_t data_stack[PREFIX_SIZE];
		uint8_t buffer[1024];
		PullParser parser(PREFIX_SIZE, data_stack);
		parser.Begin(buffer, 1024, &reader);

		parser.Pull();
		s_print_value(parser, s_prefix + PREFIX_SIZE);
		return 0;
	}
	catch(const std::exception& e){
		fflush(stdout);
		fprintf(stderr, "%s\n", e.what());
		return 1;
	}
}
