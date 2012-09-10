var _ = require('underscore');
var spawn = require('child_process').spawn;


function SSHTunnel(options) {
  this.options = options;
}

require('util').inherits(SSHTunnel, require('events').EventEmitter);

SSHTunnel.prototype.connect = function(options) {
  if (options) this.options = options;

  var plonkArgs = [
    '-ssh', '-C', '-N', '-batch',
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
    'logLevel=1',
    'socksParentProxy=127.0.0.1:' + this.options.socks_proxy_port,
    'psiphonServer=' + this.options.server_ip
  ];

  this.polipo = spawn(this.options.polipo, polipoArgs);

  this.plonk.on('exit', _.bind(this._plonkExit, this));

  console.log(plonkArgs);
  this.plonk.stdout.on('data', function(data){console.log(data.toString());});
  this.plonk.stderr.on('data', function(data){console.log(data.toString());});

  this.polipo.on('exit', _.bind(this._polipoExit, this));
};

SSHTunnel.prototype.disconnect = function() {
  // This will trigger also killing polipo
  this.plonk.kill();
};

SSHTunnel.prototype._plonkExit = function() {
  console.log('plonk exited', arguments);

  // Kill the other process
  this.polipo.removeAllListeners('exit');
  this.polipo.kill();

  // And let the caller know
  this.emit('exit');
};

SSHTunnel.prototype._polipoExit = function() {
  console.log('polipo exited', arguments);

  // Kill the other process
  this.plonk.removeAllListeners('exit');
  this.plonk.kill();

  // And let the caller know
  this.emit('exit');
};


module.exports = SSHTunnel;
