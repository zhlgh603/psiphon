var https = require('https');
var http = require('http');
var tunnel = require('tunnel');
var Q = require('q');
var _ = require('underscore');


function getTickCount() {
  return new Date().getTime();
}

// promise will be resolved with a positive value on success, negative on failure
// Port can be omitted for site requests
function makeRequest(proxyPort, tunnelReq, httpsReq, host, path, port/*optional*/) {
  if (!_.isNumber(proxyPort)) {
    console.log(arguments);
    throw 'proxyPort must be a number';
  }

  if (!_.isUndefined(port) && !_.isNumber(port)) {
    console.log(arguments);
    throw 'port must be a number';
  }

  var deferred = Q.defer();
  var startTime = 0, responseTime;

  // Make sure we don't hit (polipo's) request cache
  path = addCacheBreaker(path);

  var reqType = httpsReq ? https : http;

  var tunnelingAgent;
  if (httpsReq) {
    tunnelingAgent = tunnel.httpsOverHttp({
      proxy: {
        host: 'localhost',
        port: proxyPort,
        method: 'GET'
      },

      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20100101 Firefox/15.0'
      }
    });
  }
  else {
    tunnelingAgent = tunnel.httpOverHttp({
      proxy: {
        host: 'localhost',
        port: proxyPort,
        method: 'GET'
      },

      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20100101 Firefox/15.0'
      }
    });
  }

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

    //res.on('data', function(data) { console.log(data.length, data.toString()); });

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
    reqs.push(makeRequest(serverReqOptions.testConf.http_proxy_port,
                          serverReqOptions.tunneled, true,
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
    return makeRequest(serverReqOptions.testConf.http_proxy_port,
                       serverReqOptions.tunneled, true,
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
    reqs.push(makeRequest(siteReqOptions.testConf.http_proxy_port,
                          siteReqOptions.tunneled, siteReqOptions.httpsReq,
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
    return makeRequest(siteReqOptions.testConf.http_proxy_port,
                       siteReqOptions.tunneled, siteReqOptions.httpsReq,
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

// Takes an array of request results that look like:
//   {error: nn, responseTime: nn, endTime: nn}
// Returns an object that looks like:
//   { avg_time: 100, error_rate: 0.2 }
function reduceResults(results) {
  var reduced = { avgResponseTime: 0, avgEndTime: 0, count: 0, errorCount: 0 };

  results.forEach(function(elem) {
    //console.log(elem);
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



module.exports = {
  testServerRequest: testServerRequest,
  testSiteRequest: testSiteRequest,
  addHexInfoToReq: addHexInfoToReq
};
