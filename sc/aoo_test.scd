(
LanguageConfig.addIncludePath("C:/Repos/aoo/sc");
)

(
s.options.ugenPluginsPath = [
	Platform.resourceDir +/+ "plugins",
	"C:/Repos/aoo/install/mingw64/sc/aoo",
];
s.options.device = "ASIO : UMC ASIO Driver";
s.options.hardwareBufferSize = 256;
s.options.sampleRate = 48000;
s.boot;
)

OSCFunc.trace(true, true);

(
SynthDef(\aoo_send, {
	AooSend.ar(9998, 1, SinOsc.ar([440, 220]) * 0.1);
}).add;

SynthDef(\aoo_recv, { |out|
	Out.ar(out, AooReceive.ar(9999, 1));
}).add;
)

~s = AooSendCtl(Synth(\aoo_send));
~s.add(AooAddr.resolve("localhost", 9999), 1);
~s.add(AooPeer("test", "bar"), 1);
~s.removeAll;
~s.format(AooFormatPCM(2, 256, 48000, 2));
~s.start;
~s.stop;
~s.start(AooData(\osc, [ "/foo", 1, 2, 3 ]));
~s.start(AooData(\text, "hello world!"));
~s.start(AooData(\midi, [ 132, 100 60]));
~s.start(AooData(\float32, [ 1.0, 1.5, 2.0 ]));
~s.start(AooData(\float64, [ 1.0, 1.5, 2.0 ]));
~s.start(AooData(\int16, [ 1.0, 1.5, 2.0 ]));
~s.start(AooData(\int32, [ 1.0, 1.5, 2.0 ]));
~s.start(AooData(\float32, [ 1.0, 1.5, 2.0 ]));
~s.stop;
~s.sinks;
~s.addListener(\ping, { |...args| "AooSend: ping: %".format(args).warn });
~s.removeListener(\ping);

~r = AooReceiveCtl(Synth(\aoo_recv));
~r.addListener(\ping, { |...args| "AooReceive: ping: %".format(args).warn });
~r.sources[0]

(
fork {
	~server = AooServer();

	~c = AooClient(9998);
	~c.addListener(\msg, { |msg, time, peer| "C msg: % % %".format(msg, time, peer).warn });
	~c.addListener(\event, { |type, args| "C event: % %".format(type, args).warn });

	~d = AooClient(9999);
	~d.addListener(\msg, { |msg, time, peer| "D msg: % % %".format(msg, time, peer).warn });
	~d.addListener(\event, { |type, args| "D event: % %".format(type, args).warn });

	s.sync;

	~c.connect("localhost", action: {
		~c.joinGroup("test", nil, "foo", nil)
	});

	~d.connect("localhost", action: {
		~d.joinGroup("test", nil, "bar", nil)
	});
}
)

~c.removeListener(\event);
~d.removeListener(\event);

(
~server = AooServer();
)

(
~c = AooClient(9998);
~c.addListener(\ping, { |event| "C ping: %".format(event).warn });
~c.addListener(\peerJoin, { |event| "C peer join: %".format(event).warn });
~c.addListener(\peerLeave, { |event| "C peer leave: %".format(event).warn });
~c.addListener(\msg, { |msg, time, peer| "C msg: % % %".format(msg, time, peer).warn });
)
~c.removeListener(\ping);
~c.removeListener(\msg);

~c.connect("localhost", action: { |success| "success: %".format(success).warn });
~c.joinGroup("test", nil, "foo", nil, action: { |success| "success: %".format(success).warn; });
~c.leaveGroup;
~c.disconnect;
~c.peers;
~c.groups;
~c.sendMsg(nil, nil, [ "/foo", 1, 2, 3]);
~c.sendMsg(nil, 10, [ "/foo", 1, 2, 3]);
~c.sendMsg(nil, 10, [ "/foo", 4, 5, 6]);

~s.prResolveAddr(AooPeer("test", "bar"))
~c.prFindPeer(AooPeer("test", "bar"))

(
~d = AooClient(9999);
~d.addListener(\ping, { |event| "D ping: %".format(event).warn });
~d.addListener(\peerJoin, { |event| "D peer join: %".format(event).warn });
~d.addListener(\peerLeave, { |event| "D peer leave: %".format(event).warn });
~d.addListener(\msg, { |msg, time, peer| "D msg: % % %".format(msg, time, peer).warn });
)
~d.removeListener(\ping);
~d.removeListener(\msg);

~d.connect("localhost", action: { |success| "success: %".format(success).warn });
~d.joinGroup("test", nil, "bar", nil, action: { |success| "success: %".format(success).warn; });
~d.leaveGroup;
~d.disconnect;
~d.peers;
~d.groups;
~d.sendMsg(nil, nil, [ "/foo", 1, 2, 3]);
~d.sendMsg(nil, 10, [ "/foo", 1, 2, 3]);
~d.sendMsg(AooPeer("test", "foo"), nil, [ "/foo", 1, 2, 3]);
~d.sendMsg(AooGroup("test"), nil, [ "/foo", 1, 2, 3]);

~c.sendMsg(nil, 10, [ "/foo", 1, 2, 3]);
~c.sendMsg(nil, 10, [ "/foo", 4, 5, 6]);

(
f = OSCFunc({ |msg|
	"received message %".format(msg).postln;
}, '/foo', nil, nil, nil, ~d.dispatcher);
)
f = OSCFunc({ |msg|
	"received message %".format(msg).postln;
}, '/foo', AooPeer("test", "foo"), nil, nil, ~d.dispatcher);
)
(
f = OSCFunc({ |msg|
	"received message %".format(msg).postln;
}, '/foo', AooPeer("test", "foo"), nil, [ 1, 2 ], ~d.dispatcher);
)
f.free;

(
fork {
	SynthDef(\aoo_send, {
		AooSend.ar(9998, 1, SinOsc.ar(220) * 0.1, tag: \a);
		AooSend.ar(9998, 2, SinOsc.ar(440) * 0.1, tag: \b);
	}).add;

	s.sync;

	e = AooSendCtl.collect(Synth(\aoo_send));

	s.sync; // make sure that the AooSend UGesn have been fully initialized!

	e[\a].add(AooAddr("127.0.0.1", 9999), 1);
	e[\b].add(AooAddr("127.0.0.1", 9997), 1);
}
)


AooAddr("localhost", 9999)
AooAddr("::1", 9999)
AooAddr("192.168.0.1", 9999)

AooPeer("test", "foo", "127.0.0.1", 9999) == AooAddr("127.0.0.1", 9999);
AooAddr("127.0.0.1", 9999) == AooPeer("test", "foo", "127.0.0.1", 9999);


AooAddr.resolve("localhost", 9999)

"foo".collectAs(_.asInteger, Int8Array);

[ 5, [ \foo, 1, 2, 3] ].asRawOSC;


Main.elapsedTime;
Main.monotonicClockTime;

(
var offset = 2208988800.0;
var a = Date.getDate.rawSeconds.postln;
var b = (Date.getDate.rawSeconds + offset).postln;
var diff = (b - a).postln;
(diff - offset).postln;
)

(
fork {
	var a, b;
	a = Main.elapsedTime;
	1.wait;
	b = Main.elapsedTime;
	((b - a - 1.0) * 1000.0).postln;
}
)

(
var t = 2208988800.57.postln;
var hi = t.high32Bits;
var lo = t.low32Bits;
Float.from64Bits(hi, lo).postln;
)

[5.9].asRawOSC;

Float

(
fork {
	var a, b;
	a = thisThread.seconds;
	1.wait;
	b = thisThread.seconds;
	((b - a - 1.0) * 1000.0).postln;
}
)

(0..6)[1..5]
(0..6)[1..2]
(0..6)[8..]

switch ("foo") { \foo } { "got foo!".postln; }

nil.asString;
nil.asInteger;

Int8Array[56, 80, 81].collectAs(_.asAscii, String)