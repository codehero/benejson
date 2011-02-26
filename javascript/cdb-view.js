var sys = require("sys");
var BNJ = require("./benejson");

/* CouchDB view response parser to incrementally parse each row. */
var viewParser = {
	/* Force distinct callback for beginning of map.
	 * Key will be null here. */
	"{":function(key){
		sys.puts("Beginning couchdb response..");
	},

	/* Catchall for other data.
	 * Because we have an expectation of the couchdb view format, we know
	 * that the "rows" key will be last. */
	"*":function(data){
		sys.puts("Total Rows: " + data.total_rows);
		sys.puts("Offset : " + data.offset);
	},

	/* The '.' character prefixes a key.
	 * In this case, just match key "rows". It must be an array.
	 *
	 * -When specifying array callbacks:
	 *  The first element is the callback when the array starts (before 0th elem).
	 *  The second element is the callback for each element in the array
	 *  The third callback is for the array end.
	 *  The fourth callback is to handle a parsing exception found in a row. */
	".rows":[
		function(key){
			sys.puts("Starting Rows:");
		},
		function(row){
			sys.puts(row);
		},
		function(){
			sys.puts("Ended Rows.");
		}
	],

	/* Force distinct callback for end of map. */
	"}":function(){
		sys.puts("Ending couchdb response..");
	}
}

/*  */

var couchTest = new BNJ.JSONParser(viewParser);
var tokenizer = new BNJ.JSONTokenizer();
tokenizer.setCallback(btest.getBindFn());

var stdin = process.openStdin();
stdin.setEncoding("utf8");
stdin.on("data", function(chunk){
	tokenizer.process(chunk);
});
