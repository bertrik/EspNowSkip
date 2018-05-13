#!/usr/bin/env python3

# Forwards messages received from the serial port to MQTT.
#
# Serial messages are expected to be formatted as follows:
# * <topic> <payload>\n
# * any line starting with a '#' is ignored
#
# This program has the following dependencies, which can installed as follows
# * serial:             'apt-get install python3-serial'
# * paho.mqtt.publish:  'apt-get install python3-paho-mqtt'

import logging
import serial
import paho.mqtt.publish as publish
import argparse

class Forwarder(object):
    def __init__(self, serial_port, mqtt_server):
        self._serial_port = serial_port
        self._mqtt_server = mqtt_server

    def run(self):
        logging.info('Opening serial port %s ...', self._serial_port)
        with serial.Serial(self._serial_port, 115200) as ser:
            while True:
                line = ser.readline().decode('ascii').rstrip()
                logging.info('got line "%s"', line)
                if not line.startswith('#'):
                    split = line.split(' ', 1)
                    if len(split) == 2:
                        topic = split[0]
                        payload = split[1]
                        logging.info('Publishing to %s on topic %s: %s', self._mqtt_server, topic, payload)
                        publish.single(topic, payload, 0, False, self._mqtt_server, 1883, 'revspace-skip')

def main():
    logging.basicConfig(level=logging.INFO, format='%(relativeCreated)6d %(threadName)s %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--serial_port", help = "The serial port to read from", default = "/dev/ttyUSB0")
    parser.add_argument("mqtt_server", help = "The URL of the MQTT server", default = "aliensdetected.com")
    args = parser.parse_args()

    forwarder = Forwarder(args.serial_port, args.mqtt_server)
    forwarder.run()

if __name__ == "__main__":
    main()

