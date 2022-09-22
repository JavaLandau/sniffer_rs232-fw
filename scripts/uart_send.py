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

    __input_active = True
    __send_active = True
    __send_stop = True
    __ordered_mode = False
    __ascii_bytes = None
    __ascii_pos = 0

    LOG_RED = "\33[31m"
    LOG_GREY = "\33[37m"
    LOG_YELLOW = "\33[33m"
    LOG_GREEN = "\33[32m"
    LOG_MAGENTA = "\33[35m"
    LOG_BLUE = "\33[34m"

    __NUM_BYTES = 8
    __DELAY = 1

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
        self.__ordered_mode = False
        self.__ascii_bytes = [i for i in range(10)] + [i for i in range(ord('0'), ord(':'))] + \
                            [i for i in range(ord('a'), ord('z'))] + [i for i in range(ord('A'), ord('Z'))]
        self.__ascii_pos = 0

        random.seed()

        send_task = threading.Thread(target=self.__send, args=())
        send_task.start()

        input_task = threading.Thread(target=self.__input, args=())
        input_task.start()

    def __uart_init(self, baudrate, wordlen, parity):
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
            parity = serial.PARITY_SPACE
        else:
            return False

        self.__send_mutex.acquire()

        try:
            if self.__port is not None:
                self.__port.close()

            self.__port = serial.Serial(self.__port_name, baudrate, parity=parity, bytesize=wordlen)
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

    def __single_send(self):
        self.__send_mutex.acquire()

        if self.__port is not None and self.__port.is_open:
            if not self.__ordered_mode:
                _bytes = random.randbytes(self.__NUM_BYTES)
            else:
                _bytes = [0] * self.__NUM_BYTES
                for i in range(self.__NUM_BYTES):
                    _bytes[i] = self.__ascii_bytes[self.__ascii_pos]
                    self.__ascii_pos += 1
                    self.__ascii_pos = 0 if (self.__ascii_pos >= len(self.__ascii_bytes)) else self.__ascii_pos

            self.__port.write(_bytes)

        self.__send_mutex.release()

    def __send(self):
        while self.__send_active:
            if not self.__send_stop:
                self.__single_send()

            time.sleep(self.__DELAY)

        self.__send_active = True

    def __cmd_routine(self, data):
        pattern = r'uart\s*(\d+)\s*([7-9])(N|E|O)'
        status = True

        result = re.search(pattern, data)
        if result is not None:
            data = result.groups()

            baudrate = int(data[0])
            wordlen = int(data[1])
            parity = data[2]
            status = self.__uart_init(baudrate, wordlen, parity)

        elif data.find("send") != -1:
            self.__single_send()
        elif data.find("start") != -1:
            self.__send_stop = False
        elif data.find("stop") != -1:
            self.__send_stop = True
        elif data.find("finish") != -1:
            self.finish = True
        elif data.find("rand") != -1:
            self.__ordered_mode = False
        elif data.find("ord") != -1:
            self.__ordered_mode = True
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
