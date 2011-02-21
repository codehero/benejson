var sys = require("sys");
var BNJ = require("./benejson");

/* Define an Array of Maps parser module. */
var arrMapParser = [
	function(key){
		sys.puts("ARRMAP: BEGIN");
	},
	{
		"{":function(key){
			sys.puts("ARRMAP: item begin");
		},
		"*":function(data){
			sys.puts("ARRMAP: data " + JSON.stringify(data));
		},
		"}":function(){
			sys.puts("ARRMAP: item end");
		}
	},
	function(){
		sys.puts("ARRMAP: END");
	}
];

var mapMapParser = {
	"{":function(key){
		sys.puts("MAPMAP_OUTER: BEGIN, key is " + key);
	},
	"*":{
		"{":function(key){
			sys.puts("MAPMAP_INNER: item begin, key is " + key);
		},
		"*":function(data){
			sys.puts("MAPMAP_INNER: data " + JSON.stringify(data));
		},
		"}":function(){
			sys.puts("MAPMAP_INNER: item end");
		}
	},
	"}":function(){
		sys.puts("MAPMAP_OUTER: END");
	}
};

var btest = new BNJ.JSONParser({
	"{":function(){
		sys.puts("BEGIN");
	},

	"*":function(data){
		sys.puts("MISC: " + JSON.stringify(data));
	},

	".header":function(data){
		sys.puts("HEADER: " + JSON.stringify(data));
	},

	".arr":[
		function(){
			sys.puts("ARR: BEGIN");
		},
		function(data){
			sys.puts("ARR: DATA " + JSON.stringify(data));
		},
		function(){
			sys.puts("ARR: END");
		}
	],

	/* Include Array of Maps parser module here. */
	".arrMap" : arrMapParser,

	".mapMap" : mapMapParser,

	"}":function(){
		sys.puts("END");
	}
});

var tokenizer = new BNJ.JSONTokenizer();
tokenizer.setCallback(btest.getBindFn());

var stdin = process.openStdin();
stdin.setEncoding("utf8");
stdin.on("data", function(chunk){
	tokenizer.process(chunk);
});
