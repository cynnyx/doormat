var express = require('express');
var serveIndex = require('serve-index');
var fs = require('fs');
var maxport = -1;
var apps = [];

function check_headers(headers) {
	var f = false;
	var server = false;
	var length = false;
	for ( var header in headers ) {
		if ( header == 'x-cyn-date' ) { 
			f = true; 
			length = headers[header].length == 10;
		}
		if ( header == 'Server' ) { server = 'Door-mat' == headers[header]; }
	}
	return ( f || server || length ) ? "Ok" : "Wrong";
}

function logger_base( message ){
	console.log(new Date().toString(), message);
}

function fill_headers_base( req, res, port ){
	var headers = req["headers"];
	var c = check_headers( headers );
	res.setHeader('Test-Result', c );
	res.setHeader('AppNum', port );
	for (var header in headers ){
		res.setHeader( "MIRROR_" + header, headers[header] );
	}
}

function responder_base( req, res, port ){
	fill_headers_base( req, res, port );
	var status = res.getHeader('Test-Result');
	res.send("What? Headers are " + status + " at \""+ req.path +"\"\nAppNum: " + port);
	logger_base("Answering to "+ req.method +" on port "+ port + " and path \"" + req.path + "\": status code " + res.statusCode );
}

function setApps( number ) {
	var app = express();
	var port = number + 2000;
	if ( port > maxport ) {
		maxport = port;
	}

	function responder( req, res ){
		responder_base( req, res, port);
	}

	function fill_headers( req, res ){
		fill_headers_base( req, res, port);
	}

	app.get('/', responder );

	app.post('/', responder );

	app.put('/', responder );

	app.head('/', responder );

	app.head('/chunked', function(req, res){
		res.setHeader('Content-Type', 'text/html; charset=UTF-8');
		res.setHeader('Transfer-Encoding', 'chunked');
		responder(req,res);
	});

	app.delete('/', responder );

	app.patch('/', responder );

	app.options('/', responder );

	app.get('/boat', function(req, res){
		res.statusCode = 200;
		res.setHeader('Content-Type', 'application/json; charset=utf-8');		
		fs.readFile('boat.json', function(err, data) {
	  		if (err) {
				throw err; 
	  		}
			res.end(data)
		});	
		logger_base("Answering to GET on port "+ port + " and path \"error\": status code " + res.statusCode );
	});

	app.get('/error', function(req, res){	
		res.statusCode = 404;
		res.end();
		logger_base("Answering to GET on port "+ port + " and path \"error\": status code " + res.statusCode );
	});

	app.get('/timeout', function(req, res){
		logger_base("Received GET req on port "+ port + " and path \"timeout\ : reply will not be sent");
	});

	app.get('/invalid', function(req, res){
		res.write(JSON.stringify({"success":true}));
		logger_base("Answering to GET on port "+ port + " and path \"invalid\ : EOM will not be sent");
	});

	app.get('/chunked', function(req, res){
		fill_headers(req, res);
		var html =
		'<!DOCTYPE html>' +
		'<html lang="en">' +
		'<head>' +
		'<meta charset="utf-8">' +
		'<title>Chunked transfer encoding test</title>' +
		'</head>' +
		'<body>';
		res.write(html);
		html = '<h1>Chunked transfer encoding test</h1>'
		res.write(html);
		logger_base("Answer to GET on port "+ port + " and path \"chunked\": 1st chunk");

		// this is a chunk of data sent to a client after 2 seconds but before the final chunk below.
		setTimeout(function(){
			html = '<h5>This is a chunked response after 2 seconds. Should be displayed before 5-second chunk arrives.</h5>'
			res.write(html);
		logger_base("Answer to GET on port "+ port + " and path \"chunked\": 2nd chunk");
		}, 2000);

		// Now imitate a long request which lasts 5 seconds.
		setTimeout(function(){
			html = '<h5>This is a chunked response after 5 seconds. The server should not close the stream before all chunks are sent to a client.</h5>'
			res.write(html);

			// since this is the last chunk, close the stream.
			html =
			'</body>' +
			'</html';

		res.end(html);
		logger_base("Finalized answer to GET on port "+ port + " and path \"chunked\": status code " + res.statusCode );
		}, 5000);
	});

	app.listen(port, function() {
		logger_base("Fake Server running on port " + port + "!");
	});
	
	app.use('/static', express.static('public'));
	app.use('/static', serveIndex('public'));
	
	apps.push( app );
	++number;
}

for ( var i = 0; i < 4; ++i )
	setApps( i );
