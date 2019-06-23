# ToyHttpServer
## About
This is a Toy HTTP server with a Toy client. I implemented a simple HTTP client and server. The client is able to GET correctly from standard web servers, and browsers are be able to GET correctly from my server.

## Usage
#### Client
Your code should compile to an executable named http_client with the following usage:
`./http_client http://hostname[:port]/path/to/file`
For example:
`./http_client http://cs438.cs.illinois.edu/`
`./http_client http://localhost:5678/somedir/somefile.html`

If :port is not specified, default to port 80 – the standard port for HTTP.

The client sends an HTTP GET request based on the first argument it receives. The client will then
write the message body of the received response to a file named output. The client will also be able to handle redirects if the HTTP return code is redirections.

#### Handling Redirections
```
“The HTTP response status code 301 Moved Permanently is used for permanent URL redirection, meaning current
links or records using the URL that the response is received for should be updated. The new URL should be
provided in the Location field included with the response.”
```

#### Server
```
sudo ./http_server 80
./http_server 8888
```
The server handles HTTP GET requests by sending back HTTP responses.