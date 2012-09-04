var spawn = require('child_process').spawn;
var fs = require('fs');
var https = require('https');
var tunnel = require('tunnel');
var Q = require('q');
var _ = require('underscore');

var SAMPLE_SIZE = 20, PROCESS_COUNT = 1;

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


function testSiteRequest(httpsReq, host, path) {
  var deferredOverall = Q.defer();
  var deferredUntunneled = Q.defer(), deferredTunneled = Q.defer();
  var reqs, i;

  // Do the untunneled requests first
  reqs = [];
  for (i = 0; i < SAMPLE_SIZE; i++) {
    reqs.push(makeRequest(false, httpsReq, host, path));
  }

  Q.all(reqs).then(function(results) {
    deferredUntunneled.resolve(reduceResults(results));
  }).end();

  // Then the tunneled requests
  reqs = [];
  for (i = 0; i < SAMPLE_SIZE; i++) {
    reqs.push(makeRequest(true, httpsReq, host, path));
  }

  Q.all(reqs)
   .then(function(results) {
      deferredTunneled.resolve(reduceResults(results));
    }).end();

  // Process the overall results

  Q.spread([deferredUntunneled.promise, deferredTunneled.promise], function(untunneledResult, tunneledResult) {
    deferredOverall.resolve({ untunneled: untunneledResult, tunneled: tunneledResult });
  });

  return deferredOverall.promise;
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

function testServerRequest(tunneled, type, testConf) {
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

function addHexInfoToReq(conf, info) {
  return _.extend(_.clone(conf), {sponsor_id: info});
}

/* serial/parallel experimentation
function fi(i) {
  var deferred = Q.defer();
  setTimeout(function(){console.log(i); deferred.resolve();}, Math.random()*1000);
  return deferred.promise;
}

reqs = [];
for (i = 0; i < 10; i++) {
  reqs.push(fi(i));
}


mylog('REDUCING');
reqs.reduce(function(soFar, p) {
  return soFar.then(p);
}, Q.resolve());
mylog('REDUCED');

//Q.all(reqs).then(function(){console.log('DONE');}).end();

return;
*/

var seq = Q.resolve();


seq = seq.then(function() {
  var reqType = 'handshake';
  var tunneled = true;
  return testServerRequest(tunneled, reqType, addHexInfoToReq(testConf, '00'))
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});

seq = seq.then(function() {
  var reqType = 'handshake';
  var tunneled = false;
  return testServerRequest(tunneled, reqType, addHexInfoToReq(testConf, '01'))
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});


seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = false;
  return testServerRequest(tunneled, reqType, addHexInfoToReq(testConf, '04'))
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});

seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = true;
  return testServerRequest(tunneled, reqType, addHexInfoToReq(testConf, '03'))
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});


seq = seq.then(function() {
  return testSiteRequest(true, '72.21.203.148', '/0ubz-2q11-gi9y/en.html')
          .then(function(results) {
            mylog(
              'Request BY IP to: https://s3.amazonaws.com/0ubz-2q11-gi9y/en.html\n  ',
              results);
          });
});

seq = seq.then(function() {
  return testSiteRequest(true, 's3.amazonaws.com', '/0ubz-2q11-gi9y/en.html')
          .then(function(results) {
            mylog(
              'Request BY NAME to: https://s3.amazonaws.com/0ubz-2q11-gi9y/en.html\n  ',
              results);
          });
});


seq.then(function() {
  mylog("all done");
}).end();

