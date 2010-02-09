#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <benejson/pull.hh>
#include "posix.hh"

/* Reduce typing when dealing with PullParser members.
 * Not doing full BNJ namespace since Get() conversion methods should
 * remain within namespace. */
using BNJ::PullParser;

/* Key definitions. BeneJSON can eagerly match map keys against
 * lexicographically sorted arrays of UTF-8 character bytes.
 * 
 * Use plain old strings for keys!
 * TODO: Presently keys are not decoded from JSON string encodings,
 * so the key string in the JSON file must match byte-for-byte the
 * keys listed here.
 * */

/* Top level keys. */
enum {
	KEY_BODY,
	KEY_HEADER,
	KEY_METADATA,
	COUNT_TOP_KEYS
};
const char* top_keys[COUNT_TOP_KEYS] = {
	"body",
	"header",
	"metadata",
};

/* Metadata spam keys. */
enum {
	KEY_SPAM_AGENT,
	KEY_SPAM_SCORE,
	COUNT_SPAM_KEYS
};
const char* spam_keys[COUNT_SPAM_KEYS] = {
	"spam_agent",
	"spam_score",
};

/* Header keys. */
enum {
	KEY_CC,
	KEY_FROM,
	KEY_RECV_DATE,
	KEY_SUBJECT,
	KEY_TO,
	COUNT_HEADER_KEYS
};
const char* header_keys[COUNT_HEADER_KEYS] = {
	"cc",
	"from",
	"recv_date",
	"subject",
	"to",
};

/* Date keys. */
enum {
	KEY_DAY,
	KEY_HOUR,
	KEY_MINUTE,
	KEY_MONTH,
	KEY_YEAR,
	COUNT_DATE_KEYS
};
const char* date_keys[COUNT_DATE_KEYS] = {
	"day",
	"hour",
	"minute",
	"month",
	"year",
};

/* Prototypes for helper functions in main(). */

/* Expect parser to have entered "header":{} map. */
void OnHeader(PullParser& parser);

/* Expect parser to have entered "meta":{} map. */
void SpamMeta(PullParser& parser);

/* Expect parser to be at "body":"" string. */
void OutputBody(PullParser& parser);

int main(int argc, const char* argv[]){

	if(argc < 2){
		fprintf(stderr, "Usage: %s buffer_size\n", argv[0]);
		return 1;
	}

	/* Read arguments. */
	char* endptr;
	unsigned buffsize = strtol(argv[1], &endptr, 10);
	int ret = 0;

	/* Read json from std input. */
	FD_Reader reader(0);

	/* Deal with metadata depths up to 64 */
	uint32_t pstack[64];
	PullParser parser(64, pstack);

	/* Begin parsing session. buffer is I/O scratch space. */
	uint8_t *buffer = new uint8_t[buffsize];
	parser.Begin(buffer, buffsize, &reader);

	try{
		/* Expect top level map. Not matching against any map keys. */
		parser.Pull();
		BNJ::VerifyMap(parser);

		/* Pull top level fields. Note this loop just identifies keys
		 * and delegates responsibility to subprocedures.
		 * The subprocedures verify and read their subcontexts.
		 *
		 * Breakdown of while/switch map idiom:
		 *
		 * ' parser.Pull(top_keys, COUNT_TOP_KEYS) '
		 *  -Pull next value and match against it the top level keys.
		 *  -The key array and its length are specified here.
		 *
		 * ' != PullParser::ST_ASCEND_MAP '
		 *  -Loop as long as parser does not ascend from map.
		 *  -While the condition is false, parser is reading data within
		 *   the map context or descending into a new context.
		 *
		 * ' switch(parser.GetValue().key_enum) '
		 *  -The value inside the switch statement is the result of the match.
		 *   .key_enum is the index into top_keys[]
		 *
		 * */
		while(parser.Pull(top_keys, COUNT_TOP_KEYS) != PullParser::ST_ASCEND_MAP){
			switch(parser.GetValue().key_enum){

				/* Print body text at most 80 characters per row. */
				case KEY_BODY:
					fprintf(stdout, "BEGIN BODY, formatted for 80 chars:\n\n");
					OutputBody(parser);
					fprintf(stdout, "\nEND MESSAGE.\n");
					break;

				/* Print header information. */
				case KEY_HEADER:
					fprintf(stdout, "BEGIN HEADER:\n\n");
					OnHeader(parser);
					fprintf(stdout, "END HEADER.\n\n");
					break;

				/* Just looking to print spam related metadata here. */
				case KEY_METADATA:
					fprintf(stdout, "BEGIN SPAM METADATA:\n\n");
					SpamMeta(parser);
					fprintf(stdout, "END SPAM METADATA.\n\n");
					break;

				/* Only accepting top level keys. */
				default:
					throw std::runtime_error("Unknown top level field!");
			}
		}
	}
	catch(const std::exception& e){
		fprintf(stderr, "Runtime Error: %s\n", e.what());
		ret = 1;
	}

	/* Be nice and clean up. */
	delete [] buffer;
	return ret;
}

void OnHeader(PullParser& parser){
	/* As per spec, verify parser entered a map. */
	BNJ::VerifyMap(parser);

	/* Loop over all metadata key:values. */
	while(parser.Pull(header_keys, COUNT_HEADER_KEYS)
		!= PullParser::ST_ASCEND_MAP)
	{
		unsigned key = parser.GetValue().key_enum;
		switch(key){

			/* Just print these as one liners. */
			case KEY_FROM:
			case KEY_TO:
			case KEY_SUBJECT:
				{
					/* Just print first 80 chars. */
					char buffer[80];
					parser.ChunkRead8(buffer, 80);
					fprintf(stdout, "%s: %s\n", header_keys[key], buffer);
				}
				break;

			/* Make sure we are in a map. Expect to see 5 integers. */
			case KEY_RECV_DATE:
				BNJ::VerifyMap(parser);
				{
					/* Store date as five integers, wrt to their enum defs.
					 * Initializing to 9999 to detect malicious duplicate fields.
					 * This is just to illustrate one method dupe detection. */
					unsigned date[5] = {9999,9999,9999,9999,9999};
					for(unsigned i = 0; i < 5; ++i){

						/* Pull the date value. */
						parser.Pull(date_keys, COUNT_DATE_KEYS);

						/* Verify valid key. */
						unsigned dkey = parser.GetValue().key_enum;
						if(dkey >= COUNT_DATE_KEYS)
							throw std::runtime_error("Invalid date key!");

						/* If already set, then certainly not 9999. */
						if(date[dkey] != 9999)
							throw std::runtime_error("Duplicate date key!");

						/* Store unsigned integer value in slot indexed by key. */
						Get(date[dkey], parser);
					}

					/* Map MUST end after 5 entries. */
					if(parser.Pull() != PullParser::ST_ASCEND_MAP)
						throw std::runtime_error("Too many fields in date!");

					/* Print the data. YEAR-MONTH-DAY HOUR:MINUTE*/
					fprintf(stdout, "date: %d-%02d-%02d %02d:%02d\n",
						date[KEY_YEAR], date[KEY_MONTH], date[KEY_DAY],
						date[KEY_HOUR], date[KEY_MINUTE]);
				}
				break;

			/* Verify in a list, then print list of email addresses: */
			case KEY_CC:
				BNJ::VerifyList(parser);
				fprintf(stdout, "cc:[\n");

				/* Loop and print addresses as one liners: */
				while(parser.Pull() != PullParser::ST_ASCEND_LIST){
					char buffer[80];
					parser.ChunkRead8(buffer, 80);
					fprintf(stdout, "  %s\n", buffer);
				}
				fprintf(stdout, "]\n");
				break;


			/* Any header data we don't know about is bad news.. */
			default:
				throw std::runtime_error("Unrecognized header key!");
		}
	}
}

void SpamMeta(PullParser& parser){
	/* As per spec, verify in a map. */
	BNJ::VerifyMap(parser);

	/* Loop over all metadata key:values. */
	while(parser.Pull(spam_keys, COUNT_SPAM_KEYS) != PullParser::ST_ASCEND_MAP){
		/* Key enums recognized if value is not 'out of bounds' */
		switch(parser.GetValue().key_enum){

			/* Print agent. Ad hoc decision to only print first 64 chars of the name.
			 * Sometimes we don't really care about reading the entire string,
			 * which is why ChunkRead8() is not wrapped with a while loop. */
			case KEY_SPAM_AGENT:
				{
					char buffer[65];
					parser.ChunkRead8(buffer, 65);
					fprintf(stdout, "Agent: %s\n", buffer);
				}
				break;

			/* Print spam score. */
			case KEY_SPAM_SCORE:
				{
					float score;
					BNJ::Get(score, parser);
					fprintf(stdout, "Score: %f\n", score);
				}
				break;

			/* Unrecognized value. Skip it. */
			default:
				/* If parser entered a map or list, leave it. */
				if(parser.Descended())
					parser.Up();
		}
	}
}

void OutputBody(PullParser& parser){
	/* Output buffer holds at most 80 UTF-8 bytes + newline. */
	char buffer[81];
	while(true){
		/* Read a chunk of UTF-8 text. If length zero, no more data. */
		unsigned len = parser.ChunkRead8(buffer, 81);
		if(!len)
			break;

		/* Replace null terminator with '\n'.  */
		buffer[len] = '\n';

		/* Make sure we print trailing '\n'. */
		++len;
		fwrite(buffer, 1, len, stdout);
	}
}
