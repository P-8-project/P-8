P-8
-------
Date: October 27th, 2013

Author: Justin Karneges <justin@fanout.io>

Mailing List: http://lists.fanout.io/listinfo.cgi/fanout-users-fanout.io

Read:
  * http://blog.fanout.io/2013/02/10/http-grip-proxy-hold-technique/
  * http://blog.fanout.io/2013/04/09/an-http-reverse-proxy-for-realtime/

P-8 is an HTTP reverse proxy server that makes it easy to implement streaming and long-polling services. It communicates with backend web applications using regular, short-lived HTTP requests (GRIP protocol). This allows the backend applications to be written in any language and use any webserver.

Additionally, P-8 does all of this without exposing a proprietary protocol to clients. The HTTP content between the client and your server is whatever you want it to be. This makes it ideal for implementing APIs.

License
-------

P-8 is offered under the GNU AGPL. See the COPYING file.

Features
--------

  * Implement any realtime HTTP API using any webserver for the logic
  * Proxied requests are streamed, so non-realtime requests remain unhindered
  * Fault tolerant multiprocess design reduces risk if things go wrong
  * Handle thousands of simultaneous connections

Requirements
------------

  * qt >= 4.7
  * qca >= 2.0 (and an hmac(sha256)-supporting plugin, like qca-ossl)
  * libzmq >= 2.0
  * qjson
  * mongrel2 (git release/1.9.0 or develop branch)
  * zurl (git v1.0.0 tag)
  * python
  * python setproctitle
  * python tnetstring
  * python zmq
  * python jinja2

Install guide
-------------

https://github.com/fanout/p-8/wiki/Install

If accessing from Git, be sure to pull submodules:

    git submodule init
    git submodule update

Build and run:

    make
    cp config/p-8.conf.example p-8.conf
    cp config/routes.example routes
    ./p-8

By default, P-8 listens on port 7999 and forwards to localhost port 80. If you've got a webserver running on port 80, you can confirm that proxying works by browsing to http://localhost:7999/

Multiprocess design
-------------------

P-8 consists of five processes: mongrel2, zurl, p-8-proxy, p-8-handler, and p-8 (the "runner"). In a basic setup you don't really need to think about this. Just run p-8 to start everything up, and terminate the process (or ctrl-c) to shut everything down.

If you'd prefer to individually manage any of these processes yourself, then adjust the "services" field in p-8.conf. You can even choose to not use the runner at all. In that case, P-8's own processes can be launched as follows:

Proxy process:

    proxy/p-8-proxy --config=/path/to/p-8.conf

Handler process:

    handler/p-8-handler --config=/path/to/p-8.conf
