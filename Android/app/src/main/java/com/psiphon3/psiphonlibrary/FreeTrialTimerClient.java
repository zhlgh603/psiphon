package com.psiphon3.psiphonlibrary;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;


public class FreeTrialTimerClient {
    // Messenger for communicating with service.
    Messenger mServiceMessenger = null;

    // Target we publish to send messages to IncomingHandler.
    final Messenger mClientMessenger = new Messenger(new IncomingHandler(this));

    boolean m_isBound;
    Context m_context;

    //code expected to be seen in msg.what from service
    final public int m_msgClientUpdateCode;

    //timer update interval from service when timer is running
    final public int m_updateInterval;

    private ServiceConnection mConnection;

    // queue of client messages that
    // will be sent to Service once client is connected
    private final List<Message> m_queue;

    private TimerUpdateListener m_listener;

    public FreeTrialTimerClient(Context context, int clientUpdateCode, int updateInterval) {
        m_msgClientUpdateCode = clientUpdateCode;
        m_updateInterval = updateInterval;
        m_context = context;
        m_queue = new ArrayList<>();
        m_listener = null;

        mConnection = new android.content.ServiceConnection() {
            public void onServiceConnected(ComponentName className,
                                           IBinder service) {
                mServiceMessenger = new Messenger(service);
                /** Send all pending messages to the newly created Service. **/
                synchronized (m_queue) {
                    for (Message message : m_queue) {
                        try {
                            mServiceMessenger.send(message);
                        } catch (RemoteException e) {

                        }
                    }
                    m_queue.clear();
                }
            }

            public void onServiceDisconnected(ComponentName className) {
                // This is called when the connection with the service has been
                // unexpectedly disconnected -- that is, its process crashed.
                mServiceMessenger = null;
            }
        };
    }


    public void setTimerUpdateListener(TimerUpdateListener l) {
        m_listener = l;
    }

    public interface TimerUpdateListener {
        void onTimerUpdateSeconds(long seconds);
    }


    //Handler of incoming messages from service.
    static class IncomingHandler extends Handler {
        private final WeakReference<FreeTrialTimerClient> mTarget;

        IncomingHandler(FreeTrialTimerClient target) {
            mTarget = new WeakReference<>(target);
        }

        @Override
        public void handleMessage(Message msg) {
            FreeTrialTimerClient target = mTarget.get();
            if (target != null) {
                if (msg.what == target.m_msgClientUpdateCode) {
                    Long l = ((Number) msg.obj).longValue();
                    if (target.m_listener != null)
                        target.m_listener.onTimerUpdateSeconds(l);
                }
            } else {
                super.handleMessage(msg);
            }
        }
    }

    private final void postToService(final FreeTrialTimerService.MessageType type,
                                    int updateMessageType,
                                    int updateInterval)
    {
        postToService(type, updateMessageType, updateInterval, 0);
    }

    private final void postToService(final FreeTrialTimerService.MessageType type)
    {
        postToService(type, 0, 0, 0);
    }

    private final void postToService(final FreeTrialTimerService.MessageType type,
                                     final long seconds) {
        postToService(type, 0, 0, seconds);

    }

    private final void postToService(final FreeTrialTimerService.MessageType type,
                                     int updateMessageType,
                                     int updateInterval,
                                     long addSeconds) {
        /** Create a new message object. **/
        Object objAddSeconds = null;
        if (addSeconds > 0) {
            objAddSeconds = addSeconds;
        }
        Message message = Message.obtain(null,
                type.ordinal(),
                updateMessageType,
                updateInterval,
                objAddSeconds);

        message.replyTo = mClientMessenger;

        if (mServiceMessenger != null) {
            /** Service is running, so send message now. **/
            try {
                mServiceMessenger.send(message);
            } catch (RemoteException e) {

            }

        } else {
            /**
             * Service is not running, so queue message (to send later) and
             * then start the service.
             */
            synchronized (m_queue) {
                m_queue.add(message);
            }
            doBindService();
        }
    }

    private void doBindService() {
        if (m_context != null) {
            Intent intent = new Intent(m_context,
                    FreeTrialTimerService.class);

            m_context.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
            m_isBound = true;
        }
    }

    public void doUnbind() {
        if (m_isBound) {
            if (m_context != null) {
                m_context.unbindService(mConnection);
            }
            m_isBound = false;
        }
    }

    //Client messages
    public void registerForTimeUpdates() {
        postToService(FreeTrialTimerService.MessageType.MSG_CLIENT_REGISTER,
                m_msgClientUpdateCode, m_updateInterval);
    }

    public void unregisterFromTimeUpdates() {
        postToService(FreeTrialTimerService.MessageType.MSG_CLIENT_UNREGISTER);
    }

    public void requestStartTimer() {
        postToService(FreeTrialTimerService.MessageType.MSG_CLIENT_START_TIMER);
    }

    public void requestStopTimer() {
        postToService(FreeTrialTimerService.MessageType.MSG_CLIENT_STOP_TIMER);
    }

    public void requestAddTimeSeconds(long seconds) {
        postToService(FreeTrialTimerService.MessageType.MSG_CLIENT_ADD_TIME_SECONDS, seconds);
    }
}