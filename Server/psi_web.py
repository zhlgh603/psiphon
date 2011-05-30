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
import threading
import time
import os
import syslog
import ssl
import tempfile
import netifaces
import socket
from cherrypy import wsgiserver, HTTPError
from cherrypy.wsgiserver import ssl_builtin
from webob import Request
import psi_psk
import psi_config
import sys

sys.path.insert(0, os.path.abspath(os.path.join('..', 'Data')))
import psi_db


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


def is_valid_ip_address(str):
    try:
        socket.inet_aton(str)
        return True
    except socket.error:
        return False


# see: http://code.activestate.com/recipes/496784-split-string-into-n-size-pieces/
def split_len(seq, length):
    return [seq[i:i+length] for i in range(0, len(seq), length)]


# ===== Main Code =====

class ServerInstance(object):

    def __init__(self, ip_address, server_secret):
        self.server_ip_address = ip_address
        self.server_secret = server_secret

    class InvalidInputException(Exception):
        pass

    def __validate_and_log(self, environ, request_name, log_vpn_client_ip_address=False):
        valid_request = True
        request = Request(environ)
        try:
            client_ip_address = request.remote_addr
            client_id = request.params['client_id']
            sponsor_id = request.params['sponsor_id']
            client_version = request.params['client_version']
            server_secret = request.params['server_secret']
            if log_vpn_client_ip_address:
                vpn_client_ip_address = request.params['vpn_client_ip_address']
            if (not consists_of(client_id, string.hexdigits) or
                not consists_of(sponsor_id, string.hexdigits) or
                not consists_of(client_version, string.digits) or
                not consists_of(server_secret, string.hexdigits) or
                not (is_valid_ip_address(vpn_client_ip_address) if log_vpn_client_ip_address else True) or
                not constant_time_compare(server_secret, self.server_secret)):
                syslog.syslog(
                    syslog.LOG_ERR,
                    'Invalid input for %s [%s]' % (request_name, str(request.params)))
                raise self.InvalidInputException()
        except KeyError as e:
            syslog.syslog(
                syslog.LOG_ERR,
                'Missing input for %s [%s]' % (request_name, str(request.params)))
            raise self.InvalidInputException()
        if log_vpn_client_ip_address:
            client_location = vpn_client_ip_address
        else:
            client_location = psi_db.get_region(client_ip_address)
        syslog.syslog(
            syslog.LOG_INFO,
            '%s %s %s %s %s %s' % (request_name,
                                   self.server_ip_address,
                                   client_location,
                                   client_id,
                                   sponsor_id,
                                   client_version))
        return (client_ip_address, client_id, sponsor_id, client_version)

    def handshake(self, environ, start_response):
        try:
            (client_ip_address, client_id, sponsor_id, client_version) =\
                self.__validate_and_log(environ, 'handshake')
        except self.InvalidInputException as e:
            start_response('404 Not Found', [])
            return []
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
        status = '200 OK'
        lines = psi_db.handshake(
                    client_ip_address, client_id, sponsor_id, client_version)
        lines += [psi_psk.set_psk(self.server_ip_address)]
        response_headers = [('Content-type', 'text/plain')]
        start_response(status, response_headers)
        return ['\n'.join(lines)]

    def download(self, environ, start_response):
        # TODO: use client_version and maintain multiple versions for download?
        try:
            (client_ip_address, client_id, sponsor_id, client_version) =\
                self.__validate_and_log(environ, 'download')
        except self.InvalidInputException as e:
            start_response('404 Not Found', [])
            return []
        # e.g., /root/PsiphonV/download/<version>/psiphon-<client_id>-<sponsor_id>.exe
        try:
            filename = 'psiphon-%s-%s.exe' % (client_id, sponsor_id))
            path = os.path.join(psi_config.UPGRADE_DOWNLOAD_PATH, filename)
            with open(path, 'rb') as file:
                contents = file.read()
        # TODO: catch all possible file-related exceptions
        except IOError as e:
            start_response('404 Not Found', [])
            return []
        status = '200 OK'
        response_headers = [('Content-type', 'application/exe'),
                            ('Content-Length', '%d' % (len(contents),))]
        start_response(status, response_headers)
        return [contents]

    def connected(self, environ, start_response):
        try:
            # Log client IP address: clients should only make this request
            # when connected to VPN, so this will log the VPN address
            # which is used to link VPN disconnected events for session
            # duration reporting.
            _ = self.__validate_and_log(environ, 'connected',
                                        log_vpn_client_ip_address=True)
        except self.InvalidInputException as e:
            start_response('404 Not Found', [])
            return []
        # No action, this request is just for stats logging
        status = '200 OK'
        start_response(status, [])
        return []


def get_servers():
    # enumerate all interfaces with an IPv4 address and server entry
    # return an array of server info for each server to be run
    servers = []
    for interface in netifaces.interfaces():
        if netifaces.ifaddresses(interface).has_key(netifaces.AF_INET):
            ip_address = netifaces.ifaddresses(interface)[netifaces.AF_INET][0]['addr']
            server = filter(lambda s : s.IP_Address == ip_address, psi_db.get_servers())
            if len(server) == 1:
                servers.append((ip_address,
                                server[0].Web_Server_Port,
                                server[0].Web_Server_Secret,
                                server[0].Web_Server_Certificate,
                                server[0].Web_Server_Private_Key))
    return servers


class WebServerThread(threading.Thread):

    def __init__(self, ip_address, port, secret, certificate, private_key):
        #super(WebServerThread, self).__init__(self)
        threading.Thread.__init__(self)
        self.ip_address = ip_address
        self.port = port
        self.secret = secret
        self.certificate = certificate
        self.private_key = private_key
        self.server = None
        self.certificate_temp_file = None

    def stop_server(self):
        if self.server:
            # blocks until server stops
            self.server.stop()
            self.server = None
        if self.certificate_temp_file:
            # closing the temp file auto deletes it (NOTE: not secure wipe)
            self.certificate_temp_file.close()
            self.certificate_temp_file = None

    def run(self):
        # While loop is for recovery from 'unknown ca' issue.
        while True:
            try:
                server_instance = ServerInstance(self.ip_address, self.secret)
                self.server = wsgiserver.CherryPyWSGIServer(
                                (self.ip_address, int(self.port)),
                                wsgiserver.WSGIPathInfoDispatcher(
                                    {'/handshake': server_instance.handshake,
                                     '/download': server_instance.download,
                                     '/connected': server_instance.connected}))
                # lifetime of cert/private key temp file is lifetime of server
                # file is closed by ServerInstance, and that auto deletes tempfile
                self.certificate_temp_file = tempfile.NamedTemporaryFile()
                self.certificate_temp_file.write(
                    '-----BEGIN RSA PRIVATE KEY-----\n' +
                    '\n'.join(split_len(self.private_key, 64)) +
                    '\n-----END RSA PRIVATE KEY-----\n' +
                    '-----BEGIN CERTIFICATE-----\n' +
                    '\n'.join(split_len(self.certificate, 64)) +
                    '\n-----END CERTIFICATE-----\n');
                self.certificate_temp_file.flush()
                self.server.ssl_adapter = ssl_builtin.BuiltinSSLAdapter(
                                              self.certificate_temp_file.name, None)
                # blocks until server stopped
                syslog.syslog(syslog.LOG_INFO, 'Started server for %s' % (self.ip_address,))
                self.server.start()
                break
            except ssl.SSLError as e:
                # Ignore this SSL raised when a Firefox browser connects
                # TODO: explanation required
                if e.strerror != '_ssl.c:490: error:14094418:SSL routines:SSL3_READ_BYTES:tlsv1 alert unknown ca':
                    syslog.syslog(syslog.LOG_ERR, e.strerror)
                    raise


def main():
    syslog.openlog(psi_config.SYSLOG_IDENT, syslog.LOG_NDELAY, psi_config.SYSLOG_FACILITY)
    threads = []
    # run a web server for each server entry
    # (presently web server-per-entry since each has its own certificate;
    #  we could, in principle, run one web server that presents a different
    #  cert per IP address)
    for server_info in get_servers():
        thread = WebServerThread(*server_info)
        thread.start()
        threads.append(thread)
    # TODO: daemon
    print 'Servers running...'
    try:
        while True:
            time.sleep(60)
    except KeyboardInterrupt as e:
        pass
    print 'Stopping...'
    for thread in threads:
        thread.stop_server()
        thread.join()


if __name__ == "__main__":
    main()
