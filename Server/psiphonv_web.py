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
https://192.168.0.1:80/handshake?client_id=0987654321&sponsor_id=1234554321&client_version=1&server_secret=1234567890

'''

import string
import logging
import threading
import time
import os
from cherrypy import wsgiserver, HTTPError
from cherrypy.wsgiserver import ssl_builtin
from webob import Request
import ssl
import tempfile
import netifaces
import psiphonv_db
import psiphonv_psk


DOWNLOAD_PATH = '/root/PsiphonV/download'
DOWNLOAD_FILE_NAME = 'psiphonv.exe'


# ===== Helpers =====

# see: http://codahale.com/a-lesson-in-timing-attacks/
def constant_time_compare(a, b):
    if len(a) != len(b):
        return False
    result = 0
    for x, y in zip(a, b):
        result |= ord(x) ^ ord(y)
    return result == 0


def consists_of(str, characters):
    return 0 == len(filter(lambda x : x not in characters, str))


# see: http://code.activestate.com/recipes/496784-split-string-into-n-size-pieces/
def split_len(seq, length):
    return [seq[i:i+length] for i in range(0, len(seq), length)]


# ===== Main Code =====

class ServerInstance:

    def __init__(self, ip_address, server_secret):
        self.server_ip_address = ip_address
        self.server_secret = server_secret

    class InvalidInputException(Exception):
        pass

    def __get_inputs(self, environ):
        valid_request = True
        request = Request(environ)
        try:
            client_ip_address = request.remote_addr
            client_id = request.params['client_id']
            sponsor_id = request.params['sponsor_id']
            client_version = request.params['client_version']
            server_secret = request.params['server_secret']
            if (not consists_of(client_id, string.hexdigits) or
                not consists_of(sponsor_id, string.hexdigits) or
                not consists_of(client_version, string.digits) or
                not consists_of(server_secret, string.hexdigits) or
                not constant_time_compare(server_secret, self.server_secret)):
                raise self.InvalidInputException()
        except KeyError as e:
            raise self.InvalidInputException()
        return (client_ip_address, client_id, sponsor_id, client_version)

    def handshake(self, environ, start_response):
        try:
            (client_ip_address, client_id, sponsor_id, client_version) =\
                self.__get_inputs(environ)
        except self.InvalidInputException as e:
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
        lines = psiphonv_db.handshake(
                    client_ip_address, client_id, sponsor_id, client_version)
        lines += [psiphonv_psk.set_psk(self.server_ip_address)]
        response_headers = [('Content-type', 'text/plain')]
        start_response(status, response_headers)
        return ['\n'.join(lines)]

    def download(self, environ, start_response):
        try:
            # TODO: don't require client_id, sponsor_id if not used
            (client_ip_address, client_id, sponsor_id, client_version) =\
                self.__get_inputs(environ)
        except self.InvalidInputException as e:
            start_response('404 Not Found', [])
            return []
        status = '200 OK'
        # e.g., /root/PsiphonV/download/<version>/psiphonv.exe
        try:
            path = os.path.join(DOWNLOAD_PATH, client_version, DOWNLOAD_FILE_NAME)
            with open(path, 'rb') as file:
                contents = file.read()
        # TODO: catch all possible file-related exceptions
        except IOError as e:
            start_response('404 Not Found', [])
            return []
        response_headers = [('Content-type', 'application/exe'),
                            ('Content-Length', '%d' % (len(contents),))]
        start_response(status, response_headers)
        return [contents]


def get_servers():
    # enumerate all interfaces with an IPv4 address and server entry
    # return an array of server info for each server to be run
    servers = []
    for interface in netifaces.interfaces():
        if netifaces.ifaddresses(interface).has_key(netifaces.AF_INET):
            ip_address = netifaces.ifaddresses(interface)[netifaces.AF_INET][0]['addr']
            server = filter(lambda s : s.IP_Address == ip_address, psiphonv_db.get_servers())
            if len(server) == 1:
                servers.append((ip_address,
                                server[0].Web_Server_Port,
                                server[0].Web_Server_Secret,
                                server[0].Web_Server_Certificate,
                                server[0].Web_Server_Private_Key))
    return servers


def run_server(stop_event, ip_address, port, secret, certificate, private_key):
    # Thread and while loop is for recovery from 'unknown ca' issue.
    while True:
        certificate_temp_file = None
        server = None
        try:
            server_instance = ServerInstance(ip_address, secret)
            server = wsgiserver.CherryPyWSGIServer(
                        (ip_address, int(port)),
                        wsgiserver.WSGIPathInfoDispatcher(
                            {'/handshake': server_instance.handshake,
                             '/download': server_instance.download}))
            # lifetime of cert/private key temp file is lifetime of server
            # file is closed by ServerInstance, and that auto deletes tempfile
            certificate_temp_file = tempfile.NamedTemporaryFile()
            certificate_temp_file.write(
                '-----BEGIN RSA PRIVATE KEY-----\n' +
                '\n'.join(split_len(private_key, 64)) +
                '\n-----END RSA PRIVATE KEY-----\n' +
                '-----BEGIN CERTIFICATE-----\n' +
                '\n'.join(split_len(certificate, 64)) +
                '\n-----END CERTIFICATE-----\n');
            certificate_temp_file.flush()
            server.ssl_adapter = ssl_builtin.BuiltinSSLAdapter(certificate_temp_file.name, None)
            server.start()
            print 'Wait for stop event' # TEMP
            stop_event.wait()
            print 'Got stop event' # TEMP
            break
        except ssl.SSLError as e:
            # Ignore this SSL raised when a Firefox browser connects
            # TODO: explanation required
            if e.strerror != '_ssl.c:490: error:14094418:SSL routines:SSL3_READ_BYTES:tlsv1 alert unknown ca':
                logging.info(e.strerror)
                raise
        print 'Exiting' # TEMP
        if certificate_temp_file:
            certificate_temp_file.close()
        if server:
            server.stop()


def main():
    stop_event = threading.Event()
    threads = []
    # run a web server for each server entry
    for server_info in get_servers():
        thread = threading.Thread(target=run_server, args=(stop_event,)+server_info)
        thread.start()
        threads.append(thread)
    # TODO: daemon
    print 'Servers running...'
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt as e:
        pass
    print 'Stopping...'
    stop_event.set()
    for thread in threads:
        thread.join()


if __name__ == "__main__":
    main()
