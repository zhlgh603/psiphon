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

import os
import GeoIP


def get_unknown():
    return {'region': 'None', 'city': 'None', 'isp': 'None'}

def get_region_only(region):
    return {'region': region, 'city': 'None', 'isp': 'None'}

def get_geoip(network_address):
    try:
        geoip = get_unknown()

        # Use the commercial "city" and "isp" databases if available
        city_db_filename = '/usr/local/share/GeoIP/GeoIPCity.dat'
        isp_db_filename = '/usr/local/share/GeoIP/GeoIPISP.dat'

        if os.path.isfile(city_db_filename):
            record = GeoIP.open(city_db_filename, GeoIP.GEOIP_MEMORY_CACHE).record_by_name(network_address)
            if record:
                geoip['region'] = record['country_code']
                geoip['city'] = record['city']
                # Convert City name from ISO-8859-1 to UTF-8 encoding
                if geoip['city']:
                    try:
                        geoip['city'] = geoip['city'].decode('iso-8859-1').encode('utf-8')
                    except UnicodeDecodeError:
                        pass
        else:
            geoip['region'] = GeoIP.new(GeoIP.GEOIP_MEMORY_CACHE).country_code_by_name(network_address)

        if os.path.isfile(isp_db_filename):
            geoip['isp'] = GeoIP.open(isp_db_filename,GeoIP.GEOIP_MEMORY_CACHE).org_by_name(network_address)

        return geoip

    except NameError:
        # Handle the case where the GeoIP module isn't installed
        return get_unknown()
