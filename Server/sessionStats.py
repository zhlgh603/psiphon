from datetime import datetime
from random import choice as randomChoice

# {{{ Subclass of StrictRedis that pickles/unpickles values on get/set
import pickle
from redis import StrictRedis

class PickledRedis(StrictRedis):
    def get(self, name):
        pickled_value = super(PickledRedis, self).get(name)
        if pickled_value is None:
            return None
        return pickle.loads(pickled_value)

    def set(self, name, value, ex=None, px=None, nx=False, xx=False):
        return super(PickledRedis, self).set(name, pickle.dumps(value), ex, px, nx, xx)
# }}}

class Manager:
    def __init__(self, fragmentTtl = (60 * 15), redisDb = 2, redisHost = "localhost", redisPort = 6379):
        self.redisDb = redisDb
        self.redisHost = redisHost
        self.redisPort = redisPort
        self.pubsubThread = None
        self.fragmentTtl = fragmentTtl
        self.fragmentPrefix = "session-fragment:"
        self.fragmentExpiryPrefix = "expires:"
        self.sessionFragmentTemplate = {
            "start_time": None,
            "end_time": None,
            "duration": 0,
            "fragment_id": None,
            "bytes": 0
        }

        try:
            self.redisHandle = PickledRedis(host=self.redisHost, port=self.redisPort, db=self.redisDb)

            # http://redis.io/topics/notifications
            # set configuration to receive "key event" notifications for expired keys
            kseSettings = self.redisHandle.config_get("notify-keyspace-events")
            if "x" and "E" not in kseSettings["notify-keyspace-events"]:
                self.redisHandle.config_set("notify-keyspace-events", "xE")
        except Exception as e:
            print("[FATAL] sessionStats - Could not connect to Redis and set configuration to receive expiry events")
            raise e

    # {{{ PubSub Message Listener Start/Stop
    def startListener(self):
        l = self.redisHandle.pubsub(ignore_subscribe_messages=True)
        l.psubscribe(**{"__keyevent@" + str(self.redisDb)  + "__:expired": self.onExpiry})
        self.pubsubThread = l.run_in_thread(sleep_time=0.5)

    def stopListener(self):
        if self.pubsubThread:
            self.pubsubThread.stop()
            self.pubsubThread = None
    # }}}

    # {{{ Fragment Methods
    def startSessionFragment(self, record = None, newBytes = 0):
        #print("[INFO] startSessionFragment - Creating fragment with key: %s" % (self.fragmentPrefix + record["session_id"]))
        fragment = self.sessionFragmentTemplate.copy()

        fragment["session_id"] = record["session_id"]
        fragment["fragment_id"] = "".join(randomChoice('0123456789abcdef') for n in xrange(16))
        fragment["start_time"] = datetime.utcnow()
        fragment["end_time"] = datetime.utcnow()

        if newBytes > 0:
            fragment["bytes"] = newBytes

        # Add fields from connected record into session fragment
        for key in record:
            if key not in fragment:
                fragment[key] = record[key]

        self.redisHandle.set(self.fragmentPrefix + record["session_id"], fragment)
        self.redisHandle.setex(self.fragmentExpiryPrefix + self.fragmentPrefix + record["session_id"], self.fragmentTtl, None)

    def updateSessionFragment(self, sessionId = "", newBytes = 0, forceExpiry = False):
        fragment = self.redisHandle.get(self.fragmentPrefix + sessionId)
        if fragment:
            #print("[INFO] updateSessionFragment - Updating session duration, and adding %d to session bytes to fragment with key: %s" % (newBytes, (self.fragmentPrefix + sessionId)))

            fragment["end_time"] = datetime.utcnow()
            fragment["bytes"] += newBytes
            self.redisHandle.set(self.fragmentPrefix + sessionId, fragment)
            if forceExpiry == True:
                #print("[DEBUG] updateSessionFragment - 'forceExpiry' was set to true. Setting TTL to 1 for key: %s" % (self.fragmentExpiryPrefix + self.fragmentPrefix + sessionId))
                self.redisHandle.expire(self.fragmentExpiryPrefix + self.fragmentPrefix + sessionId, 1)
            else:
                #print("[DEBUG] updateSessionFragment - 'forceExpiry' was set to false. Resetting TTL to %d for key: %s" % (self.fragmentTtl, self.fragmentExpiryPrefix + self.fragmentPrefix + sessionId))
                self.redisHandle.expire(self.fragmentExpiryPrefix + self.fragmentPrefix + sessionId, self.fragmentTtl)
        else:
            #print("[ERROR] updateSessionFragment - Could not retrieve fragment with key: %s" % (self.fragmentPrefix + sessionId))
            return

    def flushSessionFragment(self, fragment):
        fragment["end_time"] = datetime.utcnow()
        fragment["duration"] = int(round((fragment["end_time"] - fragment["start_time"]).total_seconds()))

        fragment["end_time"] = fragment["end_time"].isoformat()
        fragment["start_time"] = fragment["start_time"].isoformat()

        key = self.fragmentPrefix + fragment["session_id"]
        try:
            self.redisHandle.delete(key)
        except Exception as e:
            raise e

        self.handleFlushedFragment(fragment)

    def handleFlushedFragment(self, fragment):
        # NOTE: This method should be overridden by the instantiator to do something meaningful with the final fragment
        print("[FLUSH] %r" % (fragment))
    # }}}

    # {{{ Input Methods
    def onConnection(self, record = None):
        #print("[DEBUG] onConnection - Session ID: %s" % (record["session_id"]))

        fragment = self.redisHandle.get(self.fragmentPrefix + record["session_id"])
        if fragment:
            #print("[INFO] onConnection - Reconnect detected. Flushing current fragment and starting a new fragment with key: %s" % (self.fragmentPrefix + record["session_id"]))
            self.flushSessionFragment(fragment)
            self.startSessionFragment(record)
        else:
            #print("[INFO] onConnection - Creating new fragment with key: %s" % (self.fragmentPrefix + record["session_id"]))
            self.startSessionFragment(record)

    def onStatus(self, sessionId = "", newBytes = 0):
        #print("[DEBUG] onStatus - Session ID: %s" % (sessionId))

        fragment = self.redisHandle.get(self.fragmentPrefix + sessionId)
        if fragment:
            #print("[INFO] onStatus - Calling 'updateSessionFragment' with bytes: %d for fragment with key: %s" % (newBytes, self.fragmentPrefix + sessionId))
            self.updateSessionFragment(sessionId, newBytes)
        else:
            #print("[INFO] onStatus - Reconnect from a different server detected. Creating new fragment with key: %s" % (self.fragmentPrefix + sessionId))
            # TODO: There will be no data besides these 2 fields in this case, is this the right way to handle it?
            self.startSessionFragment({"session_id": sessionId, "bytes": newBytes})

    def onDisconnect(self, sessionId = "", newBytes = 0):
        #print("[DEBUG] onDisconnect - Disconnection for session fragment: %s" % (self.fragmentPrefix + sessionId))
        self.updateSessionFragment(sessionId, newBytes, forceExpiry = True)

    def onExpiry(self, message):
        try:
            if message["data"][:len(self.fragmentExpiryPrefix + self.fragmentPrefix)] == self.fragmentExpiryPrefix + self.fragmentPrefix:
                fragmentKey = message["data"][len(self.fragmentExpiryPrefix):]
                #print("[DEBUG] onExpiry - received expiry notification: %r" % (message))

                fragment = self.redisHandle.get(fragmentKey)
                if fragment:
                    #print("[%s] [INFO] onExpiry - Flushing expired fragment with key: %s" % (datetime.now(), (self.fragmentPrefix + fragment["session_id"])))
                    self.flushSessionFragment(fragment)
                else:
                    pass
                    #print("[%s] [ERROR] onExpiry - Cannot flush fragment (not found) with key: %s" % (datetime.now(), fragmentKey))
            else:
                #print("[WARNING] onExpiry - Ignoring expiry notification for an unknown key: %s" % (message["data"]))
                return
        except Exception as e:
            raise e
    # }}}
