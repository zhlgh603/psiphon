package com.psiphon3.psiphonlibrary;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.os.SystemClock;
import android.provider.Settings;

import com.psiphon3.psiphonlibrary.Utils.MyLog;
import com.psiphon3.util.AESObfuscator;
import com.psiphon3.util.Obfuscator;
import com.psiphon3.util.ValidationException;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.RandomAccessFile;
import java.lang.ref.WeakReference;
import java.util.ArrayList;


public class FreeTrialTimerService extends Service {
    private static final long STORE_PERIOD_MILLIS = 60 * 1000;

    /// Keeps track of all registered clients
    ArrayList<ClientObject> mClients = new ArrayList<>();

    public enum MessageType {
        // Client message type when it wants to register for timer updates
        MSG_CLIENT_REGISTER,

        // Client message type when it wants to unregister from timer updates
        MSG_CLIENT_UNREGISTER,

        // Client message type when it wants the service to add more time
        MSG_CLIENT_ADD_TIME_SECONDS,

        // Client message type when it wants the service to start timer
        MSG_CLIENT_START_TIMER,

        // Client message type when it wants the service to stop timer
        MSG_CLIENT_STOP_TIMER,

        UNKNOWN;

        public static MessageType getType(final int input) {
            if (input < 0 || input > UNKNOWN.ordinal()) {
                return UNKNOWN;
            } else {
                return MessageType.values()[input];
            }
        }
    }

    private final Messenger m_messenger = new Messenger(new IncomingHandler(this));

    private long m_startedTimeMillis = -1;
    private long m_lastReadTimeSeconds = -1;

    private final Handler m_timerHandler = new Handler();
    private final Object m_messageTag = new Object();


    private class ClientUpdateRunnable implements Runnable {

        final ClientObject m_clientObj;

        public ClientUpdateRunnable(ClientObject clientObj) {
            m_clientObj = clientObj;
        }

        @Override
        public void run() {
            try {
                sendUpdateClient(m_clientObj.client, m_clientObj.msgCode);
            } catch (RemoteException e) {
                mClients.remove(m_clientObj);
            }
            if (m_clientObj.updateInterval > 0) {
                m_timerHandler.postAtTime(this, m_messageTag, SystemClock.uptimeMillis() + m_clientObj.updateInterval);
            }
        }
    }

    private long calcRemainingTimeSeconds(long lastStoredSeconds, long startedMillis) {
        long timeSinceStartedMillis = SystemClock.elapsedRealtime() - startedMillis;
        long currentRemainingSeconds = (long) Math.floor(Math.max(0, lastStoredSeconds * 1000 - timeSinceStartedMillis) / 1000);
        return currentRemainingSeconds;
    }

    private Runnable m_periodicStoreTimeSecondsRunnable = new Runnable() {
        @Override
        public void run() {
            if (m_lastReadTimeSeconds > 0 && m_startedTimeMillis > 0) {

                //Store remaining seconds, do not reset m_lastReadTimeSeconds
                long remainingSeconds = calcRemainingTimeSeconds(m_lastReadTimeSeconds, m_startedTimeMillis);
                FreeTrialTimeStore.setRemainingTimeSeconds(FreeTrialTimerService.this, remainingSeconds);
            }
            m_timerHandler.postAtTime(this, m_messageTag, SystemClock.uptimeMillis() + STORE_PERIOD_MILLIS);
        }
    };

    // Handler of incoming messages from clients.
    private static class IncomingHandler extends Handler {
        private final WeakReference<FreeTrialTimerService> mTarget;

        IncomingHandler(FreeTrialTimerService target) {
            mTarget = new WeakReference<>(target);
        }

        @Override
        public void handleMessage(Message msg) {

            FreeTrialTimerService target = mTarget.get();
            if (target == null) {
                super.handleMessage(msg);
                return;
            }

            MessageType msgType = MessageType.getType(msg.what);
            MyLog.d("FreeTrialTimerService:handleMessage(" + msgType.name() + ")");

            switch (msgType) {
                case MSG_CLIENT_REGISTER:
                    target.onRegisterClient(msg.replyTo, msg.arg1, msg.arg2);
                    break;
                case MSG_CLIENT_UNREGISTER:
                    target.onUnregisterClient(msg.replyTo);
                    break;
                case MSG_CLIENT_ADD_TIME_SECONDS:
                    target.onAddTimeSeconds(((Number) msg.obj).longValue());
                    break;
                case MSG_CLIENT_START_TIMER:
                    target.onStartTimer();
                    break;
                case MSG_CLIENT_STOP_TIMER:
                    target.onStopTimer();
                    break;
                default:
                    super.handleMessage(msg);
            }
        }
    }

    // Client message handlers
    private void onUnregisterClient(Messenger client) {
        ClientObject lookupClient = new ClientObject(client, 0, 0);
        mClients.remove(lookupClient);
        MyLog.d("FreeTrialTimerService:onUnregisterClient(), current clients: " + mClients.size());
    }

    private void onRegisterClient(Messenger client, int msgType, int updateInterval) {
        //Send time value to newly registered clients immediately
        try {
            sendUpdateClient(client, msgType);

            ClientObject clientObj = new ClientObject(client, msgType, updateInterval);

            //don't register same client multiple times
            mClients.remove(clientObj);
            mClients.add(clientObj);

        } catch (RemoteException e) {

        }
        MyLog.d("FreeTrialTimerService:onRegisterClient(), current clients: " + mClients.size());
    }

    private void onAddTimeSeconds(long seconds) {
        if (seconds > 0) {
            m_lastReadTimeSeconds += seconds;
            FreeTrialTimeStore.setRemainingTimeSeconds(this, m_lastReadTimeSeconds);
            sendUpdateToAllClients();
        }
    }

    private void onStartTimer() {
        MyLog.d("FreeTrialTimeService:onStartTimer()");

        // Unschedule all client updates
        m_timerHandler.removeCallbacksAndMessages(m_messageTag);


        // Do not update if timer is already running
        if (m_startedTimeMillis < 0) {
            m_startedTimeMillis = SystemClock.elapsedRealtime();
        }

        // Get time from storage if not cached
        if (m_lastReadTimeSeconds < 0) {
            m_lastReadTimeSeconds = FreeTrialTimeStore.getRemainingTimeSeconds(this);
        }


        // Schedule timer updates for all clients
        for (int i = mClients.size() - 1; i >= 0; i--) {
            ClientObject clientObj = mClients.get(i);
            ClientUpdateRunnable r = new ClientUpdateRunnable(clientObj);
            if (clientObj.updateInterval > 0) {
                m_timerHandler.postAtTime(r, m_messageTag, SystemClock.uptimeMillis() + clientObj.updateInterval);
            }
        }

        m_timerHandler.postAtTime(m_periodicStoreTimeSecondsRunnable, m_messageTag, SystemClock.uptimeMillis() + STORE_PERIOD_MILLIS);
    }

    private void onStopTimer() {
        MyLog.d("FreeTrialTimeService:onStopTimer()");
        m_timerHandler.removeCallbacksAndMessages(m_messageTag);

        sendUpdateToAllClients();
        if (m_lastReadTimeSeconds > 0 && m_startedTimeMillis > 0) {
            //store current remaining seconds
            m_lastReadTimeSeconds = calcRemainingTimeSeconds(m_lastReadTimeSeconds, m_startedTimeMillis);
            FreeTrialTimeStore.setRemainingTimeSeconds(this, m_lastReadTimeSeconds);
        }
        m_startedTimeMillis = -1;
    }

    //Service messages to client(s)
    private void sendUpdateClient(Messenger client, int msgCode) throws RemoteException {

        if (m_lastReadTimeSeconds > 0) {
            long updateSeconds;
            if (m_startedTimeMillis > 0) {
                updateSeconds = calcRemainingTimeSeconds(m_lastReadTimeSeconds, m_startedTimeMillis);
            } else {
                updateSeconds = m_lastReadTimeSeconds;
            }
            client.send(Message.obtain(null, msgCode, 0, 0, updateSeconds));
        }
    }

    private void sendUpdateToAllClients() {
        for (int i = mClients.size() - 1; i >= 0; i--) {
            ClientObject clientObj = mClients.get(i);
            try {
                sendUpdateClient(clientObj.client, clientObj.msgCode);
            } catch (RemoteException e) {
                // The client is dead.  Remove it from the list;
                // we are going through the list from back to front
                // so this is safe to do inside the loop.
                mClients.remove(i);
            }
        }
    }

    @Override
    public void onCreate() {
        MyLog.d("Service onCreate()");

        // Get time from storage if not cached
        if (m_lastReadTimeSeconds < 0) {
            m_lastReadTimeSeconds = FreeTrialTimeStore.getRemainingTimeSeconds(this);
        }
    }

    @Override
    public void onDestroy() {
        MyLog.d("FreeTrialTimeService:onDestroy()");
        onStopTimer();
        mClients.clear();
    }

    @Override
    public IBinder onBind(Intent intent) {
        MyLog.d("FreeTrialTimeService:onBind()");
        return m_messenger.getBinder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        MyLog.d("FreeTrialTimeService:onUnbind()");
        return super.onUnbind(intent);
    }

    static private class ClientObject extends Object {
        private Messenger client;
        private int msgCode;
        private int updateInterval;

        public ClientObject(Messenger client, int msgCode, int updateInterval) {
            this.client = client;
            this.msgCode = msgCode;
            this.updateInterval = updateInterval;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == null) {
                return false;
            }
            if (!ClientObject.class.isAssignableFrom(obj.getClass())) {
                return false;
            }
            final ClientObject other = (ClientObject) obj;
            if ((this.client == null) && (other.client == null)) {
                return true;
            }
            if ((this.client != null) && (other.client != null) && this.client.equals(other.client)) {
                return true;
            }
            return false;
        }
    }

    private static class FreeTrialTimeStore {
        private static final String FREE_TRIAL_TIME_FILENAME = "com.psiphon3.pro.FreeTrialTimer";
        private static final String KEY_FREE_TRIAL_TIME_SECONDS = "freeTrialTimeSeconds";
        private static final byte[] SALT = {19, -116, -92, -120, 30, 43, 79, -99, 125, -124, -41, -46, 67, -117, 39, 80, -33, -73, -6, 3};

        private static long getRemainingTimeSeconds(Context context) {
            FileInputStream in = null;
            try {
                in = context.openFileInput(FREE_TRIAL_TIME_FILENAME);
                return getRemainingTimeSeconds(context, in);

            } catch (FileNotFoundException e) {
                // do nothing, will return 0
            } finally {
                try {
                    if (in != null) {
                        in.close();
                    }
                } catch (IOException e) {
                    // do nothing
                }
            }
            return 0;
        }

        private static long getRemainingTimeSeconds(Context context, FileInputStream in) {

            long remainingSeconds;

            String deviceId = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.ANDROID_ID);
            Obfuscator obfuscator = new AESObfuscator(SALT, context.getPackageName(), deviceId);

            try {
                BufferedReader reader = new BufferedReader(new InputStreamReader(in));
                String line;
                line = reader.readLine();

                reader.close();
                try {
                    remainingSeconds = Long.parseLong(obfuscator.unobfuscate(line, KEY_FREE_TRIAL_TIME_SECONDS));
                } catch (ValidationException | NumberFormatException e) {
                    remainingSeconds = 0;
                }
            } catch (IOException e) {
                remainingSeconds = 0;
            }

            return remainingSeconds;
        }

        private static void setRemainingTimeSeconds(Context context, long seconds) {
            FileOutputStream out = null;
            FileInputStream in = null;
            java.nio.channels.FileLock lock = null;
            RandomAccessFile raf = null;
            long remainingSeconds;

            String deviceId = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.ANDROID_ID);
            Obfuscator obfuscator = new AESObfuscator(SALT, context.getPackageName(), deviceId);

            try {
                String filePath = context.getFilesDir() + File.separator + FREE_TRIAL_TIME_FILENAME;

                raf = new RandomAccessFile(filePath, "rw");

                lock = raf.getChannel().lock();

                FileDescriptor fd = raf.getFD();

                out = new FileOutputStream(fd);
                in = new FileInputStream(fd);

                seconds = Math.max(0, seconds);


                // Overwrite file
                raf.setLength(0);

                String lineWrite = obfuscator.obfuscate(String.valueOf(seconds), KEY_FREE_TRIAL_TIME_SECONDS);

                BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(out));

                writer.write(lineWrite);
                writer.close();
                out.flush();
            } catch (IOException e) {
                // do nothing
            } finally {
                if (raf != null) {
                    try {
                        raf.close();
                    } catch (IOException e) {
                        // do nothing
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        // do nothing
                    }
                }
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        // do nothing
                    }
                }
                if (lock != null) {
                    try {
                        lock.release();
                    } catch (IOException e) {
                        // do nothing
                    }
                }
            }
        }
    }
}
