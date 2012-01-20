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


import urllib2
import httplib
import ssl
import socket


#
# Psiphon 3 Server API
#

# TODO: add support to server for indicating platform
CLIENT_VERSION = 1

class Psiphon3Server(object):

    def __init__(self, ip_address, web_server_port, web_server_secret,
                 web_server_certificate, propagation_channel_id, sponsor_id):
        self.ip_address = ip_address
        self.web_server_port = web_server_port
        self.web_server_secret = web_server_secret
        self.web_server_certificate = web_server_certificate
        self.propagation_channel_id = propagation_channel_id
        self.sponsor_id = sponsor_id
        # TODO: add proxy support
        handler = CertificateMatchingHTTPSHandler(self.web_server_certificate)
        self.opener = urllib2.build_opener(handler)

    # handshake
    def handshake(self):
        # TODO: known_servers
        # TODO: discovery
        # TODO: page view regexes
        response = self.opener.open(self.common_request_url() % ('handshake',)).read()
        handshake_response = {'Upgrade': '',
                              'SSHPort': '',
                              'SSHUsername': '',
                              'SSHPassword': '',
                              'SSHHostKey': '',
                              'SSHSessionID': '',
                              'SSHObfuscatedPort': '',
                              'SSHObfuscatedKey': '',
                              'PSK': ''}

        for line in response.split('\n'):
            key, value = line.split(': ')
            if key in handshake_response.keys():
                handshake_response[key] = value

        return handshake_response

    # TODO: download

    # TODO: connected

    # TODO: failed

    # TODO: status

    def common_request_url(self):
        return 'https://%s:%s/%%s?server_secret=%s&propagation_channel_id=%s&sponsor_id=%s&client_version=%s' % (
            self.ip_address, self.web_server_port, self.web_server_secret,
            self.propagation_channel_id, self.sponsor_id, CLIENT_VERSION)


#
# CertificateMatchingHTTPSHandler
#
# Adapted from CertValidatingHTTPSConnection and VerifiedHTTPSHandler
# http://stackoverflow.com/questions/1087227/validate-ssl-certificates-with-python
#

class InvalidCertificateException(httplib.HTTPException, urllib2.URLError):

    def __init__(self, host, cert, reason):
        httplib.HTTPException.__init__(self)
        self.host = host
        self.cert = cert
        self.reason = reason

    def __str__(self):
        return ('Host %s returned an invalid certificate (%s) %s\n' %
                (self.host, self.reason, self.cert))


class CertificateMatchingHTTPSConnection(httplib.HTTPConnection):

    def __init__(self, host, expected_server_certificate, **kwargs):
        httplib.HTTPConnection.__init__(self, host, **kwargs)
        self.expected_server_certificate = expected_server_certificate

    def connect(self):
        sock = socket.create_connection((self.host, self.port))
        self.sock = ssl.wrap_socket(sock)
        cert = ssl.DER_cert_to_PEM_cert(self.sock.getpeercert(True))
        # Remove newlines and -----BEGIN CERTIFICATE----- and -----END CERTIFICATE-----
        cert = ''.join(cert.splitlines())[len('-----BEGIN CERTIFICATE-----'):-len('-----END CERTIFICATE-----')]
        if cert != self.expected_server_certificate:
            raise InvalidCertificateException(self.host, cert,
                                              'server presented the wrong certificate')


class CertificateMatchingHTTPSHandler(urllib2.HTTPSHandler):

    def __init__(self, expected_server_certificate):
        urllib2.AbstractHTTPHandler.__init__(self)
        self.expected_server_certificate = expected_server_certificate

    def https_open(self, req):
        def http_class_wrapper(host, **kwargs):
            return CertificateMatchingHTTPSConnection(
                    host, self.expected_server_certificate, **kwargs)

        try:
            return self.do_open(http_class_wrapper, req)
        except urllib2.URLError, e:
            if type(e.reason) == ssl.SSLError and e.reason.args[0] == 1:
                raise InvalidCertificateException(req.host, '',
                                                  e.reason.args[1])
            raise

    https_request = urllib2.HTTPSHandler.do_request_ 


