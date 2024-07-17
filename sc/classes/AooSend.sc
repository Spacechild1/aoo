AooSend : UGen {
	var <>desc;
	var <>tag;
	var <>port;
	var <>id;

	*ar { arg port, id=0, channels, gate=0, tag;
		^this.multiNewList([\audio, tag, port, id, gate] ++ channels);
	}
	*kr { ^this.shouldNotImplement(thisMethod) }

	init { arg tag, port, id, gate ... inputs;
		if (tag.isKindOf(UGen)) {
			MethodError("'tag' must not be a UGen!", this).throw;
		};
		if (port.isInteger.not) {
			MethodError("'port' must be an Integer!", this).throw;
		};
		this.tag = tag !? { tag.asSymbol }; // !
		this.port = port;
		this.id = id;
		this.inputs = [port, id, gate, inputs.size] ++ inputs;
		^0; // doesn't have any output
	}

	optimizeGraph {
		Aoo.prMakeMetadata(this);
	}

	synthIndex_ { arg index;
		super.synthIndex_(index); // !
		// update metadata (ignored if reconstructing from disk)
		this.desc.notNil.if { this.desc.index = index; };
	}
}

AooSendCtl : AooCtl {
	classvar <>ugenClass;

	var <>sinks;

	*initClass {
		Class.initClassTree(AooSend);
		ugenClass = AooSend;
	}

	init {
		sinks = [];
	}

	free {
		super.free;
		sinks = nil;
	}

	prParseEvent { arg type, args;
		// \invite, \uninvite, \add, \remove, \ping
		// currently, all events start with AOO endpoint
		var addr = this.prResolveAddr(AooAddr(args[0], args[1]));
		var id = args[2];
		var sink = AooEndpoint(addr, id);
		var event = [sink];
		// If sink doesn't exist, fake an \add event.
		// This happens if the sink has been added
		// automatically before we could create the controller.
		if (this.prFind(sink).isNil and: { type != \add }) {
			this.prAdd(sink);
			this.eventHandler.value(\add, event);
		};
		type.switch(
			\add, { this.prAdd(sink); ^event },
			\remove, { this.prRemove(sink); ^event },
			\invite, { ^event ++ args[3] ++ AooData.fromBytes(*args[4..]) },
			\uninvite, { ^event ++ args[3] },
			\ping, { ^event ++ args[3..] },
			\frameResend, { ^event ++ args[3] },
			{ "%: ignore unknown event '%'".format(this.class, type).warn; ^nil }
		)
	}

	add { arg addr, id, active=true, action;
		var replyID = AooCtl.prNextReplyID;
		addr = this.prResolveAddr(addr);

		this.prMakeOSCFunc({ arg success, ip, port, id;
			var newAddr, sink;
			success.if {
				newAddr = this.prResolveAddr(AooAddr(ip, port));
				sink = AooEndpoint(newAddr, id);
				this.prAdd(sink);
				action.value(sink);
			} { action.value(nil) }
		}, '/aoo/add', replyID).oneShot;

		this.prSendMsg('/add', replyID, addr.ip, addr.port, id, active);
	}

	remove { arg addr, id, action;
		var replyID = AooCtl.prNextReplyID;
		addr = this.prResolveAddr(addr);

		this.prMakeOSCFunc({ arg success;
			action.value(success);
		}, '/aoo/remove', replyID).oneShot;

		this.prSendMsg('/remove', replyID, addr.ip, addr.port, id);
	}

	prAdd { arg sink;
		this.sinks = this.sinks.add(sink);
	}

	prRemove { arg sink;
		var index = this.sinks.indexOfEqual(sink);
		index !? { this.sinks.removeAt(index) };
	}

	prFind { arg sink;
		var index = this.sinks.indexOfEqual(sink);
		^index !? { this.sinks[index] };
	}

	removeAll { arg action;
		var replyID = AooCtl.prNextReplyID;

		this.prMakeOSCFunc({
			this.sinks = [];
			action.value;
		}, '/aoo/remove', replyID).oneShot;

		this.prSendMsg('/remove', replyID);
	}

	handleInvite { arg addr, id, token, accept;
		// TODO
	}

	handleUninvite { arg addr, id, token, accept;
		// TODO
	}

	autoInvite { arg enable;
		this.prSendMsg('/auto_invite', enable);
	}

	start { arg metadata;
		if (metadata.notNil) {
			this.prSendMsg('/start', *metadata.asOSCArgArray);
		} {
			this.prSendMsg('/start', $N, $N);
		}
	}

	stop {
		this.prSendMsg('/stop');
	}

	format { arg fmt, action;
		var replyID = AooCtl.prNextReplyID;
		fmt.isKindOf(AooFormat).not.if {
			MethodError("aoo: bad type for 'fmt' parameter", this).throw;
		};
		this.prMakeOSCFunc({ arg success, codec ...args;
			var f;
			success.if {
				f = codec.switch(
					\pcm, { AooFormatPCM(*args) },
					\opus, { AooFormatOpus(*args) },
					{ "%: unknown format '%'".format(this.class.name, codec).error; nil }
				)
			};
			action.value(f);
		}, '/aoo/format', replyID).oneShot;
		this.prSendMsg('/format', replyID, *fmt.asOSCArgArray);
	}

	setCodecParam { arg codec, param, value;
		this.prSendMsg('/codec_set', codec, param, value);
	}

	getCodecParam { arg codec, param, action;
		var replyID = AooCtl.prNextReplyID;

		this.prMakeOSCFunc({ arg success, codec, param, value;
			success.if { action.value(value) }
			{ action.value };
		}, '/aoo/codec/get', replyID).oneShot;

		this.prSendMsg('/codec_get', replyID);
	}

	sinkChannelOffset { arg addr, id, onset;
		addr = this.prResolveAddr(addr);
		this.prSendMsg('/sink_channel', addr.ip, addr.port, id, onset);
	}

	packetSize { arg size;
		this.prSendMsg('/packet_size', size);
	}

	pingInterval { arg sec;
		this.prSendMsg('/ping', sec);
	}

	resendBufferSize { arg sec;
		this.prSendMsg('/resend', sec);
	}

	redundancy { arg n;
		this.prSendMsg('/redundancy', n);
	}

	dynamicResampling { arg b;
		this.prSendMsg('/dynamic_resampling', b);
	}

	dllBandwidth { arg bw;
		this.prSendMsg('/dll_bw', bw);
	}
}
