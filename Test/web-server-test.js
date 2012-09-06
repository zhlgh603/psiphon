var spawn = require('child_process').spawn;
var fs = require('fs');
var https = require('https');
var tunnel = require('tunnel');
var Q = require('q');
var _ = require('underscore');

var SAMPLE_SIZE = 100, PROCESS_COUNT = 1;
var TIMEOUT = 30000;

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
    // Commenting this out because there are a lot of ETIMEDOUT errors with big parallel tests
    //mylog(e + ': ' + e.code);
    deferred.resolve({error: 999});
    return;
  });

  // This doesn't seem to affect the amount of time allowed to connect. Some googling
  // suggests it's instead for how long to leave the socket open with no traffic.
  //req.setTimeout(TIMEOUT);

  req.end();

  return deferred.promise;
}


// Takes an array of request results that look like:
//   {error: nn, responseTime: nn, endTime: nn}
// Returns an object that looks like:
//   { avg_time: 100, error_rate: 0.2 }
function reduceResults(results) {
  var reduced = { avgResponseTime: 0, avgEndTime: 0, count: 0, errorCount: 0 };

  results.forEach(function(elem) {
    reduced.count += 1;
    if (elem.error) {
      // error
      reduced.errorCount += 1;
    }
    else {
      reduced.avgResponseTime += elem.responseTime;
      reduced.avgEndTime += elem.endTime;
    }
  });

  var successCount = reduced.count - reduced.errorCount;
  if (successCount > 0) {
    reduced.avgResponseTime = reduced.avgResponseTime / successCount;
    reduced.avgEndTime = reduced.avgEndTime / successCount;
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

function testServerRequestParallel(serverReqOptions) {
  var deferred = Q.defer();

  var reqs, i;

  reqs = [];
  for (i = 0; i < serverReqOptions.count; i++) {
    reqs.push(makeRequest(serverReqOptions.tunneled, true,
                          serverReqOptions.testConf.server_ip,
                          requestString(serverReqOptions.reqType, serverReqOptions.testConf),
                          8080));
  }

  Q.all(reqs).then(function(results) {
    results = reduceResults(results);
    results.tunneled = serverReqOptions.tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testServerRequestSerial(serverReqOptions) {
  var deferred = Q.defer();
  var results = [];

  var reqFunc = function(result) {
    if (result) results.push(result);
    return makeRequest(serverReqOptions.tunneled, true,
                       serverReqOptions.testConf.server_ip,
                       requestString(serverReqOptions.reqType, serverReqOptions.testConf),
                       8080);
  };

  var req = Q.resolve();
  var i;
  for (i = 0; i < serverReqOptions.count; i++) {
    req = req.then(reqFunc);
  }

  // Handle the last request
  req.then(function(result) {
    // We get the result for the last request...
    results.push(result);
    // ...and then we process all the results
    results = reduceResults(results);
    results.tunneled = serverReqOptions.tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testServerRequest(serverReqOptions) {
  if (arguments.length !== 1) {
    throw('testServerRequest: bad args');
  }

  if (serverReqOptions.parallel) {
    return testServerRequestParallel(serverReqOptions);
  }
  else {
    return testServerRequestSerial(serverReqOptions);
  }
}

function testSiteRequestParallel(siteReqOptions) {
  var deferred = Q.defer();
  var reqs, i;

  reqs = [];
  for (i = 0; i < siteReqOptions.count; i++) {
    reqs.push(makeRequest(siteReqOptions.tunneled, siteReqOptions.httpsReq,
                          siteReqOptions.host, siteReqOptions.path));
  }

  Q.all(reqs).then(function(results) {
    results = reduceResults(results);
    results.tunneled = siteReqOptions.tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}

function testSiteRequestSerial(siteReqOptions) {
  var deferred = Q.defer();
  var results = [];

  var reqFunc = function(result) {
    if (result) results.push(result);
    return makeRequest(siteReqOptions.tunneled, siteReqOptions.httpsReq,
                       siteReqOptions.host, siteReqOptions.path);
  };

  var req = Q.resolve();
  var i;
  for (i = 0; i < siteReqOptions.count; i++) {
    req = req.then(reqFunc);
  }

  // Handle the last request
  req.then(function(result) {
    // We get the result for the last request...
    results.push(result);
    // ...and then we process all the results
    results = reduceResults(results);
    results.tunneled = siteReqOptions.tunneled;
    deferred.resolve(results);
  }).end();

  return deferred.promise;
}


function testSiteRequest(siteReqOptions) {
  if (arguments.length !== 1) {
    throw('testSiteRequest: bad args');
  }

  if (siteReqOptions.parallel) {
    return testSiteRequestParallel(siteReqOptions);
  }
  else {
    return testSiteRequestSerial(siteReqOptions);
  }
}

function addHexInfoToReq(conf, info) {
  info = info.toString();

  var padLeft = function (string, pad, length) {
    return (new Array(length+1).join(pad)+string).slice(-length);
  };

  info = padLeft(info, '0', info.length + (info.length % 2));

  return _.extend(_.clone(conf), {sponsor_id: info});
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
    mylog('delay', delay, 'ms');
    deferred.resolve();
  }, delay);
  return deferred.promise;
}

function makeServerRequestAndOutputFn(serverReqOptions) {
  var serverReqOptionsClone = _.clone(serverReqOptions);
  var fn = function() {
    return testServerRequest(serverReqOptionsClone)
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
    return testSiteRequest(siteReqOptionsClone)
              .then(function(results) {
                outputSiteReqResults(siteReqOptionsClone, results);
                return Q.resolve();
              });
  };
  return fn;
}

// Start out with a resolved promise
var seq = Q.resolve();

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
  serverReqOptions.testConf = addHexInfoToReq(serverReqOptions.testConf, serverReqOptions.count);
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
serverReqOptions.testConf = addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
serverReqOptions.testConf = addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));

serverReqOptions.reqType = 'connected';
serverReqOptions.testConf = addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = true;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));
serverReqOptions.testConf = addHexInfoToReq(serverReqOptions.testConf, testNum++);
serverReqOptions.tunneled = false;
seq = seq.then(delayNext);
seq = seq.then(makeServerRequestAndOutputFn(serverReqOptions));

var siteReqOptions = {
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

