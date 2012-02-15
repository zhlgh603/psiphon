#!/usr/bin/python
#
# Copyright (c) 2012, Psiphon Inc.
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import string
import os
import sys
import pam
import GeoIP
import syslog
import traceback
import redis
import psi_config
import psi_geoip


syslog.openlog(psi_config.SYSLOG_IDENT, syslog.LOG_NDELAY, psi_config.SYSLOG_FACILITY)

try:
    user = os.environ['PAM_USER']

    # Only apply this logic to the 'psiphon' user accounts;
    # system accounts still use normal authentication stack.

    if not user.startswith('psiphon'):
        sys.exit(1)

    # Client sends a session ID prepended to the SSH password.
    # Extract the sesssion ID, and then perform standard PAM
    # authentication with the username and remaining password.

    authtok = sys.stdin.readline()
    session_id = authtok[0:SESSION_ID_LENGTH]
    password = authtok[SESSION_ID_LENGTH:]

    if (len(session_id) != SESSION_ID_LENGTH or
        0 != len(filter(lambda x : x not in SESSION_ID_CHARACTERS, session_id))):
        sys.exit(1)

    try:
        pam.authenticate(user, password)
    except pam.PamException as e:
        sys.exit(1)

    # Determine the user's region by client IP address and store
    # it in a lookup database keyed by session ID.

    rhost = os.environ['PAM_RHOST']
    region = psi_geoip.get_region(rhost)

    r = redis.StrictRedis(
            host=SESSION_DB_HOST, port=SESSION_DB_PORT, db=SESSION_DB_INDEX)

    r.setex(SESSION_EXPIRE_SECONDS, session_id, region)

except Exception as e:
    for line in traceback.format_exc().split('\n'):
        syslog.syslog(syslog.LOG_ERR, line)
    sys.exit(1)

sys.exit(0)
