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
and 'exit' when the proxies have stopped. 'exit' has an `unexpected` argument
indicating whether the stoppage was requested or not.
*/

var _ = require('underscore');
var spawn = require('child_process').spawn;


function SSHTunnel(options) {
  this.options = options;
}

require('util').inherits(SSHTunnel, require('events').EventEmitter);

SSHTunnel.prototype.connect = function(options) {
  if (options) this.options = options;

  var plonkArgs = [
    '-ssh', '-C', '-N', '-batch', '-v',
    '-l', this.options.ssh_username,
    '-pw', this.options.ssh_password,
    '-D', this.options.socks_proxy_port,
    this.options.server_ip
  ];

  if (this.options.ossh) {
    plonkArgs.push(
      '-P', this.options.ssh_obfsport,
      '-z',
      '-Z', this.options.ssh_obfskey);
  }
  else {
    plonkArgs.push(
      '-P', this.options.ssh_port);
  }

  this.plonk = spawn(this.options.plonk, plonkArgs);

  var polipoArgs = [
    'proxyPort=' + this.options.http_proxy_port,
    'diskCacheRoot=""',
    'disableLocalInterface=true',
    'logLevel=0xFF',
    'socksParentProxy=127.0.0.1:' + this.options.socks_proxy_port,
    'psiphonServer=' + this.options.server_ip
  ];

  this.polipo = spawn(this.options.polipo, polipoArgs);

  this.plonk.on('exit', _.bind(this._plonkExit, this));

  this.polipo.on('exit', _.bind(this._polipoExit, this));

  this._disconnectExpected = false;

  // TODO: Figure out how to tell when plonk and polipo are up and running.
  // (Maybe try to open a socket to them like we do in the Windows client?)
  // Cheap hack: With verbose logging on, we see this message (as part of a longer,
  // multi-line message) twice from plonk when the connection is complete:
  var magicMessage = 'Initialised AES-256 SDCTR server->client encryption';
  var magicMessageSeen = 0;
  var that = this;
  this.plonk.stderr.on('data', function(data) {
    if (data.toString().indexOf(magicMessage) >= 0) {
      if (magicMessageSeen) { that.emit('connected'); }
      magicMessageSeen += 1;
    }
  });
};

SSHTunnel.prototype.disconnect = function() {
  // This will trigger also killing polipo
  this._disconnectExpected = true;
  this.plonk.kill();
};

SSHTunnel.prototype._plonkExit = function() {
  if (!this._disconnectExpected) console.log('plonk exited', arguments);

  clearTimeout(this._connectTimeout);

  // Kill the other process
  this.polipo.removeAllListeners('exit');
  this.polipo.kill();

  // And let the caller know
  this.emit('exit', this._disconnectExpected);
};

SSHTunnel.prototype._polipoExit = function() {
  if (!this._disconnectExpected) console.log('polipo exited', arguments);

  clearTimeout(this._connectTimeout);

  // Kill the other process
  this.plonk.removeAllListeners('exit');
  this.plonk.kill();

  // And let the caller know
  this.emit('exit', this._disconnectExpected);
};


module.exports = SSHTunnel;
