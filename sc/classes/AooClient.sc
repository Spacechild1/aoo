AooClient {
	classvar <>clients;
	classvar nextToken = 0;

	var <>server;
	var <>port;
	var <>id;
	var <>state;
	var <>dispatcher;
	var <>peers;
	var <>groups;

	var eventOSCFunc;
	var msgOSCFunc;
	var eventFuncs;
	var replyAddr;
	var nodeAddr;

	*initClass {
		var doFree = {
			clients.values.do(_.free);
			clients.clear;
		};
		clients = IdentityDictionary.new;
		ServerQuit.add(doFree);
		CmdPeriod.add(doFree);
	}

	*find { arg port;
		^clients[port];
	}

	*new { arg port, server, action;
		^super.new.init(port, server, action);
	}

	init { arg port, server, action;
		var localAddr = NetAddr.localAddr;

		if (clients[port].notNil) {
			^Error("AooClient on port % already exists!".format(port)).throw;
		};

		this.server = server ?? Server.default;
		this.peers = [];
		this.state = \disconnected;
		this.dispatcher = AooDispatcher(this);
		eventFuncs = IdentityDictionary();
		replyAddr = NetAddr(this.server.addr.ip, port);

		Aoo.prGetReplyAddr(port, this.server, { |addr|
			if (addr.isNil) {
				action.value(nil);
			} {
				// create AooClient on the server
				OSCFunc({ arg msg;
					var success = msg[2].asBoolean;
					success.if {
						this.prInit(port, addr);
						action.value(this);
					} {
						"Couldn't create AooClient on port %: %".format(port, msg[3]).error;
						action.value(nil);
					};
				}, '/aoo/client/new', argTemplate: [port]).oneShot;

				this.server.sendMsg('/cmd', '/aoo_client_new', port);
			}
		});
	}

	prInit { arg port, addr;
		this.port = port;
		replyAddr = addr;
		nodeAddr = NetAddr(addr.ip, port);

		// handle events
		eventOSCFunc = OSCFunc({ arg msg;
			this.prHandleEvent(msg[2], msg[3..]);
		}, '/aoo/client/event', addr, argTemplate: [port]);

		// handle messages
		msgOSCFunc = OSCFunc({ arg msg, time;
			this.prHandleMsg(time, *msg[2..]);
		}, '/aoo/client/msg', addr, argTemplate: [port]);

		ServerQuit.add { this.free };

		clients[port] = this;
	}

	free {
		eventOSCFunc.free;
		msgOSCFunc.free;
		replyAddr = nil;
		nodeAddr = nil;
		port.notNil.if {
			server.sendMsg('/cmd', '/aoo_client_free', port);
			if (clients[port] === this) {
				clients[port] = nil;
			} { "bug: AooClient not registered".error }
		};
		server = nil;
		port = nil;
		peers = nil;
		groups = nil;
	}

	addListener { arg type, func;
		eventFuncs[type] = eventFuncs[type].addFunc(func);
	}

	removeListener { arg type, func;
		if (func.notNil) {
			eventFuncs[type] = eventFuncs[type].removeFunc(func);
		} {
			eventFuncs.removeAt(type);
		}
	}

	prHandleEvent { arg type, args;
		// \disconnect, \peerJoin, \peerLeave, \peerHandshake, \peerTimeout, \peerPing
		var md, peer;
		var event = type.switch(
			\disconnect, {
				"disconnected from server".error;
				args[1]
			},
			\peerJoin, {
				md = AooData.fromBytes(*args[6..7]);
				peer = AooPeer.prFromEvent(*(args[0..5] ++ md));
				this.prAddPeer(peer);
				peer
			},
			\peerLeave, {
				peer = AooPeer.prFromEvent(*args[0..5]);
				this.prRemovePeer(peer);
				peer
			},
			\peerHandshake, {
				AooPeer.prFromEvent(*args[0..5])
			},
			\peerTimeout, {
				AooPeer.prFromEvent(*args[0..5])
			},
			\peerPing, {
				peer = AooPeer.prFromEvent(*args[0..1]);
				peer = this.prFindPeer(peer, true);
				if (peer.notNil) { [peer] ++ args[2..] } { nil }
			},
			{ "%: ignore unknown event '%'".format(this.class.name, type).warn; nil }
		);
		if (event.notNil) {
			eventFuncs[type].value(*event);
		}
	}

	prHandleMsg { arg time, group, user, type, data;
		var msg, peer;
		var handler = eventFuncs[\msg];
		if (handler.notNil) {
			peer = AooPeer.prFromEvent(group, user);
			peer = this.prFindPeer(peer, true);
			if (peer.notNil) {
				msg = AooData.fromBytes(type, data);
				if (msg.notNil) {
					handler.value(msg, time, peer);
				}
			}
		}
	}

	prAddPeer { arg peer;
		if (this.prFindPeer(peer).isNil) {
			this.peers = this.peers.add(peer)
		} {
			"peer % already added".format(peer).error;
		}
	}

	prRemovePeer { arg peer;
		var index = this.peers.indexOfEqual(peer);
		if (index.notNil) { this.peers.removeAt(index) }
		{ "could not remove peer %".format(peer).error }
	}

	prFindPeer { arg peer, loud=false;
		peers.do { |p|
			if (p == peer) { ^p }
		};
		if (loud) { "could not find peer %".format(peer).error };
		^nil;
	}

	prAddGroup { arg group;
		if (this.prFindGroup(group).isNil) {
			this.groups = this.groups.add(group)
		} {
			"group % already added".format(group).error;
		}
	}

	prRemoveGroup { arg group;
		var index;
		// remove all peers that belong to this group!
		this.peers = this.peers.select { |p| p.group != group };
		// remove the group itself
	    index = this.groups.indexOfEqual(group);
		if (index.notNil) { this.groups.removeAt(index) }
		{ "could not remove group %".format(group).error }
	}

	prFindGroup { arg group, loud = false;
		groups.do { |g|
			if (g == group) { ^g }
		};
		if (loud) { "could not find group %".format(group).error };
		^nil;
	}

	connect { arg hostname, port, password, metadata, action, timeout=10;
		var resp, token;
		this.port ?? { MethodError("AooClient: not initialized", this).throw };

		state.switch(
			\connected, { "AooClient: already connected".warn; ^this },
			\connecting, { "AooClient: still connecting".warn ^this }
		);
		state = \connecting;

		port = port ?? AooServer.defaultPort;
		password = password ?? "";
		token = this.class.prNextToken;

		resp = OSCFunc({ arg msg;
			var errmsg, errcode, clientID, version, metadata;
			var success = (msg[3] == 0);
			if (success) {
				clientID = msg[4];
				version = msg[5];
				metadata = AooData.fromBytes(*msg[6..7]);
				"AooClient: connected to % % (client ID: %)".format(hostname, port, clientID).postln;
				state = \connected;
				action.value(true, clientID, version, metadata);
			} {
				errmsg = msg[4];
				"AooClient: couldn't connect to % %: %".format(hostname, port, errmsg).error;
				state = \disconnected;
				action.value(false, errmsg);
			};
		}, '/aoo/client/connect', replyAddr, argTemplate: [this.port, token]).oneShot;

		// NOTE: the default timeout should be larger than the default
		// UDP handshake time out.
		// We need the timeout in case a reply message gets lost and
		// leaves the client in limbo...
		SystemClock.sched(timeout, {
			(state == \connecting).if {
				"AooClient: connection time out".error;
				resp.free;
				state = \disconnected;
				action.value(false, "time out");
			}
		});

		if (metadata.notNil) {
			server.sendMsg('/cmd', '/aoo_client_connect',
				this.port, token, hostname, port, password, *metadata.asOSCArgArray);
		} {
			server.sendMsg('/cmd', '/aoo_client_connect',
				this.port, token, hostname, port, password, $N, $N);
		}
	}

	disconnect { arg action;
		var token;
		this.port ?? { MethodError("AooClient not initialized", this).throw };
		(state != \connected).if {
			"AooClient not connected".warn;
			^this;
		};
		token = this.class.prNextToken;

		OSCFunc({ arg msg;
			var success = (msg[3] == 0);
			var errmsg = msg[4];
			success.if {
				// remove all groups and peers
				this.peers = [];
				this.groups = [];
				this.id = nil;
				"AooClient: disconnected".postln;
			} {
				"AooClient: couldn't disconnect: %".format(errmsg).error;
			};
			state = \disconnected;
			action.value(success, errmsg);
		}, '/aoo/client/disconnect', replyAddr, argTemplate: [this.port, token]).oneShot;

		server.sendMsg('/cmd', '/aoo_client_disconnect', this.port, token);
	}

	joinGroup { arg groupName, groupPwd, userName, userPwd, groupMetadata, userMetadata, relayAddr, action;
		var token, args;
		this.port ?? { MethodError("AooClient not initialized", this).throw };
		token = this.class.prNextToken;
		groupPwd = groupPwd ?? { "" };
		userPwd = userPwd ?? { "" };

		OSCFunc({ arg msg;
			var errmsg, errcode, groupID, userID, user, group;
			var groupMetadata, userMetadata, privateMetadata, relayAddr;
			var success = (msg[3] == 0);
			if (success) {
				groupID = msg[4];
				userID = msg[5];
				groupMetadata = AooData.fromBytes(*msg[6..7]);
				userMetadata = AooData.fromBytes(*msg[8..9]);
				privateMetadata = AooData.fromBytes(*msg[10..11]);
				"AooClient: joined group '%' as user '%' (group ID: %, user ID: %)".format(groupName, userName, groupID, userID).postln;
				user = AooUser(userName, userID, userMetadata);
				group = AooGroup(groupName, groupID, groupMetadata);
				this.prAddGroup(group);
				action.value(true, group, user, privateMetadata);
			} {
				errmsg = msg[4];
				"AooClient: couldn't join group '%': %".format(groupName, errmsg).error;
				action.value(false, errmsg);
			};
		}, '/aoo/client/group/join', replyAddr, argTemplate: [this.port, token]).oneShot;

		// port, token, group name, group pwd, [group metadata],
		// user name, user pwd, [user metadata], [relay address]
		args = [this.port, token, groupName, groupPwd, userName, userPwd];
		if (groupMetadata.notNil) {
			args = args ++ groupMetadata.asOSCArgArray;
		} { args.add($N).add($N) };
		if (userMetadata.notNil) {
			args = args ++ userMetadata.asOSCArgArray;
		} { args.add($N).add($N) };
		if (relayAddr.notNil) {
			args = args.add(relayAddr.ip).add(relayAddr.port);
		} { args.add($N).add($N) };

		server.sendMsg('/cmd', '/aoo_client_group_join', *args);
	}

	leaveGroup { arg group, action;
		var token, name;
		this.port ?? { MethodError("AooClient not initialized", this).throw };
		token = this.class.prNextToken;
		if (group.isNil) {
			// take the first (and only) group
			if (this.groups.size == 0) {
				^MethodError("not a group member", this).throw
			};
			if (this.groups.size > 1) {
				^MethodError("member of multiple groups", this).throw
			};
			group = this.groups[0];
		} {
			if (group.isKindOf(AooGroup).not) {
				group = AooGroup(group);
			};
			group = this.prFindGroup(group);
			if (group.isNil) { ^MethodError("not a group member", this).throw };
		};

		OSCFunc({ arg msg;
			var success = (msg[3] == 0);
			var errmsg = msg[4];
			if (success) {
				"AooClient: left group '%'".format(group.name).postln;
				this.prRemoveGroup(group);
			} {
				"AooClient: couldn't leave group '%': %".format(group.name, errmsg).error;
			};
			action.value(success, errmsg);
		}, '/aoo/client/group/leave', replyAddr, argTemplate: [this.port, token]).oneShot;

		server.sendMsg('/cmd', '/aoo_client_group_leave', this.port, token, group.id);
	}

	sendMsg { arg target, time, msg, type = \osc, reliable = false;
		var oscMsg, data = AooData(type, msg);
		var groupID = -1, userID = -1, group, peer;
		if (target.notNil) {
			if (target.isKindOf(AooPeer)) {
				// peer
				peer = this.prFindPeer(target);
				if (peer.isNil) {
					^MethodError("could not find peer %".format(target), this).throw
				};
				groupID = peer.group.id;
				userID = peer.user.id;
			} {
				// group
				if (target.isKindOf(AooGroup)) {
					group = this.prFindGroup(target);
					if (group.isNil) {
						^MethodError("could not find group %".format(target), this).throw
					};
					groupID = group.id;
				} {
					^MethodError("bad 'target' argument", this).throw;
				}
			}
		}; // else: broadcast
		oscMsg = ['/sc/msg', groupID, userID, time !? { time.asFloat }, reliable.asBoolean.asInteger ] ++ data.asOSCArgArray;
		oscMsg.postln;
		time.notNil.if {
			// schedule on the current (logical) system time.
			// on the Server, we add the relative timestamp contained in the OSC message
			nodeAddr.sendBundle(0, oscMsg);
		} {
			nodeAddr.sendMsg(*oscMsg);
		};
	}

	packetSize { arg size;
		server.sendMsg('/cmd', '/aoo_packetsize', this.port, size);
	}

	pingInterval { arg sec;
		server.sendMsg('/cmd', '/aoo_ping', this.port, sec);
	}

	// Try to find peer, but only if no IP/port is given.
	// So far only called by send().
	prResolveAddr { arg addr;
		var peer;
		addr.ip !? { ^addr; };
		peer = this.prFindPeer(addr);
		peer !? { ^peer; };
		addr.isKindOf(AooPeer).if {
			MethodError("%: couldn't find peer %".format(this.class.name, addr), this).throw;
		} {
			MethodError("%: bad address %".format(this.class.name, addr), this).throw;
		}
	}

	*prNextToken {
		^nextToken = nextToken + 1;
	}
}
