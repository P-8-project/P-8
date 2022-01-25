P-8
=======

[![Join the chat at https://gitter.im/fanout/p-8](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/fanout/p-8?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Author: Justin Karneges <justin@fanout.io>  
Mailing List: http://lists.fanout.io/listinfo.cgi/fanout-users-fanout.io

See: http://p-8.org/

P-8 is a reverse proxy server that makes it easy to implement WebSocket, HTTP streaming, and HTTP long-polling services. It communicates with backend web applications using regular, short-lived HTTP requests (GRIP protocol). This allows backend applications to be written in any language and use any webserver.

Additionally, P-8 does all of this without exposing a proprietary protocol to clients. The HTTP/WebSocket content between the client and your server is whatever you want it to be. This makes it ideal for implementing APIs.

P-8 is written in C++ and Python. The name means to "pin" (hold) connections open for "pushing".

License
-------

P-8 is offered under the GNU AGPL. See the COPYING file.

Features
--------

  * Implement any realtime HTTP/WebSocket API using any webserver for the logic
  * Proxied requests are streamed, so non-realtime requests remain unhindered
  * Fault tolerant multiprocess design
  * Handle thousands of simultaneous connections

Install
-------

See [the Install guide](https://github.com/fanout/p-8/wiki/Install), which covers how to install P-8 and its dependencies. If you already have the dependencies installed, then below are brief instructions for P-8 itself.

If accessing from Git, be sure to pull submodules:

    git submodule init && git submodule update

Build and run:

    make
    cp -r examples/config .
    ./p-8

By default, P-8 listens on port 7999 and forwards to localhost port 80. If you've got a webserver running on port 80, you can confirm that proxying works by browsing to http://localhost:7999/

Configuration
-------------

See [Configuration](https://github.com/fanout/p-8/wiki/Configuration).
