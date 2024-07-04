import sys
import os
import serial
from array import array

class Program:
    def __init__(self, intelhex):
        self.program = b''
        self.secure = b''
        self.checksum = b''

        with open(intelhex, 'r') as f:
            mode = 0
            for i, l in enumerate(f.readlines()):
                l = l.rstrip()
                if not l.startswith(':') or len(l) < 11:
                    continue
                size = int(l[1:3], 16)
                addr = int(l[3:7], 16)
                rtype = int(l[7:9], 16)
                if len(l) != 11 + size * 2:
                    raise ValueError(
                        f"{intelhex}:{i} invalid number of characters, given {size * 2} found {len(l) - 11}")
                data = bytes.fromhex(l[9:-2])
                csum = (-sum(bytes.fromhex(l[1:-2]))) & 0x0FF
                csum_given = int(l[-2:], 16)
                if csum != csum_given:
                    raise ValueError(f"{intelhex}:{i} checksum failed, given 0x{csum_given.to_bytes(1, 'big').hex()} "
                                     f"calculated 0x{csum.to_bytes(1, 'big').hex()}")
                if rtype == 0:
                    if mode == 0:
                        if addr != len(self.program):
                            raise ValueError(f"{intelhex}:{i} expected address to be continuous")
                        self.program += data
                    elif mode == 16:
                        if size != 64:
                            raise ValueError(f"{intelhex}:{i} expected data size was 64 bytes, found {size}")
                        self.secure = data
                    elif mode == 32:
                        self.checksum = data
                elif rtype == 4:
                    mode = int.from_bytes(data, 'big')
                elif rtype == 1:
                    break


class PSoC1Prog:
    DEVICE_IDS = {
        9: "CY8C27143",
        10: "CY8C27243",
        11: "CY8C27443",
        12: "CY8C27543",
        13: "CY8C27643",
        50: "CY8C24123A",
        51: "CY8C24223A",
        52: "CY8C24423A",
        2225: "CY8C23533",
        2224: "CY8C23433",
        2226: "CY8C23033",
        23: "CY8C21123",
        24: "CY8C21223",
        25: "CY8C21323",
        54: "CY8C21234",
        2103: "CY8C21312",
        55: "CY8C21334",
        56: "CY8C21434",
        2112: "CY8C21512",
        64: "CY8C21534",
        73: "CY8C21634",
        1848: "CY8CTMG110_32LTXI",
        1849: "CY8CTMG110_00PVXI",
        1592: "CY8CTST110_32LTXI",
        1593: "CY8CTST110_00PVXI",
    }

    def __init__(self, serial_port):
        self.ser = serial.Serial(port=serial_port, baudrate=9600, timeout=2, write_timeout=2)
        self.ser.write(b'\n')
        x = self.ser.read_until(b'> ')
        buf1 = os.urandom(64)
        self._write_blk(buf1)
        buf2 = self._read_blk()
        if buf1 != buf2:
            print("Sent: " + buf1.hex(), file=sys.stderr)
            print("Recv: " + buf2.hex(), file=sys.stderr)
            raise ValueError("Programmer self-test failed!")

    def _wait(self):
        x = self.ser.read(4)
        if len(x) == 0:
            raise ValueError("Chip response timeout")
        if x != b'\r\n> ':
            raise ValueError("Programmer resturned unexpected result: " + x.hex())

    def reinitialise(self):
        self.ser.write(b'i')
        self._wait()

    def reset_device(self):
        self.ser.write(b'a')
        self._wait()

    def get_device_id(self):
        self.ser.write(b'D')
        res = self.ser.read(2)
        self._wait()
        return int.from_bytes(res, 'little')

    def get_firmware_id(self):
        self.ser.write(b'F')
        res = self.ser.read(2)
        self._wait()
        return int.from_bytes(res, 'little')

    def erase_memory(self):
        self.ser.write(b'e')
        self._wait()

    def read_checksum(self):
        self.ser.write(b'C')
        res = self.ser.read(2)
        self._wait()
        return res[::-1]

    def read_memory(self, n=64, offset=0x80):
        # read n x 64 memory blocks
        blocks = b''
        for i in range(n):
            self.ser.write(b'r' + array('B', [i, offset]).tobytes())
            self._wait()
            block = self._read_blk()
            blocks += block
            print(f"\rReading 0x{i.to_bytes(1, 'big').hex()} [{(i+1)/64*100:.2f}%]", end='')
        print('')
        return blocks

    def get_device_name(self):
        devid = self.get_device_id()
        return self.DEVICE_IDS.get(devid, devid.to_bytes(2, 'big').hex())

    def _write_blk(self, data):
        if len(data) != 64:
            raise ValueError("Data is not 64 bytes")
        self.ser.write(b't')
        self.ser.write(data)
        self._wait()
        return

    def _read_blk(self):
        self.ser.write(b's')
        blk = self.ser.read(64)
        self._wait()
        return blk

    def write_program(self, data):
        self.erase_memory()
        for i in range(len(data)//64):
            self._write_blk(data[i*64:(i+1)*64])
            self.ser.write(b'w' + array('B', [i]).tobytes())
            self._wait()
            print(f"\rWriting 0x{i.to_bytes(1, 'big').hex()} [{(i+1)/64*100:.2f}%]", end='', file=sys.stderr)
        print('')

    def write_secure(self, data):
        self._write_blk(data)
        self.ser.write(b'x')
        self._wait()

    def close(self):
        self.ser.close()

def log(text):
    print("PSoC1_Prog: " + text, file=sys.stderr)

if __name__ == '__main__':
    import argparse
    import sys

    parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("port", help="Serial port")
    parser.add_argument("cmd", choices=['flash', 'checksum', 'device', 'read', 'erase', 'reset', 'fw'], 
        help="Command to run\n" +
        "flash - write .hex to device\n" +
        "checksum - returns program checksum from device\n" +
        "device - returns device name or identification hex\n" +
        "read - dumps device program to file\n" +
        "erase - deletes all devices program memory\n" +
        "reset - restarts device\n"
        "fw - returns programmer firmware version\n"
    )
    parser.add_argument("-i", "--input", help="Input intel hex file for flashing")
    parser.add_argument("-o", "--output", help="Output binary for memory dump")
    parser.add_argument("--offset", type=int, default=0x80, help="Memory dump read address offset")
    parser.add_argument("--count", type=int, default=64, help="Memory dump read count")
    parser.add_argument("--read", action="store_true", help="Read back program when flashing to double check")
    parser.add_argument("--reset", action="store_true", help="Reset device after command is complete")
    parser.add_argument("--init", type=bool, default=True, help="Reinitialise programming mode on device")

    args = parser.parse_args()

    prog = PSoC1Prog(args.port)
    log("programmer initialised")
    if args.init:
        prog.reinitialise()
    if args.cmd == 'flash':
        if args.input is None:
            log("input is not specified")
            exit(1)
        ifile = Program(args.input)
        log("erasing memory")
        prog.erase_memory()
        log("writing program")
        prog.write_program(ifile.program)
        log("writing secure")
        prog.write_secure(ifile.secure)
        log("checking checksum")
        csum = prog.read_checksum()
        if csum != ifile.checksum:
            log(f"checksum mismatch, device={csum.hex()} file={ifile.checksum.hex()}")
            exit(1)
        if args.read:
            mem_data = prog.read_memory()
            if mem_data != ifile.program:
                log(f"written program does not match one on device")
                exit(1)
        log("success")
    elif args.cmd == 'checksum':
        if args.input is not None:
            log("file checksum: " + Program(args.input).checksum.hex())
        print(prog.read_checksum().hex())
    elif args.cmd == 'device':
        print(prog.get_device_name())
    elif args.cmd == 'read':
        if args.output is None:
            log("output is not specified")
            exit(1)
        with open(args.output, 'wb') as f:
            f.write(prog.read_memory(args.count, args.offset))
        log("success")
    elif args.cmd == 'erase':
        prog.erase_memory()
        log("success")
    elif args.cmd == 'reset':
        prog.reset_device()
        exit(0)
        log("success")
    elif args.cmd == 'fw':
        print(prog.get_firmware_id())
    if args.reset:
        prog.reset_device()
    exit(0)
