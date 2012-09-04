var fs = require('fs');
var https = require('https');
var tunnel = require('tunnel');
var Q = require('q');

var SAMPLE_SIZE = 20;

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
  var startTime = 0;

  var reqType = httpsReq ? https : http;

  var reqOptions = {
    host: host,
    port: port ? port : (httpsReq ? 443 : 80),
    path: path,
    agent: tunnelReq ? tunnelingAgent : null
  };

  var req = reqType.request(reqOptions, function(res) {
    if (res.statusCode === 200) {
      return deferred.resolve(getTickCount() - startTime);
    }
    else {
      mylog('Error: ' + res.statusCode + ' for ' + path);
      return deferred.resolve(-res.statusCode);
    }
  });

  startTime = getTickCount();

  req.end();

  req.on('error', function(e) {
    mylog(e + ': ' + e.code);
    return deferred.resolve(-999);
  });

  return deferred.promise;
}


// Takes an array of times and errors.
// Returns an object that looks like:
//   { avg_time: 100, error_rate: 0.2 }
function reduceResults(results) {
  var reduced = { avg_time: 0, error_rate: 0, count: 0 };
  results.forEach(function(elem) {
    if (elem < 0) {
      // error
      reduced.error_rate += 1;
    }
    else {
      reduced.avg_time += elem;
      reduced.count += 1;
    }
  });

  if (reduced.count > 0) {
    reduced.avg_time = reduced.avg_time / reduced.count;
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
  return testServerRequest(tunneled, reqType, testConf)
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});

seq = seq.then(function() {
  var reqType = 'handshake';
  var tunneled = false;
  return testServerRequest(tunneled, reqType, testConf)
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});


seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = true;
  return testServerRequest(tunneled, reqType, testConf)
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});

seq = seq.then(function() {
  var reqType = 'connected';
  var tunneled = false;
  return testServerRequest(tunneled, reqType, testConf)
          .then(function(results) {
            mylog(
              reqType + ' test: ' + (results.tunneled?'':'not ') + 'tunneled\n  ',
              results);
          });
});


seq = seq.then(function() {
  return testSiteRequest(true, 's3.amazonaws.com', '/0ubz-2q11-gi9y/en.html')
          .then(function(results) {
            mylog(
              'Request to: https://s3.amazonaws.com/0ubz-2q11-gi9y/en.html\n  ',
              results);
          });
});

seq.then(function() {
  mylog("all done");
}).end();

