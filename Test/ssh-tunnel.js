/*
Creates a SSH tunnel/SOCKS proxy and accompanying chained HTTP proxy.

`options` is the `testConf` object from `web-server-test.js` along with a few
more fields:
  {
    plonk: path to plonk.exe (can be relative)
    polipo: path to polipo.exe (can be relative)
    socks_proxy_port: the local port that the SOCKS proxy should listen on
    http_proxy_port: the local port that the HTTP proxy should listen on
  }

SSHTunnel is an event emitter. It will emit 'connected' when the proxies are ready
and 'exit' when the proxies have stopped. 'exit' has an `expected` argument
indicating whether the stoppage was requested or not.
*/

var spawn = require('child_process').spawn;
var net = require('net');
var _ = require('underscore');
var Q = require('q');


function SSHTunnel(options) {
  this.options = options;
}

require('util').inherits(SSHTunnel, require('events').EventEmitter);


SSHTunnel.prototype.connect = function(options) {
  if (options) this.options = options;

  this._polipoRun();
  this._plonkRun();

  var self = this;
  Q.all([this._polipoPromise, this._plonkPromise])
    .then(function() {
      self.emit('connected');
    }).end();

  this._disconnectExpected = false;
  this._exited = false;
};


SSHTunnel.prototype.disconnect = function() {
  if (this._exited) {
    var self = this;
    _.defer(function() { self.emit('exit', this._disconnectExpected); });
  }
  else {
    // This will trigger also killing polipo
    this._disconnectExpected = true;
    this.plonk.kill();
  }
};


SSHTunnel.prototype._plonkExit = function() {
  if (!this._disconnectExpected) console.log('plonk exited', this.lastError, arguments);

  clearTimeout(this._connectTimeout);

  // Kill the other process
  this.polipo.removeAllListeners('exit');
  this.polipo.kill();

  // And let the caller know
  this.emit('exit', this._disconnectExpected);
  this._exited = true;
};


SSHTunnel.prototype._polipoExit = function() {
  if (!this._disconnectExpected) console.log('polipo exited', this.lastError, arguments);

  clearTimeout(this._connectTimeout);

  // Kill the other process
  this.plonk.removeAllListeners('exit');
  this.plonk.kill();

  // And let the caller know
  this.emit('exit', this._disconnectExpected);
  this._exited = true;
};


SSHTunnel.prototype._plonkRun = function() {
  var self = this, args;

  if (process.platform === 'win32') {
    args = [
      '-ssh', '-C', '-N', '-batch', '-v',
      '-l', this.options.ssh_username,
      '-pw', this.options.ssh_password,
      '-D', this.options.socks_proxy_port,
      this.options.server_ip
    ];

    if (this.options.ossh) {
      args.push(
        '-P', this.options.ssh_obfsport,
        '-z',
        '-Z', this.options.ssh_obfskey);
    }
    else {
      args.push(
        '-P', this.options.ssh_port);
    }

    this.plonk = spawn(this.options.plonk, args);
  }
  else {
    // Linux, probably. We're not really using plonk, but we'll keep using that name.

    if (this.options.ossh) {
      // A customized SSH client is needed for obfuscated support
      throw new Error('OSSH is only supported on Windows');
    }

    args = [
      '-N', '-tt', '-o', 'ConnectTimeout=100', '-o', 'UserKnownHostsFile=/dev/null', '-o', 'StrictHostKeyChecking=no', '-o', 'NumberOfPasswordPrompts=1',
      '-p', this.options.ssh_port,
      '-D', this.options.socks_proxy_port,
      this.options.ssh_username + '@' + this.options.server_ip
    ];

    var pty = require('pty.js');

    this.plonk = pty.spawn('ssh', args);

    this.plonk.on('data', function(data) {
      if (data.toString().indexOf('Password:') >= 0) {
        self.plonk.write(self.options.ssh_password + '\n');
      }
    });
  }

  this.plonk.on('exit', _.bind(this._plonkExit, this));

  this.lastError = null;
  if (this.plonk.hasOwnProperty('stderr')) {
    this.plonk.stderr.on('data', function(data) {
      self.lastError = data.toString();
    });
  }
  else {
    // The pty.js version doesn't have stderr -- it emits 'data' directly
    this.plonk.on('data', function(data) {
      self.lastError = data.toString();
    });
  }

  // Resolve a promise when the process is up and running.
  var deferred = Q.defer();
  this._plonkPromise = deferred.promise;
  this._resolveWhenConnectable(deferred, this.options.socks_proxy_port);
};


SSHTunnel.prototype._polipoRun = function() {
  var args = [
    'proxyPort=' + this.options.http_proxy_port,
    'diskCacheRoot=""',
    'disableLocalInterface=true',
    'logLevel=0xFF',
    'allowedPorts=1-65535',
    'tunnelAllowedPorts=1-65535',
    'socksParentProxy=127.0.0.1:' + this.options.socks_proxy_port
  ];

  if (process.platform !== 'win32') {
    // Linux
    this.options.polipo = 'polipo';
  }

  this.polipo = spawn(this.options.polipo, args);

  this.polipo.on('exit', _.bind(this._polipoExit, this));

  // Resolve a promise when the process is up and running.
  var deferred = Q.defer();
  this._polipoPromise = deferred.promise;
  this._resolveWhenConnectable(deferred, this.options.http_proxy_port);
};


SSHTunnel.prototype._resolveWhenConnectable = function(deferred, port) {
  var doConnectabilityCheck = function() {
    var client = net.connect({ port: port });

    client.on('connect', function() {
      deferred.resolve();
      client.end();
      client.removeAllListeners();
    });

    // TODO: This is not very robust. If there's some permanent problem, we'll
    // keep retrying forever. We should probably figure out the set of expected
    // errors (e.g., ECONNREFUSED) and also use a timeout. And then reject
    // deferred if all fails.
    client.on('error', function() {
      // We expect errors. Try again a bit later.
      _.delay(doConnectabilityCheck, 100);
    });
  };

  _.delay(doConnectabilityCheck, 100);
};


module.exports = SSHTunnel;
