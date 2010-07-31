#include <android/log.h>
#include "decoder_audio.h"

#include <drivers_map.h>

#define TAG "FFMpegAudioDecoder"

static DecoderAudio* sInstance;

DecoderAudio::DecoderAudio(AVCodecContext*            codec_ctx,
						   struct DecoderAudioConfig* config)
{
    mQueue = new PacketQueue();
    mCodecCtx = codec_ctx; 
    mConfig = config;
    sInstance = this;
}

DecoderAudio::~DecoderAudio()
{
    if(mDecoding)
    {
        stop();
    }
}

bool DecoderAudio::prepare(const char *err)
{
    mSamplesSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    mSamples = (int16_t *) av_malloc(mSamplesSize);

    if(AudioDriver_set(mConfig->streamType,
                       mConfig->sampleRate,
                       mConfig->format,
                       mConfig->channels) != ANDROID_AUDIOTRACK_RESULT_SUCCESS) {
       err = "Couldnt' set audio track";
       return false;
    }
    if(AudioDriver_start() != ANDROID_AUDIOTRACK_RESULT_SUCCESS) {
       err = "Couldnt' start audio track";
       return false;
    }
    return true;
}

bool DecoderAudio::process(AVPacket *packet)
{
    int size = mSamplesSize;
    int len = avcodec_decode_audio3(mCodecCtx, mSamples, &size, packet);
    if(AudioDriver_write(mSamples, size) <= 0) {
        return false;
    }
    return true;
}

bool DecoderAudio::decode(void* ptr)
{
    AVPacket        pPacket;

    __android_log_print(ANDROID_LOG_INFO, TAG, "decoding audio");

    mDecoding = true;
    while(mDecoding)
    {
        if(mQueue->get(&pPacket, true) < 0)
        {
            mDecoding = false;
            return false;
        }
        if(!process(&pPacket))
        {
            mDecoding = false;
            return false;
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&pPacket);
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "decoding audio ended");

    AudioDriver_unregister();

    // Free audio samples buffer
    av_free(mSamples);
    return true;
}
