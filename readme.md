# Cypress PSoC1 Programmer

This allows to program PSoC1 devices using atmega168 (Arduino Nano) board.

## Setup

Flash arduino nano with:
```bash
platformio run --target upload --environment atmega168
```
Connections:

|Arduino | Cypress Chip |
| --- | --- |
| +5V | +5V |
| GND | GND |
| D8  | SCL |
| D11 | XRES |
| D12 | SDA |

## Programmer
Programmer options:
```
python programmer.py --help
usage: programmer.py [-h] [-i INPUT] [-o OUTPUT] [--offset OFFSET] [--count COUNT] [--read] [--reset] [--init INIT] port {flash,checksum,device,read,erase,reset}

positional arguments:
  port                  Serial port
  {flash,checksum,device,read,erase,reset}
                        Command to run
                        flash - write .hex to device
                        checksum - returns program checksum from device
                        device - returns device name or identification hex
                        read - dumps device program to file
                        erase - deletes all devices program memory
                        reset - restarts device

optional arguments:
  -h, --help            show this help message and exit
  -i INPUT, --input INPUT
                        Input intel hex file for flashing
  -o OUTPUT, --output OUTPUT
                        Output binary for memory dump
  --offset OFFSET       Memory dump read address offset
  --count COUNT         Memory dump read count
  --read                Read back program when flashing to double check
  --reset               Reset device after command is complete
  --init INIT           Reinitialise programming mode on device
```
You can flash using .hex file made by PSoC Designer like so:
```bash
programmer.py {PORT} flash -i {hex file}
```

## TODO
* Chips without XRES has special procedure to start programming mode which is not implemented.
* Only tested on CY8C24423A chip

## Supported devices

 * CY8C27143
 * CY8C27243
 * CY8C27443
 * CY8C27543
 * CY8C27643
 * CY8C24123A
 * CY8C24223A
 * CY8C24423A
 * CY8C23533
 * CY8C23433
 * CY8C23033
 * CY8C21123
 * CY8C21223
 * CY8C21323
 * CY8C21234
 * CY8C21312
 * CY8C21334
 * CY8C21434
 * CY8C21512
 * CY8C21534
 * CY8C21634
 * CY8CTMG110-32LTXI
 * CY8CTMG110-00PVXI
 * CY8CTST110-32LTXI
 * CY8CTST110-00PVXI
 * Probably other PSoC1 chips


Project is based on [https://www.cypress.com/file/42196/download](https://www.cypress.com/file/42196/download)