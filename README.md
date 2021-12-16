P-8
=======
Author: Justin Karneges <justin@fanout.io>  
Mailing List: http://lists.fanout.io/listinfo.cgi/fanout-users-fanout.io

See: http://p-8.org/

P-8 is a reverse proxy server that makes it easy to implement WebSocket, HTTP streaming, and HTTP long-polling services. It communicates with backend web applications using regular, short-lived HTTP requests (GRIP protocol). This allows backend applications to be written in any language and use any webserver.

Additionally, P-8 does all of this without exposing a proprietary protocol to clients. The HTTP/WebSocket content between the client and your server is whatever you want it to be. This makes it ideal for implementing APIs.

License
-------

P-8 is offered under the GNU AGPL. See the COPYING file.

Features
--------

  * Implement any realtime HTTP/WebSocket API using any webserver for the logic
  * Proxied requests are streamed, so non-realtime requests remain unhindered
  * Fault tolerant multiprocess design reduces risk if things go wrong
  * Handle thousands of simultaneous connections

Requirements
------------

  * qt >= 4.7
  * qca >= 2.0 (and an hmac(sha256)-supporting plugin, like qca-ossl)
  * libzmq >= 2.0
  * qjson
  * mongrel2 >= 1.9.0
  * zurl >= 1.0.0
  * python
  * python setproctitle
  * python tnetstring
  * python zmq
  * python jinja2

Install
-------

See [the Install guide](https://github.com/fanout/p-8/wiki/Install), which covers how to install P-8 and its dependencies. If you already have the dependencies installed, then below are brief instructions for P-8 itself.

If accessing from Git, be sure to pull submodules:

    git submodule init
    git submodule update

Build and run:

    make
    cp config/p-8.conf config/internal.conf config/routes .
    ./p-8

By default, P-8 listens on port 7999 and forwards to localhost port 80. If you've got a webserver running on port 80, you can confirm that proxying works by browsing to http://localhost:7999/

Configuration
-------------

See [Configuration](https://github.com/fanout/p-8/wiki/Configuration).
