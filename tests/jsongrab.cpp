#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

/* Reduce typing when dealing with PullParser members.
 * Not doing full BNJ namespace since Get() conversion methods should
 * remain within namespace. */
using BNJ::PullParser;

void s_print_value(PullParser& data_parser){
	const bnj_val& v = data_parser.GetValue();
	unsigned type = bnj_val_type(&v);
	switch(type){
		case BNJ_NUMERIC:
			/* If nonzero exponent, then float. */
			if(v.exp_val){
				double d;
				BNJ::Get(d, data_parser);
				printf("%f", d);
			}
			else{
				if(v.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND){
					int d;
					BNJ::Get(d, data_parser);
					printf("%i", d);
				}
				else{
					unsigned d;
					BNJ::Get(d, data_parser);
					printf("%u", d);
				}
			}
			break;

		case BNJ_STRING:
			/* TODO loop to get entire string value..for now just output fixed chars. */
			{
				char buffer[1024];
				data_parser.ChunkRead8(buffer, 1024);
				printf("%s", buffer);
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
			throw PullParser::invalid_value("Expecting simple value!", data_parser);
	}
}

int main(int argc, const char* argv[]){

	if(argc < 2){
		fprintf(stderr, "Usage: %s JSON_PATH\n", argv[0]);
		return 1;
	}

	try{

		/* Read json from std input. */
		FD_Reader reader(0);

		/* TODO allocate more depth! */
		/* Deal with data depths up to 64 */
		uint32_t data_stack[64];
		uint8_t buffer[1024];
		PullParser data_parser(64, data_stack);
		data_parser.Begin(buffer, 1024, &reader);

		/* Begin parsing the path string; make sure we are in an array.*/
		uint32_t path_stack[3];
		PullParser path_parser(3, path_stack);
		int len = strlen(argv[1]);
		path_parser.Begin(reinterpret_cast<const uint8_t*>(argv[1]), len);
		if(PullParser::ST_LIST != path_parser.Pull())
			throw PullParser::invalid_value("Path must be an array!", path_parser);

		/* Get the first element. */
		data_parser.Pull();
		while(PullParser::ST_ASCEND_LIST != path_parser.Pull()){
			/* Expecting to descend into the data. */
			if(!data_parser.Descended())
				throw PullParser::invalid_value("Expecting a map or array in data!", data_parser);

			const bnj_val& v = path_parser.GetValue();
			unsigned type = bnj_val_type(&v);
			if(BNJ_NUMERIC == type){
				/* Data parser must be in an array. */
				if(data_parser.InMap())
					throw PullParser::invalid_value("Expected array in data!", data_parser);

				unsigned idx;
				BNJ::Get(idx, path_parser);

				/* Iterate until idx is reached. */
				unsigned count = 0;
				while(count < idx){
					if(PullParser::ST_ASCEND_LIST == data_parser.Pull())
						throw PullParser::invalid_value("Premature end of array!", data_parser);
					/* If descended, go back up. */
					if(data_parser.Descended())
						data_parser.Up();
					++count;
				}
				data_parser.Pull();
			}
			else if(BNJ_STRING == type){
				/* Data parser must be in a map. */
				if(!data_parser.InMap())
					throw PullParser::invalid_value("Expected map in data!", data_parser);

				/* Define keyset we are looking for (only the key at this level). */
				char keybuff[512];
				path_parser.ChunkRead8(keybuff, 512);
				const char* keys[] = {
					keybuff
				};

				/* Iterate until key is matched. */
				while(true){
					if(data_parser.Pull(keys, 1) == PullParser::ST_ASCEND_MAP)
						throw PullParser::invalid_value("Expected key in data!", data_parser);
					if(0 == data_parser.GetValue().key_enum)
						break;
					if(data_parser.Descended())
						data_parser.Up();
				}
			}
			else
				throw PullParser::invalid_value("Element must be string or integer!", path_parser);
		}

		s_print_value(data_parser);
		return 0;
	}
	catch(const std::exception& e){
		fprintf(stderr, "%s\n", e.what());
		return 1;
	}
}
