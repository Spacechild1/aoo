AooServer {
	classvar <>servers;

	var <>server;
	var <>port;
	var <>replyAddr;
	var <>eventHandler;
	var <>users;

	var eventOSCFunc;

	*initClass {
		servers = IdentityDictionary.new;
	}

	*find { arg port;
		^servers[port];
	}

	*new { arg port, server, action;
		^super.new.init(port, server, action);
	}

	init { arg port, server, action, password, relay = false;
		var localAddr = NetAddr.localAddr;

		if (servers[port].notNil) {
			^Error("AooServer on port % already exists!".format(port)).throw;
		};

		this.server = server ?? Server.default;
		this.users = [];

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
		users = nil;
	}

	prHandleEvent { arg type, args;
		// \clientAdd, \clientRemove, \groupAdd, \groupRemove,
		// \groupJoin, \groupLeave
		var event = type.switch(
			\clientAdd, {
				var id = args[0];
				var version = args[1];
				var metadata = AooData.fromBytes(*args[2..3]);
				[ id, version, metadata ];
			},
			\clientRemove, {
				var id = args[0];
				var code = args[1];
				var msg = args[2];
				if (code == 0) { [ id ] } { [ id, msg, code ] };
			},
			\groupAdd, {
				var id = args[0];
				var name = args[1];
				var metadata = AooData.fromBytes(*args[2..3]);
				// [ AooServerGroup.prNew(this, name, id, metadata) ];
				[ id, name, metadata ];
			},
			\groupRemove, {
				var id = args[0];
				var name = args[1];
				[ id, name ];
			},
			\groupJoin, {
				var groupID = args[0];
				var userID = args[1];
				var groupName = args[2];
				var userName = args[3];
				var clientID = args[4];
				var userMetadata = AooData.fromBytes(*args[5..6]);
				[ groupID, userID, groupName, userName, clientID, userMetadata ];
			},
			\groupLeave, {
				var groupID = args[0];
				var userID = args[1];
				var groupName = args[2];
				var userName = args[3];
				[ groupID, userID, groupName, userName ];
			},
			{ "ignore unknown event '%'".format(type).warn; nil }
		);
		if (event.notNil) {
			this.eventHandler.value(type, event);
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