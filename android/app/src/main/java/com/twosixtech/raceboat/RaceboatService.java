package com.twosixtech.raceboat;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
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