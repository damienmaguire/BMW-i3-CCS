#!/usr/bin/python3
#
# spi2pcap - tool to convert Saleae Logic 1.2.x and Saleae Logic 2.3.x SPI
# decodes of a Qualcomm Atheros QCA7000 HomePlug Green PHY into Ethernet
# frames in a PCAP file suitable for opening in Wireshark or similar.
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
import time


class PacketProcessor:
    """
    The PacketProcessor takes incoming SPI packets and builds them into valid Ethernet frames
    """

    _state = None

    def __init__(self, prefix: str, filename: str) -> None:
        self._packet = bytearray()
        self._expected_length = 0
        self._packet_count = 0
        self._packet_time = None
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

    def start_packet(self, packet_time: float, spidata: bytes) -> None:
        self._packet_time = packet_time
        self._packet = bytearray(spidata)

    def append_packet(self, spidata: bytes) -> None:
        self._packet = self._packet + spidata

    def is_buffer_full(self) -> bool:
        return len(self._packet) >= self._expected_length

    def write_packet(self) -> None:
        # trim over-long packets
        if len(self._packet) > self._expected_length:
            self._packet = self._packet[:self._expected_length]
        pkt = Ether(bytes(self._packet))
        pkt.time = self._packet_time
        print(self._prefix, pkt.summary())
        wrpcap(self._filename, pkt, append=True)
        self._packet_count += 1
        self._packet_time = None

    @property
    def packet_count(self) -> int:
        return self._packet_count

    def process(self, packet_time: float, spidata: bytes):
        self._state.process(packet_time, spidata)


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
    def process(self, packet_time: float, spidata: bytes) -> None:
        pass


class WaitingState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        if spidata == b'\xAA\xAA':
            self.processor.transition_to(HeaderStartState())


class HeaderStartState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        if spidata == b'\xAA\xAA':
            self.processor.transition_to(HeaderEndState())
        else:
            print('Bad packet header: ', spidata)
            self.processor.transition_to(WaitingState())


class HeaderEndState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        # Packet length has been byte swapped to little-endian
        self.processor.set_expected_length(unpack('<H', spidata)[0])
        self.processor.transition_to(LengthAState())


class LengthAState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        if spidata == b'\x00\x00':
            self.processor.transition_to(LengthBState())
        else:
            print('Bad packet length padding: ', spidata)
            self.processor.transition_to(WaitingState())


class LengthBState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        self.processor.start_packet(packet_time, spidata)
        self.processor.transition_to(ReceivingFrameState())


class ReceivingFrameState(State):
    def process(self, packet_time: float, spidata: bytes) -> None:
        if self.processor.is_buffer_full():
            self.processor.write_packet()
            self.processor.transition_to(WaitingState())
        else:
            self.processor.append_packet(spidata)


# Main program start here
parser = OptionParser()
parser.add_option("-i", "--input", dest="input",
                  help="Saleae Logic 1.x/2.x decoded QCA7000 SPI packets in CSV")
parser.add_option("-o", "--output", dest="output",
                  help="Wireshark PCAP file")

(options, args) = parser.parse_args()

if not options.input:
    parser.error('Input filename not given')
    exit()
if not options.output:
    parser.error('Output filename not given')
    exit()


def ReadSaleaeLogic1XRow(row):
    packet_time = float(row[0])
    mosi = bytes.fromhex(row[2].removeprefix('0x'))
    miso = bytes.fromhex(row[3].removeprefix('0x'))
    return (packet_time, mosi, miso)


def ReadSaleaeLogic2XRow(row):
    # Only process SPI result rows
    if row[0] == 'SPI' and row[1] == 'result':
        packet_time = float(row[2])
        mosi = bytes.fromhex(row[4].removeprefix('0x'))
        miso = bytes.fromhex(row[5].removeprefix('0x'))
        return (packet_time, mosi, miso)
    else:
        return None


# Open the CSV input file we are trying to process
with open(options.input, newline='') as spifile:
    reader = csv.reader(spifile, delimiter=',')

    # Try to identify the format of the file we have been given
    header_row = next(reader)

    csv_row_reader = None
    if header_row == ['Time [s]', 'Packet ID', 'MOSI', 'MISO']:
        print('Logic 1.x SPI decode CSV detected')
        csv_row_reader = ReadSaleaeLogic1XRow
    elif header_row == ['name', 'type', 'start_time', 'duration', 'mosi', 'miso']:
        print('Logic 2.x SPI decode CSV detected')
        csv_row_reader = ReadSaleaeLogic2XRow
    else:
        print('Unrecognised CSV format')
        exit()

    # Initialise our pcap file
    if (exists(options.output)):
        remove(options.output)

    # Use the current time as the base packet time. Relative timestamps come
    # from the capture
    start_time = time.time()

    receive = PacketProcessor("RX:", options.output)
    transmit = PacketProcessor("TX:", options.output)

    for row in reader:
        row_values = csv_row_reader(row)

        if row_values:
            (packet_time, mosi, miso) = row_values
            packet_time += start_time
            receive.process(packet_time, miso)
            transmit.process(packet_time, mosi)

    print('TX packets: ', transmit.packet_count,
          ' RX packets: ', receive.packet_count)
