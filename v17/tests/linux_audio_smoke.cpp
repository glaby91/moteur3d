#include "radio_audio.h"
#include "radio_audio_linux_mixer.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QString>
#include <iostream>
#include <cmath>
using namespace cse;
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    std::array<RadioSpeaker,4> speakers{};
    speakers[0]={{-5,0,0},{1,0,0}};
    for(int i=1;i<4;++i)speakers[i]={{100000.f,0,0},{-1,0,0}};
    const float eye[3]{0,0,0},forward[3]{0,0,1},back[3]{0,0,-1};
    const auto right=linux_audio::gains(speakers,eye,forward);
    const auto left=linux_audio::gains(speakers,eye,back);
    if(right.right<.8f||right.left>.01f||left.left<.8f||left.right>.01f)return 1;
    const float far[3]{35,0,0},farTarget[3]{35,0,1};
    const auto distant=linux_audio::gains(speakers,far,farTarget);
    if(distant.right>right.right*.5f)return 2;
    speakers[0].front[0]=-1.f;
    if(linux_audio::gains(speakers,eye,forward).right>right.right*.25f)return 3;
    speakers[0].front[0]=1.f;
    std::cout<<"Spatial mixer direction, distance, cone PASS"<<std::endl;
    if(argc==1)return 0;
    RadioAudio radio;
    if(radio.start(L"/no/such/wav",speakers))return 4;
    const auto path=QString::fromLocal8Bit(argv[1]).toStdWString();
    radio.setVolume(.5f);
    if(!radio.start(path,speakers))return 5;
    radio.update(eye,forward);wait(2000);
    if(!radio.playing())return 6;
    radio.update(eye,back);wait(2000);
    if(!radio.playing())return 7;
    radio.setVolume(0);wait(1000);
    radio.stop();
    if(radio.playing())return 8;
    radio.setVolume(.5f);
    if(!radio.start(path,speakers))return 9;
    radio.update(eye,forward);wait(500);radio.stop();
    std::cout<<"Linux local WAV output, update, mute, stop, restart PASS"<<std::endl;
    return 0;
}
