import serial
import time
import threading
from datetime import datetime
import numpy as np
import colorama
import msvcrt
import re
import sys
import random


class UARTSender:
    finish = False

    MODE_RAND = 0
    MODE_ORD = 1
    MODE_LINE = 2

    __input_active = True
    __send_active = True
    __send_stop = True
    __mode = MODE_RAND
    __ascii_bytes = None
    __ascii_pos = 0
    __ninth_bit = False
    __delay = 1
    __num_bytes = 8

    LOG_RED = "\33[31m"
    LOG_GREY = "\33[37m"
    LOG_YELLOW = "\33[33m"
    LOG_GREEN = "\33[32m"
    LOG_MAGENTA = "\33[35m"
    LOG_BLUE = "\33[34m"

    __is_debug_log_enabled = True

    def close(self):
        self.__input_active = False
        while not self.__input_active:
            self.__log_msg('waiting \'__input_active\' task')
            time.sleep(0.2)
        self.__log_msg('\'__input_active\' task finished')

        self.__send_stop = True
        self.__send_active = False
        while not self.__send_active:
            self.__log_msg('waiting \'__send_active\' task')
            time.sleep(0.2)
        self.__log_msg('\'__send_active\' task finished')

        if self.__port is not None:
            self.__port.close()

    def __init__(self, port_name):
        self.__console_buffer = []
        self.__port_name = port_name
        self.__send_mutex = threading.Lock()
        self.__port = None
        self.__mode = False
        self.__ascii_bytes = [i for i in range(10)] + [i for i in range(ord('0'), ord(':'))] + \
                            [i for i in range(ord('a'), ord('{'))] + [i for i in range(ord('A'), ord('['))]
        self.__ascii_pos = 0
        self.__line_pos = 0

        random.seed()

        send_task = threading.Thread(target=self.__send, args=())
        send_task.start()

        input_task = threading.Thread(target=self.__input, args=())
        input_task.start()

    def __uart_init(self, baudrate, wordlen, parity, stop_bits):
        if parity == 'N':
            parity = serial.PARITY_NONE
        elif parity == 'E':
            parity = serial.PARITY_EVEN
        elif parity == 'O':
            parity = serial.PARITY_ODD
        else:
            return False

        if wordlen == 7:
            wordlen = serial.SEVENBITS
        elif wordlen == 8:
            wordlen = serial.EIGHTBITS
        elif wordlen == 9 and parity == serial.PARITY_NONE:
            wordlen = serial.EIGHTBITS
            parity = serial.PARITY_MARK if self.__ninth_bit else serial.PARITY_SPACE
        else:
            return False

        if stop_bits == 1:
            stop_bits = serial.STOPBITS_ONE
        elif stop_bits == 2:
            stop_bits = serial.STOPBITS_TWO
        else:
            return False

        self.__send_mutex.acquire()

        try:
            if self.__port is not None:
                self.__port.close()

            self.__port = serial.Serial(self.__port_name, baudrate, parity=parity, bytesize=wordlen, stopbits=stop_bits)
        except:
            self.__send_mutex.release()
            self.__log_err('Failed to open port ' + self.__port_name + '!')
            return False

        self.__send_mutex.release()

        return True

    def __input(self):
        while self.__input_active:
            if msvcrt.kbhit():
                self.__console_buffer.append(msvcrt.getwche())
                if self.__console_buffer[-1] == '\r' or self.__console_buffer[-1] == '\n':
                    print('\n', end='')
                    s = ''.join(self.__console_buffer)
                    self.__console_buffer.clear()
                    self.__cmd_routine(s)
            else:
                time.sleep(0.1)

        self.__input_active = True

    def __single_send(self, byte=None):
        self.__send_mutex.acquire()

        num = self.__num_bytes

        if self.__port is not None and self.__port.is_open:
            if byte is None:
                if self.__mode == self.MODE_RAND:
                    _bytes = random.randbytes(num)
                elif self.__mode == self.MODE_ORD:
                    _bytes = [self.__ascii_bytes[(i + self.__ascii_pos) % len(self.__ascii_bytes)] for i in range(num)]
                    self.__ascii_pos += num
                    self.__ascii_pos %= len(self.__ascii_bytes)
                elif self.__mode == self.MODE_LINE:
                    _bytes = [(i + self.__line_pos) % 256 for i in range(num)]
                    self.__line_pos += num
                    self.__line_pos %= 256
            else:
                _bytes = (byte,)

            self.__port.write(_bytes)

        self.__send_mutex.release()

    def __send(self):
        while self.__send_active:
            if not self.__send_stop:
                self.__single_send()

            time.sleep(self.__delay)

        self.__send_active = True

    def __cmd_routine(self, data):
        status = True

        result_uart = re.search(r'uart\s*(\d+)\s*([7-9])(N|E|O)(1|2)', data)
        result_hex = re.search(r'hex\s*([0-9a-fA-F]{2})', data)
        result_ninth = re.search(r'ninth\s*(0|1)', data)
        result_delay = re.search(r'delay\s*(\d+)', data)
        result_num = re.search(r'num\s*(\d+)', data)
        result_mode = re.search(r'mode\s*(ord|rand|line)', data)

        if result_uart is not None:
            data = result_uart.groups()

            baudrate = int(data[0])
            wordlen = int(data[1])
            parity = data[2]
            stop_bits = int(data[3])
            status = self.__uart_init(baudrate, wordlen, parity, stop_bits)

        elif result_hex is not None:
            data = result_hex.groups()
            value = int(data[0], 16)
            self.__single_send(value)

        elif result_ninth is not None:
            data = result_ninth.groups()
            self.__ninth_bit = True if int(data[0]) else False

            self.__send_mutex.acquire()

            if self.__port.is_open:
                self.__port.parity = serial.PARITY_MARK if self.__ninth_bit else serial.PARITY_SPACE

            self.__send_mutex.release()

        elif result_delay is not None:
            data = result_delay.groups()
            self.__delay = int(data[0])

        elif result_num is not None:
            data = result_num.groups()
            self.__num_bytes = int(data[0])

        elif result_mode is not None:
            data = result_mode.groups()

            if data[0] == 'rand':
                self.__mode = self.MODE_RAND
            elif data[0] == 'ord':
                self.__mode = self.MODE_ORD
            elif data[0] == 'line':
                self.__mode = self.MODE_LINE

        elif data.find("send") != -1:
            self.__single_send()
        elif data.find("start") != -1:
            self.__send_stop = False
        elif data.find("stop") != -1:
            self.__send_stop = True
        elif data.find("finish") != -1:
            self.finish = True
        else:
            status = False

        if status:
            self.__log_info("status OK")
        else:
            self.__log_err("status FAIL")

    def __logging(self, data, type):
        color = None
        if type == 'msg':
            color = self.LOG_GREY
        elif type == 'info':
            color = self.LOG_GREEN
        elif type == 'warn':
            color = self.LOG_YELLOW
        elif type == 'err':
            color = self.LOG_RED
        elif type == 'debug' and self.__is_debug_log_enabled:
            color = self.LOG_MAGENTA

        if color is not None:
            print(color, data, self.LOG_GREY)

    def __log_msg(self, data):
        self.__logging(data, 'msg')

    def __log_info(self, data):
        self.__logging(data, 'info')

    def __log_warn(self, data):
        self.__logging(data, 'warn')

    def __log_err(self, data):
        self.__logging(data, 'err')

    def __log_debug(self, data):
        self.__logging(data, 'debug')

if __name__ == "__main__":
    colorama.init()

    try:
        com_port = sys.argv[sys.argv.index('-p') + 1]
    except:
        print(UARTSender.LOG_RED, "COM port is not specified!", UARTSender.LOG_GREY)
        com_port = None

    if com_port is not None:
        print("START PROGRAM")

        uart_sender = UARTSender(com_port)

        while True:
            if uart_sender.finish:
                print("FINISH PROGRAM")
                uart_sender.close()
                break

            time.sleep(1)
