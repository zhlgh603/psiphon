## Copyright (c) 2016, Psiphon Inc.
## All rights reserved.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
<style>
    table {
        padding: 0;
        border: 1px solid black;
        width: 100%
    }
    
    table tr {
        border: 0;
        border-top: 1px solid #CCC;
        background-color: white;
        margin: 0;
        /*padding: 0; */
    }
    
    table, tbody tr, td{
        border: 1px solid black;
        padding-left: 5px;
        vertical-align: top;
        padding-top: 5px;
    }
</style>

<%
    import datetime
    
    header, host_records = data
    
    added_hosts_count = len([hosts for hosts in host_records for h in hosts['added_hosts']])
    pruned_hosts_count = len([hosts for hosts in host_records for h in hosts['pruned_hosts']])
%>

<h1>${header}</h1>

<h3>Added Hosts: ${added_hosts_count}</h3>
<table>
<tr>
    <th>Host Name</th>
    <th>Provider</th>
    <th>Time Added</th>
</tr>
% for record in host_records:
    % if len(record['added_hosts']) > 0:
        % for host_id in record['added_hosts']:
            <tr>
                <td>${host_id}</td>
                % if host_id.startswith('do-'):
                    <td>digitalocean</td>
                % elif host_id.startswith('li'):
                    <td>linode</td>
                % elif host_id.startswith('vn-'):
                    <td>vps.net</td>
                % elif host_id.startswith('fh-'):
                    <td>fasthosts</td>
                % else:
                    <td>unknown</td>
                % endif
                <td>${datetime.datetime.fromtimestamp(record['timestamp']).strftime("%Y-%m-%d %H:%M:%S")}</td>
            </tr>
        % endfor
    % endif
% endfor
</table>

<h3>Pruned Hosts: ${pruned_hosts_count}</h3>
<table>
<tr>
    <th>Host Name</th>
    <th>Provider</th>
    <th>Time Pruned</th>
</tr>
% for record in host_records:
    % if len(record['pruned_hosts']) > 0:
        % for host_id in record['pruned_hosts']:
            <tr>
                <td>${host_id}</td>
                % if host_id.startswith('do-'):
                    <td>digitalocean</td>
                % elif host_id.startswith('li'):
                    <td>linode</td>
                % elif host_id.startswith('vn-'):
                    <td>vps.net</td>
                % elif host_id.startswith('fh-'):
                    <td>fasthosts</td>
                % else:
                    <td>unknown</td>
                % endif
                <td>${datetime.datetime.fromtimestamp(record['timestamp']).strftime("%Y-%m-%d %H:%M:%S")}</td>
            </tr>
        % endfor
    % endif
% endfor
</table>