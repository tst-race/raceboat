package com.twosixtech.raceboat;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.IBinder;
import android.util.Log;

import java.io.File;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;


import androidx.core.app.NotificationCompat;
import androidx.core.app.ServiceCompat;

public class RaceboatService extends Service {
    private static final String TAG = "RaceboatService";
    private static final String SERVICE_CHANNEL_ID = "com.twosix.raceboat.SERVICE_CHANNEL";
    private final Executor executor = Executors.newSingleThreadExecutor();
    private NotificationManagerCompat notificationManager;

    public native int main();
    private static final String[] NATIVE_LIBS = {
//            "yaml-cpp",
//            "stdc++",
            "c++_shared",
            "boost_system",
            "boost_filesystem",
            "ffi",
            "python3.7m",
            "crypto",
            "ssl",
            "thrift",
            "zip",
            "archive"
    };


    private static void loadNativeLibs() {
        for (String lib : NATIVE_LIBS) {
            try {
                Log.v(TAG, "Loading " + lib);
                System.loadLibrary(lib);
                Log.d(TAG, "Loaded " + lib);
            } catch (Error err) {
                Log.e(TAG, "Error loading native lib " + lib, err);
                throw err;
            }
        }
    }
    static {
        loadNativeLibs();
        System.loadLibrary("raceSdkCommon");
        System.loadLibrary("raceboat-driver");
    }

//    public RaceboatService() {
//        main();
//    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "Binding Raceboat Service");
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public void onCreate() {
        Log.d(TAG, "Creating Raceboat service");
        super.onCreate();
        notificationManager = NotificationManagerCompat.from(this);
        NotificationChannel channel =
                new NotificationChannel(
                        SERVICE_CHANNEL_ID, "Raceboat Service", NotificationManager.IMPORTANCE_MIN);
        notificationManager.createNotificationChannel(channel);

        extractPythonPackages();
        extractPlugins();
    }

    private void extractPythonPackages() {
        File appDataDir = getDataDir();
        File pythonDir = new File(appDataDir, "python3.7");
        if (pythonDir.isDirectory()) {
            return; // Already extracted
        }
        try {
            Log.d(TAG, "Beginning extraction of python packages");
            AndroidFileSystemHelpers.extractTar(
                    "python-packages-3.7.16-3-android-arm64-v8a.tar",
                    pythonDir.getAbsolutePath() + "/",
                    getApplicationContext()
            );
            Log.d(TAG, "Completed extraction of python packages");

            Log.d(TAG, "Beginning extraction of python bindings");
            AndroidFileSystemHelpers.copyAssetDir("python", "python3.7", getApplicationContext());
        } catch (Exception e) {
            Log.e(TAG, "Error extracting python packages", e);
        }
    }

    private void extractPlugins() {
        try {
            Log.d(TAG, "Beginning extraction of plugins");
            AndroidFileSystemHelpers.copyAssetDir("plugins", getApplicationContext());
        } catch (Exception e) {
            Log.e(TAG, "Error extracting plugins", e);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "Starting Raceboat service");

        Intent launchIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingLaunchIntent =
                PendingIntent.getActivity(this, 0, launchIntent, PendingIntent.FLAG_IMMUTABLE);

        Notification notification =
                new NotificationCompat.Builder(this, SERVICE_CHANNEL_ID)
                        .build();
        startForeground(1, notification);
        executor.execute(() -> main());

        return START_NOT_STICKY;
    }
}