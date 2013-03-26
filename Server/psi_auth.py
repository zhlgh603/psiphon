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
import json
import socket
import urllib
import urllib2
import psi_config

def handle_auth(pam_user, pam_rhost):

    # Read parameters tunneled through password field

    authtok = sys.stdin.readline().rstrip()[:-1]

    auth_params = None
    try:
        auth_params = json.loads(authtok)
        session_id = auth_params['SessionId']
        password = auth_params['SshPassword']
    except (ValueError, KeyError) as e:

        # Backwards compatibility case
        # Client sends a session ID prepended to the SSH password.
        # Extract the sesssion ID, and then perform standard PAM
        # authentication with the username and remaining password.
        # Older backwards compatibility case: if the password length is
        # not correct, skip the session ID logic.

        # Two hex characters per byte, plus pam_exec adds a null character
        expected_authtok_length = (
            2*(psi_config.SESSION_ID_BYTE_LENGTH +
               psi_config.SSH_PASSWORD_BYTE_LENGTH)
            + 1)
            
        if len(authtok) == expected_authtok_length:
            session_id = authtok[0:psi_config.SESSION_ID_BYTE_LENGTH*2]
            password = authtok[psi_config.SESSION_ID_BYTE_LENGTH*2:]
            if 0 != len(filter(lambda x : x not in psi_config.SESSION_ID_CHARACTERS, session_id)):
                return False
        else:

            # Older backwards compatibility case
            session_id = None
            password = authtok

    # Authenticate user

    try:
        pam.authenticate(pam_user, password)
    except pam.PamException as e:
        return False

    # Call 'auth' plugins

    for (path, plugin) in psi_config.PSI_AUTH_PLUGINS:
        sys.path.insert(0, path)
        module = __import__(plugin)
        if hasattr(module, 'auth') and not module.auth(auth_params):
            return False

    # Store session_id/region mapping for stats

    if session_id:
        set_session_region(pam_rhost, session_id)

    return True


def set_session_region(pam_rhost, session_id):

    # TODO: make this a plugin

    # Determine the user's region by client IP address and store
    # it in a lookup database keyed by session ID.

    # Request GeoIP lookup from web service, which has a cached database
    try:
        request = 'http://127.0.0.1:%d/geoip?ip=%s' % (psi_config.GEOIP_SERVICE_PORT, pam_rhost)
        geoip = json.loads(urllib2.urlopen(request, timeout=1).read())
    except urllib2.URLError:
        # No GeoIP info when the web service doesn't response, but proceed with tunnel...
        # TODO: load this value from psi_geoip.get_unknown(), without incurring overhead
        # of loading psi_geoip
        geoip = {'region': 'None', 'city': 'None', 'isp': 'None'}

    redis_session = redis.StrictRedis(
            host=psi_config.SESSION_DB_HOST,
            port=psi_config.SESSION_DB_PORT,
            db=psi_config.SESSION_DB_INDEX)

    redis_session.set(session_id, json.dumps(geoip))
    redis_session.expire(session_id, psi_config.SESSION_EXPIRE_SECONDS)
    
    # Now fill in the discovery database
    # NOTE: We are storing the last octet of the user's IP address
    # to be used by the discovery algorithm done in handshake when
    # the web request is made through the SSH/SSH+ tunnel
    # This is potentially PII.  We have a short (5 minute) expiry on
    # this data, and it will also be discarded immediately after use
    # TODO: Consider implementing the discovery algorithm here so that
    # this does not need to be stored
    try:
        client_ip_last_octet = str(ord(socket.inet_aton(pam_rhost)[-1]))
        redis_discovery = redis.StrictRedis(
                host=psi_config.DISCOVERY_DB_HOST,
                port=psi_config.DISCOVERY_DB_PORT,
                db=psi_config.DISCOVERY_DB_INDEX)
        redis_discovery.set(session_id, json.dumps({'client_ip_last_octet' : client_ip_last_octet}))
        redis_discovery.expire(session_id, psi_config.DISCOVERY_EXPIRE_SECONDS)
    except socket.error:
        pass


def handle_close_session(pam_user, pam_rhost):

    # Call 'close_session' plugins

    for (path, plugin) in psi_config.PSI_AUTH_PLUGINS:
        sys.path.insert(0, path)
        module = __import__(plugin)
        if hasattr(module, 'close_session') and not module.close_session():
            return False

    return True


def main():
    try:
        pam_user = os.environ['PAM_USER']
        pam_rhost = os.environ['PAM_RHOST']
        pam_type = os.environ['PAM_TYPE']

        # Only apply this logic to the 'psiphon' user accounts;
        # system accounts still use normal authentication stack.

        if not pam_user.startswith('psiphon'):
            sys.exit(1)

        result = False

        if pam_type == 'auth':
            result = handle_auth(pam_user, pam_rhost)
        elif pam_type == 'close_session':
            result = handle_close_session(pam_user, pam_rhost)

        if not result:
            sys.exit(1)

    except Exception as e:
        for line in traceback.format_exc().split('\n'):
            syslog.syslog(syslog.LOG_ERR, line)
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    syslog.openlog(psi_config.SYSLOG_IDENT, syslog.LOG_NDELAY, psi_config.SYSLOG_FACILITY)
    main()

