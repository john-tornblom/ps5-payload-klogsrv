#!/usr/bin/env python

import logging
import re
import sys


LEVELS = {
    0: logging.CRITICAL,
    1: logging.CRITICAL,
    2: logging.CRITICAL,
    3: logging.ERROR,
    4: logging.WARN,
    5: logging.INFO,
    6: logging.INFO,
    7: logging.DEBUG
}

LOGGERS = dict()


class ColorFormatter(logging.Formatter):
    grey = "\x1b[38;20m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_red = "\x1b[31;1m"
    reset = "\x1b[0m"
    format = "[%(asctime)s] %(name)s: %(message)s"

    FORMATS = {
        logging.DEBUG: grey + format + reset,
        logging.INFO: grey + format + reset,
        logging.WARNING: yellow + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_red + format + reset
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


sh = logging.StreamHandler()
sh.setFormatter(ColorFormatter())
sh.setLevel(logging.DEBUG)


def logline(line):
    priority = 118
    match = re.match('^<(\d+)>', line)
    if match:
        priority = match.group(1)
        line = line[len(priority)+2:]
        priority = int(priority)

    facility = priority >> 3
    severity = priority & 7

    match = re.match('\[(.*?)\]', line)
    category = ''

    if match:
        category = match.group(1)
        line = line[len(category)+2:]

    if line.startswith('[ERROR]'):
        severity = 3
        line = line[7:]

    if not category in LOGGERS:
        logger = logging.getLogger(category)
        logger.addHandler(sh)
        logger.setLevel(logging.DEBUG)
        LOGGERS[category] = logger

    LOGGERS[category].log(LEVELS[severity], line.strip())


for line in sys.stdin:
    if line.strip():
        logline(line)


