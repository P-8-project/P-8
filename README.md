P-8
=======

Website: http://p-8.org/  
Mailing List: http://lists.fanout.io/listinfo.cgi/fanout-users-fanout.io  
Chat Room: [![Join the chat at https://gitter.im/fanout/p-8](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/fanout/p-8?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

P-8 is a reverse proxy server written in C++ that makes it easy to implement WebSocket, HTTP streaming, and HTTP long-polling services. The project is unique among realtime push solutions in that it is designed to address the needs of API creators. P-8 is transparent to clients and integrates easily into an API stack.

How it works
------------

P-8 is placed in the network path between the backend and any clients:

<p align="center">
  <img src="docs/image/p-8-abstract.png" alt="p-8-abstract"/>
</p>

P-8 communicates with backend web applications using regular, short-lived HTTP requests. This allows backend applications to be written in any language and use any webserver. There are two main integration points:

1. The backend must handle proxied requests. For HTTP, each incoming request is proxied to the backend. For WebSockets, the activity of each connection is translated into a series of HTTP requests<sup>[1](#proxy-modes)</sup> sent to the backend. P-8's behavior is determined by how the backend responds to these requests.
2. The backend must tell P-8 to push data. Regardless of how clients are connected, data may be pushed to them by making an HTTP POST request to P-8's private control API (`http://localhost:5561/publish/` by default). P-8 will inject this data into any client connections as necessary.

To assist with integration, there are [libraries](http://p-8.org/docs/#libraries) for many backend languages and frameworks. P-8 has no libraries on the client side because it is transparent to clients.

Example
-------

To create an HTTP streaming connection, respond to a proxied request with `Grip-Hold` and `Grip-Channel`<sup>[2](#grip)</sup> headers:

```http
HTTP/1.1 200 OK
Grip-Hold: stream
Grip-Channel: test
Content-Type: text/plain
Content-Length: 22

welcome to the stream
```

When P-8 receives the above response from the backend, it will process it and send an initial response to the client that instead looks like this:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Transfer-Encoding: chunked
Connection: Transfer-Encoding

welcome to the stream
```

P-8 eats the GRIP headers and switches to chunked encoding (notice there's no `Content-Length`). The request between P-8 and the backend is now complete, but the request between the client and P-8 remains held open. The request is subscribed to a channel called `test`.

Data can then be pushed to the client by publishing data on the `test` channel:

```bash
curl -d '{ "items": [ { "channel": "test", "http-stream": \
    { "content": "hello there\n" } } ] }' \
    http://localhost:5561/publish
```

The client would then see the line "hello there" appended to the response stream. Ta-da, transparent realtime push!

For more details, see the [HTTP streaming](http://p-8.org/docs/#http-streaming) section of the documentation. P-8 also supports [HTTP long-polling](http://p-8.org/docs/#http-long-polling) and [WebSockets](http://p-8.org/docs/#websockets).

Why another realtime solution?
------------------------------

P-8 is an ambitious project with two primary goals:

* Make realtime API development easier. There are many other solutions out there that are excellent for building realtime apps, but few are useful within the context of *APIs*. For example, you can't use Socket.io to build Twitter's streaming API. A new kind of project is needed in this case.
* Make realtime push behavior delegable. The reason there isn't a realtime push CDN yet is because the standards and practices necessary for delegating to a third party in a transparent way are not yet established. P-8 is more than just another realtime push solution; it represents the next logical step in the evolution of realtime web architectures.

On a practical level, there are many benefits to P-8 that you don't see anywhere else:

* The proxy design allows P-8 to fit nicely within an API stack. This means it can inherit other facilities from your REST API, such as authentication, logging, throttling, etc. It can be combined with an API management system.
* As your API scales, a multi-tiered architecture will become inevitable. With P-8 you can easily do this from the start.
* It works well with microservices. Each microservice can have its own P-8 instance. No central bus needed.
* Hot reload. Restarting the backend doesn't disconnect clients.
* In the case of WebSocket messages being proxied out as HTTP requests, the messages may be handled statelessly by the backend. Messages from a single connection can even be load balanced across a set of backend instances.

Install
-------

Check out the [the Install guide](http://p-8.org/docs/#install), which covers how to install and run. There are packages available for Debian/Ubuntu and Homebrew (Mac), or you can build from source.

If you want to build the git version and have the dependencies installed already, then below are brief build instructions:

```
# pull submodules
git submodule init && git submodule update

# build
make

# copy default config
cp -r examples/config .

# run!
./p-8
```

By default, P-8 listens on port 7999 and forwards to localhost port 80. If you've got a webserver running on port 80, you can confirm that proxying works by browsing to `http://localhost:7999/`.

Scalability
-----------

P-8 is horizontally scalable. Instances don’t talk to each other, and sticky routing is not needed. Backends must publish data to all instances to ensure clients connected to any instance will receive the data. Most of the backend libraries support configuring more than one P-8 instance, so that a single publish call will send data to multiple instances at once.

Optionally, ZeroMQ PUB/SUB can be used to send data to P-8 instead of using HTTP POST. When this method is used, subscription information is forwarded to each publisher, such that data will only be published to instances that have listeners.

As for vertical scalability, P-8 has been tested reliably with 10,000 concurrent connections running on a single Amazon EC2 m3.xlarge instance. 20,000 connections and beyond are possible with some latency degradation. We definitely want to increase this number, but it is not high priority as P-8 is already horizontally scalable which is far more important.

What does the name mean?
------------------------

P-8 means to "pin" connections open for "pushing".

License
-------

P-8 is offered under the GNU AGPL. See the COPYING file.

Footnotes
---------

<a name="proxy-modes">1</a>: P-8 can communicate WebSocket activity to the backend using either HTTP or WebSockets. Conversion to HTTP is generally recommended as it makes the backend easier to reason about.

<a name="grip">2</a>: GRIP (Generic Realtime Intermediary Protocol) is the name of P-8's backend protocol. More about that [here](https://github.com/fanout/p-8/blob/master/docs/grip-protocol.md).
