#!/usr/bin/env python3

# Forwards messages received from the serial port to MQTT.
#
# Serial messages are formatted as follows:
# * <topic> <payload>\n
# * any line starting with a '#' is ignored
#
# This program has the following dependencies, which can installed as follows
# * serial:             'apt-get install python3-serial'
# * paho.mqtt.publish:  'apt-get install python3-paho-mqtt'

import logging
import serial
import paho.mqtt.publish as publish

def main(serial_port, server):
    logging.basicConfig(level=logging.INFO, format='%(relativeCreated)6d %(threadName)s %(message)s')
    logging.info('Opening serial port %s ...', serial_port)
    with serial.Serial(serial_port, 115200) as ser:
        while True:
            line = ser.readline().decode('ascii').rstrip()
            logging.info('got line "%s"', line)
            if not line.startswith('#'):
                split = line.split(' ', 1)
                if len(split) == 2:
                    topic = split[0]
                    payload = split[1]
                    logging.info('Publishing to %s on topic %s: %s', server, topic, payload)
                    publish.single(topic, payload, 0, False, server, 1883, 'revspace-skip')

if __name__ == "__main__":
    main('/dev/ttyUSB0', 'aliensdetected.com')

