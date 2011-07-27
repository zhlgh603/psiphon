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
https://192.168.0.1:80/handshake?propagation_channel_id=0987654321&sponsor_id=1234554321&client_version=1&server_secret=1234567890

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
        self.COMMON_INPUTS = [
            ('server_secret', lambda x: constant_time_compare(x, self.server_secret)),
            ('propagation_channel_id', lambda x: consists_of(x, string.hexdigits)),
            ('sponsor_id', lambda x: consists_of(x, string.hexdigits)),
            ('client_version', lambda x: consists_of(x, string.digits))]

    def __get_inputs(self, request, request_name, additional_inputs=None):
        if additional_inputs is None:
            additional_inputs = []

        input_values = []

        # Add server IP address and client region for logging
        input_values.append(('server_ip_address', self.server_ip_address))
        client_ip_address = request.remote_addr
        input_values.append(('client_region', psi_db.get_region(client_ip_address)))

        # Check for each expected input
        for (input_name, validator) in self.COMMON_INPUTS + additional_inputs:
            try:
                value = request.params[input_name]
            except KeyError as e:
                syslog.syslog(
                    syslog.LOG_ERR,
                    'Missing %s in %s [%s]' % (input_name, request_name, str(request.params)))
                return False
            if not validator(value):
                syslog.syslog(
                    syslog.LOG_ERR,
                    'Invalid %s in %s [%s]' % (input_name, request_name, str(request.params)))
                return False
            # Special case: omit server_secret from logging
            if input_name != 'server_secret':
                input_values.append((input_name, value))

        # Caller gets a list of name/value input tuples, used for logging etc.
        return input_values

    def __log_event(self, event_name, log_values):
        syslog.syslog(
            syslog.LOG_INFO,
            ' '.join([event_name] + [value for (_, value) in log_values]))

    def handshake(self, environ, start_response):
        request = Request(environ)
        inputs = self.__get_inputs(request, 'handshake')
        if not inputs:
            start_response('404 Not Found', [])
            return []
        # Client submits a list of known servers which is used to
        # flag "new" discoveries in the stats logging
        # NOTE: not validating that server IP addresses are part of the network
        #       (if we do add this, be careful to not introduce a timing based
        #        attack that could be used to enumerate valid server IPs)
        known_servers = request.str_params.getall('known_server')
        for known_server in known_servers:
            if not is_valid_ip_address(known_server):
                syslog.syslog(
                    syslog.LOG_ERR,
                    'Invalid known server in handshake [%s]' % (str(request.params),))
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
        self.__log_event('handshake', inputs)
        status = '200 OK'
        client_ip_address = request.remote_addr
        inputs_lookup = dict(inputs)
        # logger callback will add log entry for each server IP address discovered
        def discovery_logger(server_ip_address):
            unknown = '0' if server_ip_address in known_servers else '1'
            self.__log_event('discovery',
                             inputs + [('server_ip_address', server_ip_address),
                                       ('unknown', unknown)])
        lines = psi_db.handshake(
                    client_ip_address,
                    inputs_lookup['propagation_channel_id'],
                    inputs_lookup['sponsor_id'],
                    inputs_lookup['client_version'],
                    logger=discovery_logger)
        lines += [psi_psk.set_psk(self.server_ip_address)]
        response_headers = [('Content-type', 'text/plain')]
        start_response(status, response_headers)
        return ['\n'.join(lines)]

    def download(self, environ, start_response):
        # NOTE: currently we ignore client_version and just download whatever
        # version is currently in place for the propagation channel ID and sponsor ID.
        inputs = self.__get_inputs(Request(environ), 'download')
        if not inputs:
            start_response('404 Not Found', [])
            return []
        self.__log_event('download', inputs)
        # e.g., /root/PsiphonV/download/psiphon-<propagation_channel_id>-<sponsor_id>.exe
        inputs_lookup = dict(inputs)
        try:
            filename = 'psiphon-%s-%s.exe' % (
                            inputs_lookup['propagation_channel_id'],
                            inputs_lookup['sponsor_id'])
            path = os.path.join(psi_config.UPGRADE_DOWNLOAD_PATH, filename)
            with open(path, 'rb') as file:
                contents = file.read()
        # NOTE: exceptions other than IOError will kill the server thread, but
        # we expect only IOError in normal circumstances ("normal" being,
        # for example, an invalid ID so no file exists)
        except IOError as e:
            start_response('404 Not Found', [])
            return []
        status = '200 OK'
        response_headers = [('Content-type', 'application/exe'),
                            ('Content-Length', '%d' % (len(contents),))]
        start_response(status, response_headers)
        return [contents]

    def connected(self, environ, start_response):
        request = Request(environ)
        additional_inputs = [('vpn_client_ip_address', lambda x: is_valid_ip_address(x))]
        inputs = self.__get_inputs(request, 'connected', additional_inputs)
        if not inputs:
            start_response('404 Not Found', [])
            return []
        self.__log_event('connected', inputs)
        # No action, this request is just for stats logging
        status = '200 OK'
        start_response(status, [])
        return []

    def failed(self, environ, start_response):
        request = Request(environ)
        additional_inputs = [('error_code', lambda x: consists_of(x, string.digits))]
        inputs = self.__get_inputs(request, 'failed', additional_inputs)
        if not inputs:
            start_response('404 Not Found', [])
            return []
        self.__log_event('failed', inputs)
        # No action, this request is just for stats logging
        status = '200 OK'
        start_response(status, [])
        return []


def get_servers():
    # enumerate all interfaces with an IPv4 address and server entry
    # return an array of server info for each server to be run
    servers = []
    for interface in netifaces.interfaces():
        try:
            if (interface.find('ipsec') == -1 and interface.find('mast') == -1 and
                    netifaces.ifaddresses(interface).has_key(netifaces.AF_INET)):
                ip_address = netifaces.ifaddresses(interface)[netifaces.AF_INET][0]['addr']
                server = filter(lambda s : s.IP_Address == ip_address, psi_db.get_servers())
                if len(server) == 1:
                    servers.append((ip_address,
                                    server[0].Web_Server_Port,
                                    server[0].Web_Server_Secret,
                                    server[0].Web_Server_Certificate,
                                    server[0].Web_Server_Private_Key))
        except ValueError as e:
            if str(e) != 'You must specify a valid interface name.':
                raise
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
                                     '/connected': server_instance.connected,
                                     '/failed': server_instance.failed}))

                # Set maximum request input sizes to avoid processing DoS inputs
                self.server.max_request_header_size = 100000
                self.server.max_request_body_size = 100000

                # Lifetime of cert/private key temp file is lifetime of server
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
                # Blocks until server stopped
                syslog.syslog(syslog.LOG_INFO, 'started %s' % (self.ip_address,))
                self.server.start()
                break
            except (ssl.SSLError, socket.error) as e:
                # Log socket/SSL errors and try again
                syslog.syslog(syslog.LOG_ERR, str(e))
                if self.server:
                    self.server.stop()


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
