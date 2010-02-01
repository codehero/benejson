/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#ifndef __BENEGON_BNJ_H__
#define __BENEGON_BNJ_H__

/************************************************************************/
/* Includes. */
/************************************************************************/

#include <stdint.h>


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
	BNJ_STRING = 4,

	/** @brief UTF-8 string. */
	BNJ_UTF_8 = 4,

	/** @brief String contains characters in BMP. */
	BNJ_UTF_16 = 5,

	/** @brief String contains characters in Extended plane. */
	BNJ_UTF_32 = 6
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
	BNJ_VFLAG_KEY_FRAGMENT = 0x80
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
	BNJ_KEY_INCOMPLETE = 0x4
};


/** @brief Error codes. */
enum {
	/** @brief Successful parsing */
	BNJ_SUCCESS = 0x20000000,

	/** @brief All errors have the high bit set. */
	BNJ_ERROR_MASK = 0x40000000,

	/** @brief Extra comma detected. */
	BNJ_ERR_EXTRA_COMMA,

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

	/** @brief Number of errors defined. */
	BNJ_ERROR_COUNT = BNJ_ERR_MISSING_COLON - BNJ_ERROR_MASK + 1
};


/************************************************************************/
/* Type and struct definitions.. */
/************************************************************************/

/* Forward declarations. */
struct bnj_state_s;

/** @brief Callback function type. */
typedef int(*bnj_cb)(const struct bnj_state_s* state, const uint8_t* buff);


/** @brief Parsing context structure. Governs general parsing behaviour. */
typedef struct bnj_ctx_s{
	/** @brief User's callback function. Called if not NULL; otherwise
	 * bnj_parse returns with populated bnj_val. */
	bnj_cb user_cb;

	/** @brief Sorted list of apriori known null terminated key strings.
	 * THIS SHOULD NEVER CHANGE WHILE IN KEY FRAGMENT STATE. */
	char const * const * key_set;

	/** @brief Length of key set.
	 * THIS SHOULD NEVER CHANGE WHILE IN KEY FRAGMENT STATE. */
	unsigned key_set_length;
} bnj_ctx;


/** @brief Holds parsed JSON value. */
typedef struct bnj_val_s {
	/** @brief Key begins at offset from buffer. */
	uint16_t key_offset;

	/** @brief Key's length of MB character data.  */
	uint16_t key_length;

	/** @brief String value begins at offset from buffer. */
	uint16_t strval_offset;

	/** @brief String value's character count. */
	uint16_t strval_length;

	/** @brief Numeric significand fragment.
	 * NOTE ACTUAL SIZE is ARCHITECTURE DEPENDENT! */
	SIGNIFICAND significand_val;

	/** @brief Valid when parsing numeric, holds the present exponent value.
	 * Internal use only: When type is BNJ_UTF_*, the raw UTF-8 encoded length. */
	int32_t exp_val;

	/** @brief Numeric key id if key_set defined. */
	uint16_t key_enum;

	/** @brief Value type. */
	uint8_t type;
} bnj_val;


/** @brief Parsing context structure updates before every callback call. */
typedef struct bnj_state_s{

	/* User configurable fields. */

	/** @brief Handle to context. May be changed during parsing. */
	const bnj_ctx* user_ctx;

	/** @brief User state data. May be changed during parsing. */
	void* user_data;

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

	/** @brief Holds value state between calls. */
	bnj_val _save;

	/** @brief Supremum of key set tracking. */
	uint16_t _key_set_sup;

	/** @brief Internal key length counter. */
	uint16_t _key_len;


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
const uint8_t* bnj_parse(bnj_state* state, const uint8_t* buffer, uint32_t len);


/* Value accessors. */

/** @brief Copy JSON encoded string to normal UTF-8 destination.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_UTF_8 string data.
 *  @param buff buffer containing string data.
 *  @param which If 0, then copy string value; otherwise copy key.
 *  @return pointer to dst's null terminator. NULL if src type incorrect. */
char* bnj_stpcpy(char* dst, const bnj_val* src, const uint8_t* buff, int which);

/** @brief Copy encoded string to normal UTF-8 destination, with length limit.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_UTF_8 string data.
 *  @param len Character length including null terminator.
 *  @param buff buffer containing string data.
 *  @param which If 0, then copy string value; otherwise copy key.
 *  @return pointer to first character past null terminator (end of string). */
char* bnj_stpncpy(char* dst, const bnj_val* src, size_t len,
	const uint8_t* buff, int which);

/** @brief Compare JSON encoded string to normal string.
 *  @param s1 Normal string.
 *  @param s2 BNJ value containing BNJ_UTF_8 string data.
 *  @param buff buffer containing string data.
 *  @param which If 0, then compare string value; otherwise key value.
 *  @return negative if s1 < s2, 0 is s1 == s2, positive if s1 > s2.
 *  magnitude of return value is first char in str that does not match. */
int bnj_strcmp(const char* s1, const bnj_val* val, const uint8_t* buff,
	int which);

#ifdef BNJ_WCHAR_SUPPORT

/** @brief Copy JSON encoded string to wide character string.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_UTF_8 string data.
 *  @param buff buffer containing string data.
 *  @return pointer to dst's null terminator. */
wchar_t* bnj_wcpcpy(wchar_t* dst, const bnj_val* src, const uint8_t* buff);

/** @brief Copy encoded string to wide character string, with length limit.
 *  @param dst Where to copy data. Must hold at least src->string_val + 1 chars.
 *  @param src BNJ value containing BNJ_UTF_8 string data.
 *  @param len Character length including null terminator.
 *  @param buff buffer containing string data.
 *  @return pointer to first character past null terminator (end of string). */
wchar_t* bnj_wcpncpy(wchar_t* dst, const bnj_val* src, size_t len, const uint8_t* buff);

/** @brief Compare JSON encoded string to wide character string.
 *  @param s1 Normal string.
 *  @param s2 BNJ value containing BNJ_UTF_8 string data.
 *  @param buff buffer containing string data.
 *  @return negative if s1 < s2, 0 is s1 == s2, positive if s1 > s2.
 *  magnitude of return value is first char in str that does not match. */
int bnj_wcscmp(const wchar_t* s1, const bnj_val* s2, const uint8_t* buff);

#endif

#ifdef BNJ_FLOAT_SUPPORT

/** @brief Extract double precision floating point from src.
 *  @return 0 if no errors; nonzero if error. */
double bnj_double(const bnj_val* src);

/** @brief Extract double precision floating point from src.
 *  @return 0 if no errors; nonzero if error. */
float bnj_float(const bnj_val* src);

#endif


/************************************************************************/
/* Inline functions. */
/************************************************************************/

inline char* bnj_stpcpy(char* dst, const bnj_val* src, const uint8_t* buff,
	int which)
{
	return bnj_stpncpy(dst, src, (which ? src->key_length: src->strval_length)+1,
		buff, which);
}

#ifdef BNJ_WCHAR_SUPPORT

inline wchar_t* bnj_wcpcpy(wchar_t* dst, const bnj_val* src, const uint8_t* buff){
	return bnj_wcpncpy(dst, src, src->strval_length + 1, buff);
}

#endif

#ifdef BNJ_FLOAT_SUPPORT
inline double bnj_double(const bnj_val* src){
	/* First copy integral value. */
	double dest = src->significand_val;

	/* Scale by exponent. */
	dest *= pow(10, src->exp_val);
	dest *= (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -1.0 : 1.0;
	return dest;
}

inline float bnj_float(const bnj_val* src){
	/* First copy integral value. */
	float dest = src->significand_val;

	/* Scale by exponent. */
	dest *= powf(10, src->exp_val);
	dest *= (src->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -1.0 : 1.0;
	return dest;
}
#endif

#endif
