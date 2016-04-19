## Copyright (c) 2013, Psiphon Inc.
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

  /* Make numbers easier to visually compare. */
  .numcompare {
    text-align: right;
    font-family: monospace;
  }

  /* Some fields are easier to compare left-aligned. */
  .numcompare-left {
    text-align: left;
    font-family: monospace;
  }

  .better {
    background-color: #EFE;
  }

  .worse {
    background-color: #FEE;
  }

  table {
    padding: 0;
    border-collapse: collapse;
    border-spacing: 0;
    font-size: 1em;
    font: inherit;
    border: 0;
  }

  tbody {
    margin: 0;
    padding: 0;
    border: 0;
    font-size: 0.8em;
  }

  table tr {
    border: 0;
    border-top: 1px solid #CCC;
    background-color: white;
    margin: 0;
    padding: 0;
  }

  table tr.row-even {
  }

  table tr.row-odd {
    background-color: #F8F8F8;
  }

  table tr th, table tr td {
    font-size: 1em;
    border: 1px solid #CCC;
    margin: 0;
    padding: 0.5em 1em;
  }

  table tr th {
   font-weight: bold;
    background-color: #F0F0F0;
  }

  table tr td[align="right"] {
    text-align: right;
  }

  table tr td[align="left"] {
    text-align: left;
  }

  table tr td[align="center"] {
    text-align: center;
  }
</style>

<h1>Psiphon 3 Stats</h1>

<h2> Connections Stats </h2>
## Iterate through the tables
% for key, connections_data in data['connections']['platform'].iteritems():
  % for platform_key, platform_data in connections_data.iteritems():
    <h3>${platform_key}</h3>
    <p>Total connections: ${platform_data['doc_count']}</p>

    <table>

      <thead>
        <tr>
          <th>Region</th>
          <th>Yesterday</th>
          <th>Past Week</th>
          <th>One Week Ago</th>
        </tr>
      </thead>

      <tbody>
        % for row_index, row_data in enumerate(platform_data['region']['buckets']):
          <tr class="row-${'odd' if row_index%2 else 'even'}">
            <%
              # A row is of the form: ('Total', defaultdict(int, {'Past Week': 46400L, 'Yesterday': 0L, '1 week ago': 6406L}))
              row_head = row_data['key']
            %>


            ## First column is the region
            <th>${row_head}</th>

            ## Data
            % for col_index, col_data in enumerate(row_data['time_range']['buckets']):
              <td>${col_data['doc_count']}</td>
            % endfor


          </tr>
        % endfor
      </tbody>

    </table>
  %endfor
% endfor

<h2> Unique Users Stats </h2>
## Iterate through the tables
% for key, connections_data in data['unique_users']['platform'].iteritems():
  % for platform_key, platform_data in connections_data.iteritems():
    <h3>${platform_key}</h3>
    <p>Total connections: ${platform_data['doc_count']}</p>

    <table>

      <thead>
        <tr>
          <th>Region</th>
          <th>Yesterday</th>
          <th>Past Week</th>
          <th>One Week Ago</th>
          <th>Past Two Weeks</th>
        </tr>
      </thead>

      <tbody>
        % for row_index, row_data in enumerate(platform_data['region']['buckets']):
          <tr class="row-${'odd' if row_index%2 else 'even'}">
            <%
              # A row is of the form: ('Total', defaultdict(int, {'Past Week': 46400L, 'Yesterday': 0L, '1 week ago': 6406L}))
              row_head = row_data['key']
            %>


            ## First column is the region
            <th>${row_head}</th>

            ## Data
            % for col_index, col_data in enumerate(row_data['time_range']['buckets']):
              <td>${col_data['unique_daily']['value']}</td>
              <td>${col_data['unique_weekly']['value']}</td>
            % endfor


          </tr>
        % endfor
      </tbody>

    </table>
  %endfor
% endfor
