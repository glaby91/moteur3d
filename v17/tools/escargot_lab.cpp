#include "../escargot/debug.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QTextStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <filesystem>
namespace e=m3d::escargot;
static int channel(float x){x=e::sat(x);const float v=x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f;return int(std::round(e::sat(v)*255));}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTextStream log(stderr);const auto args=app.arguments();
    if(args.size()<4||args.size()>7){log<<"Usage: v17_escargot_lab scene.json cameraId outputDir [rayBudget] [reflection|refraction] [debugMask]\n";return 2;}
    e::Options options;bool ok=true;
    if(args.size()>4){options.rayBudget=args[4].toUInt(&ok);if(!ok||!e::validOptions(options))return 2;}
    if(args.size()>5){if(args[5]=="refraction")options.mode=e::Mode::Refraction;else if(args[5]!="reflection")return 2;}
    uint32_t debug=0;if(args.size()>6){debug=args[6].toUInt(&ok);if(!ok||debug>31)return 2;}
    auto loaded=m3d::v16::loadScene(args[1]);if(!loaded){log<<loaded.error<<'\n';return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,2048);if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){log<<built.error<<' '<<built.runtime.lumaTraceBvhError<<'\n';return 4;}
    const auto& rt=built.runtime;const auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& v){return v.id==args[2];});
    if(it==rt.cameras.end()){log<<"Unknown camera id\n";return 5;}
    constexpr int w=1280,h=720;const auto cam=m3d::v16::makeCamera(*it,w,h);m3d::updateLighting(rt.daylightHour);
    auto result=e::run(rt,loaded.scene,cam,w,h,options);
    if(!result.ok){log<<QString::fromStdString(result.error)<<'\n';return 6;}
    const QDir out(args[3]);if(!QDir().mkpath(out.absolutePath()))return 7;
    auto save=[&](const std::vector<e::Pixel>& pixels,int iw,int ih,const QString& path){
        if(pixels.size()!=size_t(iw)*ih)return false;
        QImage image(iw,ih,QImage::Format_ARGB32);
        for(int y=0;y<ih;++y)for(int x=0;x<iw;++x){
            const auto& p=pixels[size_t(y)*iw+x];const auto c=p.alpha>1e-6f?p.rgb/p.alpha:m3d::V{};
            image.setPixel(x,y,qRgba(channel(c.x),channel(c.y),channel(c.z),int(std::round(e::sat(p.alpha*p.weight)*255))));
        }return image.save(out.filePath(path));
    };
    if(!save(e::composeScreen(result,w,h),w,h,"optical-diagnostic.png"))return 8;
    QFile report(out.filePath("metrics.json"));if(!report.open(QIODevice::WriteOnly))return 8;
    const QJsonObject data{{"mode",options.mode==e::Mode::Reflection?"reflection":"refraction"},{"rayBudget",int(options.rayBudget)},{"rays",int(result.rays)},
        {"candidates",int(result.candidates)},{"deferred",int(result.deferred)},{"paths",int(result.paths)},{"measured",double(result.measured)},{"reconstructed",double(result.reconstructed)},{"zero",double(result.zero)},
        {"outputCells",double(result.outputCells)},{"matrixMs",result.matrixMs},{"compositionMs",result.compositionMs},{"secondaryRasterPasses",0}};
    const auto bytes=QJsonDocument(data).toJson();if(report.write(bytes)!=bytes.size())return 8;
    QFile csv(out.filePath("matrices.csv"));if(!csv.open(QIODevice::WriteOnly|QIODevice::Text))return 8;
    QTextStream rows(&csv);rows<<"receiver,source,crossing,material,x,y,state,complete,u,v,depth,visibility,opacity\n";
    for(const auto& s:result.surfaces){
        if(s.image.empty())continue;
        if(!save(s.image,s.candidate.bounds.w,s.candidate.bounds.h,QString("receiver-%1.png").arg(s.candidate.object)))return 8;
        // Persist original known cells, including explicit zeros for each
        // layer. Unknown paths remain distinguishable, not fabricated misses.
        for(const auto& kv:s.layers)for(size_t i=0;i<s.samples.size();++i){
            const auto& p=s.samples[i];auto v=kv.second.measured.find(i);e::Value value;
            if(v!=kv.second.measured.end())value=v->second;else if(p.complete)value.state=e::State::Zero;
            rows<<s.candidate.object<<','<<kv.first.object<<','<<kv.first.crossing<<','<<kv.first.material<<','<<p.x<<','<<p.y<<','<<int(value.state)<<','<<int(p.complete)<<','<<value.uv.x<<','<<value.uv.y<<','<<value.depth<<','<<value.visibility<<','<<value.opacity<<'\n';
        }
    }
    if(debug){std::ofstream obj(std::filesystem::path(out.filePath("escargots.obj").toStdWString()));if(!obj)return 8;e::debug::obj(obj,e::debug::geometry(result,debug));}
    log<<"Escargot queries="<<result.rays<<" / "<<options.rayBudget<<", candidates="<<result.candidates<<", deferred="<<result.deferred<<'\n';return 0;
}
