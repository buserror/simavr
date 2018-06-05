#!/usr/bin/env python3
#
#	Copyright 2017 Torbjorn Tyridal <ttyridal@gmail.com>
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
#
# This is a quickly thrown together emulation of the kernel + usbip utilities
# side of the usbip protocol
#
# It will connect to the default usbip port on localhost, enumerate and
# configure the device, and run though some cdc commands.
#
# It's used as a developing aid for parts/usbip.c

from collections import namedtuple
from contextlib import closing
import socket
import struct
import binascii
import unittest
import time

Common_s = struct.Struct("!HHI")
Common = namedtuple("Common", "version code status")
USBIP_OP_IMPORT = 0x03
USBIP_VER = 0x0106
USBIP_SYSFS_BUS_ID_SIZE = 32
USBIP_SYSFS_PATH_MAX = 256
USBIP_CMD_SUBMIT = 0x0001

class USBNak(Exception):
    pass

class USBIPError(Exception):
    pass

def setuppkt(typ, req, val, idx, ln=None, dta=None):
    if ln is None and dta is not None:
        return struct.pack("BBHHH", typ, req, val, idx, len(dta)) + dta
    else:
        return struct.pack("BBHHH", typ, req, val, idx, ln)

def usbip_submit_out(s, setuppkt=None, ep=0, dta = b""):
    direction = 0
    seqno = 1
    if setuppkt is None:
        setuppkt = b"\0\0\0\0\0\0\0\0"
    elif len(setuppkt) > 8:
        dta = setuppkt[8:]
        setuppkt = setuppkt[:8]

    s.send(struct.pack("!IIIIIIiiii",
        USBIP_CMD_SUBMIT, seqno, 0x10002, direction, ep,
        0x200, len(dta), 0, 0, 0) + setuppkt + dta
    )
    X = struct.Struct("!IIIIIIiiiixxxxxxxx")
    x = X.unpack(s.recv(X.size))
    if x[0] != 3:
        raise USBIPError("Response is not sumbmit return")
    if x[1] != 1:
        raise USBIPError("Sequence number wrong")
##     if x[2] != 0:
##         raise USBIPError("Device id wrong")
##     if x[3] != 0:
##         raise USBIPError("Direction bit is wrong")
##     if x[4] != ep:
##         raise USBIPError("Endpoint# is wrong")
    if x[6] != 0:
        raise USBIPError("Unexpected non-zero buffer length")
    if x[5] != 0:
        raise USBNak()

def usbip_submit_in(s, setuppkt=None, ep=0, maxsz=0):
    direction = 1
    seqno = 1
    if setuppkt is None:
        setuppkt = b"\0\0\0\0\0\0\0\0"
    elif maxsz == 0:
        maxsz = struct.unpack_from("H", setuppkt, 6)[0]
        print("infer maxsz: ",maxsz)
    s.send(struct.pack("!IIIIIIiiii",
        USBIP_CMD_SUBMIT, seqno, 0x10002, direction, ep,
        0x200, maxsz, 0, 0, 0) + setuppkt
    )

    X = struct.Struct("!IIIIIIiiiixxxxxxxx")
    x = X.unpack(s.recv(X.size))
    if x[0] != 3:
        raise USBIPError("Response is not sumbmit return")
    if x[1] != 1:
        raise USBIPError("Sequence number wrong")
##     if x[2] != 0:
##         raise USBIPError("Device id wrong")
##     if x[3] != 0:
##         raise USBIPError("Direction bit is wrong")
##     if x[4] != 0:
##         raise USBIPError("Endpoint# is wrong")
    if x[5] != 0:
        raise USBNak()
    return s.recv(x[6])


class TestStuff(unittest.TestCase):
    def usbip_attach(self, s):
        s.send(struct.pack("!HHxxxx32s", USBIP_VER, USBIP_OP_IMPORT | 0x8000, b"1-1"))
        X = struct.Struct("!HHI")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], USBIP_VER)
        self.assertEqual(x[1], USBIP_OP_IMPORT)
        self.assertEqual(x[2], 0, "status ok import-op")


        X = struct.Struct("!%ds%dsIIIHHHBBBBBB"%(USBIP_SYSFS_PATH_MAX, USBIP_SYSFS_BUS_ID_SIZE))
        x = X.unpack(s.recv(X.size))
        x = (x[0].strip(b'\0'),  x[1].strip(b'\0'), x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10],x[11],x[12],x[13],)
        print("attached {} {}\n {} {} {}\n {:x} {:x} {:x}\n {} {} {}\n {} {} {}".format(*x))


    def get_device_desc(self, s):
        print("get device desc")
        dta = usbip_submit_in(s, setuppkt(0x80, 6, 0x100, 0, 64))
        self.assertEqual(dta, b'\x12\x01\x00\x02\x02\x00\x00\x10\xc0\x16\x7a\x04\x00\x01\x01\x02\x03\x01')


    def get_device_qualifier(self, s):
        print("get device qualifier")

        with self.assertRaises(USBNak):
            usbip_submit_in(s, setuppkt(0x80, 6, 0x600, 0, 10))


    def get_config_descriptor(self, s):
        print("get config1")


        dta = usbip_submit_in(s, setuppkt(0x80, 6, 0x200, 0, 9))
        self.assertEqual(dta, b'\t\x02C\x00\x02\x01\x00\xc02')

        config_size = struct.unpack_from("H", dta, 2)[0]
        self.assertEqual(config_size, 67)

        print("get config2")
        dta = usbip_submit_in(s, setuppkt(0x80, 6, 0x200, 0, config_size))
        data = b'\x09\x02\x43\x00\x02\x01\x00\xc0\x32\x09\x04\x00\x00\x01\x02\x02\x01\x00\x05\x24\x00\x10\x01\x05\x24\x01\x01\x01\x04\x24\x02\x06\x05\x24\x06\x00\x01\x07\x05\x82\x03\x10\x00\x40\x09\x04\x01\x00\x02\x0a\x00\x00\x00\x07\x05\x03\x02\x20\x00\x00\x07\x05\x84\x02 \x00\x00'
        self.assertEqual(dta, data)

    def get_strings(self, s):
        print("get language code")
        self.assertEqual(
                usbip_submit_in(s, setuppkt(0x80, 6, 0x300, 0, 255)),
                b'\x04\x03\t\x04')

        print("get name1")
        self.assertEqual(
                usbip_submit_in(s, setuppkt(0x80, 6, 0x302, 0x409, 255)),
                b'\x16\x03U\x00S\x00B\x00 \x00S\x00e\x00r\x00i\x00a\x00l\x00')

        print("get manuf")
        self.assertEqual(
                usbip_submit_in(s, setuppkt(0x80, 6, 0x301, 0x409, 255)),
                b'\x14\x03Y\x00o\x00u\x00r\x00 \x00N\x00a\x00m\x00e\x00')


        print("get serial")
        self.assertEqual(
                usbip_submit_in(s, setuppkt(0x80, 6, 0x303, 0x409, 255)),
                b'\x0c\x031\x002\x003\x004\x005\x00')


    def set_config(self, s):
        usbip_submit_out(s, setuppkt(0x00, 9, 0x01, 0, 0))

    def set_line_coding(self, s):
        print("send cdc ctrl 1")
        usbip_submit_out(s, setuppkt(0x21, 0x20, 0x00, 0, dta=b'\x80\x25\x00\x00\x00\x00\x08'))

        print("send cdc ctrl 2")
        usbip_submit_out(s, setuppkt(0x21, 0x20, 0x00, 0, dta=b'\x00\xe1\x00\x00\x00\x00\x08'))

        # sleep some

        print("send cdc ctrl 3")
        usbip_submit_out(s, setuppkt(0x21, 0x22, 0x03, 0, 0))


    def poll_int_ep(self, s):
        print("poll_int")

        with self.assertRaises(USBNak):
            usbip_submit_in(s, ep=2, maxsz=64)


    def write_bulk(self, s, dta=b"H", ep=3):
        print("write_bulk")
        max_retry = 2
        for retry in range(max_retry + 1):
            try:
                usbip_submit_out(s, ep=ep, dta=dta)
            except USBNak:
                if retry < max_retry:
                    time.sleep(0.05)
                    continue
                else:
                    raise
            break

    def read_bulk(self, s, ep=4, expectNak=False):
        print("read_bulk")
        max_retry = 12
        for retry in range(max_retry + 1):

            if expectNak:
                with self.assertRaises(USBNak):
                    usbip_submit_in(s, ep=ep, maxsz=64)
            else:
                try:
                    dta = usbip_submit_in(s, ep=ep, maxsz=64)
                    if dta == b'':
                        raise USBNak
                except USBNak:
                    if retry < max_retry:
                        time.sleep(0.05)
                        continue
                    else:
                        self.assertTrue(False, "expected data but none arrived")
                self.assertEqual(dta, b'H')
            break

    def xtest1(self):
        with socket.create_connection(("localhost", 3240)) as s:

            self.usbip_attach(s)

            self.get_device_desc(s)

            self.get_device_qualifier(s)

            self.get_config_descriptor(s)

            self.get_strings(s)

            self.set_config(s)

            time.sleep(0.1)

            self.set_line_coding(s)

            for x in range(10):
                self.poll_int_ep(s)

            for x in range(10):
                self.write_bulk(s)
                self.read_bulk(s)




    def test1(self):
#        for i in range(10):
#            print("="*20,"iteration",i,"="*20)
            self.xtest1()


if __name__ =="__main__":
    unittest.main()
