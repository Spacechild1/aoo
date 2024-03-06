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

	init { arg port, server, action;
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

				this.server.sendMsg('/cmd', '/aoo_server_new', port);
			}
		});
	}

	prInit { arg port, addr;
		this.port = port;
		replyAddr = addr;

		// handle events
		eventOSCFunc = OSCFunc({ arg msg, time;
			var event = this.prHandleEvent(msg[2..], time);
			event !? {
				this.eventHandler.value(*event);
			}
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

	prHandleEvent { arg type ...args;
		// /disconnect, /error, /user/join, /user/leave,
		// /group/join, /group/leave
		var user, addr;

		type.switch(
			'/error', {
				"AooServer: % (%)".format(args[1], args[0]).error;
			},
			'/user/join', {
				user = ( name: args[0], id: args[1],
					addr: AooAddr(*args[2..3]));
				this.prAdd(user);
				^[type, user.name, user.id, user.addr];
			},
			'/user/leave', {
				user = ( name: args[0], id: args[1],
					addr: AooAddr(*args[2..3]));
				this.prRemove(user);
				^[type, user.name, user.id, user.addr];
			}
		);
		^[type] ++ args; // return original event
	}

	prAdd { arg user;
		this.users = this.users.add(user);
	}

	prRemove { arg user;
		var index = this.users.indexOfEqual(user);
		index !? { this.users.removeAt(index) };
	}
}