/*
 * Copyright (C) 2020-2022 Fanout, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Note: Always Be Receiving (ABR)
//
// Connection handlers are expected to read ZHTTP messages as fast as
// possible. If they don't, the whole thread could stall. This is by design,
// to limit the number of to-be-processed messages in memory. They either
// need to do something immediately with the messages, or discard them.
//
// Every await point must ensure messages keep getting read/processed, by
// doing one of:
//
// - Directly awaiting a message.
// - Awaiting a select that is awaiting a message.
// - Wrapping other activity with discard_while().
// - Calling handle_other(), which itself will read messages.
// - Awaiting something known to not block.

use crate::arena;
use crate::buffer::{Buffer, LimitBufs, RefRead, RingBuffer, TmpBuffer, VECTORED_MAX};
use crate::future::{
    io_split, poll_async, select_2, select_3, select_4, select_5, select_option,
    AsyncLocalReceiver, AsyncLocalSender, AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt,
    CancellationToken, ReadHalf, Select2, Select3, Select4, Select5, StdWriteWrapper, Timeout,
    WriteHalf,
};
use crate::http1;
use crate::net::SocketAddr;
use crate::pin_mut;
use crate::reactor::Reactor;
use crate::websocket;
use crate::zhttppacket;
use arrayvec::{ArrayString, ArrayVec};
use log::debug;
use std::cell::{Ref, RefCell};
use std::cmp;
use std::collections::VecDeque;
use std::convert::TryFrom;
use std::future::Future;
use std::io;
use std::io::Write;
use std::pin::Pin;
use std::rc::Rc;
use std::str;
use std::str::FromStr;
use std::sync::mpsc;
use std::task::Context;
use std::task::Poll;
use std::time::Duration;

const URI_SIZE_MAX: usize = 4096;
const HEADERS_MAX: usize = 64;
const WS_HASH_INPUT_MAX: usize = 256;
const WS_ACCEPT_MAX: usize = 28; // base64_encode(sha1_hash) = 28 bytes
const ZHTTP_SESSION_TIMEOUT: Duration = Duration::from_secs(60);

pub trait CidProvider {
    fn get_new_assigned_cid(&mut self) -> ArrayString<32>;
}

pub trait Identify {
    fn set_id(&mut self, id: &str);
}

#[derive(PartialEq)]
enum Mode {
    HttpReq,
    HttpStream,
    WebSocket,
}

fn get_host<'a>(headers: &'a [httparse::Header]) -> &'a str {
    for h in headers.iter() {
        if h.name.eq_ignore_ascii_case("Host") {
            match str::from_utf8(h.value) {
                Ok(s) => return s,
                Err(_) => break,
            }
        }
    }

    "localhost"
}

fn calculate_ws_accept(key: &[u8]) -> Result<ArrayString<WS_ACCEPT_MAX>, ()> {
    let input_len = key.len() + websocket::WS_GUID.len();

    if input_len > WS_HASH_INPUT_MAX {
        return Err(());
    }

    let mut input = [0; WS_HASH_INPUT_MAX];

    input[..key.len()].copy_from_slice(key);
    input[key.len()..input_len].copy_from_slice(&websocket::WS_GUID.as_bytes());

    let input = &input[..input_len];

    let digest = sha1::Sha1::from(input).digest();

    let mut output = [0; WS_ACCEPT_MAX];

    let size = base64::encode_config_slice(&digest.bytes(), base64::STANDARD, &mut output);

    let output = match str::from_utf8(&output[..size]) {
        Ok(s) => s,
        Err(_) => return Err(()),
    };

    Ok(ArrayString::from_str(output).unwrap())
}

fn make_zhttp_request(
    instance: &str,
    ids: &[zhttppacket::Id],
    method: &str,
    path: &str,
    headers: &[httparse::Header],
    body: &[u8],
    more: bool,
    mode: Mode,
    credits: u32,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    packet_buf: &mut [u8],
) -> Result<zmq::Message, io::Error> {
    let mut data = zhttppacket::RequestData::new();

    data.method = method;

    let host = get_host(headers);

    let mut zheaders = [zhttppacket::EMPTY_HEADER; HEADERS_MAX];
    let mut zheaders_len = 0;

    for h in headers.iter() {
        zheaders[zheaders_len] = zhttppacket::Header {
            name: h.name,
            value: h.value,
        };
        zheaders_len += 1;
    }
    data.headers = &zheaders[..zheaders_len];

    let scheme = match mode {
        Mode::HttpReq | Mode::HttpStream => {
            if secure {
                "https"
            } else {
                "http"
            }
        }
        Mode::WebSocket => {
            if secure {
                "wss"
            } else {
                "ws"
            }
        }
    };

    let mut uri = [0; URI_SIZE_MAX];
    let mut c = io::Cursor::new(&mut uri[..]);

    write!(&mut c, "{}://{}{}", scheme, host, path)?;

    let size = c.position() as usize;

    data.uri = match str::from_utf8(&uri[..size]) {
        Ok(s) => s,
        Err(_) => return Err(io::Error::from(io::ErrorKind::InvalidData)),
    };

    data.body = body;
    data.more = more;

    if mode == Mode::HttpStream {
        data.stream = true;
    }

    data.credits = credits;

    let mut addr = [0; 128];

    if let Some(SocketAddr::Ip(peer_addr)) = peer_addr {
        let mut c = io::Cursor::new(&mut addr[..]);
        write!(&mut c, "{}", peer_addr.ip()).unwrap();
        let size = c.position() as usize;

        data.peer_address = str::from_utf8(&addr[..size]).unwrap();
        data.peer_port = peer_addr.port();
    }

    let mut zreq = zhttppacket::Request::new_data(instance.as_bytes(), &ids, data);
    zreq.multi = true;

    let size = zreq.serialize(packet_buf)?;

    Ok(zmq::Message::from(&packet_buf[..size]))
}

#[derive(Debug)]
enum ServerError {
    Io(io::Error),
    Utf8(str::Utf8Error),
    Http(http1::ServerError),
    WebSocket(websocket::Error),
    InvalidWebSocketRequest,
    BadMessage,
    HandlerError,
    HandlerCancel,
    BufferExceeded,
    BadFrame,
    Timeout,
    Stopped,
}

impl From<io::Error> for ServerError {
    fn from(e: io::Error) -> Self {
        Self::Io(e)
    }
}

impl From<str::Utf8Error> for ServerError {
    fn from(e: str::Utf8Error) -> Self {
        Self::Utf8(e)
    }
}

impl<T> From<mpsc::SendError<T>> for ServerError {
    fn from(_e: mpsc::SendError<T>) -> Self {
        Self::Io(io::Error::from(io::ErrorKind::BrokenPipe))
    }
}

impl<T> From<mpsc::TrySendError<T>> for ServerError {
    fn from(e: mpsc::TrySendError<T>) -> Self {
        let kind = match e {
            mpsc::TrySendError::Full(_) => io::ErrorKind::WriteZero,
            mpsc::TrySendError::Disconnected(_) => io::ErrorKind::BrokenPipe,
        };

        Self::Io(io::Error::from(kind))
    }
}

impl From<mpsc::RecvError> for ServerError {
    fn from(_e: mpsc::RecvError) -> Self {
        Self::Io(io::Error::from(io::ErrorKind::UnexpectedEof))
    }
}

impl From<http1::ServerError> for ServerError {
    fn from(e: http1::ServerError) -> Self {
        Self::Http(e)
    }
}

impl From<websocket::Error> for ServerError {
    fn from(e: websocket::Error) -> Self {
        Self::WebSocket(e)
    }
}

// our own range-like struct that supports copying
#[derive(Clone, Copy, Default)]
struct Range {
    start: u32,
    end: u32,
}

impl Range {
    fn from_slice<T: AsRef<[u8]>>(base: &[u8], s: T) -> Self {
        let sref = s.as_ref();
        let start = (sref.as_ptr() as usize) - (base.as_ptr() as usize);
        let end = start + sref.len();

        // panic if the indexes don't fit in a u32 (4GB). nobody should ever
        // set such a large buffer size, so this should never happen
        let start = u32::try_from(start).expect("start index greater than u32");
        let end = u32::try_from(end).expect("end index greater than u32");

        Self {
            start: start as u32,
            end: end as u32,
        }
    }

    fn to_slice<'a>(&self, base: &'a [u8]) -> &'a [u8] {
        &base[(self.start as usize)..(self.end as usize)]
    }

    unsafe fn to_str_unchecked<'a>(&self, base: &'a [u8]) -> &'a str {
        str::from_utf8_unchecked(&base[(self.start as usize)..(self.end as usize)])
    }
}

#[derive(Clone, Copy, Default)]
struct HeaderRanges {
    name: Range,
    value: Range,
}

const EMPTY_HEADER_RANGES: HeaderRanges = HeaderRanges {
    name: Range { start: 0, end: 0 },
    value: Range { start: 0, end: 0 },
};

#[derive(Clone, Copy)]
struct RequestHeaderRanges {
    method: Range,
    uri: Range,
    headers: [HeaderRanges; HEADERS_MAX],
    headers_count: usize,
}

impl Default for RequestHeaderRanges {
    fn default() -> Self {
        Self {
            method: Range::default(),
            uri: Range::default(),
            headers: [EMPTY_HEADER_RANGES; HEADERS_MAX],
            headers_count: 0,
        }
    }
}

#[derive(Clone, Copy)]
struct MessageItem {
    mtype: u8,
    avail: usize,
}

struct MessageTracker {
    items: VecDeque<MessageItem>,
    last_partial: bool,
}

impl MessageTracker {
    fn new(max_messages: usize) -> Self {
        Self {
            items: VecDeque::with_capacity(max_messages),
            last_partial: false,
        }
    }

    fn in_progress(&self) -> bool {
        self.last_partial
    }

    fn start(&mut self, mtype: u8) -> Result<(), ()> {
        if self.last_partial || self.items.len() == self.items.capacity() {
            return Err(());
        }

        self.items.push_back(MessageItem { mtype, avail: 0 });

        self.last_partial = true;

        Ok(())
    }

    fn extend(&mut self, amt: usize) {
        assert_eq!(self.last_partial, true);

        self.items.back_mut().unwrap().avail += amt;
    }

    fn done(&mut self) {
        self.last_partial = false;
    }

    // type, avail, done
    fn current(&self) -> Option<(u8, usize, bool)> {
        if self.items.len() > 1 {
            let m = self.items.front().unwrap();
            Some((m.mtype, m.avail, true))
        } else if self.items.len() == 1 {
            let m = self.items.front().unwrap();
            Some((m.mtype, m.avail, !self.last_partial))
        } else {
            None
        }
    }

    fn consumed(&mut self, amt: usize, done: bool) {
        assert!(amt <= self.items[0].avail);

        self.items[0].avail -= amt;

        if done {
            assert_eq!(self.items[0].avail, 0);

            self.items.pop_front().unwrap();
        }
    }
}

struct ServerStreamSharedDataInner {
    to_addr: Option<ArrayVec<u8, 64>>,
    out_seq: u32,
}

pub struct AddrRef<'a> {
    s: Ref<'a, ServerStreamSharedDataInner>,
}

impl<'a> AddrRef<'a> {
    pub fn get(&self) -> Option<&[u8]> {
        match &self.s.to_addr {
            Some(addr) => Some(addr.as_ref()),
            None => None,
        }
    }
}

pub struct ServerStreamSharedData {
    inner: RefCell<ServerStreamSharedDataInner>,
}

impl ServerStreamSharedData {
    pub fn new() -> Self {
        Self {
            inner: RefCell::new(ServerStreamSharedDataInner {
                to_addr: None,
                out_seq: 0,
            }),
        }
    }

    fn reset(&self) {
        let s = &mut *self.inner.borrow_mut();

        s.to_addr = None;
        s.out_seq = 0;
    }

    fn set_to_addr(&self, addr: Option<ArrayVec<u8, 64>>) {
        let s = &mut *self.inner.borrow_mut();

        s.to_addr = addr;
    }

    pub fn to_addr(&self) -> AddrRef {
        AddrRef {
            s: self.inner.borrow(),
        }
    }

    pub fn out_seq(&self) -> u32 {
        self.inner.borrow().out_seq
    }

    pub fn inc_out_seq(&self) {
        let s = &mut *self.inner.borrow_mut();

        s.out_seq += 1;
    }
}

async fn recv_nonzero<R: AsyncRead>(r: &mut R, buf: &mut RingBuffer) -> Result<(), io::Error> {
    if buf.write_avail() == 0 {
        return Err(io::Error::from(io::ErrorKind::WriteZero));
    }

    let size = match r.read(buf.write_buf()).await {
        Ok(size) => size,
        Err(e) => return Err(e),
    };

    buf.write_commit(size);

    if size == 0 {
        return Err(io::Error::from(io::ErrorKind::UnexpectedEof));
    }

    Ok(())
}

struct LimitedRingBuffer<'a> {
    inner: &'a mut RingBuffer,
    limit: usize,
}

impl AsRef<[u8]> for LimitedRingBuffer<'_> {
    fn as_ref(&self) -> &[u8] {
        let buf = self.inner.read_buf();
        let limit = cmp::min(buf.len(), self.limit);

        &buf[..limit]
    }
}

struct HttpRead<'a, R: AsyncRead> {
    stream: ReadHalf<'a, R>,
    buf1: &'a mut RingBuffer,
    buf2: &'a mut RingBuffer,
}

struct HttpWrite<'a, W: AsyncWrite> {
    stream: WriteHalf<'a, W>,
}

struct RequestHandler<'a, R: AsyncRead, W: AsyncWrite> {
    r: HttpRead<'a, R>,
    w: HttpWrite<'a, W>,
}

impl<'a, R: AsyncRead, W: AsyncWrite> RequestHandler<'a, R, W> {
    fn new(
        stream: (ReadHalf<'a, R>, WriteHalf<'a, W>),
        buf1: &'a mut RingBuffer,
        buf2: &'a mut RingBuffer,
    ) -> Self {
        buf1.align();
        buf2.clear();

        Self {
            r: HttpRead {
                stream: stream.0,
                buf1,
                buf2,
            },
            w: HttpWrite { stream: stream.1 },
        }
    }

    // read from stream into buf, and parse buf as a request header
    async fn recv_request(
        mut self,
        ranges: &'a mut RequestHeaderRanges,
    ) -> Result<RequestHeader<'a, R, W>, ServerError> {
        let mut protocol = http1::ServerProtocol::new();

        assert_eq!(protocol.state(), http1::ServerState::ReceivingRequest);

        loop {
            {
                let mut hbuf = io::Cursor::new(self.r.buf1.read_buf());

                let mut headers = [httparse::EMPTY_HEADER; HEADERS_MAX];

                if let Some(ret) = protocol.recv_request(&mut hbuf, &mut headers) {
                    let req = match ret {
                        Ok(req) => req,
                        Err(e) => return Err(e.into()),
                    };

                    assert!([
                        http1::ServerState::ReceivingBody,
                        http1::ServerState::AwaitingResponse
                    ]
                    .contains(&protocol.state()));

                    let hsize = hbuf.position() as usize;

                    let hbuf = self.r.buf1.read_buf();

                    *ranges = RequestHeaderRanges {
                        method: Range::from_slice(hbuf, req.method),
                        uri: Range::from_slice(hbuf, req.uri),
                        headers: [EMPTY_HEADER_RANGES; HEADERS_MAX],
                        headers_count: req.headers.len(),
                    };

                    for (i, h) in req.headers.iter().enumerate() {
                        ranges.headers[i] = HeaderRanges {
                            name: Range::from_slice(hbuf, h.name),
                            value: Range::from_slice(hbuf, h.value),
                        };
                    }

                    let body_size = req.body_size;
                    let expect_100 = req.expect_100;

                    break Ok(RequestHeader {
                        r: self.r,
                        w: self.w,
                        protocol,
                        hsize,
                        ranges,
                        body_size,
                        expect_100,
                    });
                }
            }

            if let Err(e) = recv_nonzero(&mut self.r.stream, self.r.buf1).await {
                if e.kind() == io::ErrorKind::WriteZero {
                    return Err(ServerError::BufferExceeded);
                }

                return Err(e.into());
            }
        }
    }
}

fn request_from_ranges<'buf, 'headers>(
    buf: &'buf [u8],
    ranges: &RequestHeaderRanges,
    body_size: http1::BodySize,
    expect_100: bool,
    scratch: &'headers mut [httparse::Header<'buf>; HEADERS_MAX],
) -> http1::Request<'buf, 'headers> {
    let method = unsafe { ranges.method.to_str_unchecked(buf) };
    let uri = unsafe { ranges.uri.to_str_unchecked(buf) };

    let ranges_headers = &ranges.headers[..ranges.headers_count];

    for (i, h) in ranges_headers.iter().enumerate() {
        scratch[i].name = unsafe { h.name.to_str_unchecked(buf) };
        scratch[i].value = h.value.to_slice(buf);
    }

    let headers = &scratch[..ranges.headers_count];

    http1::Request {
        method,
        uri,
        headers,
        body_size,
        expect_100,
    }
}

struct RequestHeader<'a, R: AsyncRead, W: AsyncWrite> {
    r: HttpRead<'a, R>,
    w: HttpWrite<'a, W>,
    protocol: http1::ServerProtocol,
    hsize: usize,
    ranges: &'a mut RequestHeaderRanges,
    body_size: http1::BodySize,
    expect_100: bool,
}

impl<'buf, 'a: 'buf, R: AsyncRead, W: AsyncWrite> RequestHeader<'a, R, W> {
    fn request<'headers>(
        &'buf self,
        scratch: &'headers mut [httparse::Header<'buf>; HEADERS_MAX],
    ) -> http1::Request<'buf, 'headers> {
        request_from_ranges(
            self.r.buf1.read_buf(),
            self.ranges,
            self.body_size,
            self.expect_100,
            scratch,
        )
    }

    async fn start_recv_body(self) -> Result<RequestRecvBody<'a, R, W>, ServerError> {
        let (recv_body, _) = self.start_recv_body_inner().await?;

        Ok(recv_body)
    }

    async fn start_recv_body_and_keep_header(
        mut self,
    ) -> Result<RequestRecvBodyKeepHeader<'a, R, W>, ServerError> {
        self.move_header()?;

        let body_size = self.body_size;
        let expect_100 = self.expect_100;

        let (recv_body, ranges) = self.start_recv_body_inner().await?;

        Ok(RequestRecvBodyKeepHeader {
            inner: recv_body,
            ranges,
            body_size,
            expect_100,
        })
    }

    fn recv_done(mut self) -> Result<RequestStartResponse<'a, R, W>, ServerError> {
        self.clear_header();

        Ok(RequestStartResponse::new(self.r, self.w, self.protocol))
    }

    async fn start_recv_body_inner(
        mut self,
    ) -> Result<(RequestRecvBody<'a, R, W>, &'a mut RequestHeaderRanges), ServerError> {
        self.clear_header();

        if self.expect_100 {
            let mut cont = [0; 32];

            let cont = {
                let mut c = io::Cursor::new(&mut cont[..]);

                if let Err(e) = self.protocol.send_100_continue(&mut c) {
                    return Err(e.into());
                }

                let size = c.position() as usize;

                &cont[..size]
            };

            let mut left = cont.len();

            while left > 0 {
                let pos = cont.len() - left;

                let size = match self.w.stream.write(&cont[pos..]).await {
                    Ok(size) => size,
                    Err(e) => return Err(e.into()),
                };

                left -= size;
            }
        }

        Ok((
            RequestRecvBody {
                r: RefCell::new(RecvBodyRead {
                    stream: self.r.stream,
                    buf: self.r.buf1,
                }),
                wstream: self.w.stream,
                buf2: self.r.buf2,
                protocol: RefCell::new(self.protocol),
            },
            self.ranges,
        ))
    }

    fn move_header(&mut self) -> Result<(), ServerError> {
        let hbuf = self.r.buf1.read_buf();

        // move header data to buf2
        if let Err(e) = self.r.buf2.write_all(&hbuf[..self.hsize]) {
            return Err(e.into());
        }

        self.clear_header();

        Ok(())
    }

    fn clear_header(&mut self) {
        self.r.buf1.read_commit(self.hsize);
        self.hsize = 0;
    }
}

struct RecvBodyRead<'a, R: AsyncRead> {
    stream: ReadHalf<'a, R>,
    buf: &'a mut RingBuffer,
}

struct RequestRecvBody<'a, R: AsyncRead, W: AsyncWrite> {
    r: RefCell<RecvBodyRead<'a, R>>,
    wstream: WriteHalf<'a, W>,
    buf2: &'a mut RingBuffer,
    protocol: RefCell<http1::ServerProtocol>,
}

impl<'a, R: AsyncRead, W: AsyncWrite> RequestRecvBody<'a, R, W> {
    fn more(&self) -> bool {
        self.protocol.borrow().state() == http1::ServerState::ReceivingBody
    }

    async fn recv_body(&self, dest: &mut [u8]) -> Result<usize, ServerError> {
        let r = &mut *self.r.borrow_mut();
        let protocol = &mut *self.protocol.borrow_mut();

        if protocol.state() == http1::ServerState::ReceivingBody {
            r.buf.align();

            loop {
                let (size, read_size) = {
                    let mut buf = io::Cursor::new(r.buf.read_buf());

                    let mut headers = [httparse::EMPTY_HEADER; HEADERS_MAX];

                    let (size, _) = protocol.recv_body(&mut buf, dest, &mut headers)?;

                    let read_size = buf.position() as usize;

                    (size, read_size)
                };

                if protocol.state() == http1::ServerState::ReceivingBody && read_size == 0 {
                    if let Err(e) = recv_nonzero(&mut r.stream, r.buf).await {
                        if e.kind() == io::ErrorKind::WriteZero {
                            return Err(ServerError::BufferExceeded);
                        }

                        return Err(e.into());
                    }

                    continue;
                }

                r.buf.read_commit(read_size);

                return Ok(size);
            }
        }

        assert_eq!(protocol.state(), http1::ServerState::AwaitingResponse);

        Ok(0)
    }

    async fn recv_body_shared<'b, B, F>(
        &self,
        dest: &RefCell<B>,
        limit: usize,
        bytes_read: &F,
    ) -> Result<usize, ServerError>
    where
        B: AsMut<[u8]>,
        F: Fn(),
    {
        let r = &mut *self.r.borrow_mut();
        let protocol = &mut *self.protocol.borrow_mut();

        if protocol.state() == http1::ServerState::ReceivingBody {
            r.buf.align();

            loop {
                let (size, read_size) = {
                    let mut buf = io::Cursor::new(r.buf.read_buf());

                    let mut headers = [httparse::EMPTY_HEADER; HEADERS_MAX];

                    let (size, _) = {
                        let dest = &mut *dest.borrow_mut();

                        protocol.recv_body(&mut buf, &mut dest.as_mut()[..limit], &mut headers)?
                    };

                    let read_size = buf.position() as usize;
                    r.buf.read_commit(read_size);

                    (size, read_size)
                };

                if protocol.state() == http1::ServerState::ReceivingBody && read_size == 0 {
                    if let Err(e) = recv_nonzero(&mut r.stream, r.buf).await {
                        if e.kind() == io::ErrorKind::WriteZero {
                            return Err(ServerError::BufferExceeded);
                        }

                        return Err(e.into());
                    }

                    continue;
                }

                bytes_read();

                return Ok(size);
            }
        }

        assert_eq!(protocol.state(), http1::ServerState::AwaitingResponse);

        Ok(0)
    }

    fn recv_done(self) -> RequestStartResponse<'a, R, W> {
        let r = self.r.into_inner();

        RequestStartResponse::new(
            HttpRead {
                stream: r.stream,
                buf1: r.buf,
                buf2: self.buf2,
            },
            HttpWrite {
                stream: self.wstream,
            },
            self.protocol.into_inner(),
        )
    }
}

struct RequestRecvBodyKeepHeader<'a, R: AsyncRead, W: AsyncWrite> {
    inner: RequestRecvBody<'a, R, W>,
    ranges: &'a RequestHeaderRanges,
    body_size: http1::BodySize,
    expect_100: bool,
}

impl<'buf, 'a: 'buf, R: AsyncRead, W: AsyncWrite> RequestRecvBodyKeepHeader<'a, R, W> {
    fn request<'headers>(
        &'buf self,
        scratch: &'headers mut [httparse::Header<'buf>; HEADERS_MAX],
    ) -> http1::Request<'buf, 'headers> {
        request_from_ranges(
            self.inner.buf2.read_buf(),
            self.ranges,
            self.body_size,
            self.expect_100,
            scratch,
        )
    }

    async fn recv_body(&self, dest: &mut [u8]) -> Result<usize, ServerError> {
        self.inner.recv_body(dest).await
    }

    fn recv_done(self) -> RequestStartResponse<'a, R, W> {
        self.inner.recv_done()
    }
}

struct RequestStartResponse<'a, R: AsyncRead, W: AsyncWrite> {
    r: HttpRead<'a, R>,
    w: HttpWrite<'a, W>,
    protocol: http1::ServerProtocol,
}

impl<'a, R: AsyncRead, W: AsyncWrite> RequestStartResponse<'a, R, W> {
    fn new(r: HttpRead<'a, R>, w: HttpWrite<'a, W>, protocol: http1::ServerProtocol) -> Self {
        Self { r, w, protocol }
    }

    async fn fill_recv_buffer(&mut self) -> ServerError {
        loop {
            if let Err(e) = recv_nonzero(&mut self.r.stream, self.r.buf1).await {
                if e.kind() == io::ErrorKind::WriteZero {
                    // if there's no more space, suspend forever
                    let () = std::future::pending().await;
                }

                return e.into();
            }
        }
    }

    fn prepare_response(
        mut self,
        code: u32,
        reason: &str,
        headers: &[http1::Header<'_>],
        body_size: http1::BodySize,
    ) -> Result<RequestSendHeader<'a, R, W>, ServerError> {
        self.r.buf2.clear();

        let mut hbuf = io::Cursor::new(self.r.buf2.write_buf());

        if let Err(e) = self
            .protocol
            .send_response(&mut hbuf, code, reason, headers, body_size)
        {
            return Err(e.into());
        }

        let size = hbuf.position() as usize;
        self.r.buf2.write_commit(size);

        let (stream, buf1, buf2) = ((self.r.stream, self.w.stream), self.r.buf1, self.r.buf2);

        Ok(RequestSendHeader::new(
            stream,
            buf1,
            buf2,
            self.protocol,
            size,
        ))
    }
}

struct SendHeaderRead<'a, R: AsyncRead> {
    stream: ReadHalf<'a, R>,
    buf: &'a mut RingBuffer,
}

struct EarlyBody {
    overflow: Option<Buffer>,
    done: bool,
}

struct RequestSendHeader<'a, R: AsyncRead, W: AsyncWrite> {
    r: RefCell<SendHeaderRead<'a, R>>,
    wstream: RefCell<WriteHalf<'a, W>>,
    wbuf: RefCell<LimitedRingBuffer<'a>>,
    protocol: http1::ServerProtocol,
    early_body: RefCell<EarlyBody>,
}

impl<'a, R: AsyncRead, W: AsyncWrite> RequestSendHeader<'a, R, W> {
    fn new(
        stream: (ReadHalf<'a, R>, WriteHalf<'a, W>),
        buf1: &'a mut RingBuffer,
        buf2: &'a mut RingBuffer,
        protocol: http1::ServerProtocol,
        header_size: usize,
    ) -> Self {
        Self {
            r: RefCell::new(SendHeaderRead {
                stream: stream.0,
                buf: buf1,
            }),
            wstream: RefCell::new(stream.1),
            wbuf: RefCell::new(LimitedRingBuffer {
                inner: buf2,
                limit: header_size,
            }),
            protocol,
            early_body: RefCell::new(EarlyBody {
                overflow: None,
                done: false,
            }),
        }
    }

    async fn send_header(&self) -> Result<(), ServerError> {
        let mut stream = self.wstream.borrow_mut();

        // limit = header bytes left
        while self.wbuf.borrow().limit > 0 {
            let size = stream.write_shared(&self.wbuf).await?;

            let mut wbuf = self.wbuf.borrow_mut();

            wbuf.inner.read_commit(size);
            wbuf.limit -= size;
        }

        let mut wbuf = self.wbuf.borrow_mut();
        let mut early_body = self.early_body.borrow_mut();

        if let Some(overflow) = &mut early_body.overflow {
            wbuf.inner.write(overflow.read_buf())?;

            early_body.overflow = None;
        }

        Ok(())
    }

    fn append_body(&self, body: &[u8], more: bool, id: &str) -> Result<(), ServerError> {
        let mut wbuf = self.wbuf.borrow_mut();
        let mut early_body = self.early_body.borrow_mut();

        // limit = header bytes left
        if wbuf.limit > 0 {
            // if there are still header bytes in the buffer, then we may
            // need to overflow into a separate buffer if there's not enough
            // room

            let accepted = if early_body.overflow.is_none() {
                wbuf.inner.write(body)?
            } else {
                0
            };

            if accepted < body.len() {
                debug!("conn {}: overflowing {} bytes", id, body.len() - accepted);

                if early_body.overflow.is_none() {
                    // only allow overflowing as much as there are header
                    // bytes left
                    early_body.overflow = Some(Buffer::new(wbuf.limit));
                }

                let overflow = early_body.overflow.as_mut().unwrap();

                overflow.write_all(&body[accepted..])?;
            }
        } else {
            // if the header has been fully cleared from the buffer, then
            // always write directly to the buffer
            wbuf.inner.write_all(body)?;
        }

        early_body.done = !more;

        Ok(())
    }

    fn send_header_done(self) -> RequestSendBody<'a, R, W> {
        let r = self.r.into_inner();
        let wstream = self.wstream.into_inner();
        let wbuf = self.wbuf.into_inner();
        let early_body = self.early_body.borrow();

        assert_eq!(wbuf.limit, 0);
        assert_eq!(early_body.overflow.is_none(), true);

        let (stream, buf1, buf2) = { ((r.stream, wstream), r.buf, wbuf.inner) };

        RequestSendBody {
            r: RefCell::new(HttpSendBodyRead {
                stream: stream.0,
                buf: buf1,
            }),
            w: RefCell::new(HttpSendBodyWrite {
                stream: stream.1,
                buf: buf2,
                body_done: early_body.done,
            }),
            protocol: RefCell::new(self.protocol),
        }
    }
}

struct HttpSendBodyRead<'a, R: AsyncRead> {
    stream: ReadHalf<'a, R>,
    buf: &'a mut RingBuffer,
}

struct HttpSendBodyWrite<'a, W: AsyncWrite> {
    stream: WriteHalf<'a, W>,
    buf: &'a mut RingBuffer,
    body_done: bool,
}

struct SendBodyFuture<'a, 'b, W: AsyncWrite> {
    w: &'a RefCell<HttpSendBodyWrite<'b, W>>,
    protocol: &'a RefCell<http1::ServerProtocol>,
}

impl<'a, 'b, W: AsyncWrite> Future for SendBodyFuture<'a, 'b, W> {
    type Output = Result<usize, ServerError>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Self::Output> {
        let f = &*self;

        let w = &mut *f.w.borrow_mut();

        let stream = &mut w.stream;

        if !stream.is_writable() {
            return Poll::Pending;
        }

        let protocol = &mut *f.protocol.borrow_mut();

        let mut buf_arr = [&b""[..]; VECTORED_MAX - 2];
        let bufs = w.buf.get_ref_vectored(&mut buf_arr);

        match protocol.send_body(
            &mut StdWriteWrapper::new(Pin::new(&mut w.stream), cx),
            bufs,
            w.body_done,
            None,
        ) {
            Ok(size) => Poll::Ready(Ok(size)),
            Err(http1::ServerError::Io(e)) if e.kind() == io::ErrorKind::WouldBlock => {
                Poll::Pending
            }
            Err(e) => Poll::Ready(Err(e.into())),
        }
    }
}

impl<W: AsyncWrite> Drop for SendBodyFuture<'_, '_, W> {
    fn drop(&mut self) {
        self.w.borrow_mut().stream.cancel();
    }
}

struct RequestSendBody<'a, R: AsyncRead, W: AsyncWrite> {
    r: RefCell<HttpSendBodyRead<'a, R>>,
    w: RefCell<HttpSendBodyWrite<'a, W>>,
    protocol: RefCell<http1::ServerProtocol>,
}

impl<'a, R: AsyncRead, W: AsyncWrite> RequestSendBody<'a, R, W> {
    fn append_body(&self, body: &[u8], more: bool) -> Result<(), ServerError> {
        let w = &mut *self.w.borrow_mut();

        w.buf.write_all(body)?;
        w.body_done = !more;

        Ok(())
    }

    fn can_flush(&self) -> bool {
        let w = &*self.w.borrow();

        w.buf.read_avail() > 0 || w.body_done
    }

    async fn flush_body(&self) -> Result<(usize, bool), ServerError> {
        {
            let protocol = &*self.protocol.borrow();

            assert_eq!(protocol.state(), http1::ServerState::SendingBody);

            let w = &*self.w.borrow();

            if w.buf.read_avail() == 0 && !w.body_done {
                return Ok((0, false));
            }
        }

        let size = SendBodyFuture {
            w: &self.w,
            protocol: &self.protocol,
        }
        .await?;

        let w = &mut *self.w.borrow_mut();
        let protocol = &*self.protocol.borrow();

        w.buf.read_commit(size);

        if w.buf.read_avail() > 0
            || !w.body_done
            || protocol.state() == http1::ServerState::SendingBody
        {
            return Ok((size, false));
        }

        assert_eq!(protocol.state(), http1::ServerState::Finished);

        Ok((size, true))
    }

    async fn send_body(&self, body: &[u8], more: bool) -> Result<usize, ServerError> {
        let w = &mut *self.w.borrow_mut();
        let protocol = &mut *self.protocol.borrow_mut();

        assert_eq!(protocol.state(), http1::ServerState::SendingBody);

        Ok(protocol
            .send_body_async(&mut w.stream, &[body], !more, None)
            .await?)
    }

    async fn fill_recv_buffer(&self) -> ServerError {
        let r = &mut *self.r.borrow_mut();

        loop {
            if let Err(e) = recv_nonzero(&mut r.stream, r.buf).await {
                if e.kind() == io::ErrorKind::WriteZero {
                    // if there's no more space, suspend forever
                    let () = std::future::pending().await;
                }

                return e.into();
            }
        }
    }

    fn finish(self) -> bool {
        self.protocol.borrow().is_persistent()
    }
}

struct WebSocketRead<'a, R: AsyncRead> {
    stream: ReadHalf<'a, R>,
    buf: &'a mut RingBuffer,
}

struct WebSocketWrite<'a, W: AsyncWrite> {
    stream: WriteHalf<'a, W>,
    buf: &'a mut RingBuffer,
}

struct SendMessageContentFuture<'a, 'b, W: AsyncWrite> {
    w: &'a RefCell<WebSocketWrite<'b, W>>,
    protocol: &'a websocket::Protocol,
    avail: usize,
    done: bool,
}

impl<'a, 'b, W: AsyncWrite> Future for SendMessageContentFuture<'a, 'b, W> {
    type Output = Result<(usize, bool), ServerError>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Self::Output> {
        let f = &*self;

        let w = &mut *f.w.borrow_mut();

        let stream = &mut w.stream;

        if !stream.is_writable() {
            return Poll::Pending;
        }

        let mut buf_arr = [&b""[..]; VECTORED_MAX - 1];
        let bufs = w.buf.get_ref_vectored(&mut buf_arr).limit(f.avail);

        match f.protocol.send_message_content(
            &mut StdWriteWrapper::new(Pin::new(&mut w.stream), cx),
            bufs,
            f.done,
        ) {
            Ok(ret) => Poll::Ready(Ok(ret)),
            Err(websocket::Error::Io(e)) if e.kind() == io::ErrorKind::WouldBlock => Poll::Pending,
            Err(e) => Poll::Ready(Err(e.into())),
        }
    }
}

impl<W: AsyncWrite> Drop for SendMessageContentFuture<'_, '_, W> {
    fn drop(&mut self) {
        self.w.borrow_mut().stream.cancel();
    }
}

struct WebSocketHandler<'a, R: AsyncRead, W: AsyncWrite> {
    r: RefCell<WebSocketRead<'a, R>>,
    w: RefCell<WebSocketWrite<'a, W>>,
    protocol: websocket::Protocol,
}

impl<'a, R: AsyncRead, W: AsyncWrite> WebSocketHandler<'a, R, W> {
    fn new(
        stream: (ReadHalf<'a, R>, WriteHalf<'a, W>),
        buf1: &'a mut RingBuffer,
        buf2: &'a mut RingBuffer,
    ) -> Self {
        buf1.align();
        buf2.clear();

        Self {
            r: RefCell::new(WebSocketRead {
                stream: stream.0,
                buf: buf1,
            }),
            w: RefCell::new(WebSocketWrite {
                stream: stream.1,
                buf: buf2,
            }),
            protocol: websocket::Protocol::new(),
        }
    }

    fn state(&self) -> websocket::State {
        self.protocol.state()
    }

    async fn recv_message_content<'b, B, F>(
        &self,
        dest: &RefCell<B>,
        limit: usize,
        bytes_read: &F,
    ) -> Result<(u8, usize, bool), ServerError>
    where
        B: AsMut<[u8]>,
        F: Fn(),
    {
        loop {
            let r = &mut *self.r.borrow_mut();

            let ret = {
                let dest = &mut *dest.borrow_mut();

                self.protocol
                    .recv_message_content(r.buf, &mut dest.as_mut()[..limit])
            };

            match ret {
                Some(Ok((opcode, size, end))) => {
                    bytes_read();

                    return Ok((opcode, size, end));
                }
                Some(Err(e)) => return Err(e.into()),
                None => {
                    if let Err(e) = recv_nonzero(&mut r.stream, r.buf).await {
                        if e.kind() == io::ErrorKind::WriteZero {
                            return Err(ServerError::BufferExceeded);
                        }

                        return Err(e.into());
                    }
                }
            }
        }
    }

    fn accept_body(&self, body: &[u8]) -> Result<(), ServerError> {
        let w = &mut *self.w.borrow_mut();

        w.buf.write_all(body)?;

        Ok(())
    }

    fn is_sending_message(&self) -> bool {
        self.protocol.is_sending_message()
    }

    fn send_message_start(&self, opcode: u8, mask: Option<[u8; 4]>) {
        self.protocol.send_message_start(opcode, mask);
    }

    async fn send_message_content<F>(
        &self,
        avail: usize,
        done: bool,
        bytes_sent: &F,
    ) -> Result<(usize, bool), ServerError>
    where
        F: Fn(),
    {
        loop {
            let (size, done) = SendMessageContentFuture {
                w: &self.w,
                protocol: &self.protocol,
                avail,
                done,
            }
            .await?;

            let w = &mut *self.w.borrow_mut();

            if size == 0 && !done {
                continue;
            }

            w.buf.read_commit(size);

            bytes_sent();

            return Ok((size, done));
        }
    }
}

struct ZhttpStreamSessionOut<'a> {
    instance_id: &'a str,
    id: &'a str,
    packet_buf: &'a RefCell<Vec<u8>>,
    sender_stream: &'a AsyncLocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
    shared: &'a ServerStreamSharedData,
}

impl<'a> ZhttpStreamSessionOut<'a> {
    fn new(
        instance_id: &'a str,
        id: &'a str,
        packet_buf: &'a RefCell<Vec<u8>>,
        sender_stream: &'a AsyncLocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
        shared: &'a ServerStreamSharedData,
    ) -> Self {
        Self {
            instance_id,
            id,
            packet_buf,
            sender_stream,
            shared,
        }
    }

    async fn check_send(&self) {
        self.sender_stream.check_send().await
    }

    // it's okay to run multiple instances of this future within the same
    // task, because it's okay to run multiple instances of check_send
    async fn send_msg(
        &self,
        mut zreq: zhttppacket::Request<'_, '_, '_>,
    ) -> Result<(), ServerError> {
        // wait for the ability to send before actually sending. this way we
        // can atomically create and send the message using non-blocking I/O
        self.sender_stream.check_send().await;

        let msg = {
            let ids = [zhttppacket::Id {
                id: self.id.as_bytes(),
                seq: Some(self.shared.out_seq()),
            }];

            zreq.from = self.instance_id.as_bytes();
            zreq.ids = &ids;
            zreq.multi = true;

            let packet_buf = &mut *self.packet_buf.borrow_mut();

            let size = zreq.serialize(packet_buf)?;

            self.shared.inc_out_seq();

            zmq::Message::from(&packet_buf[..size])
        };

        let mut addr = ArrayVec::new();
        if addr
            .try_extend_from_slice(self.shared.to_addr().get().unwrap())
            .is_err()
        {
            return Err(io::Error::from(io::ErrorKind::InvalidInput).into());
        }

        self.sender_stream.try_send((addr, msg))?;

        Ok(())
    }
}

struct ZhttpStreamSessionIn<'a, R> {
    id: &'a str,
    send_buf_size: usize,
    websocket: bool,
    receiver: &'a AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    shared: &'a ServerStreamSharedData,
    msg_read: &'a R,
    next: Option<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    seq: u32,
    credits: u32,
    first_data: bool,
}

impl<'a, R> ZhttpStreamSessionIn<'a, R>
where
    R: Fn(),
{
    fn new(
        id: &'a str,
        send_buf_size: usize,
        websocket: bool,
        receiver: &'a AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
        shared: &'a ServerStreamSharedData,
        msg_read: &'a R,
    ) -> Self {
        Self {
            id,
            send_buf_size,
            websocket,
            receiver,
            shared,
            msg_read,
            next: None,
            seq: 0,
            credits: 0,
            first_data: true,
        }
    }

    fn credits(&self) -> u32 {
        self.credits
    }

    fn subtract_credits(&mut self, amount: u32) {
        self.credits -= amount;
    }

    async fn peek_msg(&mut self) -> Result<&arena::Rc<zhttppacket::OwnedResponse>, ServerError> {
        if self.next.is_none() {
            let (r, id_index) = loop {
                let (r, id_index) = self.receiver.recv().await?;

                let zresp = r.get().get();

                if zresp.ids[id_index].id != self.id.as_bytes() {
                    // skip messages addressed to old ids
                    continue;
                }

                break (r, id_index);
            };

            let zresp = r.get().get();

            if !zresp.ptype_str.is_empty() {
                debug!("conn {}: handle packet: {}", self.id, zresp.ptype_str);
            } else {
                debug!("conn {}: handle packet: (data)", self.id);
            }

            if zresp.ids.len() == 0 {
                return Err(ServerError::BadMessage);
            }

            if let Some(seq) = zresp.ids[id_index].seq {
                if seq != self.seq {
                    debug!(
                        "conn {}: bad seq (expected {}, got {}), skipping",
                        self.id, self.seq, seq
                    );
                    return Err(ServerError::BadMessage);
                }

                self.seq += 1;
            }

            let mut addr = ArrayVec::new();
            if addr.try_extend_from_slice(zresp.from).is_err() {
                return Err(ServerError::BadMessage);
            }

            self.shared.set_to_addr(Some(addr));

            (self.msg_read)();

            match &zresp.ptype {
                zhttppacket::ResponsePacket::Data(rdata) => {
                    let mut credits = rdata.credits;

                    if self.first_data {
                        self.first_data = false;

                        if self.websocket && credits == 0 {
                            // workaround for p-8-proxy, which doesn't
                            //   send credits on websocket accept
                            credits = self.send_buf_size as u32;
                            debug!(
                                "conn {}: no credits in websocket accept, assuming {}",
                                self.id, credits
                            );
                        }
                    }

                    self.credits += credits;
                }
                zhttppacket::ResponsePacket::Error(edata) => {
                    debug!(
                        "conn {}: zhttp error condition={}",
                        self.id, edata.condition
                    );
                }
                zhttppacket::ResponsePacket::Credit(cdata) => {
                    self.credits += cdata.credits;
                }
                zhttppacket::ResponsePacket::Ping(pdata) => {
                    self.credits += pdata.credits;
                }
                zhttppacket::ResponsePacket::Pong(pdata) => {
                    self.credits += pdata.credits;
                }
                _ => {}
            }

            self.next = Some((r, id_index));
        }

        Ok(&self.next.as_ref().unwrap().0)
    }

    async fn recv_msg(&mut self) -> Result<arena::Rc<zhttppacket::OwnedResponse>, ServerError> {
        self.peek_msg().await?;

        Ok(self.next.take().unwrap().0)
    }
}

async fn send_msg(
    sender: &AsyncLocalSender<zmq::Message>,
    msg: zmq::Message,
) -> Result<(), ServerError> {
    Ok(sender.send(msg).await?)
}

async fn discard_while<F, T>(
    receiver: &AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    fut: F,
) -> F::Output
where
    F: Future<Output = Result<T, ServerError>> + Unpin,
{
    loop {
        match select_2(fut, receiver.recv()).await {
            Select2::R1(v) => break v,
            Select2::R2(_) => {
                // unexpected message in current state
                return Err(ServerError::BadMessage);
            }
        }
    }
}

// return true if persistent
async fn server_req_handler<S: AsyncRead + AsyncWrite>(
    id: &str,
    stream: &mut S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buf1: &mut RingBuffer,
    buf2: &mut RingBuffer,
    body_buf: &mut Buffer,
    packet_buf: &RefCell<Vec<u8>>,
    zsender: &AsyncLocalSender<zmq::Message>,
    zreceiver: &AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
) -> Result<bool, ServerError> {
    let stream = RefCell::new(stream);

    let handler = RequestHandler::new(io_split(&stream), buf1, buf2);
    let mut ranges = RequestHeaderRanges::default();

    // receive request header

    // ABR: discard_while
    let handler = {
        let fut = handler.recv_request(&mut ranges);
        pin_mut!(fut);

        match discard_while(zreceiver, fut).await {
            Ok(handler) => handler,
            Err(ServerError::Io(e)) if e.kind() == io::ErrorKind::UnexpectedEof => {
                return Ok(false)
            }
            Err(e) => return Err(e),
        }
    };

    // log request

    {
        let mut scratch = [httparse::EMPTY_HEADER; HEADERS_MAX];
        let req = handler.request(&mut scratch);
        let host = get_host(req.headers);
        let scheme = if secure { "https" } else { "http" };

        debug!(
            "conn {}: request: {} {}://{}{}",
            id, req.method, scheme, host, req.uri
        );
    }

    // receive request body

    // ABR: discard_while
    let handler = {
        let fut = handler.start_recv_body_and_keep_header();
        pin_mut!(fut);

        discard_while(zreceiver, fut).await?
    };

    loop {
        // ABR: discard_while
        let size = {
            let fut = handler.recv_body(body_buf.write_buf());
            pin_mut!(fut);

            discard_while(zreceiver, fut).await?
        };

        if size == 0 {
            break;
        }

        body_buf.write_commit(size);
    }

    // determine how to respond

    let msg = {
        let mut scratch = [httparse::EMPTY_HEADER; HEADERS_MAX];
        let req = handler.request(&mut scratch);

        let mut websocket = false;

        for h in req.headers.iter() {
            if h.name.eq_ignore_ascii_case("Upgrade") && h.value == b"websocket" {
                websocket = true;
                break;
            }
        }

        if websocket {
            // websocket requests are not supported in req mode

            // toss the request body
            body_buf.clear();

            None
        } else {
            // regular http requests we can handle

            // prepare zmq message

            let ids = [zhttppacket::Id {
                id: id.as_bytes(),
                seq: None,
            }];

            let msg = make_zhttp_request(
                "",
                &ids,
                req.method,
                req.uri,
                req.headers,
                body_buf.read_buf(),
                false,
                Mode::HttpReq,
                0,
                peer_addr,
                secure,
                &mut *packet_buf.borrow_mut(),
            )?;

            // body consumed
            body_buf.clear();

            Some(msg)
        }
    };

    let (handler, websocket) = if let Some(msg) = msg {
        // handle as http

        let handler = handler.recv_done();

        // send message

        // ABR: discard_while
        {
            let fut = send_msg(&zsender, msg);
            pin_mut!(fut);

            discard_while(zreceiver, fut).await?;
        }

        // receive message

        let zresp = loop {
            // ABR: direct read
            let (zresp, id_index) = zreceiver.recv().await?;

            let zresp_ref = zresp.get().get();

            if zresp_ref.ids[id_index].id != id.as_bytes() {
                // skip messages addressed to old ids
                continue;
            }

            if !zresp_ref.ptype_str.is_empty() {
                debug!("conn {}: handle packet: {}", id, zresp_ref.ptype_str);
            } else {
                debug!("conn {}: handle packet: (data)", id);
            }

            // skip non-data messages

            match &zresp_ref.ptype {
                zhttppacket::ResponsePacket::Data(_) => break zresp,
                _ => debug!(
                    "conn {}: unexpected packet in req mode: {}",
                    id, zresp_ref.ptype_str
                ),
            }
        };

        let zresp = zresp.get().get();

        let rdata = match &zresp.ptype {
            zhttppacket::ResponsePacket::Data(rdata) => rdata,
            _ => unreachable!(), // we confirmed the type above
        };

        // send response header

        let handler = {
            let mut headers = [http1::EMPTY_HEADER; HEADERS_MAX];
            let mut headers_len = 0;

            for h in rdata.headers.iter() {
                if headers_len >= headers.len() {
                    return Err(ServerError::BadMessage);
                }

                headers[headers_len] = http1::Header {
                    name: h.name,
                    value: h.value,
                };

                headers_len += 1;
            }

            let headers = &headers[..headers_len];

            handler.prepare_response(
                rdata.code,
                rdata.reason,
                headers,
                http1::BodySize::Known(rdata.body.len()),
            )?
        };

        // ABR: discard_while
        {
            let fut = handler.send_header();
            pin_mut!(fut);

            discard_while(zreceiver, fut).await?;
        }

        let handler = handler.send_header_done();

        body_buf.write_all(&rdata.body)?;

        (handler, false)
    } else {
        // handle as websocket

        // send response header

        let headers = &[http1::Header {
            name: "Content-Type",
            value: b"text/plain",
        }];

        let body = "WebSockets not supported on req mode interface.\n";

        let handler = handler.recv_done();

        let handler = handler.prepare_response(
            400,
            "Bad Request",
            headers,
            http1::BodySize::Known(body.len()),
        )?;

        // ABR: discard_while
        {
            let fut = handler.send_header();
            pin_mut!(fut);

            discard_while(zreceiver, fut).await?;
        }

        let handler = handler.send_header_done();

        body_buf.write_all(body.as_bytes())?;

        (handler, true)
    };

    // send response body

    while body_buf.read_avail() > 0 {
        // ABR: discard_while
        let size = {
            let fut = handler.send_body(body_buf.read_buf(), false);
            pin_mut!(fut);

            discard_while(zreceiver, fut).await?
        };

        body_buf.read_commit(size);
    }

    let persistent = handler.finish();

    if websocket {
        return Ok(false);
    }

    Ok(persistent)
}

async fn server_req_connection_inner<P: CidProvider, S: AsyncRead + AsyncWrite + Identify>(
    token: CancellationToken,
    cid: &mut ArrayString<32>,
    cid_provider: &mut P,
    mut stream: S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buffer_size: usize,
    body_buffer_size: usize,
    rb_tmp: &Rc<TmpBuffer>,
    packet_buf: Rc<RefCell<Vec<u8>>>,
    timeout: Duration,
    zsender: AsyncLocalSender<zmq::Message>,
    zreceiver: &AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
) -> Result<(), ServerError> {
    let reactor = Reactor::current().unwrap();

    let mut buf1 = RingBuffer::new(buffer_size, rb_tmp);
    let mut buf2 = RingBuffer::new(buffer_size, rb_tmp);
    let mut body_buf = Buffer::new(body_buffer_size);

    loop {
        stream.set_id(cid);

        // this was originally logged when starting the non-async state
        // machine, so we'll keep doing that
        debug!("conn {}: assigning id", cid);

        let reuse = {
            let handler = server_req_handler(
                cid.as_ref(),
                &mut stream,
                peer_addr,
                secure,
                &mut buf1,
                &mut buf2,
                &mut body_buf,
                &packet_buf,
                &zsender,
                zreceiver,
            );
            pin_mut!(handler);

            let timeout = Timeout::new(reactor.now() + timeout);

            match select_3(handler, timeout.elapsed(), token.cancelled()).await {
                Select3::R1(ret) => ret?,
                Select3::R2(_) => return Err(ServerError::Timeout),
                Select3::R3(_) => return Err(ServerError::Stopped),
            }
        };

        if !reuse {
            break;
        }

        // note: buf1 is not cleared as there may be data to read

        buf2.clear();
        body_buf.clear();

        *cid = cid_provider.get_new_assigned_cid();
    }

    stream.close().await?;

    Ok(())
}

pub async fn server_req_connection<P: CidProvider, S: AsyncRead + AsyncWrite + Identify>(
    token: CancellationToken,
    mut cid: ArrayString<32>,
    cid_provider: &mut P,
    stream: S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buffer_size: usize,
    body_buffer_size: usize,
    rb_tmp: &Rc<TmpBuffer>,
    packet_buf: Rc<RefCell<Vec<u8>>>,
    timeout: Duration,
    zsender: AsyncLocalSender<zmq::Message>,
    zreceiver: AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
) {
    match server_req_connection_inner(
        token,
        &mut cid,
        cid_provider,
        stream,
        peer_addr,
        secure,
        buffer_size,
        body_buffer_size,
        rb_tmp,
        packet_buf,
        timeout,
        zsender,
        &zreceiver,
    )
    .await
    {
        Ok(()) => debug!("conn {}: finished", cid),
        Err(e) => debug!("conn {}: process error: {:?}", cid, e),
    }
}

// this function will either return immediately or await messages
async fn handle_other<R>(
    zresp: &zhttppacket::Response<'_, '_, '_>,
    zsess_in: &mut ZhttpStreamSessionIn<'_, R>,
    zsess_out: &ZhttpStreamSessionOut<'_>,
) -> Result<(), ServerError>
where
    R: Fn(),
{
    match &zresp.ptype {
        zhttppacket::ResponsePacket::KeepAlive => Ok(()),
        zhttppacket::ResponsePacket::Credit(_) => Ok(()),
        zhttppacket::ResponsePacket::HandoffStart => {
            let zreq = zhttppacket::Request::new_handoff_proceed(b"", &[]);

            // discarding here is fine. the sender should cease sending
            // messages until we've replied with proceed
            {
                let fut = zsess_out.send_msg(zreq);
                pin_mut!(fut);

                discard_while(&zsess_in.receiver, fut).await?;
            }

            // pause until we get a msg
            zsess_in.peek_msg().await?;

            Ok(())
        }
        zhttppacket::ResponsePacket::Error(_) => Err(ServerError::HandlerError),
        zhttppacket::ResponsePacket::Cancel => Err(ServerError::HandlerCancel),
        _ => Err(ServerError::BadMessage), // unexpected type
    }
}

async fn stream_recv_body<'a, R1, R2, R, W>(
    tmp_buf: &RefCell<Vec<u8>>,
    bytes_read: &R1,
    handler: RequestHeader<'a, R, W>,
    zsess_in: &mut ZhttpStreamSessionIn<'_, R2>,
    zsess_out: &ZhttpStreamSessionOut<'_>,
) -> Result<RequestStartResponse<'a, R, W>, ServerError>
where
    R1: Fn(),
    R2: Fn(),
    R: AsyncRead,
    W: AsyncWrite,
{
    let handler = {
        let start_recv_body = handler.start_recv_body();

        pin_mut!(start_recv_body);

        // ABR: poll_async doesn't block
        match poll_async(start_recv_body.as_mut()).await {
            Poll::Ready(ret) => ret?,
            Poll::Pending => {
                // if we get here, then the send buffer with the client is full

                // keep trying to process while reading messages
                loop {
                    let ret = {
                        let recv_msg = zsess_in.recv_msg();

                        pin_mut!(recv_msg);

                        // ABR: select contains read
                        select_2(start_recv_body.as_mut(), recv_msg).await
                    };

                    match ret {
                        Select2::R1(ret) => break ret?,
                        Select2::R2(ret) => {
                            let r = ret?;

                            let zresp_ref = r.get().get();

                            // note: if we get a data message, handle_other will
                            // error out. technically a data message should be
                            // allowed here, but we're not in a position to do
                            // anything with it, so we error.
                            //
                            // fortunately, the conditions to hit this are unusual:
                            //   * we need to receive a subsequent request over
                            //     a persistent connection
                            //   * that request needs to be one for which a body
                            //     would be expected, and the request needs to
                            //     include an expect header
                            //   * the send buffer to that connection needs to be
                            //     full
                            //   * the handler needs to provide an early response
                            //     before receiving the request body
                            //
                            // in other words, a client needs to send a large
                            // pipelined POST over a reused connection, before it
                            // has read the previous response, and the handler
                            // needs to reject the request

                            // ABR: handle_other
                            handle_other(zresp_ref, zsess_in, zsess_out).await?;
                        }
                    }
                }
            }
        }
    };

    {
        let check_send = None;
        let recv_body = None;

        pin_mut!(check_send, recv_body);

        loop {
            if recv_body.is_none() && check_send.is_none() {
                check_send.set(Some(zsess_out.check_send()));
            }

            let orig_in_credits = zsess_in.credits();

            let ret = {
                let peek_msg = zsess_in.peek_msg();

                pin_mut!(peek_msg);

                // ABR: select contains read
                select_3(
                    select_option(check_send.as_mut().as_pin_mut()),
                    select_option(recv_body.as_mut().as_pin_mut()),
                    peek_msg,
                )
                .await
            };

            match ret {
                Select3::R1(()) => {
                    check_send.set(None);

                    assert_eq!(recv_body.is_none(), true);

                    let max_read = cmp::min(tmp_buf.borrow().len(), zsess_in.credits() as usize);

                    recv_body.set(Some(
                        handler.recv_body_shared(tmp_buf, max_read, bytes_read),
                    ));
                }
                Select3::R2(ret) => {
                    recv_body.set(None);

                    let size = ret?;

                    let body = &tmp_buf.borrow()[..size];

                    zsess_in.subtract_credits(size as u32);

                    let mut rdata = zhttppacket::RequestData::new();
                    rdata.body = body;
                    rdata.more = handler.more();

                    let zreq = zhttppacket::Request::new_data(b"", &[], rdata);

                    // ABR: we've already called check_send, so this shouldn't block
                    zsess_out.send_msg(zreq).await?;

                    if !handler.more() {
                        break;
                    }
                }
                Select3::R3(ret) => {
                    let r = ret?;

                    let zresp_ref = r.get().get();

                    match &zresp_ref.ptype {
                        zhttppacket::ResponsePacket::Data(_) => break,
                        _ => {
                            // ABR: direct read
                            let r = zsess_in.recv_msg().await?;

                            let zresp_ref = r.get().get();

                            // ABR: handle_other
                            handle_other(zresp_ref, zsess_in, zsess_out).await?;
                        }
                    }

                    if zsess_in.credits() > orig_in_credits {
                        // restart the recv
                        recv_body.set(None);
                    }
                }
            }
        }
    }

    Ok(handler.recv_done())
}

async fn stream_send_body<'a, R1, R2, R, W>(
    bytes_read: &R1,
    handler: &RequestSendBody<'a, R, W>,
    zsess_in: &mut ZhttpStreamSessionIn<'_, R2>,
    zsess_out: &ZhttpStreamSessionOut<'_>,
) -> Result<(), ServerError>
where
    R1: Fn(),
    R2: Fn(),
    R: AsyncRead,
    W: AsyncWrite,
{
    let mut out_credits = 0;

    let flush_body = None;
    let send_msg = None;

    pin_mut!(flush_body, send_msg);

    'main: loop {
        let ret = {
            if flush_body.is_none() && handler.can_flush() {
                flush_body.set(Some(handler.flush_body()));
            }

            if out_credits > 0 && send_msg.is_none() {
                let zreq = zhttppacket::Request::new_credit(b"", &[], out_credits);

                out_credits = 0;

                send_msg.set(Some(zsess_out.send_msg(zreq)));
            }

            let recv_msg = zsess_in.recv_msg();
            let fill_recv_buffer = handler.fill_recv_buffer();

            pin_mut!(recv_msg, fill_recv_buffer);

            // ABR: select contains read
            select_4(
                select_option(flush_body.as_mut().as_pin_mut()),
                select_option(send_msg.as_mut().as_pin_mut()),
                recv_msg,
                fill_recv_buffer,
            )
            .await
        };

        match ret {
            Select4::R1(ret) => {
                flush_body.set(None);

                let (size, done) = ret?;

                if done {
                    break;
                }

                out_credits += size as u32;

                if size > 0 {
                    bytes_read();
                }
            }
            Select4::R2(ret) => {
                send_msg.set(None);

                ret?;
            }
            Select4::R3(ret) => {
                let r = ret?;

                let zresp_ref = r.get().get();

                match &zresp_ref.ptype {
                    zhttppacket::ResponsePacket::Data(rdata) => {
                        handler.append_body(rdata.body, rdata.more)?;
                    }
                    _ => {
                        // if handoff requested, flush send buffer before
                        // before responding
                        match &zresp_ref.ptype {
                            zhttppacket::ResponsePacket::HandoffStart => {
                                if flush_body.is_none() && handler.can_flush() {
                                    flush_body.set(Some(handler.flush_body()));
                                }

                                while let Some(fut) = flush_body.as_mut().as_pin_mut() {
                                    let ret = discard_while(zsess_in.receiver, fut).await;
                                    flush_body.set(None);

                                    let (size, done) = ret?;

                                    if done {
                                        break 'main;
                                    }

                                    out_credits += size as u32;

                                    if size > 0 {
                                        bytes_read();
                                    }

                                    if handler.can_flush() {
                                        flush_body.set(Some(handler.flush_body()));
                                    }
                                }
                            }
                            _ => {}
                        }

                        // ABR: handle_other
                        handle_other(zresp_ref, zsess_in, zsess_out).await?;
                    }
                }
            }
            Select4::R4(e) => return Err(e),
        }
    }

    Ok(())
}

async fn stream_websocket<S, R1, R2>(
    id: &str,
    stream: RefCell<&mut S>,
    buf1: &mut RingBuffer,
    buf2: &mut RingBuffer,
    messages_max: usize,
    tmp_buf: &RefCell<Vec<u8>>,
    bytes_read: &R1,
    zsess_in: &mut ZhttpStreamSessionIn<'_, R2>,
    zsess_out: &ZhttpStreamSessionOut<'_>,
) -> Result<(), ServerError>
where
    S: AsyncRead + AsyncWrite,
    R1: Fn(),
    R2: Fn(),
{
    let handler = WebSocketHandler::new(io_split(&stream), buf1, buf2);
    let mut ws_in_tracker = MessageTracker::new(messages_max);

    let mut out_credits = 0;

    let check_send = None;
    let recv_content = None;
    let send_content = None;
    let send_msg = None;

    pin_mut!(check_send, recv_content, send_content, send_msg);

    loop {
        let (do_send, do_recv) = match handler.state() {
            websocket::State::Connected => (true, true),
            websocket::State::PeerClosed => (true, false),
            websocket::State::Closing => (false, true),
            websocket::State::Finished => break,
        };

        if out_credits > 0 {
            if send_msg.is_none() {
                // send credits

                let zreq = zhttppacket::Request::new_credit(b"", &[], out_credits as u32);

                out_credits = 0;

                send_msg.set(Some(zsess_out.send_msg(zreq)));
            }
        } else {
            if do_recv && recv_content.is_none() && check_send.is_none() {
                check_send.set(Some(zsess_out.check_send()));
            }
        }

        if do_send && send_content.is_none() {
            if let Some((mtype, avail, done)) = ws_in_tracker.current() {
                if !handler.is_sending_message() {
                    handler.send_message_start(mtype, None);
                }

                if avail > 0 || done {
                    send_content.set(Some(handler.send_message_content(avail, done, bytes_read)));
                }
            }
        }

        let orig_in_credits = zsess_in.credits();

        let ret = {
            let recv_msg = zsess_in.recv_msg();

            pin_mut!(recv_msg);

            // ABR: select contains read
            select_5(
                select_option(check_send.as_mut().as_pin_mut()),
                select_option(recv_content.as_mut().as_pin_mut()),
                select_option(send_content.as_mut().as_pin_mut()),
                select_option(send_msg.as_mut().as_pin_mut()),
                recv_msg,
            )
            .await
        };

        match ret {
            Select5::R1(()) => {
                check_send.set(None);

                assert_eq!(recv_content.is_none(), true);

                let max_read = cmp::min(tmp_buf.borrow().len(), zsess_in.credits() as usize);

                recv_content.set(Some(
                    handler.recv_message_content(tmp_buf, max_read, bytes_read),
                ));
            }
            Select5::R2(ret) => {
                recv_content.set(None);

                let (opcode, size, end) = ret?;

                let body = &tmp_buf.borrow()[..size];

                let zreq = match opcode {
                    websocket::OPCODE_TEXT | websocket::OPCODE_BINARY => {
                        if body.is_empty() && !end {
                            // don't bother sending empty message
                            continue;
                        }

                        let mut data = zhttppacket::RequestData::new();

                        data.body = body;

                        data.content_type = if opcode == websocket::OPCODE_TEXT {
                            Some(zhttppacket::ContentType::Text)
                        } else {
                            Some(zhttppacket::ContentType::Binary)
                        };

                        data.more = !end;

                        zhttppacket::Request::new_data(b"", &[], data)
                    }
                    websocket::OPCODE_CLOSE => {
                        let status = if body.len() >= 2 {
                            let mut arr = [0; 2];
                            arr[..].copy_from_slice(&body[..2]);

                            let code = u16::from_be_bytes(arr);

                            let reason = match str::from_utf8(&body[2..]) {
                                Ok(reason) => reason,
                                Err(e) => return Err(e.into()),
                            };

                            Some((code, reason))
                        } else {
                            None
                        };

                        zhttppacket::Request::new_close(b"", &[], status)
                    }
                    websocket::OPCODE_PING => zhttppacket::Request::new_ping(b"", &[], body),
                    websocket::OPCODE_PONG => zhttppacket::Request::new_pong(b"", &[], body),
                    opcode => {
                        debug!("conn {}: unsupported websocket opcode: {}", id, opcode);
                        return Err(ServerError::BadFrame);
                    }
                };

                zsess_in.subtract_credits(size as u32);

                // ABR: we've already called check_send, so this shouldn't block
                zsess_out.send_msg(zreq).await?;
            }
            Select5::R3(ret) => {
                send_content.set(None);

                let (size, done) = ret?;

                ws_in_tracker.consumed(size, done);

                if handler.state() == websocket::State::Connected
                    || handler.state() == websocket::State::PeerClosed
                {
                    out_credits += size as u32;
                }
            }
            Select5::R4(ret) => {
                send_msg.set(None);

                ret?;
            }
            Select5::R5(ret) => {
                let r = ret?;

                let zresp_ref = r.get().get();

                match &zresp_ref.ptype {
                    zhttppacket::ResponsePacket::Data(rdata) => match handler.state() {
                        websocket::State::Connected | websocket::State::PeerClosed => {
                            handler.accept_body(rdata.body)?;

                            let opcode = match &rdata.content_type {
                                Some(zhttppacket::ContentType::Binary) => websocket::OPCODE_BINARY,
                                _ => websocket::OPCODE_TEXT,
                            };

                            if !ws_in_tracker.in_progress() {
                                if ws_in_tracker.start(opcode).is_err() {
                                    return Err(ServerError::BufferExceeded);
                                }
                            }

                            ws_in_tracker.extend(rdata.body.len());

                            if !rdata.more {
                                ws_in_tracker.done();
                            }
                        }
                        _ => {}
                    },
                    zhttppacket::ResponsePacket::Close(cdata) => match handler.state() {
                        websocket::State::Connected | websocket::State::PeerClosed => {
                            let (code, reason) = match cdata.status {
                                Some(v) => v,
                                None => (1000, ""),
                            };

                            let arr: [u8; 2] = code.to_be_bytes();

                            handler.accept_body(&arr)?;

                            handler.accept_body(reason.as_bytes())?;

                            if ws_in_tracker.start(websocket::OPCODE_CLOSE).is_err() {
                                return Err(ServerError::BadFrame);
                            }

                            ws_in_tracker.extend(arr.len() + reason.len());
                            ws_in_tracker.done();
                        }
                        _ => {}
                    },
                    zhttppacket::ResponsePacket::Ping(pdata) => match handler.state() {
                        websocket::State::Connected | websocket::State::PeerClosed => {
                            handler.accept_body(pdata.body)?;

                            if ws_in_tracker.start(websocket::OPCODE_PING).is_err() {
                                return Err(ServerError::BadFrame);
                            }

                            ws_in_tracker.extend(pdata.body.len());
                            ws_in_tracker.done();
                        }
                        _ => {}
                    },
                    zhttppacket::ResponsePacket::Pong(pdata) => match handler.state() {
                        websocket::State::Connected | websocket::State::PeerClosed => {
                            handler.accept_body(pdata.body)?;

                            if ws_in_tracker.start(websocket::OPCODE_PONG).is_err() {
                                return Err(ServerError::BadFrame);
                            }

                            ws_in_tracker.extend(pdata.body.len());
                            ws_in_tracker.done();
                        }
                        _ => {}
                    },
                    _ => {
                        // if handoff requested, flush send buffer before
                        // before responding
                        match &zresp_ref.ptype {
                            zhttppacket::ResponsePacket::HandoffStart => loop {
                                if send_content.is_none() {
                                    if let Some((mtype, avail, done)) = ws_in_tracker.current() {
                                        if !handler.is_sending_message() {
                                            handler.send_message_start(mtype, None);
                                        }

                                        if avail > 0 || done {
                                            send_content
                                                .set(Some(handler.send_message_content(
                                                    avail, done, bytes_read,
                                                )));
                                        }
                                    }
                                }

                                if let Some(fut) = send_content.as_mut().as_pin_mut() {
                                    let ret = discard_while(zsess_in.receiver, fut).await;
                                    send_content.set(None);

                                    let (size, done) = ret?;

                                    ws_in_tracker.consumed(size, done);

                                    if handler.state() == websocket::State::Connected
                                        || handler.state() == websocket::State::PeerClosed
                                    {
                                        out_credits += size as u32;
                                    }
                                } else {
                                    break;
                                }
                            },
                            _ => {}
                        }

                        // ABR: handle_other
                        handle_other(zresp_ref, zsess_in, zsess_out).await?;
                    }
                }

                if zsess_in.credits() > orig_in_credits {
                    // restart the recv
                    recv_content.set(None);
                }
            }
        }
    }

    Ok(())
}

// return true if persistent
async fn server_stream_handler<S, R1, R2>(
    id: &str,
    stream: &mut S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buf1: &mut RingBuffer,
    buf2: &mut RingBuffer,
    messages_max: usize,
    packet_buf: &RefCell<Vec<u8>>,
    tmp_buf: &RefCell<Vec<u8>>,
    instance_id: &str,
    zsender: &AsyncLocalSender<zmq::Message>,
    zsender_stream: &AsyncLocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
    zreceiver: &AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    shared: &ServerStreamSharedData,
    refresh_stream_timeout: &R1,
    refresh_session_timeout: &R2,
) -> Result<bool, ServerError>
where
    S: AsyncRead + AsyncWrite,
    R1: Fn(),
    R2: Fn(),
{
    let stream = RefCell::new(stream);

    let send_buf_size = buf1.capacity();
    let recv_buf_size = buf2.capacity();

    let handler = RequestHandler::new(io_split(&stream), buf1, buf2);
    let mut ranges = RequestHeaderRanges::default();

    let zsess_out = ZhttpStreamSessionOut::new(instance_id, id, packet_buf, zsender_stream, shared);

    // receive request header

    // ABR: discard_while
    let handler = {
        let fut = handler.recv_request(&mut ranges);
        pin_mut!(fut);

        match discard_while(zreceiver, fut).await {
            Ok(handler) => handler,
            Err(ServerError::Io(e)) if e.kind() == io::ErrorKind::UnexpectedEof => {
                return Ok(false)
            }
            Err(e) => return Err(e),
        }
    };

    refresh_stream_timeout();

    let (body_size, ws_accept, msg) = {
        let mut scratch = [httparse::EMPTY_HEADER; HEADERS_MAX];
        let req = handler.request(&mut scratch);

        let mut websocket = false;
        let mut ws_key = None;

        for h in req.headers.iter() {
            if h.name.eq_ignore_ascii_case("Upgrade") && h.value == b"websocket" {
                websocket = true;
            }

            if h.name.eq_ignore_ascii_case("Sec-WebSocket-Key") {
                ws_key = Some(h.value);
            }
        }

        // log request

        let host = get_host(req.headers);

        let scheme = if websocket {
            if secure {
                "wss"
            } else {
                "ws"
            }
        } else {
            if secure {
                "https"
            } else {
                "http"
            }
        };

        debug!(
            "conn {}: request: {} {}://{}{}",
            id, req.method, scheme, host, req.uri
        );

        let ws_accept: Option<ArrayString<WS_ACCEPT_MAX>> = if websocket {
            if req.method != "GET" || req.body_size != http1::BodySize::NoBody || ws_key.is_none() {
                return Err(ServerError::InvalidWebSocketRequest);
            }

            let accept = match calculate_ws_accept(&ws_key.unwrap()) {
                Ok(s) => s,
                Err(_) => return Err(ServerError::InvalidWebSocketRequest),
            };

            Some(accept)
        } else {
            None
        };

        let ids = [zhttppacket::Id {
            id: id.as_bytes(),
            seq: Some(shared.out_seq()),
        }];

        let (mode, more) = if websocket {
            (Mode::WebSocket, false)
        } else {
            let more = match req.body_size {
                http1::BodySize::NoBody => false,
                http1::BodySize::Known(x) => x > 0,
                http1::BodySize::Unknown => true,
            };

            (Mode::HttpStream, more)
        };

        let msg = make_zhttp_request(
            instance_id,
            &ids,
            req.method,
            &req.uri,
            &req.headers,
            b"",
            more,
            mode,
            recv_buf_size as u32,
            peer_addr,
            secure,
            &mut *packet_buf.borrow_mut(),
        )?;

        shared.inc_out_seq();

        (req.body_size, ws_accept, msg)
    };

    // send request message

    // ABR: discard_while
    {
        let fut = send_msg(&zsender, msg);
        pin_mut!(fut);

        discard_while(zreceiver, fut).await?;
    }

    let mut zsess_in = ZhttpStreamSessionIn::new(
        id,
        send_buf_size,
        ws_accept.is_some(),
        zreceiver,
        shared,
        refresh_session_timeout,
    );

    // receive any message, in order to get a handler address
    // ABR: direct read
    zsess_in.peek_msg().await?;

    let mut handler = if body_size != http1::BodySize::NoBody {
        // receive request body and send to handler

        stream_recv_body(
            tmp_buf,
            refresh_stream_timeout,
            handler,
            &mut zsess_in,
            &zsess_out,
        )
        .await?
    } else {
        handler.recv_done()?
    };

    // receive response message

    let zresp = loop {
        let ret = {
            let recv_msg = zsess_in.recv_msg();
            let fill_recv_buffer = handler.fill_recv_buffer();

            pin_mut!(recv_msg, fill_recv_buffer);

            // ABR: select contains read
            select_2(recv_msg, fill_recv_buffer).await
        };

        match ret {
            Select2::R1(ret) => {
                let r = ret?;

                let zresp_ref = r.get().get();

                match &zresp_ref.ptype {
                    zhttppacket::ResponsePacket::Data(_)
                    | zhttppacket::ResponsePacket::Error(_) => break r,
                    _ => {
                        // ABR: handle_other
                        handle_other(zresp_ref, &mut zsess_in, &zsess_out).await?;
                    }
                }
            }
            Select2::R2(e) => return Err(e),
        }
    };

    // determine how to respond

    let handler = {
        let zresp = zresp.get().get();

        let rdata = match &zresp.ptype {
            zhttppacket::ResponsePacket::Data(rdata) => rdata,
            zhttppacket::ResponsePacket::Error(edata) => {
                if ws_accept.is_some() && edata.condition == "rejected" {
                    // send websocket rejection

                    let rdata = edata.rejected_info.as_ref().unwrap();

                    let handler = {
                        let mut headers = [http1::EMPTY_HEADER; HEADERS_MAX];
                        let mut headers_len = 0;

                        for h in rdata.headers.iter() {
                            // don't send these headers
                            if h.name.eq_ignore_ascii_case("Upgrade")
                                || h.name.eq_ignore_ascii_case("Connection")
                                || h.name.eq_ignore_ascii_case("Sec-WebSocket-Accept")
                            {
                                continue;
                            }

                            if headers_len >= headers.len() {
                                return Err(ServerError::BadMessage);
                            }

                            headers[headers_len] = http1::Header {
                                name: h.name,
                                value: h.value,
                            };

                            headers_len += 1;
                        }

                        let headers = &headers[..headers_len];

                        handler.prepare_response(
                            rdata.code,
                            rdata.reason,
                            headers,
                            http1::BodySize::Known(rdata.body.len()),
                        )?
                    };

                    // ABR: discard_while
                    {
                        let fut = handler.send_header();
                        pin_mut!(fut);

                        discard_while(zreceiver, fut).await?;
                    }

                    let handler = handler.send_header_done();

                    handler.append_body(rdata.body, false)?;

                    loop {
                        // ABR: discard_while
                        let (_, done) = {
                            let fut = handler.flush_body();
                            pin_mut!(fut);

                            discard_while(zreceiver, fut).await?
                        };

                        if done {
                            break;
                        }
                    }

                    return Ok(false);
                } else {
                    // ABR: handle_other
                    return Err(handle_other(zresp, &mut zsess_in, &zsess_out)
                        .await
                        .unwrap_err());
                }
            }
            _ => unreachable!(), // we confirmed the type above
        };

        // send response header

        let handler = {
            let mut headers = [http1::EMPTY_HEADER; HEADERS_MAX];
            let mut headers_len = 0;

            let mut body_size = http1::BodySize::Unknown;

            for h in rdata.headers.iter() {
                if ws_accept.is_some() {
                    // don't send these headers
                    if h.name.eq_ignore_ascii_case("Upgrade")
                        || h.name.eq_ignore_ascii_case("Connection")
                        || h.name.eq_ignore_ascii_case("Sec-WebSocket-Accept")
                    {
                        continue;
                    }
                } else {
                    if h.name.eq_ignore_ascii_case("Content-Length") {
                        let s = str::from_utf8(h.value)?;

                        let clen: usize = match s.parse() {
                            Ok(clen) => clen,
                            Err(_) => {
                                return Err(io::Error::from(io::ErrorKind::InvalidInput).into())
                            }
                        };

                        body_size = http1::BodySize::Known(clen);
                    }
                }

                if headers_len >= headers.len() {
                    return Err(ServerError::BadMessage);
                }

                headers[headers_len] = http1::Header {
                    name: h.name,
                    value: h.value,
                };

                headers_len += 1;
            }

            if body_size == http1::BodySize::Unknown && !rdata.more {
                body_size = http1::BodySize::Known(rdata.body.len());
            }

            if let Some(accept_data) = &ws_accept {
                if headers_len + 3 > headers.len() {
                    return Err(ServerError::BadMessage);
                }

                headers[headers_len] = http1::Header {
                    name: "Upgrade",
                    value: b"websocket",
                };
                headers_len += 1;

                headers[headers_len] = http1::Header {
                    name: "Connection",
                    value: b"Upgrade",
                };
                headers_len += 1;

                headers[headers_len] = http1::Header {
                    name: "Sec-WebSocket-Accept",
                    value: accept_data.as_bytes(),
                };
                headers_len += 1;
            }

            let headers = &headers[..headers_len];

            handler.prepare_response(rdata.code, rdata.reason, headers, body_size)?
        };

        handler.append_body(rdata.body, rdata.more, id)?;

        {
            let send_header = handler.send_header();

            pin_mut!(send_header);

            loop {
                let ret = {
                    let recv_msg = zsess_in.recv_msg();

                    pin_mut!(recv_msg);

                    // ABR: select contains read
                    select_2(send_header.as_mut(), recv_msg).await
                };

                match ret {
                    Select2::R1(ret) => {
                        ret?;

                        break;
                    }
                    Select2::R2(ret) => {
                        let r = ret?;

                        let zresp_ref = r.get().get();

                        match &zresp_ref.ptype {
                            zhttppacket::ResponsePacket::Data(rdata) => {
                                handler.append_body(rdata.body, rdata.more, id)?;
                            }
                            _ => {
                                // ABR: handle_other
                                handle_other(zresp_ref, &mut zsess_in, &zsess_out).await?;
                            }
                        }
                    }
                }
            }
        }

        let handler = handler.send_header_done();

        refresh_stream_timeout();

        handler
    };

    if ws_accept.is_some() {
        drop(handler);

        // handle as websocket connection

        stream_websocket(
            id,
            stream,
            buf1,
            buf2,
            messages_max,
            tmp_buf,
            refresh_stream_timeout,
            &mut zsess_in,
            &zsess_out,
        )
        .await?;

        Ok(false)
    } else {
        // send response body

        stream_send_body(refresh_stream_timeout, &handler, &mut zsess_in, &zsess_out).await?;

        let persistent = handler.finish();

        Ok(persistent)
    }
}

async fn server_stream_connection_inner<P: CidProvider, S: AsyncRead + AsyncWrite + Identify>(
    token: CancellationToken,
    cid: &mut ArrayString<32>,
    cid_provider: &mut P,
    mut stream: S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buffer_size: usize,
    messages_max: usize,
    rb_tmp: &Rc<TmpBuffer>,
    packet_buf: Rc<RefCell<Vec<u8>>>,
    tmp_buf: Rc<RefCell<Vec<u8>>>,
    stream_timeout: Duration,
    instance_id: &str,
    zsender: AsyncLocalSender<zmq::Message>,
    zsender_stream: AsyncLocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
    zreceiver: &AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    shared: arena::Rc<ServerStreamSharedData>,
) -> Result<(), ServerError> {
    let reactor = Reactor::current().unwrap();

    let mut buf1 = RingBuffer::new(buffer_size, rb_tmp);
    let mut buf2 = RingBuffer::new(buffer_size, rb_tmp);

    loop {
        stream.set_id(cid);

        // this was originally logged when starting the non-async state
        // machine, so we'll keep doing that
        debug!("conn {}: assigning id", cid);

        let reuse = {
            let stream_timeout_time = RefCell::new(reactor.now() + stream_timeout);
            let session_timeout_time = RefCell::new(reactor.now() + ZHTTP_SESSION_TIMEOUT);

            let soonest_time = || {
                cmp::min(
                    *stream_timeout_time.borrow(),
                    *session_timeout_time.borrow(),
                )
            };

            let timeout = Timeout::new(soonest_time());

            let refresh_stream_timeout = || {
                stream_timeout_time.replace(reactor.now() + stream_timeout);
                timeout.set_deadline(soonest_time());
            };

            let refresh_session_timeout = || {
                session_timeout_time.replace(reactor.now() + ZHTTP_SESSION_TIMEOUT);
                timeout.set_deadline(soonest_time());
            };

            let handler = server_stream_handler(
                cid.as_ref(),
                &mut stream,
                peer_addr,
                secure,
                &mut buf1,
                &mut buf2,
                messages_max,
                &packet_buf,
                &tmp_buf,
                instance_id,
                &zsender,
                &zsender_stream,
                zreceiver,
                shared.get(),
                &refresh_stream_timeout,
                &refresh_session_timeout,
            );
            pin_mut!(handler);

            match select_3(handler, timeout.elapsed(), token.cancelled()).await {
                Select3::R1(ret) => match ret {
                    Ok(reuse) => reuse,
                    Err(e) => {
                        let handler_caused = match &e {
                            ServerError::BadMessage
                            | ServerError::HandlerError
                            | ServerError::HandlerCancel => true,
                            _ => false,
                        };

                        let shared = shared.get();

                        if !handler_caused && shared.to_addr().get().is_some() {
                            let id = cid.as_ref();

                            let msg = {
                                let mut zreq = zhttppacket::Request::new_cancel(b"", &[]);

                                let ids = [zhttppacket::Id {
                                    id: id.as_bytes(),
                                    seq: Some(shared.out_seq()),
                                }];

                                zreq.from = instance_id.as_bytes();
                                zreq.ids = &ids;
                                zreq.multi = true;

                                let packet_buf = &mut *packet_buf.borrow_mut();

                                let size = zreq.serialize(packet_buf)?;

                                shared.inc_out_seq();

                                zmq::Message::from(&packet_buf[..size])
                            };

                            let mut addr = ArrayVec::new();
                            if addr
                                .try_extend_from_slice(shared.to_addr().get().unwrap())
                                .is_err()
                            {
                                return Err(io::Error::from(io::ErrorKind::InvalidInput).into());
                            }

                            // best effort
                            let _ = zsender_stream.try_send((addr, msg));
                        }

                        return Err(e);
                    }
                },
                Select3::R2(_) => return Err(ServerError::Timeout),
                Select3::R3(_) => return Err(ServerError::Stopped),
            }
        };

        if !reuse {
            break;
        }

        // note: buf1 is not cleared as there may be data to read

        buf2.clear();
        shared.get().reset();

        *cid = cid_provider.get_new_assigned_cid();
    }

    {
        let fut = async { Ok(stream.close().await?) };
        pin_mut!(fut);

        discard_while(zreceiver, fut).await?;
    }

    Ok(())
}

pub async fn server_stream_connection<P: CidProvider, S: AsyncRead + AsyncWrite + Identify>(
    token: CancellationToken,
    mut cid: ArrayString<32>,
    cid_provider: &mut P,
    stream: S,
    peer_addr: Option<&SocketAddr>,
    secure: bool,
    buffer_size: usize,
    messages_max: usize,
    rb_tmp: &Rc<TmpBuffer>,
    packet_buf: Rc<RefCell<Vec<u8>>>,
    tmp_buf: Rc<RefCell<Vec<u8>>>,
    timeout: Duration,
    instance_id: &str,
    zsender: AsyncLocalSender<zmq::Message>,
    zsender_stream: AsyncLocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
    zreceiver: AsyncLocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    shared: arena::Rc<ServerStreamSharedData>,
) {
    match server_stream_connection_inner(
        token,
        &mut cid,
        cid_provider,
        stream,
        peer_addr,
        secure,
        buffer_size,
        messages_max,
        rb_tmp,
        packet_buf,
        tmp_buf,
        timeout,
        instance_id,
        zsender,
        zsender_stream,
        &zreceiver,
        shared,
    )
    .await
    {
        Ok(()) => debug!("conn {}: finished", cid),
        Err(e) => debug!("conn {}: process error: {:?}", cid, e),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::buffer::TmpBuffer;
    use crate::channel;
    use crate::waker;
    use std::fmt;
    use std::future::Future;
    use std::io::Read;
    use std::mem;
    use std::rc::Rc;
    use std::sync::Arc;
    use std::task::{Context, Poll, Waker};
    use std::time::Instant;

    struct NoopWaker {}

    impl NoopWaker {
        fn new() -> Self {
            Self {}
        }

        fn into_std(self: Rc<NoopWaker>) -> Waker {
            waker::into_std(self)
        }
    }

    impl waker::RcWake for NoopWaker {
        fn wake(self: Rc<Self>) {}
    }

    struct StepExecutor<F> {
        reactor: Reactor,
        fut: Pin<Box<F>>,
    }

    impl<F> StepExecutor<F>
    where
        F: Future,
    {
        fn new(reactor: Reactor, fut: F) -> Self {
            Self {
                reactor,
                fut: Box::pin(fut),
            }
        }

        fn step(&mut self) -> Poll<F::Output> {
            self.reactor.poll_nonblocking(self.reactor.now()).unwrap();

            let waker = Rc::new(NoopWaker::new()).into_std();
            let mut cx = Context::from_waker(&waker);

            self.fut.as_mut().poll(&mut cx)
        }

        fn advance_time(&mut self, now: Instant) {
            self.reactor.poll_nonblocking(now).unwrap();
        }
    }

    #[track_caller]
    fn check_poll<T, E>(p: Poll<Result<T, E>>) -> Option<T>
    where
        E: fmt::Debug,
    {
        match p {
            Poll::Ready(v) => match v {
                Ok(t) => Some(t),
                Err(e) => panic!("check_poll error: {:?}", e),
            },
            Poll::Pending => None,
        }
    }

    struct FakeSock {
        inbuf: Vec<u8>,
        outbuf: Vec<u8>,
        out_allow: usize,
    }

    impl FakeSock {
        fn new() -> Self {
            Self {
                inbuf: Vec::new(),
                outbuf: Vec::new(),
                out_allow: 0,
            }
        }

        fn add_readable(&mut self, buf: &[u8]) {
            self.inbuf.extend_from_slice(buf);
        }

        fn take_writable(&mut self) -> Vec<u8> {
            self.outbuf.split_off(0)
        }

        fn allow_write(&mut self, size: usize) {
            self.out_allow += size;
        }
    }

    impl Read for FakeSock {
        fn read(&mut self, buf: &mut [u8]) -> Result<usize, io::Error> {
            if self.inbuf.is_empty() {
                return Err(io::Error::from(io::ErrorKind::WouldBlock));
            }

            let size = cmp::min(buf.len(), self.inbuf.len());

            buf[..size].copy_from_slice(&self.inbuf[..size]);

            let mut rest = self.inbuf.split_off(size);
            mem::swap(&mut self.inbuf, &mut rest);

            Ok(size)
        }
    }

    impl Write for FakeSock {
        fn write(&mut self, buf: &[u8]) -> Result<usize, io::Error> {
            if buf.len() > 0 && self.out_allow == 0 {
                return Err(io::Error::from(io::ErrorKind::WouldBlock));
            }

            let size = cmp::min(buf.len(), self.out_allow);
            let buf = &buf[..size];

            self.outbuf.extend_from_slice(buf);
            self.out_allow -= size;

            Ok(buf.len())
        }

        fn write_vectored(&mut self, bufs: &[io::IoSlice]) -> Result<usize, io::Error> {
            let mut total = 0;

            for buf in bufs {
                if self.out_allow == 0 {
                    break;
                }

                let size = cmp::min(buf.len(), self.out_allow);
                let buf = &buf[..size];

                self.outbuf.extend_from_slice(buf.as_ref());
                self.out_allow -= size;

                total += buf.len();
            }

            Ok(total)
        }

        fn flush(&mut self) -> Result<(), io::Error> {
            Ok(())
        }
    }

    struct AsyncFakeSock {
        inner: Rc<RefCell<FakeSock>>,
    }

    impl AsyncFakeSock {
        fn new(sock: Rc<RefCell<FakeSock>>) -> Self {
            Self { inner: sock }
        }
    }

    impl AsyncRead for AsyncFakeSock {
        fn poll_read(
            self: Pin<&mut Self>,
            _cx: &mut Context,
            buf: &mut [u8],
        ) -> Poll<Result<usize, io::Error>> {
            let inner = &mut *self.inner.borrow_mut();

            match inner.read(buf) {
                Ok(usize) => Poll::Ready(Ok(usize)),
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => Poll::Pending,
                Err(e) => Poll::Ready(Err(e)),
            }
        }

        fn cancel(&mut self) {}
    }

    impl AsyncWrite for AsyncFakeSock {
        fn poll_write(
            self: Pin<&mut Self>,
            _cx: &mut Context,
            buf: &[u8],
        ) -> Poll<Result<usize, io::Error>> {
            let inner = &mut *self.inner.borrow_mut();

            match inner.write(buf) {
                Ok(usize) => Poll::Ready(Ok(usize)),
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => Poll::Pending,
                Err(e) => Poll::Ready(Err(e)),
            }
        }

        fn poll_close(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Result<(), io::Error>> {
            Poll::Ready(Ok(()))
        }

        fn is_writable(&self) -> bool {
            true
        }

        fn cancel(&mut self) {}
    }

    impl Identify for AsyncFakeSock {
        fn set_id(&mut self, _id: &str) {
            // do nothing
        }
    }

    struct SimpleCidProvider {
        cid: ArrayString<32>,
    }

    impl CidProvider for SimpleCidProvider {
        fn get_new_assigned_cid(&mut self) -> ArrayString<32> {
            self.cid
        }
    }

    #[test]
    fn message_tracker() {
        let mut t = MessageTracker::new(2);

        assert_eq!(t.in_progress(), false);
        assert_eq!(t.current(), None);

        t.start(websocket::OPCODE_TEXT).unwrap();
        assert_eq!(t.in_progress(), true);
        assert_eq!(t.current(), Some((websocket::OPCODE_TEXT, 0, false)));

        t.extend(5);
        assert_eq!(t.in_progress(), true);
        assert_eq!(t.current(), Some((websocket::OPCODE_TEXT, 5, false)));

        t.consumed(2, false);
        assert_eq!(t.in_progress(), true);
        assert_eq!(t.current(), Some((websocket::OPCODE_TEXT, 3, false)));

        t.done();
        assert_eq!(t.in_progress(), false);
        assert_eq!(t.current(), Some((websocket::OPCODE_TEXT, 3, true)));

        t.consumed(3, true);
        assert_eq!(t.current(), None);

        for _ in 0..t.items.capacity() {
            t.start(websocket::OPCODE_TEXT).unwrap();
            t.done();
        }
        let r = t.start(websocket::OPCODE_TEXT);
        assert!(r.is_err());
    }

    #[test]
    fn early_body() {
        let reactor = Reactor::new(100);

        let sock = Rc::new(RefCell::new(FakeSock::new()));
        sock.borrow_mut().allow_write(1024);

        let sock = RefCell::new(AsyncFakeSock::new(sock));

        let rb_tmp = Rc::new(TmpBuffer::new(12));

        let mut buf1 = RingBuffer::new(12, &rb_tmp);
        let mut buf2 = RingBuffer::new(12, &rb_tmp);

        buf2.write(b"foo").unwrap();

        let handler = RequestSendHeader::new(
            io_split(&sock),
            &mut buf1,
            &mut buf2,
            http1::ServerProtocol::new(),
            3,
        );
        assert_eq!(handler.early_body.borrow().overflow.is_none(), true);

        handler.append_body(b"hello", false, "").unwrap();
        assert_eq!(handler.early_body.borrow().overflow.is_none(), true);

        handler.append_body(b" world", false, "").unwrap();
        assert_eq!(handler.early_body.borrow().overflow.is_some(), true);

        handler.append_body(b"!", false, "").unwrap();

        handler.append_body(b"!", false, "").unwrap_err();

        {
            let mut executor = StepExecutor::new(reactor, handler.send_header());
            assert_eq!(check_poll(executor.step()), Some(()));
        }

        assert_eq!(handler.early_body.borrow().overflow.is_none(), true);

        let handler = handler.send_header_done();
        let header = sock.borrow_mut().inner.borrow_mut().take_writable();
        assert_eq!(header, b"foo");

        let w = handler.w.borrow();
        let mut buf_arr = [&b""[..]; VECTORED_MAX - 2];
        let bufs = w.buf.get_ref_vectored(&mut buf_arr);
        assert_eq!(bufs[0], b"hello wor");
        assert_eq!(bufs[1], b"ld!");
    }

    async fn req_fut(
        token: CancellationToken,
        sock: Rc<RefCell<FakeSock>>,
        secure: bool,
        s_from_conn: channel::LocalSender<zmq::Message>,
        r_to_conn: channel::LocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    ) -> Result<(), ServerError> {
        let mut cid = ArrayString::from_str("1").unwrap();
        let mut cid_provider = SimpleCidProvider { cid };

        let sock = AsyncFakeSock::new(sock);

        let r_to_conn = AsyncLocalReceiver::new(r_to_conn);
        let s_from_conn = AsyncLocalSender::new(s_from_conn);
        let buffer_size = 1024;

        let rb_tmp = Rc::new(TmpBuffer::new(1024));
        let packet_buf = Rc::new(RefCell::new(vec![0; 2048]));

        let timeout = Duration::from_millis(5_000);

        server_req_connection_inner(
            token,
            &mut cid,
            &mut cid_provider,
            sock,
            None,
            secure,
            buffer_size,
            buffer_size,
            &rb_tmp,
            packet_buf,
            timeout,
            s_from_conn,
            &r_to_conn,
        )
        .await
    }

    #[test]
    fn server_req_without_body() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            req_fut(token, sock, false, s_from_conn, r_to_conn)
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "GET /path HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "Connection: close\r\n",
            "\r\n"
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T148:2:id,1:1,3:ext,15:5:multi,4:true!}6:method,3:GET,3:ur",
            "i,23:http://example.com/path,7:headers,52:22:4:Host,11:exa",
            "mple.com,]22:10:Connection,5:close,]]}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T100:2:id,1:1,4:code,3:200#6:reason,2:OK,7:h",
            "eaders,34:30:12:Content-Type,10:text/plain,]]4:body,6:hell",
            "o\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        assert_eq!(check_poll(executor.step()), Some(()));

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Connection: close\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_req_with_body() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            req_fut(token, sock, false, s_from_conn, r_to_conn)
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "POST /path HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "Content-Length: 6\r\n",
            "Connection: close\r\n",
            "\r\n",
            "hello\n"
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T191:2:id,1:1,3:ext,15:5:multi,4:true!}6:method,4:POST,3:u",
            "ri,23:http://example.com/path,7:headers,78:22:4:Host,11:ex",
            "ample.com,]22:14:Content-Length,1:6,]22:10:Connection,5:cl",
            "ose,]]4:body,6:hello\n,}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T100:2:id,1:1,4:code,3:200#6:reason,2:OK,7:h",
            "eaders,34:30:12:Content-Type,10:text/plain,]]4:body,6:hell",
            "o\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        assert_eq!(check_poll(executor.step()), Some(()));

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Connection: close\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_req_timeout() {
        let now = Instant::now();
        let reactor = Reactor::new_with_time(100, now);

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (_s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, _r_from_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();

            req_fut(token, sock, false, s_from_conn, r_to_conn)
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        executor.advance_time(now + Duration::from_millis(5_000));

        match executor.step() {
            Poll::Ready(Err(ServerError::Timeout)) => {}
            _ => panic!("unexpected state"),
        }
    }

    #[test]
    fn server_req_pipeline() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            req_fut(token, sock, false, s_from_conn, r_to_conn)
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "GET /path1 HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "\r\n",
            "GET /path2 HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "\r\n",
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T123:2:id,1:1,3:ext,15:5:multi,4:true!}6:method,3:GET,3:ur",
            "i,24:http://example.com/path1,7:headers,26:22:4:Host,11:ex",
            "ample.com,]]}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T100:2:id,1:1,4:code,3:200#6:reason,2:OK,7:h",
            "eaders,34:30:12:Content-Type,10:text/plain,]]4:body,6:hell",
            "o\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T123:2:id,1:1,3:ext,15:5:multi,4:true!}6:method,3:GET,3:ur",
            "i,24:http://example.com/path2,7:headers,26:22:4:Host,11:ex",
            "ample.com,]]}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T100:2:id,1:1,4:code,3:200#6:reason,2:OK,7:h",
            "eaders,34:30:12:Content-Type,10:text/plain,]]4:body,6:hell",
            "o\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_req_secure() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            req_fut(token, sock, true, s_from_conn, r_to_conn)
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "GET /path HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "Connection: close\r\n",
            "\r\n"
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T149:2:id,1:1,3:ext,15:5:multi,4:true!}6:method,3:GET,3:ur",
            "i,24:https://example.com/path,7:headers,52:22:4:Host,11:ex",
            "ample.com,]22:10:Connection,5:close,]]}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T100:2:id,1:1,4:code,3:200#6:reason,2:OK,7:h",
            "eaders,34:30:12:Content-Type,10:text/plain,]]4:body,6:hell",
            "o\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        assert_eq!(check_poll(executor.step()), Some(()));

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Connection: close\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    async fn stream_fut(
        token: CancellationToken,
        sock: Rc<RefCell<FakeSock>>,
        secure: bool,
        s_from_conn: channel::LocalSender<zmq::Message>,
        s_stream_from_conn: channel::LocalSender<(ArrayVec<u8, 64>, zmq::Message)>,
        r_to_conn: channel::LocalReceiver<(arena::Rc<zhttppacket::OwnedResponse>, usize)>,
    ) -> Result<(), ServerError> {
        let mut cid = ArrayString::from_str("1").unwrap();
        let mut cid_provider = SimpleCidProvider { cid };

        let sock = AsyncFakeSock::new(sock);

        let r_to_conn = AsyncLocalReceiver::new(r_to_conn);
        let s_from_conn = AsyncLocalSender::new(s_from_conn);
        let s_stream_from_conn = AsyncLocalSender::new(s_stream_from_conn);
        let buffer_size = 1024;

        let rb_tmp = Rc::new(TmpBuffer::new(1024));
        let packet_buf = Rc::new(RefCell::new(vec![0; 2048]));
        let tmp_buf = Rc::new(RefCell::new(vec![0; buffer_size]));

        let timeout = Duration::from_millis(5_000);

        let shared_mem = Rc::new(arena::RcMemory::new(1));
        let shared = arena::Rc::new(ServerStreamSharedData::new(), &shared_mem).unwrap();

        server_stream_connection_inner(
            token,
            &mut cid,
            &mut cid_provider,
            sock,
            None,
            secure,
            buffer_size,
            10,
            &rb_tmp,
            packet_buf,
            tmp_buf,
            timeout,
            "test",
            s_from_conn,
            s_stream_from_conn,
            &r_to_conn,
            shared,
        )
        .await
    }

    #[test]
    fn server_stream_without_body() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (s_stream_from_conn, _r_stream_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            stream_fut(
                token,
                sock,
                false,
                s_from_conn,
                s_stream_from_conn,
                r_to_conn,
            )
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data =
            concat!("GET /path HTTP/1.1\r\n", "Host: example.com\r\n", "\r\n").as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T179:4:from,4:test,2:id,1:1,3:seq,1:0#3:ext,15:5:multi,4:t",
            "rue!}6:method,3:GET,3:uri,23:http://example.com/path,7:hea",
            "ders,26:22:4:Host,11:example.com,]]7:credits,4:1024#6:stre",
            "am,4:true!}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T127:2:id,1:1,6:reason,2:OK,7:headers,34:30:12:Content-Typ",
            "e,10:text/plain,]]3:seq,1:0#4:from,7:handler,4:code,3:200#",
            "4:body,6:hello\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        // connection reusable
        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_stream_with_body() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (s_stream_from_conn, r_stream_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            stream_fut(
                token,
                sock,
                false,
                s_from_conn,
                s_stream_from_conn,
                r_to_conn,
            )
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "POST /path HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n"
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T220:4:from,4:test,2:id,1:1,3:seq,1:0#3:ext,15:5:multi,4:t",
            "rue!}6:method,4:POST,3:uri,23:http://example.com/path,7:he",
            "aders,52:22:4:Host,11:example.com,]22:14:Content-Length,1:",
            "6,]]7:credits,4:1024#4:more,4:true!6:stream,4:true!}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg =
            concat!("T69:7:credits,4:1024#3:seq,1:0#2:id,1:1,4:from,7:handler,4:type,6:credit,}",);

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        // read message
        let (addr, msg) = r_stream_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        assert_eq!(addr.as_ref(), "handler".as_bytes());

        let buf = &msg[..];

        let expected = concat!(
            "T74:4:from,4:test,2:id,1:1,3:seq,1:1#3:ext,15:5:multi,4:tr",
            "ue!}4:body,6:hello\n,}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T127:2:id,1:1,6:reason,2:OK,7:headers,34:30:12:Content-Typ",
            "e,10:text/plain,]]3:seq,1:1#4:from,7:handler,4:code,3:200#",
            "4:body,6:hello\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        // connection reusable
        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
            "hello\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_stream_chunked() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(2));
        let scratch_mem = Rc::new(arena::RcMemory::new(2));
        let resp_mem = Rc::new(arena::RcMemory::new(2));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (s_stream_from_conn, _r_stream_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            stream_fut(
                token,
                sock,
                false,
                s_from_conn,
                s_stream_from_conn,
                r_to_conn,
            )
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data =
            concat!("GET /path HTTP/1.1\r\n", "Host: example.com\r\n", "\r\n").as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T179:4:from,4:test,2:id,1:1,3:seq,1:0#3:ext,15:5:multi,4:t",
            "rue!}6:method,3:GET,3:uri,23:http://example.com/path,7:hea",
            "ders,26:22:4:Host,11:example.com,]]7:credits,4:1024#6:stre",
            "am,4:true!}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T125:4:more,4:true!2:id,1:1,6:reason,2:OK,7:headers,34:30:",
            "12:Content-Type,10:text/plain,]]3:seq,1:0#4:from,7:handler",
            ",4:code,3:200#}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let msg = concat!("T52:3:seq,1:1#2:id,1:1,4:from,7:handler,4:body,6:hello\n,}");

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        // connection reusable
        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/plain\r\n",
            "Connection: Transfer-Encoding\r\n",
            "Transfer-Encoding: chunked\r\n",
            "\r\n",
            "6\r\n",
            "hello\n",
            "\r\n",
            "0\r\n",
            "\r\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }

    #[test]
    fn server_stream_early_response() {
        let reactor = Reactor::new(100);

        let msg_mem = Arc::new(arena::ArcMemory::new(1));
        let scratch_mem = Rc::new(arena::RcMemory::new(1));
        let resp_mem = Rc::new(arena::RcMemory::new(1));

        let sock = Rc::new(RefCell::new(FakeSock::new()));

        let (s_to_conn, r_to_conn) =
            channel::local_channel(1, 1, &reactor.local_registration_memory());
        let (s_from_conn, r_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (s_stream_from_conn, _r_stream_from_conn) =
            channel::local_channel(1, 2, &reactor.local_registration_memory());
        let (_cancel, token) = CancellationToken::new(&reactor.local_registration_memory());

        let fut = {
            let sock = sock.clone();
            let s_from_conn = s_from_conn
                .try_clone(&reactor.local_registration_memory())
                .unwrap();

            stream_fut(
                token,
                sock,
                false,
                s_from_conn,
                s_stream_from_conn,
                r_to_conn,
            )
        };

        let mut executor = StepExecutor::new(reactor, fut);

        assert_eq!(check_poll(executor.step()), None);

        // no messages yet
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // fill the connection's outbound message queue
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_ok(), true);
        assert_eq!(s_from_conn.try_send(zmq::Message::new()).is_err(), true);
        drop(s_from_conn);

        let req_data = concat!(
            "POST /path HTTP/1.1\r\n",
            "Host: example.com\r\n",
            "Content-Length: 6\r\n",
            "\r\n",
        )
        .as_bytes();

        sock.borrow_mut().add_readable(req_data);

        // connection won't be able to send a message yet
        assert_eq!(check_poll(executor.step()), None);

        // read bogus message
        let msg = r_from_conn.try_recv().unwrap();
        assert_eq!(msg.is_empty(), true);

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        // now connection will be able to send a message
        assert_eq!(check_poll(executor.step()), None);

        // read real message
        let msg = r_from_conn.try_recv().unwrap();

        // no other messages
        assert_eq!(r_from_conn.try_recv().is_err(), true);

        let buf = &msg[..];

        let expected = concat!(
            "T220:4:from,4:test,2:id,1:1,3:seq,1:0#3:ext,15:5:multi,4:t",
            "rue!}6:method,4:POST,3:uri,23:http://example.com/path,7:he",
            "aders,52:22:4:Host,11:example.com,]22:14:Content-Length,1:",
            "6,]]7:credits,4:1024#4:more,4:true!6:stream,4:true!}",
        );

        assert_eq!(str::from_utf8(buf).unwrap(), expected);

        let msg = concat!(
            "T150:2:id,1:1,6:reason,11:Bad Request,7:headers,34:30:12:C",
            "ontent-Type,10:text/plain,]]3:seq,1:0#4:from,7:handler,4:c",
            "ode,3:400#4:body,18:stopping this now\n,}",
        );

        let msg = zmq::Message::from(msg.as_bytes());
        let msg = arena::Arc::new(msg, &msg_mem).unwrap();

        let scratch = arena::Rc::new(
            RefCell::new(zhttppacket::ResponseScratch::new()),
            &scratch_mem,
        )
        .unwrap();

        let resp = zhttppacket::OwnedResponse::parse(msg, 0, scratch).unwrap();
        let resp = arena::Rc::new(resp, &resp_mem).unwrap();

        assert_eq!(s_to_conn.try_send((resp, 0)).is_ok(), true);

        assert_eq!(check_poll(executor.step()), None);

        let data = sock.borrow_mut().take_writable();
        assert_eq!(data.is_empty(), true);

        sock.borrow_mut().allow_write(1024);

        assert_eq!(check_poll(executor.step()), Some(()));

        let data = sock.borrow_mut().take_writable();

        let expected = concat!(
            "HTTP/1.1 400 Bad Request\r\n",
            "Content-Type: text/plain\r\n",
            "Connection: close\r\n",
            "Content-Length: 18\r\n",
            "\r\n",
            "stopping this now\n",
        );

        assert_eq!(str::from_utf8(&data).unwrap(), expected);
    }
}
