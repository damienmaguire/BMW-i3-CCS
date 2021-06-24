#!/usr/bin/python3
#
# spi2pcap - tool to convert Saleae Logic 1.2.x SPI decodes of a
# Qualcomm Atheros QCA7000 HomePlug Green PHY into Ethernet frames in a PCAP
# file suitable for opeingin in Wireshark or similar.
#
# Requirements:
#  - Python3
#  - Scapy
#     Fedora: sudo dnf install scapy
#     Ubuntu: sudo apt-get install python3-scapy
#     Windows: https://scapy.readthedocs.io/en/latest/installation.html

# The MIT License (MIT)
# Copyright (c) 2021 David J. Fiddes <D.J@fiddes.net>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
# OR OTHER DEALINGS IN THE SOFTWARE.

from __future__ import annotations
from abc import ABC, abstractmethod
from optparse import OptionParser
from struct import unpack
from scapy.all import wrpcap, Ether
import csv
from os import remove
from os.path import exists

parser = OptionParser()
parser.add_option("-i", "--input", dest="input",
                  help="Saleae Logic 1.x decoded QCA7000 SPI packets in CSV")
parser.add_option("-o", "--output", dest="output",
                  help="Wireshark PCAP file")

(options, args) = parser.parse_args()

if not options.input:
    parser.error('Input filename not given')
    exit()
if not options.output:
    parser.error('Output filename not given')
    exit()


class PacketProcessor:
    """
    The PacketProcessor takes incoming SPI packets and builds them into valid Ethernet frames
    """

    _state = None

    def __init__(self, prefix: str, filename: str) -> None:
        self._packet = bytearray()
        self._expected_length = 0
        self._packet_count = 0
        self._prefix = prefix
        self._filename = filename
        self.transition_to(WaitingState())

    def transition_to(self, state: State) -> None:
        """
        The PacketProcessor allows changing the State object at runtime.
        """
        self._state = state
        self._state.processor = self

    def set_expected_length(self, length: int) -> None:
        self._expected_length = length

    def start_packet(self, spidata: bytes) -> None:
        self._packet = bytearray(spidata)

    def append_packet(self, spidata: bytes) -> None:
        self._packet = self._packet + spidata

    def is_buffer_full(self) -> bool:
        return self._expected_length == len(self._packet)

    def write_packet(self) -> None:
        pkt = Ether(bytes(self._packet))
        print(self._prefix, pkt.summary())
        wrpcap(self._filename, pkt, append=True)
        self._packet_count += 1

    @property
    def packet_count(self) -> int:
        return self._packet_count

    def process(self, spidata):
        self._state.process(spidata)


class State(ABC):
    """
    The base State class declares methods that all concrete states should
    implement and also provides a backreference to the PacketProcessor object,
    associated with the State. This backreference can be used by States to
    transition the PacketProcessor to another State.
    """

    @property
    def processor(self) -> PacketProcessor:
        return self._processor

    @processor.setter
    def processor(self, processor: PacketProcessor) -> None:
        self._processor = processor

    @abstractmethod
    def process(self, spidata) -> None:
        pass


class WaitingState(State):
    def process(self, spidata) -> None:
        if (spidata == b'\xAA\xAA'):
            self.processor.transition_to(HeaderStartState())


class HeaderStartState(State):
    def process(self, spidata) -> None:
        if (spidata == b'\xAA\xAA'):
            self.processor.transition_to(HeaderEndState())
        else:
            print('Bad packet header: ', spidata)
            self.processor.transition_to(WaitingState())


class HeaderEndState(State):
    def process(self, spidata) -> None:
        # Packet length has been byte swapped to little-endian
        self.processor.set_expected_length(unpack('<H', spidata)[0])
        self.processor.transition_to(LengthAState())


class LengthAState(State):
    def process(self, spidata) -> None:
        if (spidata == b'\x00\x00'):
            self.processor.transition_to(LengthBState())
        else:
            print('Bad packet length padding: ', spidata)
            self.processor.transition_to(WaitingState())


class LengthBState(State):
    def process(self, spidata) -> None:
        self.processor.start_packet(spidata)
        self.processor.transition_to(ReceivingFrameState())


class ReceivingFrameState(State):
    def process(self, spidata) -> None:
        if (self.processor.is_buffer_full()):
            self.processor.write_packet()
            self.processor.transition_to(WaitingState())
        else:
            self.processor.append_packet(spidata)


# State not used
# TODO: Figure out how to better check the footer
class FooterState(State):
    def process(self, spidata) -> None:
        if (spidata == b'\x55\x55'):
            self.processor.write_packet()
        else:
            print('Bad packet footer received: ', spidata)
        self.processor.transition_to(WaitingState())


# Open a Saleae Logic 1.2.x exported SPI CSV file of the form:
# Time [s],Packet ID,MOSI,MISO
# 0.024508375000000,0,0xCD00,0x5B5B
#
with open(options.input, newline='') as spifile:
    reader = csv.reader(spifile, delimiter=',')

    # Skip over the header
    reader.__next__()

    # Initialise our pcap file
    if (exists(options.output)):
        remove(options.output)

    receive = PacketProcessor("RX:", options.output)
    transmit = PacketProcessor("TX:", options.output)

    for row in reader:
        packetTime = float(row[0])
        mosi = bytes.fromhex(row[2].removeprefix('0x'))
        miso = bytes.fromhex(row[3].removeprefix('0x'))
        receive.process(miso)
        transmit.process(mosi)

    print('TX packets: ', transmit.packet_count,
          ' RX packets: ', receive.packet_count)
