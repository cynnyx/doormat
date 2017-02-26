var ServerMock = require("mock-http-server");
function logger_base( message ){
console.log(new Date().toString(), message);
}
function printMessage() {
logger_base("start listening...");
}
var server = new ServerMock({ host: "::1", port: 2003 });
server.on({
    method: 'GET',
    path: '/ciao',
    reply: {
        status:  200,
        headers: { "content-type": "application/json" },
        body:    JSON.stringify({ hello: "world" })
    }
});
    
server.start(printMessage);
