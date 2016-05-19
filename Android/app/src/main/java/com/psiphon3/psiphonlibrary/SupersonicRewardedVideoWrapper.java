package com.psiphon3.psiphonlibrary;

import android.app.Activity;
import android.os.AsyncTask;

import com.google.android.gms.ads.identifier.AdvertisingIdClient;
import com.google.android.gms.common.GooglePlayServicesNotAvailableException;
import com.google.android.gms.common.GooglePlayServicesRepairableException;
import com.supersonic.mediationsdk.logger.SupersonicError;
import com.supersonic.mediationsdk.model.Placement;
import com.supersonic.mediationsdk.sdk.RewardedVideoListener;
import com.supersonic.mediationsdk.sdk.Supersonic;
import com.supersonic.mediationsdk.sdk.SupersonicFactory;

import java.io.IOException;

public class SupersonicRewardedVideoWrapper implements RewardedVideoListener {
    private boolean mIsInitialized = false;
    private Supersonic mMediationAgent;
    private  String mPlacement;
    private Activity mActivity;

    private AsyncTask mGAIDRequestTask;

    //Set the Application Key - can be retrieved from Supersonic platform
    private final String mAppKey = "49a684d5";

    public SupersonicRewardedVideoWrapper(Activity activity, String placement) {
        mPlacement = placement;
        mActivity = activity;
        mMediationAgent = SupersonicFactory.getInstance();
        mMediationAgent.setRewardedVideoListener(SupersonicRewardedVideoWrapper.this);
        initialize();
    }

    public void initialize() {
        if(mIsInitialized) {
            return;
        }

        if (mGAIDRequestTask != null && !mGAIDRequestTask.isCancelled()) {
            mGAIDRequestTask.cancel(false);
        }
        mGAIDRequestTask = new UserIdRequestTask().execute();
    }

    @Override
    public void onRewardedVideoInitSuccess() {

    }

    @Override
    public void onRewardedVideoInitFail(SupersonicError supersonicError) {

    }

    @Override
    public void onRewardedVideoAdOpened() {

    }

    @Override
    public void onRewardedVideoAdClosed() {

    }

    public void  playVideo() {
        if(mIsInitialized && mMediationAgent.isRewardedVideoAvailable()) {
            mMediationAgent.showRewardedVideo();
        }
    }

    @Override
    public void onVideoAvailabilityChanged(boolean available) {
        if(available){
//            mMediationAgent.showRewardedVideo(mPlacement);
        }
    }

    @Override
    public void onVideoStart() {

    }

    @Override
    public void onVideoEnd() {

    }

    @Override
    public void onRewardedVideoAdRewarded(Placement placement) {

    }

    @Override
    public void onRewardedVideoShowFail(SupersonicError supersonicError) {

    }

    private final class UserIdRequestTask extends AsyncTask<Void, Void, String> {

        @Override
        protected String doInBackground(Void... params) {
            try {
                String GAID = AdvertisingIdClient.getAdvertisingIdInfo(SupersonicRewardedVideoWrapper.this.mActivity).getId();
                return new String("unique_user_id");
            } catch (final IOException e) {

            } catch (final GooglePlayServicesNotAvailableException e) {

            } catch (final GooglePlayServicesRepairableException e) {

            }
            return null;
        }

        @Override
        protected void onPostExecute(String GAID) {
            if (GAID != null) {
                mMediationAgent.initRewardedVideo(mActivity, SupersonicRewardedVideoWrapper.this.mAppKey, GAID);
                SupersonicRewardedVideoWrapper.this.mIsInitialized = true;
            }
        }
    }

    public void onPause() {
        if (mMediationAgent != null) {
            mMediationAgent.onPause(mActivity);
        }
    }
    public void onResume() {
        if (mMediationAgent != null) {
            mMediationAgent.onResume(mActivity);
        }
    }
}

