AooServer {
	classvar <>servers;
	classvar <>defaultPort = 7078;

	var <>server;
	var <>port;
	var <>replyAddr;

	var eventOSCFunc;
	var eventFuncs;

	*initClass {
		servers = IdentityDictionary();
	}

	*find { arg port;
		^servers[port];
	}

	*new { arg port, server, action;
		^super.new.init(port, server, action);
	}

	init { arg port, server, action, password, relay = false;
		var localAddr = NetAddr.localAddr;
		port = port ?? this.class.defaultPort;

		if (servers[port].notNil) {
			^Error("AooServer on port % already exists!".format(port)).throw;
		};

		this.server = server ?? Server.default;
		eventFuncs = IdentityDictionary();

		Aoo.prGetReplyAddr(port, server, { |addr|
			if (addr.isNil) {
				action.value(nil);
			} {
				// create AooServer on the server
				OSCFunc({ arg msg;
					var success = msg[2].asBoolean;
					success.if {
						this.prInit(port, addr);
						action.value(addr);
					} {
						"Couldn't create AooServer on port %: %".format(port, msg[3]).error;
						action.value(nil);
					};
				}, '/aoo/server/new', argTemplate: [port]).oneShot;

				this.server.sendMsg('/cmd', '/aoo_server_new', port,
					password ? "", relay.asBoolean;
				);
			}
		});
	}

	prInit { arg port, addr;
		this.port = port;
		replyAddr = addr;

		// handle events
		eventOSCFunc = OSCFunc({ arg msg;
			this.prHandleEvent(msg[2], msg[3..]);
		}, '/aoo/server/event', addr, argTemplate: [port]);

		ServerQuit.add { this.free };

		servers[port] = this;
	}

	free {
		eventOSCFunc.free;
		port.notNil.if {
			server.sendMsg('/cmd', '/aoo_server_free', port);
			if (servers[port] === this) {
				servers[port] = nil;
			} { "bug: AooServer not registered".error }
		};
		server = nil;
		port = nil;
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
		// \clientAdd, \clientRemove, \groupAdd, \groupRemove,
		// \groupJoin, \groupLeave
		var id, name, version, metadata, code, msg;
		var groupID, userID, groupName, userName, clientID, userMetadata;
		var event = type.switch(
			\clientAdd, {
				id = args[0];
				version = args[1];
				metadata = AooData.fromBytes(*args[2..3]);
				[ id, version, metadata ];
			},
			\clientRemove, {
				id = args[0];
				code = args[1];
				msg = args[2];
				if (code == 0) { [ id ] } { [ id, msg, code ] };
			},
			\groupAdd, {
				id = args[0];
				name = args[1];
				metadata = AooData.fromBytes(*args[2..3]);
				// [ AooServerGroup.prNew(this, name, id, metadata) ];
				[ id, name, metadata ];
			},
			\groupRemove, {
				id = args[0];
				name = args[1];
				[ id, name ];
			},
			\groupJoin, {
				groupID = args[0];
				userID = args[1];
				groupName = args[2];
				userName = args[3];
				clientID = args[4];
				userMetadata = AooData.fromBytes(*args[5..6]);
				[ groupID, userID, groupName, userName, clientID, userMetadata ];
			},
			\groupLeave, {
				groupID = args[0];
				userID = args[1];
				groupName = args[2];
				userName = args[3];
				[ groupID, userID, groupName, userName ];
			},
			{ "AooServer: ignore unknown event '%'".format(type).warn; nil }
		);
		if (event.notNil) {
			eventFuncs[type].value(*event);
		}
	}

	prAdd { arg user;
		this.users = this.users.add(user);
	}

	prRemove { arg user;
		var index = this.users.indexOfEqual(user);
		index !? { this.users.removeAt(index) };
	}
}