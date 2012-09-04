var spawn = require('child_process').spawn;
var fs = require('fs');
var https = require('https');
var tunnel = require('tunnel');
var Q = require('q');
var _ = require('underscore');

var SAMPLE_SIZE = 10, PROCESS_COUNT = 1;

var i;

/* Not spawning child processes for now, but I'll leave the code
if (!_.include(process.argv, 'childproc')) {
  for (i = 0; i < PROCESS_COUNT; i++) {
    var child = spawn(process.argv[0], process.argv.slice(1).concat('childproc'));
    child.on('exit', function(code) {
      console.log('childproc exited with', code);
    });

    child.stdout.on('data', function(data) {
      console.log('childproc said:', data.toString());
    });
  }
  return;
}
*/

var args = process.argv.splice(2);

// Load the connection info from the file written by the client.
var testConf = JSON.parse(fs.readFileSync('test-conn-info.json'));

// Use the "Testing" propagation channel
if (args.length === 0 || args[0].length !== 16) {
  console.log('Usage: test propagation_channel_id required');
  process.exit(1);
}
testConf.propagation_channel_id = args[0];

function mylog() {
  console.log.apply(console, arguments);
  console.log('');
}

function getTickCount() {
  return new Date().getTime();
}

var tunnelingAgent = tunnel.httpsOverHttp({
  proxy: { // Proxy settings
    host: 'localhost',
    port: 8080,
    method: 'GET'
  }
});

// promise will be resolved with a positive value on success, negative on failure
function makeRequest(tunnelReq, httpsReq, host, path, port) {
  var deferred = Q.defer();
  var startTime = 0, responseTime;

  // Make sure we don't hit (polipo's) request cache
  path = addCacheBreaker(path);

  var reqType = httpsReq ? https : http;

  var reqOptions = {
    host: host,
    port: port ? port : (httpsReq ? 443 : 80),
    path: path,
    agent: tunnelReq ? tunnelingAgent : null
  };

  var req = reqType.request(reqOptions, function(res) {
    responseTime = getTickCount() - startTime;

    var resDone = function() {
      // Protect again getting called twice
      if (deferred) {
        deferred.resolve({
          error: null,
          responseTime: responseTime,
          endTime: getTickCount() - startTime});
        deferred = null;
        return;
      }
    };

    res.on('end', resDone);
    res.on('close', resDone);

    //res.on('data', function(data) { console.log(data.length); });

    if (res.statusCode !== 200) {
      mylog('Error: ' + res.statusCode + ' for ' + path);
      deferred.resolve({error: res.statusCode});
      return;
    }
  });

  startTime = getTickCount();

  req.on('error', function(e) {
    mylog(e + ': ' + e.code);
    deferred.resolve({error: 999});
    return;
  });

  req.end();

  return deferred.promise;
}


// Takes an array of request results that look like:
//   {error: nn, responseTime: nn, endTime: nn}
// Returns an object that looks like:
//   { avg_time: 100, error_rate: 0.2 }
function reduceResults(results) {
  var reduced = { avgResponseTime: 0, avgEndTime: 0, errorRate: 0, count: 0 };
  results.forEach(function(elem) {
    if (elem.error) {
      // error
      reduced.errorRate += 1;
    }
    else {
      reduced.avgResponseTime += elem.responseTime;
      reduced.avgEndTime += elem.endTime;
      reduced.count += 1;
    }
  });

  if (reduced.count > 0) {
    reduced.avgResponseTime = reduced.avgResponseTime / reduced.count;
    reduced.avgEndTime = reduced.avgEndTime / reduced.count;
  }

  return reduced;
}


function toQueryParams(obj) {
  var key;
  var params = [];
  for (key in obj) {
    params.push(key+'='+obj[key]);
  }
  return params.join('&');
}

function requestString(type, testConf) {
  return '/' + type + '?' + toQueryParams(testConf);
}

function addCacheBreaker(requestString) {
  return requestString +
         ((requestString.indexOf('?') < 0) ? '?' : '&') +
         'cachebreak=' + Math.random();
}

function testServerRequestParallel(tunneled, type, testConf) {
  var deferred = Q.defer();

  var reqs, i;

  reqs = [];
  for (i = 0; i < SAMPLE_SIZE; i++) {
    reqs.push(makeRequest(tunneled, true, testConf.server_ip, requestString(type, testConf), 8080));
  }

  Q.all(reqs).then(function(results) {
    results = reduceResults(results);
    results.tunneled = tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testServerRequestSerial(tunneled, type, testConf) {
  var deferred = Q.defer();
  var results = [];

  var reqFunc = function(result) {
    if (result) results.push(result);
    return makeRequest(tunneled, true, testConf.server_ip, requestString(type, testConf), 8080);
  };

  var req = Q.resolve();
  var i;
  for (i = 0; i < SAMPLE_SIZE; i++) {
    req = req.then(reqFunc);
  }

  // Handle the last request
  req.then(function(result) {
    // We get the result for the last request...
    results.push(result);
    // ...and then we process all the results
    results = reduceResults(results);
    results.tunneled = tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testServerRequest(parallelReqs, tunneled, type, testConf) {
  if (arguments.length !== 4) {
    throw('testServerRequest: bad args');
  }

  if (parallelReqs) {
    return testServerRequestParallel(tunneled, type, testConf);
  }
  else {
    return testServerRequestSerial(tunneled, type, testConf);
  }
}

function testSiteRequestParallel(tunneled, httpsReq, host, path) {
  var deferred = Q.defer();
  var reqs, i;

  reqs = [];
  for (i = 0; i < SAMPLE_SIZE; i++) {
    reqs.push(makeRequest(tunneled, httpsReq, host, path));
  }

  Q.all(reqs).then(function(results) {
    results = reduceResults(results);
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testSiteRequestSerial(tunneled, httpsReq, host, path) {
  var deferred = Q.defer();
  var results = [];

  var reqFunc = function(result) {
    if (result) results.push(result);
    return makeRequest(tunneled, httpsReq, host, path);
  };

  var req = Q.resolve();
  var i;
  for (i = 0; i < SAMPLE_SIZE; i++) {
    req = req.then(reqFunc);
  }

  // Handle the last request
  req.then(function(result) {
    // We get the result for the last request...
    results.push(result);
    // ...and then we process all the results
    results = reduceResults(results);
    results.tunneled = tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}


function testSiteRequest(parallelReqs, tunneled, httpsReq, host, path) {
  if (arguments.length !== 5) {
    throw('testSiteRequest: bad args');
  }

  if (parallelReqs) {
    return testSiteRequestParallel(tunneled, httpsReq, host, path);
  }
  else {
    return testSiteRequestSerial(tunneled, httpsReq, host, path);
  }
}

function addHexInfoToReq(conf, info) {
  return _.extend(_.clone(conf), {sponsor_id: info});
}

function outputServerReqResults(parallelReqs, reqType, tunneled, results) {
  console.log(
    reqType + ' test: ' +
    (tunneled ? '' : 'not ') + 'tunneled: ' +
    (parallelReqs ? 'parallel: ' : 'serial: ') +
    '\n  ',
    results);
  console.log(''); // empty line
}

function outputSiteReqResults(parallelReqs, useHttps, host, path, tunneled, results) {
  console.log(
    (tunneled ? '' : 'not ') + 'tunneled request: ' +
    (parallelReqs ? 'parallel: ' : 'serial: ') +
    (useHttps ? 'https://' : 'http://') + host + path +
    '\n  ',
    results);
  console.log(''); // empty line
}

var seq = Q.resolve();


seq = seq.then(function() {
  var reqType = 'handshake';
  var tunneled = true;
  var parallelReqs = false;
  return testServerRequest(parallelReqs, tunneled, reqType, addHexInfoToReq(testConf, '00'))
          .then(function(results) {
            outputServerReqResults(parallelReqs, reqType, tunneled, results);
          });
});

seq = seq.then(function() {
  var reqType = 'handshake';
  var tunneled = false;
  var parallelReqs = false;
  return testServerRequest(parallelReqs, tunneled, reqType, addHexInfoToReq(testConf, '01'))
          .then(function(results) {
            outputServerReqResults(parallelReqs, reqType, tunneled, results);
          });
});


seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = false;
  var parallelReqs = false;
  return testServerRequest(parallelReqs, tunneled, reqType, addHexInfoToReq(testConf, '04'))
          .then(function(results) {
            outputServerReqResults(parallelReqs, reqType, tunneled, results);
          });
});

seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = true;
  var parallelReqs = false;
  return testServerRequest(parallelReqs, tunneled, reqType, addHexInfoToReq(testConf, '03'))
          .then(function(results) {
            outputServerReqResults(parallelReqs, reqType, tunneled, results);
          });
});


seq = seq.then(function() {
  var tunneled = false;
  var useHttps = true;
  var host = 's3.amazonaws.com';
  var path = '/0ubz-2q11-gi9y/en.html';
  var parallelReqs = false;
  return testSiteRequest(parallelReqs, tunneled, useHttps, host, path)
          .then(function(results) {
            outputSiteReqResults(parallelReqs, useHttps, host, path, tunneled, results);
          });
});

seq = seq.then(function() {
  var tunneled = true;
  var useHttps = true;
  var host = 's3.amazonaws.com';
  var path = '/0ubz-2q11-gi9y/en.html';
  var parallelReqs = false;
  return testSiteRequest(parallelReqs, tunneled, useHttps, host, path)
          .then(function(results) {
            outputSiteReqResults(parallelReqs, useHttps, host, path, tunneled, results);
          });
});

seq = seq.then(function() {
  var tunneled = false;
  var useHttps = true;
  var host = '72.21.203.148';
  var path = '/0ubz-2q11-gi9y/en.html';
  var parallelReqs = false;
  return testSiteRequest(parallelReqs, tunneled, useHttps, host, path)
          .then(function(results) {
            outputSiteReqResults(parallelReqs, useHttps, host, path, tunneled, results);
          });
});

seq = seq.then(function() {
  var tunneled = true;
  var useHttps = true;
  var host = '72.21.203.148';
  var path = '/0ubz-2q11-gi9y/en.html';
  var parallelReqs = false;
  return testSiteRequest(parallelReqs, tunneled, useHttps, host, path)
          .then(function(results) {
            outputSiteReqResults(parallelReqs, useHttps, host, path, tunneled, results);
          });
});


seq.then(function() {
  mylog("all done");
}).end();

