#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

#include <benejson/benejson.h>

#define MAX_STRING_LENGTH 4096

/* Class for efficiently allocating aligned string data. */
typedef struct string_allocator_s {
	/* Where current string begins. */
	unsigned cur_begin;

	/* Current write pointer. */
	unsigned cur_offset;

	/* How long first buffer is. Each buffer is twice the length of the previous. */
	unsigned buffer_length;

	/* How many buffers allocated. */
	unsigned buffer_count;

	/* List of buffers. Note that capacity increases exponentially with idx. */
	uint8_t* buffers[32];
} string_allocator;


/* Different types of output nodes. */
enum {
	/* Leaf node outputing static string. */
	OUTPUT_STRING,

	/* Leaf node outputing single rule. */
	OUTPUT_RULE,

	/* List of output nodes. */
	OUTPUT_LIST
};

typedef struct output_node_s {
	/* Node type. */
	unsigned type;

	/* If String, then string length.
	 * If list, then number of output node pointers. */
	unsigned len;

	/* Pointer to node data. */
	const void* data;
} output_node;

enum {
	/* Match specific enum in key table. */
	RULE_STRING,

	/* Match filename in filename index. */
	RULE_FILENAME,

	/* Match Nth file. */
	RULE_FILE_INDEX,

	/* Match Nth item in array. */
	RULE_LIST_INDEX
};

typedef struct rule_s {
	/* What kind of rule. */
	unsigned type;

	/* Against which key enum or array index to match. */
	unsigned index;
} rule;

typedef struct transformer_s {
	/* Root of output tree. */
	output_node* root;

	/* Array of filenames to match against. */
	uint8_t const * const * filenames;

	/* Length of rule_keys */
	unsigned filename_lengths;

	/* Lexicographically sorted array of keys to match against. */
	uint8_t const * const * rule_keys;

	/* Length of rule_keys */
	unsigned rule_key_length;

} transformer;

/*  */
typedef struct rules_parse_state_s {
	string_allocator alloc;

	/* Whether current value is fragmented. */
	unsigned fragmented;

	/* Fragmented key. */
	uint8_t* frag_key;

	/* Fragmented value. */
	uint8_t* frag_value;

	/* Number of rules parsed so far. */
	unsigned rule_count;

	/* FIXME do not have fixed rule size! */
	const uint8_t* rules[512];
} rules_parse_state;


/* Initialize string allocator. */
void init_string_allocator(string_allocator* a, unsigned buffer_size){
	a->cur_begin = 0;
	a->cur_offset = 0;
	a->buffer_length = buffer_size;

	/* Allocate 1 buffer. */
	a->buffer_count = 1;
	a->buffers[0] = malloc(buffer_size);
}

/* Allocation functions include ALSO add the NULL terminator implicitly! */

/* Add more space in the allocator. Returns new buffer index. */
int augment_allocator(string_allocator* a){
	if(32 == a->buffer_count)
		return -1;

	a->buffers[a->buffer_count] = malloc(a->buffer_length << a->buffer_count);

	return a->buffer_count++;
}

/* Pointer to beginning of current string. */
uint8_t* current_string(string_allocator* a){
	return a->buffers[a->buffer_count - 1] + a->cur_begin;
}

/* Append space to current string. User outputs characters to string.
 * If len is nonzero, returns pointer to beginning of segment.
 * Appending with 0 will end the current string. The return value is the
 * beginning of the ENTIRE string. */
uint8_t* append_current(string_allocator* a, unsigned len){
	uint8_t* ret;
	if(len){
		unsigned x = a->cur_offset;
		unsigned cur_buff = a->buffer_count - 1;
		unsigned cur_len = a->cur_offset - a->cur_begin;
		while(x + len > a->buffer_length << (a->buffer_count - 1)){
			augment_allocator(a);
			x = cur_len;
		}

		/* If augmented, copy current data to new buffer. */
		if(a->buffer_count - 1 != cur_buff){
			memcpy(a->buffers[a->buffer_count - 1],
				a->buffers[cur_buff] + a->cur_begin, cur_len);
			a->cur_begin = 0;
			a->cur_offset = cur_len;
		}

		/* Save beginning of empty data to return. */
		ret = a->buffers[a->buffer_count - 1] + a->cur_offset;
		a->cur_offset += len;
	}
	else{
		/* Add null terminator. */
		a->buffers[a->buffer_count - 1][a->cur_offset] = '\0';
		ret = current_string(a);

		/* Move to next 4 byte alignment. */
		a->cur_begin = (a->cur_offset + 4) & ~(0x3);
		if(a->cur_begin >= a->buffer_length << (a->buffer_count - 1)){
			augment_allocator(a);
			a->cur_begin = 0;
		}
		a->cur_offset = a->cur_begin;
	}
	return ret;
}

/* Allocate a complete string. User must write char data. Ends the current string. */
uint8_t* allocate_complete(string_allocator* a, unsigned len){
	append_current(a, len);
	return append_current(a, 0);
}

/* Return length of string str. */
unsigned string_length(string_allocator* a, const uint8_t* str){
	/* FIXME, store string length in buffer. */
	return strlen((char*)str);
}

/* Adds a rule to rps. */
void add_rule(rules_parse_state* rps, const uint8_t* key, const uint8_t* value){
	rps->rules[rps->rule_count] = key;
	rps->rules[rps->rule_count + 1] = value;
	rps->rule_count += 2;
}

/* Builds output tree from completed rules list. */
void build_output_tree(rules_parse_state* rps);

void init_rules_parse_state(rules_parse_state* rps){
	init_string_allocator(&rps->alloc, MAX_STRING_LENGTH);
	rps->frag_key = NULL;
	rps->frag_value = NULL;
	rps->fragmented = 0;
}

int on_fragment(rules_parse_state* rps, const bnj_state* state,
	const bnj_val* v, const uint8_t* buff)
{
	assert(state->vi);
	assert(rps->fragmented);

	if(1 == rps->fragmented){
		/* Append to key if key is fragmented. */
		if(v->key_length){
			rps->frag_key = append_current(&rps->alloc, v->key_length);
			bnj_stpkeycpy((char*)rps->frag_key, v, buff);
		}

		/* If key complete, close out value. */
		if(!(v->type & BNJ_VFLAG_KEY_FRAGMENT)){
			rps->frag_key = append_current(&rps->alloc, 0);
			rps->fragmented = 2;
		}
		else{
			assert(0 == bnj_strlen8(v));
		}
	}

	if(rps->fragmented > 1){
		/* Make sure rule is of valid type. */
		if(bnj_val_type(v) != BNJ_STRING)
			return 1;

		/* If value data then append to allocator. */
		unsigned vlen = bnj_strlen8(v);
		if(vlen){
			rps->frag_value = append_current(&rps->alloc, vlen);
			bnj_stpcpy8(rps->frag_value, v, buff);
		}

		/* If value is complete, go onto next value. */
		if(!bnj_incomplete(state, v)){
			/* Apply completed value. */
			add_rule(rps, rps->frag_key, rps->frag_value);
			rps->frag_key = NULL;
			rps->frag_value = NULL;
			rps->fragmented = 0;
		}
	}
	return 0;
}

int rules_cb(const bnj_state* state, bnj_ctx* ctx, const uint8_t* buff){
	rules_parse_state* rps = (rules_parse_state*)ctx->user_data;

	if(state->vi){
		/* Currently only handling a 1 level deep map.
		 * NOTE in the future, rules may be scoped within maps! */
		if(state->depth > 1)
			return 1;

		/* Finish previous fragment. If fragment finishes then go onto begin. */
		unsigned begin = 0;
		if(rps->fragmented){
			const bnj_val* v = state->v;
			if(on_fragment(rps, state, v, buff))
				return 1;
			++begin;
		}

		for(unsigned i = begin; i < state->vi; ++i){
			const bnj_val* v = state->v + i;

			/* Stop on incomplete values. */
			if(bnj_incomplete(state, v)){
				assert(i == state->vi - 1);
				rps->fragmented = (v->type & BNJ_VFLAG_KEY_FRAGMENT) ? 1 : 2;
				if(on_fragment(rps, state, v, buff))
					return 1;
				break;
			}

			/* Make sure rule is of valid type. */
			if(bnj_val_type(v) != BNJ_STRING)
				return 1;

			/* Read key. */
			unsigned klen = v->key_length;
			uint8_t* key = allocate_complete(&rps->alloc, klen);
			bnj_stpkeycpy((char*)key, v, buff);

			/* Read UTF-8 string. */
			uint8_t* rule = allocate_complete(&rps->alloc, bnj_strlen8(v));
			bnj_stpcpy8(rule, v, buff);

			add_rule(rps, key, rule);
		}
	}
	return 0;
}

int parse_rules(rules_parse_state* rps, bnj_state* st, int inputfd){
	bnj_val values[16];
	bnj_ctx ctx = {
		.user_cb = rules_cb,
		.user_data = rps,
		.key_set = NULL,
		.key_set_length = 0,
	};

	st->v = values;
	st->vlen = 16;

	init_rules_parse_state(rps);

	uint8_t buff[1024];
	while(1){
		/* Read data from stdin. */
		int ret = read(inputfd, buff, 1024);
		if(ret == 0)
			break;
		if(ret < 0)
			return 1;

		const uint8_t* res = bnj_parse(st, &ctx, buff, ret);
		if(st->flags & BNJ_ERROR_MASK){
			printf("Error %s\n", res);
			return 1;
		}
	}
	return 0;
}

int main(int argc, const char* argv[]){
	/* FIXME variable stack depth. */
	uint32_t stackbuff[512];
	bnj_state mstate;

	/* FIXME Process arguments. */
	bnj_state_init(&mstate, stackbuff, 512);

	/* First parse rules. */
	rules_parse_state rps;
	int rules_fd = open("test.json", O_RDONLY);
	if(-1 == rules_fd)
		return 1;
	if(parse_rules(&rps, &mstate, rules_fd))
		return 1;
	close(rules_fd);

	fprintf(stdout, "rule count %d\n\n", rps.rule_count);
	for(unsigned i = 0; i < rps.rule_count; ++i){
		fprintf(stdout, "%s\n", rps.rules[i]);
	}

	/* Next parse input files. */

	return 0;
}
