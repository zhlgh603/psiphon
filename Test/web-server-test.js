var fs = require('fs');
var Q = require('q');
var _ = require('underscore');
var request = require('./tunneled-request');
var SSHTunnel = require('./ssh-tunnel');


var SAMPLE_SIZE = 10, PROCESS_COUNT = 1;
var TIMEOUT = 30000;
var i;

var args = process.argv.splice(2);

// Load the connection info from the file written by the client.
var testConf = JSON.parse(fs.readFileSync('test-conn-info.json'));

// Use the "Testing" propagation channel
if (args.length === 0 || args[0].length !== 16) {
  console.log('Usage: test propagation_channel_id required');
  process.exit(1);
}
testConf.propagation_channel_id = args[0];

// These only make sense for Windows. For other platforms, these are ignored.
testConf.plonk = '../Client/psiclient/3rdParty/plonk.exe';
testConf.polipo = '../Client/psiclient/3rdParty/polipo.exe';


function mylog() {
  console.log.apply(console, arguments);
  console.log('');
}

// This is super rough
function makeFilename(/*...*/) {
  var i, s = '';
  for (i = 0; i < arguments.length; i++) {
    s += encodeURIComponent(arguments[i].toString()+';');
  }
  return s;
}

function outputServerReqResults(serverReqOptions, results) {
  console.log(
    serverReqOptions.reqType + ' test: ' +
    (serverReqOptions.tunneled ? '' : 'not ') + 'tunneled: ' +
    (serverReqOptions.parallel ? 'parallel: ' : 'serial: ') +
    '\n  ',
    results);
  console.log(''); // empty line
  writeServerReqResultsToCSV(serverReqOptions.parallel, serverReqOptions.reqType,
                             serverReqOptions.tunneled, results);
}

function writeServerReqResultsToCSV(
            parallel,
            reqType,
            tunneled,
            results) {
  var filename = makeFilename(parallel, reqType, tunneled) + '.csv';
  var csv = '"' + _.values(results).join('","') + '"\n';

  fs.appendFile(filename, csv);
}

function outputSiteReqResults(siteReqOptionsClone, results) {
  console.log(
    (siteReqOptionsClone.tunneled ? '' : 'not ') + 'tunneled request: ' +
    (siteReqOptionsClone.parallel ? 'parallel: ' : 'serial: ') +
    (siteReqOptionsClone.useHttps ? 'https://' : 'http://') +
    siteReqOptionsClone.host + siteReqOptionsClone.path +
    '\n  ',
    results);
  console.log(''); // empty line
}

// Call like
//   seq = seq.then(delayNext);
function delayNext() {
  var deferred = Q.defer();
  var delay = 1000;
  setTimeout(function() {
    console.log('delay', delay, 'ms');
    deferred.resolve();
  }, delay);
  return deferred.promise;
}

function makeServerRequestAndOutputFn(serverReqOptions) {
  var serverReqOptionsClone = _.clone(serverReqOptions);
  var fn = function() {
    return request.testServerRequest(serverReqOptionsClone)
              .then(function(results) {
                outputServerReqResults(serverReqOptionsClone, results);
                return Q.resolve();
              });
  };
  return fn;
}

function makeSiteRequestAndOutputFn(siteReqOptions) {
  var siteReqOptionsClone = _.clone(siteReqOptions);
  var fn = function() {
    return request.testSiteRequest(siteReqOptionsClone)
              .then(function(results) {
                outputSiteReqResults(siteReqOptionsClone, results);
                return Q.resolve();
              });
  };
  return fn;
}

var nextLocalPort = 10000;

// testConf will be modified with the `socks_proxy_port` and `http_proxy_port`
// used, and a `sshTunnel` object.
// The returned promise will be resolved when the connection is established.
function createTunnel(testConf) {
  var deferred = Q.defer();

  testConf.socks_proxy_port = nextLocalPort++;
  testConf.http_proxy_port = nextLocalPort++;

  var sshTunnel = new SSHTunnel(testConf);

  sshTunnel.once('connected', function() { deferred.resolve(); });

  sshTunnel.once('exit', function(unexpected) {
    sshTunnel.removeAllListeners();
    if (!deferred.promise.isResolved()) {
      // unexpected ought to be true, but we won't bother checking
      deferred.reject(new Error('unexpected disconnect', testConf.socks_proxy_port, testConf.http_proxy_port));
    }
  });

  sshTunnel.connect();

  // Set the SSHTunnel object for later use (like disconnecting)
  testConf.sshTunnel = sshTunnel;

  return deferred.promise;
}

function disconnectTunnel(testConf) {
  var deferred = Q.defer();

  if (!testConf.sshTunnel) throw 'disconnectTunnel: not tunnel testConf';

  // Make sure that this doesn't conflict with any connect code.
  testConf.sshTunnel.removeAllListeners();

  testConf.sshTunnel.once('exit', function(unexpected) {
    deferred.resolve();
  });

  testConf.sshTunnel.disconnect();

  return deferred.promise;
}

function disconnectTunnels(tunnelTestConfs) {
  var promises = [];
  _.each(tunnelTestConfs, function(tunnelTestConf) {
    if (tunnelTestConf) {
      promises.push(disconnectTunnel(tunnelTestConf));
    }
  });

  return Q.all(promises);
}

function connectTunnels(tunnelCount, ossh) {
  var deferred = Q.defer();

  var tunnels = [];
  var tunnel;
  for (i = 0; i < tunnelCount; i++) {
    tunnel = { testConf: _.clone(testConf), promise: null };
    tunnels.push(tunnel);

    tunnel.testConf.ossh = ossh;

    tunnel.promise = createTunnel(tunnel.testConf);
  }

  Q.all(_.pluck(tunnels, 'promise')).then(function() {
    // success
    console.log('connected all tunnels', tunnels.length, ossh?'OSSH':'SSH');
    deferred.resolve(_.pluck(tunnels, 'testConf'));
  }, function() {
    // Failed to connect all of them.
    disconnectTunnels(_.pluck(tunnels, 'testConf'))
    .then(function() {
      deferred.reject();
    }, function() {
      deferred.reject();
    })
    .end();
  })
  .end();

  return deferred.promise;
}

function simultaneousTunnels_Test(ossh, stopAtFirstFail) {
  console.log(
    '\n\n',
    arguments.callee.name,
    (ossh ? 'OSSH' : 'SSH'),
    'stopAtFirstFail:'+stopAtFirstFail);

  var deferred = Q.defer();

  var failReported = false;
  var fail = function(numTunnels, ossh, error) {
    var msg = 'simultaneousTunnels_Test failed at ' + numTunnels + ' for ' + (ossh?'OSSH: ':'SSH: ') + error;
    if (!failReported) {
      console.log(msg);
      failReported = true;
    }
    if (stopAtFirstFail) throw new Error(msg);
  };

  // Start out with a resolved promise
  var seq = Q.resolve();
  var numTunnels = 1;

  while (numTunnels <= 300) {
    seq = seq.then(_.bind(connectTunnels, null, numTunnels, ossh));

    seq = seq.then(
      disconnectTunnels,
      _.bind(fail, null, numTunnels, ossh));

    seq = seq.then(delayNext);

    numTunnels += 1;
  }

  seq.fin(function() {
    deferred.resolve();
  });

  seq.end();

  return deferred.promise;
}

// `eachConnectionCallback` will be called each time a connection is successfully
// established. It is called like `eachConnectionCallback(tunnels)` and is expected
// to return a promise. `tunnels` is an array of `testConf`. Some items in
// `tunnels` may be null.
function cumulativeTunnels_Test(maxTunnels, ossh, stopAtFirstFail, addDelay,
                                eachConnectionCallback) {
  console.log(
    '\n\n',
    arguments.callee.name,
    (ossh ? 'OSSH' : 'SSH'),
    'stopAtFirstFail:'+stopAtFirstFail,
    'addDelay:'+addDelay);

  var deferred = Q.defer();

  // Only count non-null tunnels
  var tunnelCount = function(tunnels) {
    return _.reduce(tunnels, function(memo, item) {
      return memo + (item ? 1 : 0);
    }, 0);
  };

  var makeTunnel = function(ossh, tunnels) {
    var deferred = Q.defer();

    var tunnelTestConf = _.clone(testConf);

    tunnelTestConf.ossh = ossh;

    createTunnel(tunnelTestConf)
    .then(function() {
      tunnels.push(tunnelTestConf);
      console.log('connected', tunnelCount(tunnels));

      // Handle the case that the tunnel gets disconnected during the test
      var idx = tunnels.length-1;
      tunnelTestConf.sshTunnel.on('exit', function(expected) {
        if (!expected) {
          console.log('lost an establied tunnel:', idx);
          tunnels[idx] = null;
        }
      });

      deferred.resolve();
    }, function() {
      deferred.reject();
    });

    return deferred.promise;
  };

  var failReported = false;
  var fail = function(ossh, error) {
    var msg = 'cumulativeTunnels_Test failed at ' + tunnelCount(tunnels) + ' for ' + (ossh?'OSSH: ':'SSH: ') + error;
    if (!failReported) {
      console.log(msg);
      failReported = true;
    }
    if (stopAtFirstFail) throw new Error(msg);
  };

  // Start out with a resolved promise
  var seq = Q.resolve();
  var numTunnels = 1;
  var tunnels = [];

  while (numTunnels++ <= maxTunnels) {
    seq = seq.then(
      _.bind(makeTunnel, null, ossh, tunnels),
      _.bind(fail, null, ossh));

    if (eachConnectionCallback) {
      seq = seq.then(
        _.bind(eachConnectionCallback, null, tunnels),
        _.bind(fail, null, ossh));
    }

    // TODO: With this code, the delay will be skipped if the previous statement
    // fails. This shouldn't be the case.
    if (addDelay) {
      seq = seq.then(
        delayNext,
        _.bind(fail, null, ossh));
    }
  }

  seq.fin(function() {
    disconnectTunnels(tunnels)
    .then(function() {
      deferred.resolve();
    });
  });

  seq.end();

  return deferred.promise;
}

function perCumulativeConnectionWork_requestOnNewTunnel(tunnels) {
  if (!tunnels || !tunnels.length || !_.last(tunnels)) return;

  var serverReqOptions = {
    reqType: 'connected',
    tunneled: true,
    parallel: false,
    testConf: _.last(tunnels),
    count: 10
  };

  return makeServerRequestAndOutputFn(serverReqOptions)();
}

// Take an array of results from request.testServerRequest and combine them into
// a single result.
function combineResults(resultsArray) {
  if (!resultsArray || !resultsArray.length || !_.first(resultsArray)) return;

  // Using basic introspection, set up our result object.
  var result = _.clone(_.first(resultsArray));
  var key;
  // Clear out the numbers.
  for (key in result) {
    if (_.isNumber(result[key])) {
      result[key] = 0;
    }
  }

  // There are certain keys we need the result to have
  if (!_.has(result, 'count') || !_.has(result, 'errorCount')) {
    throw 'combineResults: bad result structure';
  }

  _.each(resultsArray, function(item) {
    if (!item) return;
    var key;
    for (key in result) {
      if (_.isNumber(result[key])) {
        result[key] += item[key];
      }
    }
  });

  var successCount = result.count - result.errorCount;
  for (key in result) {
    if (_.isNumber(result[key]) && key !== 'count' && key !== 'errorCount') {
      result[key] = result[key] / successCount;
    }
  }

  return result;
}

function perCumulativeConnectionWork_downloadOnAllTunnels(tunnelTestConfs) {
  if (!tunnelTestConfs || !tunnelTestConfs.length || !_.last(tunnelTestConfs)) return;

  var deferred = Q.defer();

  var serverReqOptions = {
    reqType: 'download',
    tunneled: true,
    parallel: false,
    count: 1,
    testConf: null
  };

  var reqPromises = [];
  _.each(tunnelTestConfs, function(tunnelTestConf) {
    if (!tunnelTestConf) return;
    serverReqOptions.testConf = tunnelTestConf;
    reqPromises.push(request.testServerRequest(_.clone(serverReqOptions)));
  });

  Q.all(reqPromises)
  .then(function(resultsArray) {
    var results = combineResults(resultsArray);
    outputServerReqResults(serverReqOptions, results);
    return Q.resolve();
  })
  .then(function() { deferred.resolve(); })
  .end();

  return deferred.promise;
}

// Start out with a resolved promise
var seq = Q.resolve();


// ssh, no delay
seq
.then(_.bind(cumulativeTunnels_Test, null, 2, false, false, false, perCumulativeConnectionWork_downloadOnAllTunnels));
return;

seq
// ossh, no delay
.then(_.bind(cumulativeTunnels_Test, null, 10000, true, false, false))
// ssh
.then(_.bind(simultaneousTunnels_Test, null, false, false))
// ossh
.then(_.bind(simultaneousTunnels_Test, null, true, false));

return;

var serverReqOptions = {
  reqType: 'handshake',
  tunneled: false,
  parallel: true,
  testConf: testConf,
  count: SAMPLE_SIZE
};

var testNum = 1;



/*
for (i = 1; i <= 25; i++) {
  serverReqOptions.count = i*5;
  serverReqOptions.testConf = request.addHexInfoToReq(serverReqOptions.testConf, serverReqOptions.count);
  serverReqOptions.reqType = 'connected';

  serverReqOptions.tunneled = true;
  seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
  seq = seq.then(delayNext);

  serverReqOptions.tunneled = false;
  seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
  seq = seq.then(delayNext);
}

return;
*/


serverReqOptions.count = SAMPLE_SIZE;

serverReqOptions.reqType = 'handshake';
serverReqOptions.testConf = request.addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
serverReqOptions.testConf = request.addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));

serverReqOptions.reqType = 'connected';
serverReqOptions.testConf = request.addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
serverReqOptions.testConf = request.addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));

var siteReqOptions = {
  testConf: testConf,
  count: SAMPLE_SIZE,
  parallel: true,
  tunneled: true,
  httpsReq: true,
  host: 's3.amazonaws.com',
  path: '/0ubz-2q11-gi9y/en.html'
};

siteReqOptions.host = 's3.amazonaws.com';
siteReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeSiteRequestAndOutputFn(siteReqOptions));
siteReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeSiteRequestAndOutputFn(siteReqOptions));

siteReqOptions.host = '72.21.203.148';
siteReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeSiteRequestAndOutputFn(siteReqOptions));
siteReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeSiteRequestAndOutputFn(siteReqOptions));


seq.then(function() {
  mylog("all done");
}).end();

