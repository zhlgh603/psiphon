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

public class SupersonicRewardedVideoAd implements RewardedVideoListener {
    private Supersonic mMediationAgent;
    private  String mPlacement;
    private AsyncTask mUserIdRequestTask;

    //Set the Application Key - can be retrieved from Supersonic platform
    private final String mAppKey = "49a684d5";

    public SupersonicRewardedVideoAd(String placement) {
        mPlacement = placement;
        mMediationAgent = SupersonicFactory.getInstance();
    }

    public void requestAd(Activity activity) {
        if (mUserIdRequestTask != null && !mUserIdRequestTask.isCancelled()) {
            mUserIdRequestTask.cancel(false);
        }
        mUserIdRequestTask = new UserIdRequestTask(activity).execute();
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

    @Override
    public void onVideoAvailabilityChanged(boolean available) {
        if(available){
            mMediationAgent.showRewardedVideo(mPlacement);
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
        private final Activity mActivity;

        UserIdRequestTask(Activity activity) {
            mActivity = activity;
        }

        @Override
        protected String doInBackground(Void... params) {
            try {
                return AdvertisingIdClient.getAdvertisingIdInfo(mActivity).getId();
            } catch (final IOException e) {

            } catch (final GooglePlayServicesNotAvailableException e) {

            } catch (final GooglePlayServicesRepairableException e) {

            }
            return null;
        }

        @Override
        protected void onPostExecute(String userId) {
            if (userId != null) {
                mMediationAgent.setRewardedVideoListener(SupersonicRewardedVideoAd.this);
                mMediationAgent.initRewardedVideo(mActivity, SupersonicRewardedVideoAd.this.mAppKey, userId);
            }
        }
    }
}

