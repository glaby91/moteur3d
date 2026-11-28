#include "radio_audio.h"
#include "radio_audio_linux_mixer.h"
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QIODevice>
#include <QFile>
#include <QString>
#include <QDebug>
#include <QtEndian>
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace cse {
namespace {
struct Pcm {std::vector<qint16> samples; int rate=0;};
bool readWav(const std::wstring& path,Pcm& pcm){
    QFile file(QString::fromStdWString(path));
    if(!file.open(QIODevice::ReadOnly)||file.size()<44||file.size()>80*1024*1024)return false;
    const QByteArray bytes=file.readAll();
    const auto* b=reinterpret_cast<const uchar*>(bytes.constData());
    const qsizetype n=bytes.size();
    if(n<44||std::memcmp(b,"RIFF",4)||std::memcmp(b+8,"WAVE",4))return false;
    quint16 format=0,channels=0,bits=0,align=0;
    quint32 rate=0;qsizetype dataPos=-1,dataLen=0;
    bool hasFmt=false;
    for(qsizetype p=12;p+8<=n;){
        const quint32 size=qFromLittleEndian<quint32>(b+p+4);
        const qsizetype start=p+8;
        if(qsizetype(size)>n-start)return false;
        if(!std::memcmp(b+p,"fmt ",4)){
            if(size<16||hasFmt)return false;
            format=qFromLittleEndian<quint16>(b+start);
            channels=qFromLittleEndian<quint16>(b+start+2);
            rate=qFromLittleEndian<quint32>(b+start+4);
            align=qFromLittleEndian<quint16>(b+start+12);
            bits=qFromLittleEndian<quint16>(b+start+14);
            hasFmt=true;
        }else if(!std::memcmp(b+p,"data",4)){
            if(dataPos>=0)return false;
            dataPos=start;dataLen=size;
        }
        p=start+size+(size&1u);
    }
    if(!hasFmt||format!=1||channels!=1||bits!=16||align!=2||
       rate<8000||rate>48000||dataPos<0||dataLen<2||(dataLen&1))return false;
    pcm.rate=int(rate);
    pcm.samples.resize(size_t(dataLen/2));
    for(size_t i=0;i<pcm.samples.size();++i)
        pcm.samples[i]=qFromLittleEndian<qint16>(b+dataPos+i*2);
    return true;
}

// Pull-mode, continuous PCM stream. Audio thread owns cursor and smoothed gains;
// camera/UI thread publishes only atomic target gains. No locks or allocations
// in readData, no dependency on frame rendering or on network availability.
class LoopDevice final:public QIODevice {
public:
    LoopDevice(Pcm pcm,int outputRate,int channels)
      :pcm_(std::move(pcm)),rate_(outputRate),channels_(channels),
       smoothing_(1.f-std::exp(-1.f/(.025f*outputRate))){}
    void setGains(float l,float r){
        left_.store(l,std::memory_order_relaxed);
        right_.store(r,std::memory_order_relaxed);
    }
    bool isSequential() const override{return true;}
    bool atEnd() const override{return false;}
    qint64 bytesAvailable() const override{return qint64(rate_)*channels_*2/5+QIODevice::bytesAvailable();}
protected:
    qint64 writeData(const char*,qint64) override{return -1;}
    qint64 readData(char* out,qint64 len) override{
        if(len<=0)return 0;
        const float tl=left_.load(std::memory_order_relaxed);
        const float tr=right_.load(std::memory_order_relaxed);
        const int frameBytes=channels_*2;
        qint64 count=0;
        while(count<len){
            if(pendingOffset_==frameBytes){
                const size_t i=size_t(phase_/quint64(rate_));
                const size_t j=(i+1)%pcm_.samples.size();
                const float f=float(phase_%quint64(rate_))/float(rate_);
                const float sample=float(pcm_.samples[i])*(1.f-f)+float(pcm_.samples[j])*f;
                phase_=(phase_+quint64(pcm_.rate))%(quint64(pcm_.samples.size())*quint64(rate_));
                smoothL_+=(tl-smoothL_)*smoothing_;
                smoothR_+=(tr-smoothR_)*smoothing_;
                auto convert=[&](float gain){
                    return qint16(std::clamp(std::lround(sample*gain),-32768L,32767L));
                };
                const qint16 l=convert(channels_==1?(smoothL_+smoothR_)*.5f:smoothL_);
                const qint16 r=convert(smoothR_);
                // Qt Int16 uses native byte order; Linux desktop is normally LE.
                std::memcpy(pending_.data(),&l,2);
                if(channels_==2)std::memcpy(pending_.data()+2,&r,2);
                pendingOffset_=0;
            }
            const int take=int(std::min<qint64>(len-count,frameBytes-pendingOffset_));
            std::memcpy(out+count,pending_.data()+pendingOffset_,size_t(take));
            pendingOffset_+=take;count+=take;
        }
        return count;
    }
private:
    Pcm pcm_;
    int rate_,channels_;
    const float smoothing_;
    quint64 phase_=0;
    std::atomic<float> left_{0},right_{0};
    float smoothL_=0,smoothR_=0;
    std::array<char,4> pending_{};
    int pendingOffset_=channels_*2;
};
QAudioFormat outputFormat(const QAudioDevice& device,int sourceRate){
    // Prefer stereo at the device's usual rate, then common PCM rates.
    // Mono is an explicit last resort for a mono-only device.
    for(int channels:{2,1})
        for(int rate:{device.preferredFormat().sampleRate(),48000,44100,sourceRate}){
            if(rate<8000||rate>192000)continue;
            QAudioFormat f;f.setSampleRate(rate);f.setChannelCount(channels);
            f.setSampleFormat(QAudioFormat::Int16);
            if(device.isFormatSupported(f))return f;
        }
    return {};
}
}

struct RadioAudio::Impl {
    std::unique_ptr<LoopDevice> stream;
    std::unique_ptr<QAudioSink> sink;
    std::array<RadioSpeaker,4> speakers{};
    linux_audio::Stereo spatial{};
    float gain=.24f;
    void publish(){if(stream)stream->setGains(spatial.left*gain,spatial.right*gain);}
};
RadioAudio::RadioAudio():impl_(new Impl()){}
RadioAudio::~RadioAudio(){stop();}
void RadioAudio::stop(){
    // Destroy audio sink before releasing the source buffer.
    if(impl_->sink){impl_->sink->stop();impl_->sink.reset();}
    if(impl_->stream){impl_->stream->close();impl_->stream.reset();}
    impl_->spatial={};
}
bool RadioAudio::start(const std::wstring& path,const std::array<RadioSpeaker,4>& speakers){
    stop();
    Pcm pcm;
    if(!readWav(path,pcm)){qWarning()<<"Linux radio: invalid PCM WAV";return false;}
    const QAudioDevice device=QMediaDevices::defaultAudioOutput();
    if(device.isNull()){qWarning()<<"Linux radio: no audio output";return false;}
    const auto format=outputFormat(device,pcm.rate);
    if(!format.isValid()){qWarning()<<"Linux radio: no compatible PCM format";return false;}
    impl_->speakers=speakers;
    impl_->stream=std::make_unique<LoopDevice>(std::move(pcm),format.sampleRate(),format.channelCount());
    if(!impl_->stream->open(QIODevice::ReadOnly|QIODevice::Unbuffered)){stop();return false;}
    impl_->sink=std::make_unique<QAudioSink>(device,format);
    impl_->sink->setBufferSize(format.bytesForDuration(80000)); // request 80 ms, platform may adjust
    impl_->sink->start(impl_->stream.get());
    if(impl_->sink->error()!=QAudio::NoError||impl_->sink->state()==QAudio::StoppedState){
        qWarning()<<"Linux radio: output initialization failed"<<impl_->sink->error();
        stop();return false;
    }
    qInfo()<<"Linux radio local PCM:" <<device.description()<<format.sampleRate()<<format.channelCount();
    return true;
}
bool RadioAudio::playing() const {
    return impl_->sink && impl_->sink->state()!=QAudio::StoppedState &&
           impl_->sink->error()!=QAudio::OpenError && impl_->sink->error()!=QAudio::IOError &&
           impl_->sink->error()!=QAudio::FatalError;
}
void RadioAudio::setVolume(float level){
    impl_->gain=std::isfinite(level)?std::clamp(level,0.f,.6f):0.f;
    impl_->publish();
}
void RadioAudio::update(const float eye[3],const float target[3]){
    if(!playing())return;
    impl_->spatial=linux_audio::gains(impl_->speakers,eye,target);
    impl_->publish();
}
}
