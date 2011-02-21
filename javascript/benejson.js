/* Copyright (c) 2011 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

var sys = require("sys");

/* Parser states:
 * 0 : Begin
 * 1 : Map
 * 2 : List
 * */
function JSONTokenizer(callback){
	this.stack = [];
	this.valFragment = "";
	this.escapeState = null;
	this.firstSurrogate = null;
	this.topFn = this.processA;
	this.curString = null;
	this.curKey = null;

	this.curState = "";
	return this;
}

/* FIXME not yet used for validating token transitions. */
JSONTokenizer.prototype.states = {
	"" : {
		"v":1,
		"{":1,
		"[":1
	},

	"{" :{
		"k":1,
		"}":1
	},

	"k":{
		":":1
	},

	":":{
		"v":1
	},

	"v":{
		",":1,
		"}":1,
		"]":1
	},

	"[":{
		"[":1,
		"{":1,
		"]":1,
		"v":1
	},

	"," :{
		"v":1,
		"k":1
	}

};

JSONTokenizer.prototype.wSpace = /\s/g;

/** @brief Enumerate JSON parsing events. */
JSONTokenizer.prototype.EVENTS = {
	/** @brief Reached end of JSON document. */
	"STOP":0,

	/** @brief Parsed single value. */
	"VALUE":1,

	/** @brief Parsed beginning of map/array. */
	"DESCEND":2,

	/** @brief Parsed end of map/array. */
	"ASCEND":3
};

JSONTokenizer.prototype.setCallback = function(callback){
	this.callback = callback;
}

JSONTokenizer.prototype.getTop = function(){
	return this.stack[this.stack.length - 1];
}


JSONTokenizer.prototype.tokenFn = {};
JSONTokenizer.prototype.tokenFn[':'] = function(tokenizer){
	tokenizer.curKey = tokenizer.curString;
}

JSONTokenizer.prototype.tokenFn['['] = function(tokenizer){
	tokenizer.callback(tokenizer.EVENTS.DESCEND, tokenizer.curKey, "["); 
	tokenizer.curKey = null;
	/* Expecting key string.  */
	tokenizer.stack.push({
		"type":"[",
		"list":[]
	});
}

JSONTokenizer.prototype.tokenFn[']'] = function(tokenizer){
	tokenizer.curKey = null;
	if(tokenizer.getTop().type != '[')
		throw new Error("] mismatch");
	tokenizer.callback(tokenizer.EVENTS.ASCEND, null, "]"); 
	tokenizer.stack.pop();
}

JSONTokenizer.prototype.tokenFn['{'] = function(tokenizer){
	tokenizer.callback(tokenizer.EVENTS.DESCEND, tokenizer.curKey, "{"); 
	tokenizer.curKey = null;
	/* Expecting key string. */
	tokenizer.stack.push({
		"type":"{",
		"map":{},
		"keyState":null
	});
}

JSONTokenizer.prototype.tokenFn['}'] = function(tokenizer){
	tokenizer.curKey = null;
	if(tokenizer.getTop().type != '{')
		throw new Error("} mismatch");

	tokenizer.callback(tokenizer.EVENTS.ASCEND, null, "}"); 
	tokenizer.stack.pop();
}

JSONTokenizer.prototype.tokenFn[','] = function(tokenizer){
	tokenizer.curKey = null;
}

JSONTokenizer.prototype.parseValue = function(v){
	switch(v){
		case "":
			break;

		case "true":
			this.callback(this.EVENTS.VALUE, this.curKey, true);
			break;

		case "false":
			this.callback(this.EVENTS.VALUE, this.curKey, false);
			break;

		case "null":
			this.callback(this.EVENTS.VALUE, this.curKey, null);
			break;

		case "NaN":
			this.callback(this.EVENTS.VALUE, this.curKey, NaN);
			break;

		default:
			/* Parse float.
			 * FIXME Does not detect trailing non-numeric characters! */
			var f = parseFloat(v);
			if(isNaN(f))
				throw new Error("Unrecognized value " + v);
			this.callback(this.EVENTS.VALUE, this.curKey, f);
			break;
	}
}

JSONTokenizer.prototype.processAHelper = function(fragment){
	/* Exactly one value must be in each val[k]:
	 * -integer,float
	 * -true,false,etc
	 * -empty compound [] or {} */

	var vi = 0;

	/* If first character is a token then fragment must end. */
	if(fragment.charAt(0) in this.tokenFn){
		this.parseValue(this.valFragment);
		this.valFragment = "";
		this.tokenFn[fragment.charAt(0)](this);
		vi = 1;
		/* Find beginning of data fragment. */
		while(vi < fragment.length){
			var c = fragment.charAt(vi);
			if(!(c in this.tokenFn))
				break;
			this.tokenFn[c](this);
			++vi;
		}
	}

	/* Find end of fragment..*/
	var end = fragment.length;
	while(end > vi + 1){
		var c = fragment.charAt(end - 1);
		if(!(c in this.tokenFn))
			break;
		--end;
	}

	/* Remaining characters form a data fragment. */
	this.valFragment += fragment.substring(vi, end);

	if(end < fragment.length){
		this.parseValue(this.valFragment);
		this.valFragment = "";
		while(end < fragment.length){
			this.tokenFn[fragment.charAt(end)](this);
			++end;
		}
	}
}

JSONTokenizer.prototype.processA = function(input){
	/* Values are separated by commas.
	 * Split further by whitespace. */
	var vals = input.split(",");
	for(var k = 0; k < vals.length; ++k)
		vals[k] = vals[k].split(this.wSpace);

	for(var i = 0; i < vals.length; ++i){
		if(i){
			this.parseValue(this.valFragment);
			this.valFragment = "";
			this.tokenFn[','](this);
		}

		var v = vals[i];
		this.processAHelper(v[0]);
		for(var k = 1; k < v.length; ++k){
			this.parseValue(this.valFragment);
			this.valFragment = "";
			this.processAHelper(v[k]);
		}
	}
}

JSONTokenizer.prototype.strFn = {};

JSONTokenizer.prototype.strFn['"'] = function(tokenizer){
	if(tokenizer.topFn == JSONTokenizer.prototype.processA){
		tokenizer.curString = "";
		tokenizer.topFn = JSONTokenizer.prototype.processStr;
	}
	else if(tokenizer.topFn == JSONTokenizer.prototype.processStr){
		/* If in escape mode, this is just a quote character.
		 * Otherwise return to processA. */
		if(tokenizer.escapeState == null){

			/* If not in a map or if in a map and key is defined do callback. */
			if(tokenizer.getTop().type != '{' || tokenizer.curKey){
				tokenizer.callback(tokenizer.EVENTS.VALUE, tokenizer.curKey, tokenizer.curString);
			}
			tokenizer.topFn = JSONTokenizer.prototype.processA;
		}
		else if(tokenizer.escapeState == ""){
			tokenizer.curString += '\"';
			tokenizer.escapeState = null;
		}
		else
			throw new Error("Escape terminated early!");
	}
	else {
		/* Error. */
	}
}

JSONTokenizer.prototype.strFn['\\'] = function(tokenizer){
	if(tokenizer.escapeState == null){
		tokenizer.escapeState = "";
	}
	else if(tokenizer.escapeState == ""){
		/* Escaped '\\' */
		tokenizer.curString += '\\';
		tokenizer.escapeState = null;
	}
	else{
		/* Prior escape character not completely resolved. */
		throw new Error("Invalid escape character!");
	}
}

JSONTokenizer.prototype.escapeMap = {
	"b" : '\b',
	"/" : '/',
	"f" : '\n',
	"r" : '\r',
	"t" : '\t'
};

JSONTokenizer.prototype.processStrHelper = function(input){
	if(!input.length)
		return;

	if(this.escapeState == null){
		this.curString += input;
		return;
	}

	if(this.escapeState == ""){
		var c = input.charAt(0);
		if(c in this.escapeMap){
			this.curString += this.escapeMap[c];
			this.escapeState = null;
			return;
		}
		else if(c == 'u'){
			/* Fallthrough. */
			input = input.substr(1);
			/* FIXME No code yet to handle this situation!*/
		}
		else
			throw new Error("Invalid escape character!");
	}

	/* escapeState.length MUST be < 4. */
	if(escapeState.length >= 4)
		throw new Error("Programming error!");

	escapeState += input.substr(4 - escapeState.length);
	if(4 == escapeState.length){
		/* FIXME */
	}
}

JSONTokenizer.prototype.processStr = function(input){
	/* Split by escape characters. */
	var escSplit = input.split("\\");

	this.processStrHelper(escSplit[0]);
	for(var k = 1; k < escSplit.length; ++k){
		this.strFn['\\'](this);
		this.processStrHelper(escSplit[k]);
	}
}

JSONTokenizer.prototype.process = function(input){
	/* Note that topFn or tokenFn can change topFn,
	 * so topFn may switch between processA and processStr */
	var quotBrk = input.split('"');
	this.topFn(quotBrk[0]);
	for(var i = 1; i < quotBrk.length; ++i){
		this.strFn['"'](this);
		if(quotBrk[i].length)
			this.topFn(quotBrk[i]);
	}
}


/** @brief  */
function JSONParser(parseTree){
	this.root = parseTree;
	this.stack = [];
	/* The accumulator stack. Builds up a JSON parse tree for leaf functions. */
	this.accStack = [];
	this.accEndCB = null;
	this.mapAcc = {};
	return this;
}

JSONParser.prototype._top = function(){
	if(0 == this.stack.length)
		throw new Error("Empty stack!");

	return this.stack[this.stack.length - 1];
}

JSONParser.prototype._pushStack = function(x, key, value){
	if(x instanceof Array && "[" == value){
		if(x[0])
			x[0](key);
		this.stack.push(x);
	}
	else if(x instanceof Object && "{" == value){
		if("{" in x)
			x["{"](key);
		this.stack.push(x);
	}
	else
		throw new Error("Unexpected type!");
}

JSONParser.prototype._flushMapAcc = function(t){
	/* Don't act on empty maps. */
	for(var x in this.mapAcc){
		t["*"](this.mapAcc);
		break;
	}
	this.mapAcc = {};
}

JSONParser.prototype.onEvent = function(evt, key, value){
	if(evt == JSONTokenizer.prototype.EVENTS.STOP){
		/* FIXME */
		return;
	}

	/* Route event to accumulator if present. */
	if(this.accStack.length){
		if(evt == JSONTokenizer.prototype.EVENTS.DESCEND){
			var append = "[" == value ? [] : {};
			this.accStack.push(append);
			if(this.accStack.length > 1){
				if(this.accStack[this.accStack.length - 2] instanceof Array){
					this.accStack[this.accStack.length - 2].push(append);
				}
				else{
					this.accStack[this.accStack.length - 2][key] = append;
				}
			}
		}
		else if(evt == JSONTokenizer.prototype.EVENTS.ASCEND){
			if(this.accStack.length == 1){
				this.accEndCB && this.accEndCB(this.accStack[0]);
				this.accEndCB = null;
			}
			this.accStack.pop();
		}
		else if(evt == JSONTokenizer.prototype.EVENTS.VALUE){
			if(this.accStack[this.accStack.length - 1] instanceof Array){
				this.accStack[this.accStack.length - 1].push(value);
			}
			else{
				this.accStack[this.accStack.length - 1][key] = value;
			}
		}
		return;
	}


	if(this.stack.length == 0){
		if(t instanceof Function){
			this.stack.push(this.root);
		}
		else{
			if(evt != JSONTokenizer.prototype.EVENTS.DESCEND)
				throw new Error("Must DESCEND into Array or Map!");
			this._pushStack(this.root, key, value);
			return;
		}
	}

	var t = this._top();

	if(evt == JSONTokenizer.prototype.EVENTS.ASCEND){
		if(t instanceof Function){
			/* Keep the function from being classified as Object. */
		}
		else if(t instanceof Array){
			t[2] && t[2]();
		}
		else if(t instanceof Object){
			this._flushMapAcc(t);
			if("}" in t){
				t["}"]();
			}
		}

		this.stack.pop();
	}
	else if(evt == JSONTokenizer.prototype.EVENTS.DESCEND){
		if(t instanceof Function){
			/* Accumulate the value locally. */
			if(this.accEndCB)
				throw new Error("Error accEndCB already defined!");
			this.accStack.push("[" == value ? [] : {});
			this.accEndCB = function(data){
				t(data);
			}
		}
		else if(t instanceof Array){
			this._pushStack(t[1], null, value);
		}
		else if(t instanceof Object){
			var accEndCB = null;
			var appKey = null;

			var testkey = "." + key;
			if(testkey in t){
				this._flushMapAcc(t);
				if(t[testkey] instanceof Function){
					accEndCB = t[testkey];
				}
				else{
					appKey = testkey;
				}
			}
			else if("*" in t){
				if(t["*"] instanceof Function){
					/* Accumulate the value locally. */
					accEndCB = function(data){
						this.mapAcc[key] = data;
					}
				}
				else{
					appKey = "*";
				}
			}

			if(accEndCB){
				this.accStack.push("[" == value ? [] : {});
				this.accEndCB = accEndCB;
			}
			else if(appKey){
				this._pushStack(t[appKey], key, value);
			}
		}
	}
	else if(evt == JSONTokenizer.prototype.EVENTS.VALUE){
		if(t instanceof Function){
			t(value);
		}
		else if(t instanceof Array){
			t[1] && t[1](value);
		}
		else if(t instanceof Object){
			/* Key must be defined for descend or value. */
			if(null == key)
				throw new Error("Expected non-null key!");

			/* Check key specific function. */
			if(("." + key) in t){
				/* If value just pass it onto function. */
				if(evt == JSONTokenizer.prototype.EVENTS.VALUE){
					t["." + key](value);
				}
			}
			else if("*" in t){
				this.mapAcc[key] = value;
			}
		}
	}
}

JSONParser.prototype.getBindFn = function(){
	var parser = this;
	return function(e, k, v){
		parser.onEvent(e, k, v);
	};
}

if(exports){
	exports.JSONTokenizer = JSONTokenizer;
	exports.JSONParser = JSONParser;
}
