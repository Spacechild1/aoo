AOO network protocol
=================================

This file documents the AOO network protocol.
Generally, AOO uses the OSC protocol, but some message have
an alternative binary implementation for maximum efficiency.

<br>

# 1 AOO peer-to-peer communication (UDP)

<br>

The following messages are exchanged between AOO sources
and AOO sinks resp. between AOO clients via UDP sockets.

<br>

## 1.1 AOO source-sink communication

<br>

Messages exchanged between AOO sources and AOO sinks.

<br>

### /start (source → sink)

Start a new stream.

#### Pattern

`/aoo/sink/<id>/start`

#### Arguments

| type  | description                         |
| ----: | ----------------------------------- |
|  `i`  | source ID                           |
|  `s`  | version string                      |
|  `i`  | stream ID                           |
|  `i`  | sequence number start               |
|  `i`  | format ID                           |
|  `i`  | number of channels                  |
|  `i`  | sample rate                         |
|  `i`  | block size                          |
|  `s`  | codec name                          |
|  `b`  | codec extension                     |
|  `t`  | start time (NTP)                    |
|  `i`  | reblock/resample latency (samples)  |
|  `i`  | codec delay (samples)               |
| (`i`) | [metadata type](#data-types)        |
| (`b`) | metadata content                    |
|  `i`  | sample offset                       |

---

### /start (sink → source)

Request a start message.

#### Pattern

`/aoo/source/<id>/start`

#### Arguments

| type | description    |
| ---: | -------------- |
|  `i` | sink ID        |
|  `s` | version string |

---

### /stop (source → sink)

Stop a stream.

#### Pattern

`/aoo/sink/<id>/stop`

#### Arguments

| type  | description          |
| ----: | -------------------- |
|  `i`  | source ID            |
|  `i`  | stream ID            |
|  `i`  | last sequence number |
|  `i`  | sample offset        |

---

### /stop (sink → source)

Request a stop message.

#### Pattern

`/aoo/source/<id>/stop`

#### Arguments

| type | description |
| ---: | ----------- |
|  `i` | source ID   |
|  `i` | stream ID   |

---

### /data (source → sink)

Send stream data.

#### Pattern

`/aoo/sink/<id>/data`

#### Arguments

| type  | description            |
| ----: | ---------------------- |
|  `i`  | source ID              |
|  `i`  | stream ID              |
|  `i`  | sequence number        |
| (`t`) | time stamp             |
| (`d`) | real sample rate       |
|  `i`  | channel onset          |
|  `i`  | total data size        |
| (`i`) | message data size      |
| (`i`) | total number of frames |
| (`i`) | frame index            |
| (`b`) | data content           |

#### Data format

*data content* contains the (encoded) audio data together
with any number of so-called stream messages. The data
layout is the following:

| byte offset           | content      |
| --------------------: | ------------ |
| `0x00`                | message data |
| `<message data size>` | audio data   |

`<message data size>` corresponds to the *message data size*
argument in the OSC message. It should be a multiple of four.

In case there are no messages, *message data size* will be
zero and the *message data* section will be empty.

The message data itself uses the following format:

| byte offset          | content               |
| -------------------: | --------------------- |
| `0x00`               | message count (int32) |
| `0x04`               | message 0             |
| `<message size 0>`   | message 1             |
| ...                  | ...                   |
| `<message size N-2>` | message N-1           |

`<message size i>` is the total size of `message i` (see below).

Finally, each individual message is structured as follows:

| byte offset  | content                             |
| -----------: | ----------------------------------- |
| `0x00`       | sample offset (`uint16`)            |
| `0x02`       | channel number (`uint16`)           |
| `0x04`       | [data type](#data-types) (`uint16`) |
| `0x06`       | data size (`uint16`)                |
| `0x08`       | message data                        |

The total message size in bytes is `<data size> + 8`, rounded up
to a multiple of four. This makes sure that each message is
aligned to a four byte boundary. Consequently, *message data size*
will be a multiple of four, ensuring that *audio data* will
start on a four byte boundary as well (see above).

---

### /data (sink → source)

Request stream data.

#### Pattern

`/aoo/source/<id>/data`

#### Arguments

| type  | description   |
| ----: | ------------- |
|  `i`  | source ID     |
|  `i`  | stream ID     |
|  `i`  | sequence 1    |
|  `i`  | frame 1       |
| [`i`] | sequence 2    |
| [`i`] | frame 2       |
|  ...  | ...           |

---

### /invite (sink → source)

Invite a source.

#### Pattern

`/aoo/source/<id>/invite`

#### Arguments

| type  | description                      |
| ----: | -------------------------------- |
|  `i`  | sink ID                          |
|  `i`  | stream ID                        |
| (`i`) | [metadata type](#data-types)     |
| (`b`) | metadata content                 |

---

### /uninvite (sink → source)

Uninvite a source.

#### Pattern

`/aoo/source/<id>/uninvite`

#### Arguments

| type | description |
| ---: | ----------- |
|  `i` | sink ID     |
|  `i` | stream ID   |

---

### /decline (source → sink)

Decline an invitation.

#### Pattern

`/aoo/sink/<id>/decline`

#### Arguments

| type | description |
| ---: | ----------- |
|  `i` | source ID   |
|  `i` | stream ID   |

---

### /ping (source ↔ sink)

Send a ping.

#### Pattern

`/aoo/sink/<id>/ping`

`/aoo/source/<id>/ping`

#### Arguments

| type | description      |
| ---: | ---------------- |
|  `i` | sink/source ID   |
|  `t` | tt (send time)   |

---

### /pong (source ↔ sink)

Reply to ping message.

#### Pattern

`/aoo/sink/<id>/pong`

`/aoo/source/<id>/pong`

#### Arguments

| type  | description            |
| ----: | ---------------------- |
|  `i`  | sink/source ID         |
|  `t`  | tt1 (send time)        |
|  `t`  | tt2 (receive time)     |
|  `t`  | tt3 (send time)        |
| [`f`] | packet loss percentage |

<br>

## 1.2 AOO peer communication

<br>

Messages exchanged between AOO peers.

<br>

### /ping (peer ↔ peer)

Send a ping to a peer.

#### Pattern

`/aoo/peer/ping`

#### Arguments

| type  | description      |
| ----: | ---------------- |
|  `i`  | group ID         |
|  `i`  | user ID (sender) |
| (`t`) | tt (send time)   |

Note: *tt* is `nil` in handshake pings.

---

### /pong (peer ↔ peer)

Reply to ping message.

#### Pattern

`/aoo/peer/pong`

#### Arguments

| type  | description        |
| ----: | ------------------ |
|  `i`  | group ID           |
|  `i`  | user ID (sender)   |
| (`t`) | tt1 (send time)    |
| (`t`) | tt2 (receive time) |
| (`t`) | tt3 (send time)    |

Note: *tt1*, *tt2* and *tt3* are `nil` in handshake pongs.

---

### /msg (peer ↔ peer)

Send a message to a peer.

#### Pattern

`/aoo/peer/msg`

#### Arguments

| type  | description                      |
| ----: | -------------------------------- |
|  `i`  | group ID                         |
|  `i`  | peer ID (sender)                 |
|  `i`  | flags                            |
|  `i`  | sequence number                  |
|  `i`  | total message size               |
| (`i`) | number of frames                 |
| (`i`) | frame index                      |
| (`t`) | timetag                          |
|  `i`  | [message type](#data-types)      |
|  `b`  | message content                  |

Possible values for *flags*:

- `0x01`: reliable transmission

---

### /ack (peer ↔ peer)

Send message acknowledgements.

#### Pattern

`/aoo/peer/ack`

#### Arguments

| type  | description      |
| ----: | ---------------- |
|  `i`  | group ID         |
|  `i`  | peer ID (sender) |
|  `i`  | sequence 1       |
|  `i`  | frame 1          |
| [`i`] | sequence 2       |
| [`i`] | frame 2          |
|  ...  | ...              |

<br>

# 2 AOO client-server communication (UDP/TCP)

<br>

The following messages are exchanged between an
AOO client and an AOO server

<br>

## 2.1 NAT traversal (UDP)

<br>

The following messages are used for NAT traversal
resp. UDP hole punching.

<br>

### /query (client → server)

Query the public IPv4 address and port.

#### Pattern

`/aoo/server/query`

#### Arguments

None

---

### /query (server → client)

Reply to a `/query` message.

#### Pattern

`/aoo/client/query`

#### Arguments

| type | description           |
| ---: | --------------------- |
|  `s` | public IP address     |
|  `i` | public port           |

---

### /ping (client → server)

Send a ping message to keep the port open.

#### Pattern

`/aoo/server/ping`

#### Arguments

None

---

### /pong (server → client)

Reply to a `/ping` message.

#### Pattern

`/aoo/client/pong`

#### Arguments

None

<br>

## 2.2 Client requests and server responses (TCP)

<br>

Client requests and server responses over a TCP socket connection.

<br>

### /login (client → server)

Login to server.

#### Pattern

`/aoo/server/login`

#### Arguments

| type  | description                      |
| ----: | -------------------------------- |
|  `i`  | request token                    |
|  `s`  | version string                   |
|  `s`  | password (encrypted)             |
| (`i`) | [metadata type](#data-types)     |
| (`b`) | metadata content                 |
|  `i`  | address count                    |
|  `s`  | IP address 1                     |
|  `i`  | port 1                           |
| [`s`] | IP address 2                     |
| [`i`] | port 2                           |
|  ...  | ...                              |

---

### /login (server → client)

Login response.

#### Pattern

`/aoo/client/login`

#### Arguments

1. success:

    | type  | description                      |
    | ----: | -------------------------------- |
    |  `i`  | request token                    |
    |  `i`  | 0 (= no error)                   |
    |  `s`  | version string                   |
    |  `i`  | client ID                        |
    |  `i`  | flags                            |
    | (`i`) | [metadata type](#data-types)     |
    | (`b`) | metadata content                 |

2. failure: see [2.1.1 Error response](#error-response)

---

### /group/join (client → server)

Join a group on the server.

#### Pattern

`/aoo/server/group/join`

#### Arguments

| type  | description                            |
| ----: | -------------------------------------- |
|  `i`  | request token                          |
|  `s`  | group name                             |
|  `s`  | group password (encrypted)             |
| (`i`) | group [metadata type](#data-types)     |
| (`b`) | group metadata content                 |
|  `s`  | user name                              |
|  `s`  | user password (encrypted)              |
| (`i`) | user [metadata type](#data-types)      |
| (`b`) | user metadata content                  |
| (`s`) | relay hostname                         |
| (`i`) | relay port                             |

---

### /group/join (server → client)

Group join response.

#### Pattern

`/aoo/client/group/join`

#### Arguments

1. success:

    | type  | description                              |
    | ----: | ---------------------------------------- |
    |  `i`  | request token                            |
    |  `i`  | 0 (= no error)                           |
    |  `i`  | group ID                                 |
    |  `i`  | group flags                              |
    | (`i`) | group [metadata type](#data-types)       |
    | (`b`) | group metadata content                   |
    |  `i`  | user ID                                  |
    |  `i`  | user flags                               |
    | (`i`) | user [metadata type](#data-types)        |
    | (`b`) | user metadata content                    |
    | (`i`) | private [metadata type](#data-types)     |
    | (`b`) | private metadata content                 |
    | (`s`) | group relay hostname                     |
    | (`i`) | group relay port                         |

2. failure: see [2.1.1 Error response](#error-response)

---

### /group/leave (client → server)

Leave a group on the server.

#### Pattern

`/aoo/server/group/leave`

#### Arguments

| type | description   |
| ---: | ------------- |
|  `i` | request token |
|  `i` | group ID      |

---

### /group/leave (server → client)

Group leave response.

#### Pattern

`/aoo/client/group/leave`

#### Arguments

1. success:

    | type | description    |
    | ---: | -------------- |
    |  `i` | request token  |
    |  `i` | 0 (= no error) |

2. failure: see [2.1.1 Error response](#error-response)

---

### /group/update (client → server)

Update group metadata.

#### Pattern

`/aoo/server/group/update`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | request token                    |
|  `i` | group ID                         |
|  `i` | [metadata type](#data-types)     |
|  `b` | metadata content                 |

---

### /group/update (server → client)

Group update response.

#### Pattern

`/aoo/client/group/update`

#### Arguments

1. success:

    | type | description                      |
    | ---: | -------------------------------- |
    |  `i` | request token                    |
    |  `i` | 0 (= no error)                   |
    |  `i` | [metadata type](#data-types)     |
    |  `b` | metadata content                 |

2. failure: see [2.1.1 Error response](#error-response)

---

### /user/update (client → server)

Update user metadata.

#### Pattern

`/aoo/server/user/update`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | request token                    |
|  `i` | group ID                         |
|  `i` | [metadata type](#data-types)     |
|  `b` | metadata content                 |

---

### /user/update (server → client)

User update response.

#### Pattern

`/aoo/client/user/update`

#### Arguments

1. success:

    | type | description                      |
    | ---: | -------------------------------- |
    |  `i` | request token                    |
    |  `i` | 0 (= no error)                   |
    |  `i` | [metadata type](#data-types)     |
    |  `b` | metadata content                 |

2. failure: see [2.1.1 Error response](#error-response)

---

### /request (client → server)

Send custom request.

#### Pattern

`/aoo/server/request`

#### Arguments

| type | description                  |
| ---: | ---------------------------- |
|  `i` | request token                |
|  `i` | flags                        |
|  `i` | [data type](#data-types)     |
|  `b` | data content                 |

---

### /request (server → client)

Response to custom request.

#### Pattern

`/aoo/client/request`

#### Arguments

1. success:

    | type | description                  |
    | ---: | ---------------------------- |
    |  `i` | request token                |
    |  `i` | 0 (= no error)               |
    |  `i` | flags                        |
    |  `i` | [data type](#data-types)     |
    |  `b` | data content                 |

2. failure: see [2.1.1 Error response](#error-response)

<br>

### 2.1.1 Error response
<a name="error-response"></a>

All error responses have the same argument structure:

| type | description                    |
| ---: | ------------------------------ |
|  `i` | request token                  |
|  `i` | [error code](#error-codes)     |
|  `i` | system/user error code         |
|  `s` | system/user error message      |

<br>

## 2.3 Server notifications (TCP)

<br>

The following messages are sent by the server over a
TCP connection to notify one or more clients.

<br>

### /peer/join (server → client)

A peer has joined the group.

#### Pattern

`/aoo/client/peer/join`

#### Arguments

| type  | description                      |
| ----: | -------------------------------- |
|  `s`  | group name                       |
|  `i`  | group ID                         |
|  `s`  | user name                        |
|  `i`  | user ID                          |
|  `s`  | version string                   |
|  `i`  | flags                            |
| (`i`) | [metadata type](#data-types)     |
| (`b`) | metadata content                 |
| (`s`) | relay hostname                   |
| (`i`) | relay port                       |
|  `i`  | address count                    |
|  `s`  | IP address 1                     |
|  `i`  | port 1                           |
| [`s`] | IP address 2                     |
| [`i`] | port 2                           |
|  ...  | ...                              |

---

### /peer/leave (server → client)

A peer has left the group.

#### Pattern

`/aoo/client/peer/leave`

#### Arguments

| type | description |
| ---: | ----------- |
|  `i` | group ID    |
|  `i` | peer ID     |

---

### /peer/changed (server → client)

Peer metadata has changed.

#### Pattern

`/aoo/client/peer/changed`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | group ID                         |
|  `i` | peer ID                          |
|  `i` | [metadata type](#data-types)     |
|  `b` | metadata content                 |

---

### /user/changed (server → client)

User metadata has changed.

#### Pattern

`/aoo/client/user/changed`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | group ID                         |
|  `i` | user ID                          |
|  `i` | [metadata type](#data-types)     |
|  `b` | metadata content                 |

---

### /group/changed (server → client)

Group metadata has changed.

#### Pattern

`/aoo/client/group/changed`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | group ID                         |
|  `i` | user ID                          |
|  `i` | [metadata type](#data-types)     |
|  `b` | metadata content                 |

The user ID refers to the user that updated the group.
-1 means that the group has been updated by the server.

---

### /group/eject (server → client)

Ejected from group.

#### Pattern

`/aoo/client/group/eject`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | group ID                         |

---

### /msg (server → client)

Generic server notification.

#### Pattern

`/aoo/client/msg`

#### Arguments

| type | description                      |
| ---: | -------------------------------- |
|  `i` | [metadata type](#data-types)     |
|  `b` | message content                  |

<br>

## 2.4 Other

<br>

### /ping (client → server)

Send a ping message to the server. This is used as a heartbeat
to keep the TCP connection open. (For example, NAT proxies or
firewalls tend to drop connections after a certain timeout.)

#### Pattern

`/aoo/server/ping`

#### Arguments

None

---

### /pong (server → client)

Reply to a client `/ping` message. This can be used to detect
broken server connections.

#### Pattern

`/aoo/client/pong`

#### Arguments

None

---

### /ping (server → client)

Send a ping message to the client. This is used as a heartbeat
to keep the TCP connection open.

#### Pattern

`/aoo/client/ping`

#### Arguments

None

---

### /pong (server → client)

Reply to a server `/ping` message. This can be used to detect
broken client connections.

#### Pattern

`/aoo/server/pong`

#### Arguments

None

<br>

# 3 Relay (UDP)

<br>

### /relay

A relayed message.

If sent from an AOO peer to an AOO relay server, the message
contains the destination endpoint address (IP address + port)
together with the original message data. The server extracts
the destination endpoint address, replaces it with the source
endpoint address and forwards the message to the destination.
The destination extracts the source endpoint address and message
data and forwards the original message to the appropriate receiver.

#### Pattern

`/aoo/relay`

#### Arguments

| type | description |
| ---: | ----------- |
|  `s` | IP address  |
|  `i` | port        |
|  `b` | message     |

<br>

# 4 Constants

<br>

### 4.1 Error codes
<a name="error-codes"></a>

| value | description                |
| ----: | -------------------------- |
|    -1 | Unspecified                |
|     0 | No error                   |

For a full list of error codes see `AooError` in `aoo/aoo_types.h`.

<br>

### 4.2 Data types
<a name="data-types"></a>

| value | description              |
| ----: | ------------------------ |
|    -1 | Unspecified              |
|     0 | Raw/binary data          |
|     1 | plain text (UTF-8)       |
|     2 | OSC (Open Sound Control) |
|     3 | MIDI                     |
|     4 | FUDI (Pure Data)         |
|     5 | JSON (UTF-8)             |
|     6 | XML (UTF-8)              |
|     7 | 32-bit float array       |
|     8 | 64-bit float array       |
|     9 | 16-bit integer array     |
|    10 | 32-bit integer array     |
|    11 | 64-bit integer array     |
| 1000< | User specified           |

<br>

### 4.3 Codec names

| value  | description |
| ------ | ----------- |
| "pcm"  | PCM codec   |
| "opus" | Opus codec  |

<br>

# 5 Binary messages

<br>

TODO
