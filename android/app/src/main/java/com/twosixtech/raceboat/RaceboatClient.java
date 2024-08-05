package com.twosixtech.raceboat;

import android.content.Context;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.google.android.material.tabs.TabLayout;

import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.OptionalInt;

public class RaceboatClient {
    private LocalSocket clientSocket;
    private Context context;
    private Handler uiHandler;

    public RaceboatClient(Context context) {
        this.context = context;
        uiHandler = new Handler(Looper.getMainLooper());
    }

    public void connectToServer(String mode, String sendChannel, String sendAddress, String recvChannel, String recvAddress, String additionalParameters) {
        try {
            clientSocket = createSocket();
            // Send the mode to the server
            Log.i("RaceboatClient", "Sending Mode " + mode);
            sendMessage(mode);
            Log.i("RaceboatClient", "Sending next " + sendChannel);
            sendMessage(sendChannel);
            Log.i("RaceboatClient", "Sending next " + sendAddress);
            sendMessage(sendAddress);
            Log.i("RaceboatClient", "Sending next " + recvChannel);
            sendMessage(recvChannel);
            Log.i("RaceboatClient", "Sending next " + recvAddress);
            sendMessage(recvAddress);
            Log.i("RaceboatClient", "Sending next " + additionalParameters);
            sendMessage(additionalParameters);
            // Start communication in a separate thread
            //clientSocket.close();
            //startCommunicationThread();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void sendMessage(String message) {
        try {
            OutputStream outputStream = clientSocket.getOutputStream();
            ByteBuffer length = ByteBuffer.allocate(4);
            length.order(ByteOrder.LITTLE_ENDIAN);
            length.putInt(message.length());
            outputStream.write(length.array());
            outputStream.write(message.getBytes());
            outputStream.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public String receiveMessage() {
        try {
            InputStream inputStream = clientSocket.getInputStream();
            List<Byte> buffer = new ArrayList<>();
            int data;
            while ((data = inputStream.read()) != -1) {
                buffer.add((byte) data);
            }
            byte[] byteArray = new byte[buffer.size()];
            for (int i = 0; i < buffer.size(); i++) {
                byteArray[i] = buffer.get(i);
            }
            return new String(byteArray);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    private LocalSocket createSocket() throws Exception {
        LocalSocket socket = new LocalSocket(LocalSocket.SOCKET_STREAM);
        LocalSocketAddress address = new LocalSocketAddress(
                context.getFilesDir().getPath() + "/Raceboat/raceboat_socket.sock",
                LocalSocketAddress.Namespace.ABSTRACT);
        address = new LocalSocketAddress("RaceboatLocalSocket");
        Log.i("RaceboatClient", "createSocket: address: " + address.getName());
        socket.connect(address);
        return socket;
    }

    private void startCommunicationThread() {
        // Define a new Runnable implementation
        Runnable communicationTask = new Runnable() {
            @Override
            public void run() {
                try {
                    // Perform socket communication operations here
                    // For example, sending and receiving messages
                    while (true) {
                        // Receive message from the server
                        String response = receiveMessage();
                        if (response != null) {
                            // Update UI with the received message
                            uiHandler.post(new Runnable() {
                                @Override
                                public void run() {
                                    // Update the UI elements here
                                    // For example, set text to a TextView
                                    // responseTextView.setText(response);
                                    // not sure I need to put anything here as I think the main activity will handle
                                    // if not need to update this
                                }
                            });
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        };

        // Create a new Thread with the Runnable task
        Thread thread = new Thread(communicationTask);

        // Start the thread
        thread.start();
    }
}

