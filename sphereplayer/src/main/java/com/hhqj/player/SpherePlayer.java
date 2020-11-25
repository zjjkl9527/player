package com.hhqj.player;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

public class SpherePlayer  {
    private final SphereSurfaceView mSphereSurfaceView;
    private final ArrayBlockingQueue<byte[]> mVideoFrameQueue = new ArrayBlockingQueue<>(100);
    private volatile boolean mRunnable = false;
    private  AtomicBoolean mIsPlaying = new AtomicBoolean(false);
    private final AtomicBoolean gyroFlag= new AtomicBoolean(true);
    private final AtomicBoolean gyroEnable = new AtomicBoolean(false);
    private PollRTMPFrameThread mPollRTMPFrameThread;
    private VideoBufferThread mVideoBufferThread;
    private VideoDecoderThread mVideoDecoderThread;
    private MediaCodec mVideoDecoder;
    private Demuxer mDemuxer;
    private Context mContext;
    private String mUrl;
    private float mStartX;
    private float mStartY;
    private float mLastDistance;
    private float mPreviousXs, mPreviousYs;
    private float mTimeStamp;
    private final float[] mAngle = new float[3];
    private static final double NS2S = 1.0 / 1000000000.0;

    private ReentrantLock mEventListenerLock = new ReentrantLock();
    private EventListener mEventListener = (type, message) -> {
    };

    static final public int ERROR_SOURCE = 0;
    static final public int ERROR_CODEC = 1;


    public SpherePlayer(@NonNull SphereSurfaceView sphereSurfaceView) {
        mSphereSurfaceView = sphereSurfaceView;
    }

    public void setEventListener(EventListener eventListener){
        if(eventListener != null){
            mEventListenerLock.lock();
            mEventListener = eventListener;
            mEventListenerLock.unlock();
        }
    }

    public interface EventListener {
        void onError(int type, String message);
    }


    public void play(@NonNull String url) {
        if(mIsPlaying.get()) return;
        mIsPlaying.set(true);
        mRunnable = true;
        mUrl = url;
        mDemuxer = new Demuxer();
        if(!initMediaCodec()) return;
        setTouchEvent();
        mPollRTMPFrameThread = new PollRTMPFrameThread();
        mVideoBufferThread = new VideoBufferThread();
        mVideoDecoderThread = new VideoDecoderThread();
        mPollRTMPFrameThread.start();
        mVideoBufferThread.start();
        mVideoDecoderThread.start();
    }


    public void stop() {
        StopThread stopThread = new StopThread();
        stopThread.start();
        try {
            stopThread.join();
        }catch (InterruptedException e){
        }
    }

    public boolean isPlaying(){
        return  mIsPlaying.get();
    }

    private final SensorEventListener mSensorListener = new SensorEventListener() {

        @Override
        public void onSensorChanged(SensorEvent event) {
            if(gyroEnable.get()){
                if (event.sensor.getType() == Sensor.TYPE_GYROSCOPE) {
                    if (mTimeStamp != 0) {
                        final double dT = (event.timestamp - mTimeStamp) * NS2S;
                        mAngle[0] += event.values[0] * dT;
                        mAngle[1] += event.values[1] * dT;
                        float angleX = (float) Math.toDegrees(mAngle[0]);
                        float angleY = (float) Math.toDegrees(mAngle[1]);
                        float dx = angleX - mPreviousXs;
                        float dy = angleY - mPreviousYs;
                        if(mContext.getResources().getConfiguration().orientation==1){
                            mSphereSurfaceView.getSphereRender().rotateX(dx);
                            mSphereSurfaceView.getSphereRender().rotateY(-dy);
                        }
                        if(mContext.getResources().getConfiguration().orientation==2){
                            mSphereSurfaceView.getSphereRender().rotateX(-dy);
                            mSphereSurfaceView.getSphereRender().rotateY(-dx);
                        }
                        mSphereSurfaceView.requestRender();

                        mPreviousYs = angleY;
                        mPreviousXs = angleX;
                    }
                    mTimeStamp = event.timestamp;
                }
            }
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
        }
    };

    public void setGyroEnable(boolean enable, @NonNull Context context){
        mContext = context;
        SensorManager sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        Sensor sensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        if(enable){
            sensorManager.registerListener(mSensorListener, sensor, 0, 0);
        }else {
            sensorManager.unregisterListener(mSensorListener, sensor);
        }
        gyroEnable.set(enable);
    }

    public boolean isGyroEnable(){
        return gyroEnable.get();
    }


    @SuppressLint("ClickableViewAccessibility")
    private void setTouchEvent(){
        mSphereSurfaceView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getPointerCount()) {
                    case 1:
                        switch (event.getAction() & MotionEvent.ACTION_MASK) {
                            case MotionEvent.ACTION_DOWN:
                                mStartX = event.getX();
                                mStartY = event.getY();
                                break;

                            case MotionEvent.ACTION_MOVE:
                                float x = event.getX();
                                float y = event.getY();
                                float deltaX = mStartX - x;
                                float deltaY = mStartY - y;

                                mStartX = x;
                                mStartY = y;

                                mSphereSurfaceView.getSphereRender().rotateY(0.05f * deltaX);
                                mSphereSurfaceView.getSphereRender().rotateX(-0.05f * deltaY);
                                mSphereSurfaceView.requestRender();
                                break;
                            case MotionEvent.ACTION_UP:
                                break;
                        }

                        break;

                    case 2:
                        switch (event.getAction() & MotionEvent.ACTION_MASK) {
                            case MotionEvent.ACTION_POINTER_DOWN: {
                                float deltaX = event.getX(0) - event.getX(1);
                                float deltaY = event.getY(0) - event.getY(1);
                                mLastDistance = (float) Math.pow((deltaX * deltaX + deltaY * deltaY), 0.5);
                            }
                            break;

                            case MotionEvent.ACTION_MOVE: {
                                float deltaX = event.getX(0) - event.getX(1);
                                float deltaY = event.getY(0) - event.getY(1);
                                float distance = (float) Math.pow((deltaX * deltaX + deltaY * deltaY), 0.5);

                                float deltaDistance = distance - mLastDistance;
                                mLastDistance = distance;

                                mSphereSurfaceView.getSphereRender().zoom(-0.05f * deltaDistance);
                                mSphereSurfaceView.requestRender();
                            }
                            break;
                            case MotionEvent.ACTION_UP:
                                break;
                        }
                        break;

                    default:
                        break;
                }
                return true;
            }
        });
    }




    private boolean initMediaCodec(){
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                mVideoDecoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            }else {
                mVideoDecoder=MediaCodec.createDecoderByType("video/avc");
            }

            MediaFormat videoFormat ;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                videoFormat = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1920, 1080);
            }else {
                videoFormat = MediaFormat.createVideoFormat("video/avc", 1920, 1080);
            }

            mVideoDecoder.configure(videoFormat,mSphereSurfaceView.getSphereRender().getSurface(),null,0);
            mVideoDecoder.start();
        } catch (Exception e) {
            new StopThread(ERROR_CODEC, e.getMessage()).start();
            return false;
        }

        return  true;
    }

    private class PollRTMPFrameThread extends Thread {
        @Override
        public void run() {
            try {
                mDemuxer.open(mUrl, new Demuxer.Callback() {
                    @Override
                    public void onVideo(byte[] frame, long pts) {
                        if (mVideoFrameQueue.remainingCapacity() > 0) {
                            mVideoFrameQueue.offer(frame);
                        }
                    }
                    @Override
                    public void onAudio(byte[] frame, long pts) {
                    }
                });

                while (mRunnable) {
                    mDemuxer.flush();
                }
            } catch (IOException e) {
                new StopThread(ERROR_SOURCE, e.getMessage()).start();
            }
         }
    }


    private class VideoBufferThread extends Thread {
        @Override
        public void run() {
            try {
                while (mRunnable) {
                    int inIndex = mVideoDecoder.dequeueInputBuffer(33333);
                    if (inIndex < 0) continue;
                    byte[] data = mVideoFrameQueue.take();
                    mVideoFrameQueue.remove(data);

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        ByteBuffer inputBuffer = mVideoDecoder.getInputBuffer(inIndex);
                        inputBuffer.put(data, 0, data.length);
                    } else {
                        ByteBuffer[] inputBuffers = mVideoDecoder.getInputBuffers();
                        ByteBuffer inputBuffer = inputBuffers[inIndex];
                        inputBuffer.put(data, 0, data.length);
                    }
                    mVideoDecoder.queueInputBuffer(inIndex, 0, data.length, 0, 0);
                }
            }catch (InterruptedException e){
            } catch (Exception e) {
                new StopThread(ERROR_CODEC, e.getMessage()).start();
            }
        }
    }

     private class VideoDecoderThread extends Thread {
        @Override
        public void run() {
            try {
                while (mRunnable) {
                    long startTime = System.currentTimeMillis();
                    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                    int outIndex = mVideoDecoder.dequeueOutputBuffer(info, 0);
                    if (outIndex < 0) continue;
                    mVideoDecoder.releaseOutputBuffer(outIndex, true);
                    mSphereSurfaceView.requestRender();

                    long endTime = System.currentTimeMillis();
                    long delta = endTime - startTime;

                    if (delta < 33) {
                        Thread.sleep(33 - delta);
                    }
                }
            }catch (InterruptedException e){
                e.printStackTrace();
            }
            catch (Exception e) {
                new StopThread(ERROR_CODEC, e.getMessage()).start();
            }
        }
    }

    private class StopThread extends Thread{
        int errorType;
        String errorMessage;

        public StopThread() {}

        public StopThread(int type, String message) {
            errorType = type;
            errorMessage = message;
        }

        @Override
        public void run() {
            super.run();
            mRunnable = false;

            if(mVideoBufferThread != null){
                try {
                    mVideoDecoderThread.interrupt();
                    mVideoDecoderThread.join();
                }catch (Exception e){}
            }

            if(mVideoBufferThread != null){
                try {
                    mVideoBufferThread.interrupt();
                    mVideoBufferThread.join();
                }catch (Exception e){}
            }

            if(mPollRTMPFrameThread != null){
                try {
                    mPollRTMPFrameThread.interrupt();
                    mPollRTMPFrameThread.join();
                }catch (Exception e){}
            }

            if(mVideoDecoder != null){
                try {
                    mVideoDecoder.stop();
                    mVideoDecoder.release();
                }catch (Exception e){}
            }
            mVideoDecoder = null;
            mDemuxer = null;
            mSphereSurfaceView.setOnTouchListener(null);

            if(mContext != null) {
                SensorManager sensorManager = (SensorManager) mContext.getSystemService(Context.SENSOR_SERVICE);
                Sensor sensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
                sensorManager.unregisterListener(mSensorListener, sensor);
            }
            gyroFlag.set(false);

            mVideoFrameQueue.clear();
            mIsPlaying.set(false);
            if(errorMessage != null){
                EventListener listener;

                mEventListenerLock.lock();
                listener = mEventListener;
                mEventListenerLock.unlock();

                listener.onError(errorType, errorMessage);
            }
        }
    }



    private static class Demuxer {

        public Demuxer(){
            mPointer = createDemuxer();
        }

        public void open(String url, com.hhqj.player.SpherePlayer.Demuxer.Callback callback) throws IOException {
            int ret =openInput(mPointer, url, callback);
            if(ret < 0) throw new IOException(getErrorString(ret));
        }

        public void flush() throws IOException{
            int ret = flush(mPointer);
            if(ret < 0) throw new IOException(getErrorString(ret));
        }

        @Override
        public void finalize() throws Throwable {
            super.finalize();
            destroyDemuxer(mPointer);
        }

        private final long mPointer;

        public interface Callback{
            void onVideo(byte[] frame, long pts);

            void onAudio(byte[] frame, long pts);
        }

        static {
            System.loadLibrary("native-lib");
        }

        private native long createDemuxer();

        private native void destroyDemuxer(long pointer);

        private native int openInput(long pointer, String url, com.hhqj.player.SpherePlayer.Demuxer.Callback callback);

        private native int flush(long pointer);

        private native String getErrorString(int value);

    }


}


