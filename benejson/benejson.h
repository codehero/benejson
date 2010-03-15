/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#ifndef __BENEGON_BNJ_H__
#define __BENEGON_BNJ_H__

/************************************************************************/
/* Includes. */
/************************************************************************/

#include <stdint.h>

/* For memcpy */
#include <string.h>


/************************************************************************/
/* Code Configuration. */
/************************************************************************/

/* Numeric parsing compile time configuration. */

/** @brief Rough upper bound for maximum exponent MAGNITUDE. */
#define ROUGH_MAX_EXPONENT 10000000

/** @brief Numeric storage type. */
typedef uintmax_t SIGNIFICAND;

/** @brief Maximum integer value to parse on each fragment. */
#define SIGNIFICAND_MAX UINTMAX_MAX

/** @brief Include conversion to wide character strings. */
#define BNJ_WCHAR_SUPPORT
//#undef BNJ_WCHAR_SUPPORT

/** @brief Include conversion to floating point functions. */
#define BNJ_FLOAT_SUPPORT
//#undef BNJ_FLOAT_SUPPORT


/************************************************************************/
/* Conditional Includes. */
/************************************************************************/

#ifdef BNJ_WCHAR_SUPPORT
#include <wchar.h>
#endif

#ifdef BNJ_FLOAT_SUPPORT
#include <math.h>
#endif

/************************************************************************/
/* Enum definitions.. */
/************************************************************************/

/** @brief Special values. */
enum {
	/** @brief Boolean false. */
	BNJ_SPC_FALSE,

	/** @brief Boolean true. */
	BNJ_SPC_TRUE,

	/** @brief Null value. */
	BNJ_SPC_NULL,

	/** @brief Value exists but is undefined. */
	BNJ_SPC_NAN,

	/** @brief Quantity infinite, also with BNJ_VFLAG_NEGATIVE_SIGNIFICAND. */
	BNJ_SPC_INFINITY,

	BNJ_COUNT_SPC
};

/** @brief Value types. */
enum {
	/** @brief Numeric value. */
	BNJ_NUMERIC = 0,

	/** @brief Reserved words (false, true, null, NaN, etc). */
	BNJ_SPECIAL = 1,

	/** @brief Type is start of array */
	BNJ_ARR_BEGIN = 2,

	/** @brief Type is start of object */
	BNJ_OBJ_BEGIN = 3,

	/** @brief Minimum value for string. */
	BNJ_STRING = 4
};


/** @brief Value state flags. */
enum {
	BNJ_TYPE_MASK = 0x7,

	/** @brief Negative significand. */
	BNJ_VFLAG_NEGATIVE_SIGNIFICAND = 0x10,

	/** @brief Negative exponent. */
	BNJ_VFLAG_NEGATIVE_EXPONENT = 0x20,

	/** @brief This is a fragment of a value. */
	BNJ_VFLAG_VAL_FRAGMENT = 0x40,

	/** @brief Key is only a fragment. Consequently no value data. */
	BNJ_VFLAG_KEY_FRAGMENT = 0x80,

	/** @brief Key completed, but value fragment not yet started. */
	BNJ_VFLAG_MIDDLE = 0x8
};


/** @brief Stack state flags.  */
enum {
	/** @brief Data is a array. */
	BNJ_ARRAY = 0x0,

	/** @brief Data is a map. */
	BNJ_OBJECT = 0x1,

	/** @brief Value not read yet. */
	BNJ_VAL_INCOMPLETE = 0x2,

	/** @brief Expecting a key, but not fully read yet. */
	BNJ_KEY_INCOMPLETE = 0x4,

	/** @brief Mask over ARRAY,OBJECT */
	BNJ_CTX_MASK = 0x1
};


/** @brief Error codes. */
enum {
	/** @brief Successful parsing */
	BNJ_SUCCESS = 0x20000000,

	/** @brief All errors have the high bit set. */
	BNJ_ERROR_MASK = 0x40000000,

	/** @brief Extra comma detected. */
	BNJ_ERR_EXTRA_COMMA = BNJ_ERROR_MASK,

	/** @brief Expected comma. */
	BNJ_ERR_NO_COMMA,

	/** @brief Invalid character following '{'. */
	BNJ_ERR_MAP_INVALID_CHAR,

	/** @brief JSON input exceeded maximum stack depth. */
	BNJ_ERR_STACK_OVERFLOW,

	/** @brief Invalid value character. */
	BNJ_ERR_BAD_VALUE_CHAR,

	/** @brief Occurs when seeing -. or -E */
	BNJ_ERR_NUM_DIGIT_MISSING,

	/** @brief Decimal point appears twice. */
	BNJ_ERR_EXTRA_DECIMAL,

	/** @brief Exponent overly large. */
	BNJ_ERR_MAX_EXPONENT,

	/** @brief No exponent digit defined. */
	BNJ_ERR_NO_EXP_DIGIT,

	/** @brief Invalid hex character in \uXXXX */
	BNJ_ERR_INV_HEXESC,

	/** @brief Invalid escape character in \X */
	BNJ_ERR_INV_ESC,
	
	/** @brief Invalid character */
	BNJ_ERR_INVALID,

	/** @brief Invalid character in reserved word. */
	BNJ_ERR_RESERVED,

	/** @brief List/Map mistmach */
	BNJ_ERR_LISTMAP_MISMATCH,

	/** @brief Missing colon in key:value pair. */
	BNJ_ERR_MISSING_COLON,

	/** @brief Invalid or unexpected UTF-8 character. */
	BNJ_ERR_UTF_8,

	/** @brief UTF-16 surrogate error. */
	BNJ_ERR_UTF_SURROGATE,

	/** @brief User halted parsing. */
	BNJ_ERR_USER,

	/** @brief Just used for counting number of error types. */
	BNJ_ERROR_SENTINEL,

	/** @brief Number of errors defined. */
	BNJ_ERROR_COUNT = BNJ_ERROR_SENTINEL - BNJ_ERROR_MASK
};

#define BNJ_EMPTY_CP 0x80000000

/************************************************************************/
/* Type and struct definitions.. */
/************************************************************************/

/* Forward declarations. */
struct bnj_state_s;
struct bnj_ctx_s;

/** @brief Callback function type. */
typedef int(*bnj_cb)(const struct bnj_state_s* state, struct bnj_ctx_s* ctx,
	const uint8_t* buff);

/** @brief Parsing context structure. Governs general parsing behaviour. */
typedef struct bnj_ctx_s{
	/** @brief User's callback function. Called if not NULL; otherwise
	 * bnj_parse returns with populated bnj_val. */
	bnj_cb user_cb;

	/** @brief User state data. May be changed during parsing. */
	void* user_data;

	/** @brief Sorted list of apriori known null terminated key strings.
	 * THIS SHOULD NEVER CHANGE WHILE IN KEY FRAGMENT STATE. */
	char const * const * key_set;

	/** @brief Length of key set.
	 * THIS SHOULD NEVER CHANGE WHILE IN KEY FRAGMENT STATE. */
	unsigned key_set_length;
} bnj_ctx;


/** @brief Holds parsed JSON value.
 * Across fragments, the parser must preserve fields marked with [PAF].*/
typedef struct bnj_val_s {
	/** @brief Value type. [PAF] */
	uint8_t type;

	/** @brief Key's length of ASCII character data. 
	 * No key should be more than 255 chars long! */
	uint8_t key_length;

	/** @brief Numeric key id if key_set defined. [PAF] */
	uint16_t key_enum;

	/** @brief Key begins at offset from buffer. */
	uint16_t key_offset;

	/** @brief String value begins at offset from buffer. */
	uint16_t strval_offset;

	/** @brief When type is BNJ_UTF_*, count of code points between [0, 128). */
	uint16_t cp1_count;

	/** @brief When type is BNJ_UTF_*, count of code points between [128, 2K). */
	uint16_t cp2_count;

	/** @brief When type is BNJ_UTF_*, count of code points between [2K, 65K). */
	uint16_t cp3_count;

	/** @brief Valid when parsing numeric, holds the present exponent value [PAF].
	 * Internal use only:
	 *  When type is BNJ_UTF_*, count of code points between [65K, 1.1M). */
	int16_t exp_val;

	/** @brief Numeric significand fragment. [PAF]
	 * NOTE ACTUAL SIZE is ARCHITECTURE DEPENDENT! MUST BE AT LEAST 32bit
	 * Internal use only:
	 *  When type is BNJ_UTF_*, holds fragmented code point value. */
	SIGNIFICAND significand_val;

} bnj_val;


/** @brief Parsing context structure updates before every callback call. */
typedef struct bnj_state_s{

	/* User configurable fields. */

	/** @brief Array in which to store json values. */
	bnj_val* v;

	/** @brief Length of v. */
	uint32_t vlen;


	/* bnj_parse Reset to 0 between each call to user_cb. */

	/** @brief Change in json depth since last call. */
	int32_t depth_change;

	/** @brief Current value being parsed. On CB, how many values parsed. */
	uint32_t vi;


	/* These fields are maintained between bnj_parse calls. */

	/** @brief Current depth. */
	uint32_t depth;

	/** @brief Total count of DIGITS in significand. */
	uint32_t digit_count;

	/** @brief SPSJSON_SFLAGS_* flags. */
	uint32_t flags;


	/* These following two are for internal use. Do not use in user code. */

	/** @brief How many digits preceded the decimal. -1 if no decimal. */
	int32_t _decimal_offset;

	/** @brief PAF last fragmented code point. */
	uint32_t _cp_fragment;

	/** @brief Supremum of key set tracking. */
	uint16_t _key_set_sup;

	/** @brief Internal key length counter. */
	uint16_t _key_len;

	/** @brief PAF key enum */
	uint16_t _paf_key_enum;

	/** @brief PAF type */
	uint16_t _paf_type;

	/** @brief PAF exp_val */
	int16_t _paf_exp_val;

	/** @brief PAF significand value. */
	SIGNIFICAND _paf_significand_val;

	/* Initialized by bnj_state_init. */

	/** @brief Length of stack[]. */
	uint32_t stack_length;

	/** @brief Parsing context stack. length == stack_length. Entry at [n] describes depth n. */
	uint32_t *stack;

} bnj_state;


/************************************************************************/
/* API */
/************************************************************************/

/* Control functions. */

/** @brief Initialize parsing context.
 *  @param st State to initialize.
 *  @param state_buffer Preallocated memory to hold json state data.
 *  @param stack_length How long stack length will be. */
bnj_state* bnj_state_init(bnj_state* st, uint32_t* state_buffer, uint32_t stack_length);

/** @brief Parse JSON txt.
 *  @param state JSON parsing state.
 *  @param buffer character data to parse.
 *  @param len    Length of buffer.
 *  @return Where parsing ended. */
const uint8_t* bnj_parse(bnj_state* state, bnj_ctx* ctx, const uint8_t* buffer, uint32_t len);

/** @brief Helper function for fragment management.
 *  Moves key:value fragments to beginning of buffer.
 *  @param frag Fragmented key:value.
 *  @param buffer Input buffer that was passed to bnj_parse.
 *  @param len Size of input buffer; on return, contains number of free bytes
 *  following the return value.
 *  @return First empty byte in the buffer. */
uint8_t* bnj_fragcompact(bnj_val* frag, uint8_t* buffer, uint32_t* len);

/** @brief Determine whether value was incompletely read.
 *  @param src Value in question
 *  @return 1 if incomplete, 0 if not. */
unsigned bnj_incomplete(const bnj_state* state, const bnj_val* src);



/* String length counting functions. */

/** @brief Get UTF-8 string value length count.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @return UTF-8 encoded string length. */
unsigned bnj_strlen8(const bnj_val* src);

/** @brief Get number of code points (NOT necessarily number of "characters")
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @return code point count. */
unsigned bnj_cpcount(const bnj_val* src);

/** @brief Get UTF-8 string value length count.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @return UTF-8 encoded string length. */
unsigned bnj_strlen8(const bnj_val* src);

/** @brief Get string value length.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @return UTF-16 encoded string length. */
unsigned bnj_strlen16(const bnj_val* src);

/** @brief Get string value length.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @return UTF-32 encoded string length. */
unsigned bnj_strlen32(const bnj_val* src);


/* String copy functions. */

/** @brief Copy key to destination buffer.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @param buff buffer containing string data.
 *  @return pointer to dst's null terminator. NULL if there is no key. */
char* bnj_stpkeycpy(char* dst, const bnj_val* src, const uint8_t* buff);

/** @brief Copy JSON encoded string to normal UTF-8 destination.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @param buff buffer containing string data.
 *  @return pointer to dst's null terminator. NULL if src type incorrect. */
uint8_t* bnj_stpcpy8(uint8_t* dst, const bnj_val* src, const uint8_t* buff);

/** @brief Copy encoded string to normal UTF-8 destination, with length limit.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @param len Character length including null terminator.
 *  @param buff buffer containing string data.
 *  @return pointer to first character past null terminator (end of string). */
uint8_t* bnj_stpncpy8(uint8_t* dst, const bnj_val* src, size_t len,
	const uint8_t* buff);

/** @brief Utility copy function; really for INTERNAL or ADVANCED use.
 *  @param dst Where to store UTF-8 output.
 *  @param destlen How many UTF-8 bytes to store.
 *  MUST NOT EXCEED PREDICTED UTF-8 LENGTH IN BUFF! (you've been warned).
 *  @param buff Source data; Upon return points to where parsing stopped.
 *  @return Pointer to next unwritten byte after dst. */
uint8_t* bnj_json2utf8(uint8_t* dst, size_t destlen, const uint8_t** buff);

/** @brief Utility function to convert code point to utf encoding
 * Does NOT null terminate.
 *  @param dst Where to store UTF-8 output.
 *  @param len Number of bytes in dst.
 *  @param cp code point value.
 *  @return Pointer to next unwritten byte after dst. */
uint8_t* bnj_utf8_char(uint8_t* dst, unsigned len, uint32_t cp);

#ifdef BNJ_WCHAR_SUPPORT

/** @brief Copy JSON encoded string to wide character string.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @param buff buffer containing string data.
 *  @return pointer to dst's null terminator. */
wchar_t* bnj_wcpcpy(wchar_t* dst, const bnj_val* src, const uint8_t* buff);

/** @brief Copy encoded string to wide character string, with length limit.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_STRING string data.
 *  @param len Character length including null terminator.
 *  @param buff buffer containing string data.
 *  @return pointer to first character past null terminator (end of string). */
wchar_t* bnj_wcpncpy(wchar_t* dst, const bnj_val* src, size_t len,
	const uint8_t* buff);

/** @brief Compare JSON encoded string to wide character string.
 *  @param s1 Normal string.
 *  @param s2 BNJ value containing BNJ_STRING string data.
 *  @param buff buffer containing string data.
 *  @return negative if s1 < s2, 0 is s1 == s2, positive if s1 > s2.
 *  magnitude of return value is first char in str that does not match. */
int bnj_wcscmp(const wchar_t* s1, const bnj_val* s2, const uint8_t* buff);

#endif


/* Value extraction functions. */

/** @brief Extract boolean (0,1) from true/false value.
	* @return 0 on false, 1 on true. */
unsigned char bnj_bool(const bnj_val* src);

#ifdef BNJ_FLOAT_SUPPORT

/** @brief Extract double precision floating point from src.
 *  @return src converted to double float value.. */
double bnj_double(const bnj_val* src);

/** @brief Extract double precision floating point from src.
 *  @return src converted to single float value. */
float bnj_float(const bnj_val* src);

#endif


/************************************************************************/
/* Inline functions. */
/************************************************************************/

inline unsigned bnj_cpcount(const bnj_val* src){
	return src->cp1_count + src->cp2_count + src->cp3_count
		+ (unsigned)(src->exp_val);
}

inline unsigned bnj_incomplete(const bnj_state* state, const bnj_val* src){
	return (src->type &
	(BNJ_VFLAG_VAL_FRAGMENT | BNJ_VFLAG_KEY_FRAGMENT | BNJ_VFLAG_MIDDLE));
}

inline unsigned bnj_val_type(const bnj_val* src){
	return src->type & BNJ_TYPE_MASK;
}

inline char* bnj_stpkeycpy(char* dst, const bnj_val* src, const uint8_t* buff){
	if(src->key_length){
		memcpy(dst, buff + src->key_offset, src->key_length);
		dst[src->key_length] = '\0';
		return dst + src->key_length;
	}
	return NULL;
}

inline unsigned bnj_strlen8(const bnj_val* src){
	return src->cp1_count + 2 * src->cp2_count + 3 * src->cp3_count
		+ 4 * (unsigned)(src->exp_val);
}

inline unsigned bnj_strlen16(const bnj_val* src){
	return 2 * (src->cp1_count + src->cp2_count + src->cp3_count)
		+ 4 * (unsigned)(src->exp_val);
}

inline unsigned bnj_strlen32(const bnj_val* src){
	return 4 * bnj_cpcount(src);
}

inline uint8_t* bnj_stpcpy8(uint8_t* dst, const bnj_val* src,
	const uint8_t* buff)
{
	return bnj_stpncpy8(dst, src, bnj_strlen8(src) + 1, buff);
}

inline uint8_t* bnj_utf8_char(uint8_t* dst, unsigned len, uint32_t v){
	/* Write UTF-8 encoded value. Advance dst to point to last byte. */
	/* If not enough space remaining do nothing. */
	if(v < 0x80 && len >= 1){
		dst[0] = v;
		++dst;
	}
	else if(v < 0x800 && len >= 2){
		dst[1] = 0x80 | (v & 0x3F); v >>= 6;
		dst[0] = 0xC0 | v;
		dst += 2;
	}
	else if(v < 0x10000 && len >= 3){
		dst[2] = 0x80 | (v & 0x3F); v >>= 6;
		dst[1] = 0x80 | (v & 0x3F); v >>= 6;
		dst[0] = 0xE0 | v;
		dst += 3;
	}
	else if(len >= 4){
		dst[3] = 0x80 | (v & 0x3F); v >>= 6;
		dst[2] = 0x80 | (v & 0x3F); v >>= 6;
		dst[1] = 0x80 | (v & 0x3F); v >>= 6;
		dst[0] = 0xF0 | v;
		dst += 4;
	}
	return dst;
}

#ifdef BNJ_WCHAR_SUPPORT

inline wchar_t* bnj_wcpcpy(wchar_t* dst, const bnj_val* src, const uint8_t* buff){
	return bnj_wcpncpy(dst, src, bnj_strlen16(src) + 1, buff);
}

#endif

inline unsigned char bnj_bool(const bnj_val* src){
	return (BNJ_SPC_FALSE == src->significand_val) ? 0 : 1;
}

#ifdef BNJ_FLOAT_SUPPORT
inline double bnj_double(const bnj_val* src){
	if(BNJ_NUMERIC == bnj_val_type(src)){
		/* First copy integral value. */
		double dest = src->significand_val;

		/* Scale by exponent. */
		dest *= pow(10, src->exp_val);
		dest *= (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -1.0 : 1.0;
		return dest;
	}
	else{
		if(BNJ_SPC_NAN == src->significand_val)
			return NAN;
		else
			return (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -INFINITY: INFINITY;
	}
}

inline float bnj_float(const bnj_val* src){
	if(BNJ_NUMERIC == bnj_val_type(src)){
		/* First copy integral value. */
		float dest = src->significand_val;

		/* Scale by exponent. */
		dest *= powf(10, src->exp_val);
		dest *= (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -1.0 : 1.0;
		return dest;
	}
	else{
		if(BNJ_SPC_NAN == src->significand_val)
			return NAN;
		else
			return (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -INFINITY: INFINITY;
	}
}
#endif

#endif
