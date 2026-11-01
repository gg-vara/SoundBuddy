// SoundBuddy
// Small companion app that feeds SoundVitrine with ITunes / Music library metadata
// Copyright (C) 2019-2023 Guillaume Vara <guillaume.vara@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Any graphical or audio resources available within the source code may
// use a different license and copyright : please refer to their metadata
// for further details. Resources without explicit references to a
// different license and copyright still refer to this GPL.

#pragma once

#include <QThread>
#include <QObject>
#include <QDebug>
#include <QMutex>
#include <functional>

#include "src/helpers/Defer.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/opt.h>
    #include <libavutil/audio_fifo.h>
    #include <libswresample/swresample.h>
    #include "src/helpers/HashHelper.hpp"
    #include "DASHCreationOrder.hpp"
}

class DASHThread : public QThread {
    Q_OBJECT

    public:
        DASHThread(QObject* parent = nullptr) : QThread(parent) {}

        void doStreamNewFile(const QString &filepathToStream, const QString &fileHash) {
            //
            if (_creationOrder.isSame(fileHash)) return;

            //
            _hasAwaitingNewFileToStream = true;
            _creationOrder.replaceOrder(filepathToStream, fileHash);

            //
            emit _hasNewFileToRead(QPrivateSignal());
        }

        void quit() {
            _hasAwaitingNewFileToStream = true; // exits loop of packet reading
            QThread::quit();
        }

    signals:
        void errorOccurred(const QString& error);
        void ready();
        void _hasNewFileToRead(QPrivateSignal);
        
        void muxStarted();
        void muxProgress(int64_t at, int64_t of);
        void muxEnding();
        void muxDone();

    private:
        bool _hasAwaitingNewFileToStream = false;
        QMutex _m;

        // latest file path of a file we want to stream
        DASHCreationOrder _creationOrder;

        ///
        const AVCodec* _inputCodec = nullptr; // codec found that could read latest file
        AVCodecContext* _inputCodecContext = nullptr;
        AVFormatContext* _inputContext = nullptr;

        const AVCodec* _outputCodec = nullptr;
        AVCodecContext* _outputCodecContext = nullptr;
        AVFormatContext* _outputContext = nullptr;
        AVDictionary* _outputOptions = nullptr;
        AVStream* _outputStream = nullptr;

        SwrContext *_swrContext = nullptr;

       static void _tryCleanupCacheDir() {
            try {
                QDir dir(DASHCreationOrder::getPathToDashTempFolder());
                dir.setFilter(QDir::NoDotAndDotDot);
                dir.removeRecursively();
            } catch(...) {}
        }

        static constexpr int cacheSubdirLimit = 20;
        static void _mayTidyUpCacheDir() {
            try {
                QDir dir(DASHCreationOrder::getPathToDashTempFolder());
                dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
                if(!dir.exists()) return;

                // Get a list of all subfolders
                auto folders = dir.entryInfoList();

                // If there are fewer subfolders, no need to delete anything
                if (folders.size() <= cacheSubdirLimit) {
                    return;
                }

                // inplace sort folders by creation date (oldest first)
                std::sort(folders.begin(), folders.end(), [](const QFileInfo &a, const QFileInfo &b) {
                    return a.birthTime() < b.birthTime();
                });

                // Calculate how many folders to delete
                int foldersToDelete = folders.size() - cacheSubdirLimit;

                // Delete the oldest folders
                for (int i = 0; i < foldersToDelete; ++i) {
                    QDir folderToDelete(folders[i].absoluteFilePath());
                    if (folderToDelete.removeRecursively()) {
                        qDebug() << "Deleted folder:" << folders[i].absoluteFilePath();
                    } else {
                        qDebug() << "Failed to delete folder:" << folders[i].absoluteFilePath();
                    }
                }

            } catch(...) {}
        }

        void _prepareContextsAndCodecs(std::function<void(int audioStreamIndex)> then) {
            Defer deferred;

            //
            // OPEN FILE and read metadata (important !)
            //

            if (avformat_open_input(&_inputContext, _creationOrder.path().toStdString().c_str(), nullptr, nullptr) != 0) {
                emit errorOccurred("Couldn't open input file.");
                return;
            }
            deferred.defer([this] { 
                avformat_free_context(_inputContext);
                _inputContext = nullptr;
            });

            // Find the stream info (Required)
            if(avformat_find_stream_info(_inputContext, nullptr) < 0) {
                emit errorOccurred("Could not find stream info");
                return;
            }

            //
            // GET FILE CODEC & audio stream context
            //
            
            int audioStreamIndex = av_find_best_stream(_inputContext, AVMEDIA_TYPE_AUDIO, -1, -1, &_inputCodec, 0); // wont work without a previous avformat_find_stream_info
            if (_inputCodec == 0) {
                emit errorOccurred("No codec could be infered to read file");
                return;
            } else if (audioStreamIndex < 0 ) {
                emit errorOccurred("Could not find audio stream");
                return;
            }

            //
            // SETUP input codec context from file context
            //

            _inputCodecContext = avcodec_alloc_context3(_inputCodec);
            if (!_inputCodecContext) {
                emit errorOccurred("Could not allocate codec context");
                return;
            }
            deferred.defer([this] { 
                avcodec_free_context(&_inputCodecContext); 
                _inputCodecContext = nullptr;
            });

            if(avcodec_parameters_to_context(_inputCodecContext, _inputContext->streams[audioStreamIndex]->codecpar) < 0) {
                emit errorOccurred("Could not copy codec parameters to context");
                return;
            }
            // _inputCodecContext->pkt_timebase = _inputContext->streams[audioStreamIndex]->time_base; // important !

            // Open input codec then feed it to muxer
            if (avcodec_open2(_inputCodecContext, _inputCodec, nullptr) < 0) {
                emit errorOccurred("Could not open codec");
                return;
            }
            deferred.defer([this] { avcodec_free_context(&_inputCodecContext); });

            //
            // CONFIGURE output stream and context
            //
            
            // create dirty file
            _creationOrder.mayGenerateDirtyFile();

            //
            avformat_alloc_output_context2(
                &_outputContext, 
                nullptr, 
                // "opus",  _creationOrder.getPathWithDestFolder("out.opus").toStdString().c_str()
                "dash", _creationOrder.getPathWithDestFolder("manifest.mpd").toStdString().c_str()
            );
            if (!_outputContext) {
                emit errorOccurred("Couldn't create output context for DASH.");
                return;
            }
            deferred.defer([this] { 
                avformat_free_context(_outputContext);
                _outputContext = nullptr;
            });

            //
            _outputStream = avformat_new_stream(_outputContext, nullptr);
            deferred.defer([this] { _outputStream = nullptr; }); // _outputStream would be cleared by avformat_free_context
            if (avcodec_parameters_from_context(_outputStream->codecpar, _outputCodecContext) < 0) {
                emit errorOccurred("Could not copy codec parameters to stream");
                return;
            }
            // _outputStream->time_base = _outputCodecContext->time_base; // important !

            //
            // OPEN DASH FILE
            //

            if (!(_outputContext->oformat->flags & AVFMT_NOFILE)) {
                if (avio_open(&_outputContext->pb, _outputContext->url, AVIO_FLAG_WRITE) < 0) {
                    emit errorOccurred("Couldn't open DASH output file.");
                    return;
                }

                deferred.defer([this] (){ avio_close(_outputContext->pb); });
            }

            //
            // SETUP HEADER WITH OPTIONS
            //

            // Apply muxer options to the format context
            _outputContext->flags |= AVFMT_FLAG_CUSTOM_IO;
            
            //
            if (QString(_outputContext->oformat->name) == "dash") {
                //
                av_dict_set(&_outputOptions, 
                    "media_seg_name", 
                    (_creationOrder.getPathWithDestFolder("chunk-stream$RepresentationID$-$Number%05d$.$ext$")).toStdString().c_str(),
                0);

                //
                av_dict_set(&_outputOptions, 
                    "init_seg_name", 
                    (_creationOrder.getPathWithDestFolder("init-stream$RepresentationID$.$ext$")).toStdString().c_str(), 
                0);

                // av_dict_set(&_outputOptions, "seg_duration", "40", 0);
                deferred.defer([this] { av_dict_free(&_outputOptions); });
            }

            if(avformat_write_header(_outputContext, &_outputOptions) < 0) {
                emit errorOccurred("Couldn't write header.");
                return;
            };
            deferred.defer([this] { av_write_trailer(_outputContext); });

            //
            // Resampler configuration
            //

            _swrContext = swr_alloc();

            swr_alloc_set_opts2(&_swrContext, 
                &_outputCodecContext->ch_layout, _outputCodecContext->sample_fmt, _outputCodecContext->sample_rate,
                &_inputCodecContext->ch_layout, _inputCodecContext->sample_fmt, _inputCodecContext->sample_rate,
                0, nullptr
            );

            swr_init(_swrContext);
            deferred.defer([this] { 
                swr_free(&_swrContext);
            });

            //
            //
            //

            then(audioStreamIndex);
        }

        //
        void _doStreamNewerFile() {
            //
            // LOCK THREAD UNTIL FILE STREAMED
            //

            QMutexLocker lock(&_m);
            _hasAwaitingNewFileToStream = false;

            //
            //
            //
            
            _mayTidyUpCacheDir();

            auto then = std::bind(&DASHThread::_process, this, std::placeholders::_1);
            _prepareContextsAndCodecs(then);
        }

        AVPacket* __inputPacket;
        AVPacket* __outputPacket;
        AVFrame* __inputFrame;
        AVAudioFifo* __fifo;
        AVFrame* __passthroughOutputFrame;
        AVFrame* __outputFrame;

        void _process(int audioStreamIndex) {
            emit muxStarted();

            Defer deferred;
            int err;
            bool eof = false;
            bool flushed = false;

            __inputPacket = av_packet_alloc();
            __outputPacket = av_packet_alloc();
            deferred.defer([this] { 
                av_packet_free(&__inputPacket); 
                av_packet_free(&__outputPacket);
            });

            __inputFrame = av_frame_alloc();
            __inputFrame->format = _inputCodecContext->sample_fmt;
            __inputFrame->ch_layout = _inputCodecContext->ch_layout;
            __inputFrame->sample_rate = _inputCodecContext->sample_rate;
            __inputFrame->time_base = _inputCodecContext->time_base;
            __inputFrame->nb_samples = _inputCodecContext->frame_size;
            deferred.defer([this] { 
                av_frame_free(&__inputFrame);
            });

            __passthroughOutputFrame = av_frame_alloc();
            __passthroughOutputFrame->format = _outputCodecContext->sample_fmt;
            __passthroughOutputFrame->ch_layout = _outputCodecContext->ch_layout;
            __passthroughOutputFrame->sample_rate = _outputCodecContext->sample_rate;
            __passthroughOutputFrame->time_base = _outputCodecContext->time_base;
            __passthroughOutputFrame->nb_samples = _outputCodecContext->frame_size;
            av_frame_get_buffer(__passthroughOutputFrame, 0);
            deferred.defer([this] { 
                av_frame_free(&__passthroughOutputFrame);
            });

            __outputFrame = av_frame_alloc();
            __outputFrame->format = _outputCodecContext->sample_fmt;
            __outputFrame->ch_layout = _outputCodecContext->ch_layout;
            __outputFrame->sample_rate = _outputCodecContext->sample_rate;
            __outputFrame->time_base = _outputCodecContext->time_base;
            __outputFrame->nb_samples = _outputCodecContext->frame_size;
            av_frame_get_buffer(__outputFrame, 0);
            deferred.defer([this] { 
                av_frame_free(&__outputFrame);
            });

            int64_t pts = 0;
            int64_t delayedSamples = 0;
            int64_t writtenDur = 0;

            while (!flushed) {
                //
                if(_hasAwaitingNewFileToStream) return;

                // read input file __inputFrame... negative values means end of file
                err = av_read_frame(_inputContext, __inputPacket);

                //
                if (!err) {
                    // skip reading data if not the right stream index
                    if (__inputPacket->stream_index != audioStreamIndex) {
                        av_packet_unref(__inputPacket);
                        continue;
                    }

                    //
                    emit muxProgress(
                        av_rescale_q(__inputPacket->pts, _inputContext->streams[audioStreamIndex]->time_base, (AVRational){1, AV_TIME_BASE}), 
                        _inputContext->duration
                    );

                    // forward data to decoder, skip if failed
                    if(avcodec_send_packet(_inputCodecContext, __inputPacket) < 0) {
                        continue;
                    }

                    av_packet_unref(__inputPacket);

                } else {
                    eof = true;
                    emit muxEnding();
                    avcodec_send_packet(_inputCodecContext, NULL); // SEND FLUSH PACKET
                }

                // analyze __inputFrame
                while (!flushed) {
                    //
                    if(_hasAwaitingNewFileToStream) return;

                    //
                    err = avcodec_receive_frame(_inputCodecContext, __inputFrame);
                    if (err < 0 && !eof) {
                        break;
                    }

                    do {
                        //
                        if (__outputFrame->nb_samples == 0 && eof) {
                            flushed = true;
                        }

                        // output frame size is guaranteed to be at <= _outputCodecContext->frame_size
                        // passing NULL flushes the remaining samples into frame
                        swr_convert_frame(_swrContext, __outputFrame, __outputFrame->nb_samples == 0 && !flushed ? __inputFrame : NULL);
                        delayedSamples = swr_get_delay(_swrContext, _outputCodecContext->sample_rate);

                        auto remainingSpace = av_audio_fifo_space(__fifo);
                        auto willPostponeSamples = remainingSpace < __outputFrame->nb_samples;
                        auto samplesToWrite = willPostponeSamples ? remainingSpace : __outputFrame->nb_samples;
                        auto samplesToPostpone = willPostponeSamples ? __outputFrame->nb_samples - remainingSpace : 0;

                        //
                        av_audio_fifo_write(__fifo, (void *const *)__outputFrame->data, samplesToWrite);

                        // TODO: HANDLE LAST FRAME remaining samples, end by draining __fifo into frame
                        if (willPostponeSamples || flushed) {
                            exploit_fifo:
                            // fill passthrough frame from __fifo
                            av_audio_fifo_read(
                                __fifo, 
                                (void *const *)__passthroughOutputFrame->data, 
                                willPostponeSamples ? _outputCodecContext->frame_size : av_audio_fifo_size(__fifo)
                            );

                            // if not flushing, report remaining samples into __fifo
                            if (willPostponeSamples) {
                                av_audio_fifo_reset(__fifo); // clear __fifo
                                av_audio_fifo_write(__fifo, (void *const *)__outputFrame->data, __outputFrame->nb_samples); // write current frames into __fifo
                                av_audio_fifo_drain(__fifo, __outputFrame->nb_samples - samplesToPostpone); // remove passed frames of __fifo
                            }

                            //
                            // __passthroughOutputFrame->pts = pts;
                            // pts += __passthroughOutputFrame->nb_samples;

                            // skip if send failed
                            if(avcodec_send_frame(_outputCodecContext, __passthroughOutputFrame) < 0) {
                                emit errorOccurred("Cannot send frame");
                                return;
                            }

                            while (1) {
                                err = avcodec_receive_packet(_outputCodecContext, __outputPacket);
                                if (err != 0) {
                                    break;
                                }

                                //
                                if(_hasAwaitingNewFileToStream) return;

                                // required !
                                av_packet_rescale_ts(__outputPacket, _inputCodecContext->time_base, _outputStream->time_base);

                                // Write the encoded packet to the output file
                                av_interleaved_write_frame(_outputContext, __outputPacket);
                            }

                            if (willPostponeSamples && flushed) {
                                willPostponeSamples = false;
                                goto exploit_fifo;
                            }
                        }
                    } while (__outputFrame->nb_samples && !flushed);

                }
            }

            //
            if (flushed) {
                _creationOrder.mayRemoveDirtyFile();
            }
            
            //
            emit muxDone();
        }

    protected:
        void run() override {
            //
            _tryCleanupCacheDir();

            // Find the encoder for the audio stream (e.g., OPUS)
            _outputCodec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
            if (!_outputCodec) {
                emit errorOccurred("Codec not found");
                return;
            }

            Defer deferred;
            _outputCodecContext = avcodec_alloc_context3(_outputCodec);
            if (!_outputCodecContext) {
                emit errorOccurred("Could not allocate codec context");
                return;
            }
            deferred.defer([this] { avcodec_free_context(&_outputCodecContext); });

            av_channel_layout_default(&_outputCodecContext->ch_layout, 2);
            deferred.defer([this] { av_channel_layout_uninit(&_outputCodecContext->ch_layout); });

            //
            const void* samplesFormatsAvailable = nullptr;
            int samplesFormatsAvailableC = 0;
            avcodec_get_supported_config(NULL, _outputCodec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &samplesFormatsAvailable, &samplesFormatsAvailableC);

            const void* sampleratesAvailable = nullptr;
            int sampleratesAvailableC = 0;
            avcodec_get_supported_config(NULL, _outputCodec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &sampleratesAvailable, &sampleratesAvailableC);

            // Set codec parameters, including the sample rate
            _outputCodecContext->sample_rate = ((const int*)sampleratesAvailable)[0];                   // Opus usually works at 48kHz
            _outputCodecContext->bit_rate = 64000;                                                      // Set target bitrate
            _outputCodecContext->sample_fmt =((const AVSampleFormat*)samplesFormatsAvailable)[0];       // Set the sample format
            _outputCodecContext->time_base = (AVRational){ 1, _outputCodecContext->sample_rate };

            if (avcodec_open2(_outputCodecContext, _outputCodec, nullptr) < 0) {
                emit errorOccurred("Could not open codec");
                return;
            }
            deferred.defer([this] { avcodec_free_context(&_outputCodecContext); });

            //
            __fifo = av_audio_fifo_alloc(
                 _outputCodecContext->sample_fmt,
                 _outputCodecContext->ch_layout.nb_channels,
                 _outputCodecContext->frame_size
            );
            deferred.defer([this] { av_audio_fifo_free(__fifo); });

            //
            //
            //

            QObject::connect(
                this, &DASHThread::_hasNewFileToRead,
                [this]() {
                    this->_doStreamNewerFile();
                }
            );

            //
            emit ready();
            
            //
            this->exec();
            
            //
            _tryCleanupCacheDir();
        }
};