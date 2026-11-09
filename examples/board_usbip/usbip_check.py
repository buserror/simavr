#!/usr/bin/env python3
"""
Minimal USB/IP client to check simusbip without a kernel: lists and
imports the device, enumerates it, then sends data through the CDC
loopback firmware (at90usb162_cdc_loopback) and reads it back, the way
vhci-hcd and cdc-acm would, including an unlink of a pending IN URB.

	./obj-*/simusbip.elf &
	./usbip_check.py [host] [port]
"""
import socket
import struct
import sys

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 3240
DEVID = (1 << 16) | 1
seq = 0


def recv_exact(s, n):
	b = b""
	while len(b) < n:
		c = s.recv(n - len(b))
		if not c:
			raise SystemExit("FAIL: connection closed")
		b += c
	return b


def op(s, code, payload=b""):
	s.sendall(struct.pack(">HHI", 0x0111, code, 0) + payload)
	ver, rcode, status = struct.unpack(">HHI", recv_exact(s, 8))
	return rcode, status


def parse_dev(b):
	path, busid = b[:256].rstrip(b"\0"), b[256:288].rstrip(b"\0")
	busnum, devnum, speed, vid, pid, bcd = struct.unpack(">IIIHHH", b[288:306])
	cls, sub, proto, cfg, ncfg, nif = b[306:312]
	return dict(path=path.decode(), busid=busid.decode(), speed=speed,
				vid=vid, pid=pid, cls=cls, nif=nif, cfg=cfg)


def submit(s, ep, direction, length=0, data=b"", setup=bytes(8), flags=0):
	global seq
	seq += 1
	hdr = struct.pack(">IIIII", 1, seq, DEVID, direction, ep)
	hdr += struct.pack(">IiiiI", flags, length if direction else len(data),
					   0, 0, 0) + setup
	s.sendall(hdr + data)
	return seq


def ret(s):
	h = recv_exact(s, 48)
	cmd, sq = struct.unpack(">II", h[:8])
	if cmd == 3:
		status, actual = struct.unpack(">ii", h[20:28])
		return cmd, sq, status, h, actual
	if cmd == 4:
		status, = struct.unpack(">i", h[20:24])
		return cmd, sq, status, h, 0
	raise SystemExit("FAIL: unexpected reply %d" % cmd)


def control(s, setup, length=0, data=b"", direction=None):
	if direction is None:
		direction = 1 if setup[0] & 0x80 else 0
	sq = submit(s, 0, direction, length, data, setup)
	cmd, rsq, status, h, actual = ret(s)
	assert cmd == 3 and rsq == sq, (cmd, rsq, sq)
	payload = recv_exact(s, actual) if direction and actual else b""
	return status, payload


# 1. device list
s = socket.create_connection((host, port))
s.sendall(struct.pack(">HHI", 0x0111, 0x8005, 0))
ver, code, status = struct.unpack(">HHI", recv_exact(s, 8))
n, = struct.unpack(">I", recv_exact(s, 4))
assert code == 0x0005 and status == 0 and n == 1, (code, status, n)
d = parse_dev(recv_exact(s, 312))
ifs = [recv_exact(s, 4) for _ in range(d["nif"])]
print("devlist: %s %04x:%04x speed %d, %d interface(s): %s" % (
	d["busid"], d["vid"], d["pid"], d["speed"], d["nif"],
	" ".join("%02x/%02x/%02x" % tuple(i[:3]) for i in ifs)))
s.close()

# 2. import
s = socket.create_connection((host, port))
code, status = op(s, 0x8003, d["busid"].encode().ljust(32, b"\0"))
assert code == 0x0003 and status == 0, (code, status)
d = parse_dev(recv_exact(s, 312))
print("imported %s" % d["busid"])

# 3. enumeration as Linux does it (SET_ADDRESS is handled by vhci-hcd)
st, dd = control(s, struct.pack("<BBHHH", 0x80, 6, 0x0100, 0, 64), 64)
assert st == 0 and len(dd) == 18 and dd[1] == 1, (st, dd)
st, cd = control(s, struct.pack("<BBHHH", 0x80, 6, 0x0200, 0, 9), 9)
total = cd[2] | cd[3] << 8
st, cd = control(s, struct.pack("<BBHHH", 0x80, 6, 0x0200, 0, total), total)
assert st == 0 and len(cd) == total, (st, len(cd), total)
eps = {}
i = 0
while i < len(cd):
	if cd[i + 1] == 5:
		eps[cd[i + 2]] = (cd[i + 3] & 3, cd[i + 4] | cd[i + 5] << 8)
	i += cd[i]
print("device %04x:%04x ep0 %d, endpoints %s" % (
	dd[8] | dd[9] << 8, dd[10] | dd[11] << 8, dd[7],
	" ".join("%02x/%d/%d" % (a, t, m) for a, (t, m) in sorted(eps.items()))))
st, _ = control(s, struct.pack("<BBHHH", 0x00, 9, 1, 0, 0))
assert st == 0, st
# unknown string index: the firmware stalls, -EPIPE
st, _ = control(s, struct.pack("<BBHHH", 0x80, 6, 0x03ee, 0x0409, 255), 255)
assert st == -32, st
# CDC: SET_LINE_CODING with a data stage, then SET_CONTROL_LINE_STATE
st, _ = control(s, struct.pack("<BBHHH", 0x21, 0x20, 0, 0, 7), 0,
				struct.pack("<IBBB", 115200, 0, 0, 8))
assert st == 0, st
st, _ = control(s, struct.pack("<BBHHH", 0x21, 0x22, 3, 0, 0))
assert st == 0, st
print("configured, line coding set")

ep_in = [a for a, (t, m) in eps.items() if a & 0x80 and t == 2][0]
ep_out = [a for a, (t, m) in eps.items() if not a & 0x80 and t == 2][0]
maxp_in = eps[ep_in][1]

# 4. two IN URBs queued like cdc-acm does, then data out
in_seqs = [submit(s, ep_in & 0xf, 1, 512) for _ in range(2)]
msg = bytes(range(32, 127)) * 3	# 285 bytes, several packets
out_seq = submit(s, ep_out & 0xf, 0, data=msg)
got = b""
done_out = False
while len(got) < len(msg) or not done_out:
	cmd, sq, status, h, actual = ret(s)
	assert cmd == 3 and status == 0, (cmd, sq, status)
	if sq == out_seq:
		assert actual == len(msg), actual
		done_out = True
		continue
	assert sq in in_seqs, (sq, in_seqs)
	got += recv_exact(s, actual)
	in_seqs.remove(sq)
	in_seqs.append(submit(s, ep_in & 0xf, 1, 512))
assert got == msg, (len(got), got[:40])
print("loopback: %d bytes out, %d bytes back" % (len(msg), len(got)))

# 5. unlink the pending IN URBs, nothing is coming
for victim in list(in_seqs):
	global_seq = seq + 1
	s.sendall(struct.pack(">IIIII", 2, global_seq, DEVID, 0, 0) +
			  struct.pack(">I", victim) + bytes(24))
	seq = global_seq
	cmd, sq, status, h, _ = ret(s)
	assert cmd == 4 and sq == global_seq and status == -104, (cmd, sq, status)
print("unlinked %d pending IN URB(s)" % len(in_seqs))
s.close()
print("PASS")
