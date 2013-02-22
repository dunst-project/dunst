#!/usr/bin/env python2

import pynotify
import sys

from optparse import OptionParser

def action_callback(arg):
    print "action invoked (%s)" % (arg)

parser = OptionParser()

parser.add_option("-a", "--appname", dest="appname", action="store", default="dunst tester")
parser.add_option("-s", "--summary", dest="summary", action="store", default="summary")
parser.add_option("-b", "--body", dest="body", action="store", default="body")
parser.add_option("-u", "--urgency", dest="urgency", action="store", default="n")
parser.add_option("-A", "--action", dest="action", action="store", default=None)
parser.add_option("-w", "--wait", dest="wait", action="store_true", default=False)
parser.add_option("-t", "--timeout", dest="timeout", action="store", default=-1)
parser.add_option("-H", "--hint", dest="hint", action="store", default=None)
parser.add_option("-r", "--replaces", dest="id", action="store", default=0)
parser.add_option("-c", "--close", dest="close", action="store", default=0)
parser.add_option("-p", "--print_id", dest="print_id", action="store_true", default=False)

(o, args) = parser.parse_args()


pynotify.init(o.appname)

n = pynotify.Notification(o.summary, o.body)

try:
    if o.urgency[0] == "l":
        n.set_urgency(pynotify.URGENCY_LOW)
    if o.urgency[0] == "n":
        n.set_urgency(pynotify.URGENCY_NORMAL)
    if o.urgency[0] == "c":
        n.set_urgency(pynotify.URGENCY_CRITICAL)
except:
    pass


if o.action:
    n.add_action(o.action.split(":")[0], o.action.split(":")[1], action_callback, None)

if o.id:
    n.props.id = int(o.id)

if o.close:
    n.props.id = int(o.id)
    n.close()
    sys.exit(0)

# FIXME hints

n.set_timeout(int(o.timeout))

n.show()

if o.print_id:
    nid = n.props.id
    print nid

# FIXME wait
