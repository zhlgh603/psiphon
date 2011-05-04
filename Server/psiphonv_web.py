#!/usr/bin/python
#
# Copyright (c) 2011, Psiphon Inc.
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

'''

Example input:
https://192.168.0.1:80/handshake?server_secret=1234567890&client_id=0987654321&client_version=1

'''

import logging
import threading
import time
from cherrypy import wsgiserver, HTTPError
from cherrypy.wsgiserver import ssl_builtin
from webob import Request
import psiphonv_list
import psiphonv_psk


SERVER_CERTIFICATE_FILE = '/root/PsiphonV/serverCert.pem'
SERVER_PRIVATE_KEY_FILE = '/root/PsiphonV/serverKey.pem'
CA_CERTIFICATE_FILE = '/root/PsiphonV/caCert.pem'

# TODO: read from spreadsheet, reference by local bind address
SERVER_SECRET = 'FEDCBA9876543210'

# TODO: use netifaces to enumerate local ip addresses, start a server for each


class ServerInstance:

    def __init__(self, ip_address):
        self.server_ip_address = ip_address

    def handshake(self, environ, start_response):
        valid_request = True
        request = Request(environ)
        try:
            client_ip_address = request.remote_addr
            server_secret = request.params['server_secret']
            client_id = request.params['client_id']
            client_version = request.params['client_version']
            if server_secret != SERVER_SECRET:
                valid_request = False
        except KeyError as e:
            valid_request = False
        if not valid_request:
            start_response('404 Not Found', [])
            return []
        status = '200 OK'
        #
        # NOTE: Change PSK *last*
        # There's a race condition between setting it and the client connecting:
        # another client may request /handshake and change the PSK before the
        # first client authenticates to the VPN.  We accept the risk and the
        # client is designed to retry.  Still, best to minimize the time
        # between the PSK change on the server side and the submit by the
        # client.  See the design notes for why we aren't using multiple PSKs
        # and why we're using PSKs instead of VPN PKI: basically, lowest
        # common denominator compatibility.
        #
        lines = psiphonv_list.handshake(client_ip_address, client_id, client_version)
        lines += [psiphonv_psk.set_psk(self.server_ip_address)]
        response_headers = [('Content-type','text/plain')]
        start_response(status, response_headers)
        return ['\n'.join(lines)]


def main():
    # TODO: handle multiple local bind addresses
    bind_address = '0.0.0.0'
    server_instance = ServerInstance('192.168.1.250')
    server = wsgiserver.CherryPyWSGIServer(
                (bind_address, 80),
                wsgiserver.WSGIPathInfoDispatcher(
                    {'/handshake': server_instance.handshake}))

    server.ssl_adapter = ssl_builtin.BuiltinSSLAdapter(
                SERVER_CERTIFICATE_FILE,
                SERVER_PRIVATE_KEY_FILE,
                CA_CERTIFICATE_FILE)

    thread = threading.Thread(target=server.start)
    thread.start()
    # TODO: daemon
    print 'Server running...'
    try:
        time.sleep(10000)
    except KeyboardInterrupt as e:
        pass
    server.stop()
    thread.join()


if __name__ == "__main__":
    main()
