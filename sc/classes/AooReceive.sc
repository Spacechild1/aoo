AooReceive : MultiOutUGen {
	var <>desc;
	var <>tag;
	var <>port;
	var <>id;

	*ar { arg port, id=0, numChannels=1, latency, tag;
		^this.multiNewList([\audio, tag, port, id, numChannels, latency]);
	}
	*kr { ^this.shouldNotImplement(thisMethod) }

	init { arg tag, port, id, numChannels, latency;
		if (tag.isKindOf(UGen)) {
			MethodError("'tag' must not be a UGen!", this).throw;
		};
		if (port.isInteger.not) {
			MethodError("'port' must be an Integer!", this).throw;
		};
		this.tag = tag !? { tag.asSymbol }; // !
		this.port = port;
		this.id = id;
		this.inputs = [port, id, latency ?? 0];
		^this.initOutputs(numChannels, rate)
	}

	optimizeGraph {
		Aoo.prMakeMetadata(this);
	}

	synthIndex_ { arg index;
		super.synthIndex_(index); // !
		// update metadata (ignored if reconstructing from disk)
		this.desc.notNil.if { this.desc.index = index; }
	}
}

AooReceiveCtl : AooCtl {
	classvar <>ugenClass;

	var <>sources;

	*initClass {
		Class.initClassTree(AooReceive);
		ugenClass = AooReceive;
	}

	init {
		sources = [];
	}

	prParseEvent { arg type, args;
		// \ping, \format, \add, \remove, \decline, \inviteTimeout,
		// \start, \stop, \state, \block/*
		// currently, all events start with AOO endpoint
		var addr = this.prResolveAddr(AooAddr(args[0], args[1]));
		var id = args[2];
		var source = AooEndpoint(addr, id);
		var event = [source];
		// If source doesn't exist, fake an \add event.
		// This happens if the source has been added
		// before we could create the controller.
		if (this.prFind(source).isNil and: { type != \add }) {
			this.prAdd(source);
			this.eventHandler.value(\add, event);
		};
		type.switch(
			\ping, { ^event ++ args[3..] },
			\add, { this.prAdd(source); ^event },
			\remove, { this.prRemove(source); ^event },
			\decline, { ^event },
			\inviteTimeout, { ^event },
			\format, {
				// make AooFormat object from OSC args
				var codec = args[3].asSymbol;
				var fmt = codec.switch(
					\pcm, { AooFormatPCM(*args[4..]) },
					\opus, { AooFormatOpus(*args[4..]) },
					{ "%: unknown format '%'".format(this.class.name, codec).error; nil }
				);
				^event ++ fmt;
			},
			\start, { args.postln; ^event ++ AooData.fromBytes(*args[3..]) },
			\stop, { ^event },
			\state, {
				var states = #[ \inactive, \active, \buffering ];
				^event ++ states[args[3]];
			},
			\blockDrop, { ^event ++ args[3] },
			\blockResend, { ^event ++ args[3] },
			\blockXRun, { ^event ++ args[3] },
			\overrund, { ^event },
			\underrun, { ^event },
			{ "%s: ignore unknown event '%'".format(this.class, type).warn; ^nil }
		)
	}

	prAdd { arg source;
		this.sources = this.sources.add(source);
	}

	prRemove { arg source;
		var index = this.sources.indexOfEqual(source);
		index !? { this.sources.removeAt(index) };
	}

	prFind { arg source;
		var index = this.sources.indexOfEqual(source);
		^index !? { this.sources[index] };
	}

	invite { arg addr, id, metadata, action;
		var replyID = AooCtl.prNextReplyID;
		addr = this.prResolveAddr(addr);

		this.prMakeOSCFunc({ arg success, ip, port, id;
			var newAddr, source;
			success.if {
				newAddr = this.prResolveAddr(AooAddr(ip, port));
				source = AooEndpoint(newAddr, id);
				this.prAdd(source);
				action.value(source);
			} { action.value }
		}, '/aoo/invite', replyID).oneShot;

		if (metadata.notNil) {
			this.prSendMsg('/invite', replyID, addr.ip, addr.port, id, *metadata.asOSCArgArray);
		} {
			this.prSendMsg('/invite', replyID, addr.ip, addr.port, id, $N, $N);
		}
	}

	uninvite { arg addr, id, action;
		var replyID = AooCtl.prNextReplyID;
		addr = this.prResolveAddr(addr);

		this.prMakeOSCFunc({ arg success;
			action.value(success);
		}, '/aoo/uninvite', replyID).oneShot;

		this.prSendMsg('/uninvite', nil, addr.ip, addr.port, id);
	}

	uninviteAll {
		this.prSendMsg('/uninvite');
	}

	packetSize { arg size;
		this.prSendMsg('/packet_size',size);
	}

	latency { arg sec;
		this.prSendMsg('/latency', sec);
	}

	reset { arg addr, id;
		addr = this.prResolveAddr(addr);
		this.prSendMsg('/reset', addr.ip, addr.port, id);
	}

	resetAll {
		this.prSendMsg('/reset');
	}

	resend { arg enable;
		this.prSendMsg('/resend', nil, enable);
	}

	resendLimit { arg limit;
		this.prSendMsg('/resend_limit', nil, limit);
	}

	resendInterval { arg sec;
		this.prSendMsg('/resend_interval', nil, sec);
	}

	bufferSize { arg sec;
		this.prSendMsg('/buffer_size', sec);
	}

	dynamicResampling { arg b;
		this.prSendMsg('/dynamic_resampling', b);
	}

	dllBandwidth { arg bw;
		this.prSendMsg('/dll_bw', bw);
	}
}
