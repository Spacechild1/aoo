AooReceive : MultiOutUGen {
	var <>desc;
	var <>tag;
	var <>port;
	var <>id;

	*ar { arg port, id=0, numChannels=1, latency=0.025, tag;
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
		this.inputs = [port, id, latency ? 0, numChannels];
		if (numChannels == 0) {
			^0; // no outputs
		} {
			^this.initOutputs(numChannels, rate);
		}
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

	free {
		super.free;
		sources = nil;
	}

	prParseEvent { arg type, args;
		// \ping, \format, \add, \remove, \decline, \inviteTimeout,
		// \start, \stop, \state, \block/*
		// currently, all events start with AOO endpoint
		var addr = this.prResolveAddr(AooAddr(args[0], args[1]));
		var id = args[2];
		var source = AooEndpoint(addr, id);
		var event = [source];
		var codec, fmt, states;
		// If source doesn't exist, fake an \add event.
		// This happens if the source has been added
		// before we could create the controller.
		if (this.prFindSource(source).isNil and: { type != \add }) {
			this.prAddSource(source);
			this.eventHandler.value(\add, event);
		};
		type.switch(
			\ping, { ^event ++ args[3..] },
			\add, { this.prAddSource(source); ^event },
			\remove, { this.prRemoveSource(source); ^event },
			\decline, { ^event },
			\inviteTimeout, { ^event },
			\format, {
				// make AooFormat object from OSC args
				codec = args[3].asSymbol;
				fmt = codec.switch(
					\pcm, { AooFormatPCM(*args[4..]) },
					\opus, { AooFormatOpus(*args[4..]) },
					{ "%: unknown format '%'".format(this.class.name, codec).error; nil }
				);
				^event ++ fmt;
			},
			\start, { args.postln; ^event ++ AooData.fromBytes(*args[3..]) },
			\stop, { ^event },
			\state, {
				states = #[ \inactive, \active, \buffering ];
				^event ++ states[args[3]];
			},
			\latency, { ^event ++ args[3..] },
			\blockDropped, { ^event ++ args[3] },
			\blockResent, { ^event ++ args[3] },
			\blockXRun, { ^event ++ args[3] },
			\overrun, { ^event },
			\underrun, { ^event },
			{ "%: ignore unknown event '%'".format(this.class, type).warn; ^nil }
		)
	}

	prAddSource { arg source;
		this.sources = this.sources.add(source);
	}

	prRemoveSource { arg source;
		var index = this.sources.indexOfEqual(source);
		index !? { this.sources.removeAt(index) };
	}

	prFindSource { arg source;
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
				this.prAddSource(source);
				action.value(source);
			} { action.value(nil) }
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

	pingInterval { arg seconds;
		this.prSendMsg('/ping', seconds);
	}

	packetSize { arg size;
		this.prSendMsg('/packet_size',size);
	}

	latency { arg seconds;
		this.prSendMsg('/latency', seconds);
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

	resendInterval { arg seconds;
		this.prSendMsg('/resend_interval', nil, seconds);
	}

	bufferSize { arg seconds;
		this.prSendMsg('/buffer_size', seconds);
	}

	dynamicResampling { arg enable;
		this.prSendMsg('/dynamic_resampling', enable);
	}

	dllBandwidth { arg bandwidth;
		this.prSendMsg('/dll_bw', bandwidth);
	}
}
